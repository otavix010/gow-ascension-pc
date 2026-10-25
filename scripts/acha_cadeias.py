"""Acha cadeias para o fundir_cadeia.py: funções promovidas (X__b com #define SPILL de
promover_regs.py) cuja última transferência é um salto estático incondicional para outra
função promovida do mesmo módulo. Cada elo X -> Y custa um salto pelo trampolim (SPILL dos
escritos, dreno, chamada indireta, RELOAD dos usados). Com um perfil de RIP
(GOWA_HOSTPROF_RIP=2 + o .map do mesmo executável), ordena pelas amostras das peças.

uso: python acha_cadeias.py port/spu/<módulo> [rip.txt mapa]"""
import bisect
import re
import sys
from collections import defaultdict
from pathlib import Path

d = Path(sys.argv[1])
mod = d.name
src = (d / 'spu_recomp.c').read_bytes().decode('latin-1')
nl = '\r\n' if '\r\n' in src else '\n'
hdr = re.compile(r'^static SPU_PN void (' + mod + r'_spu_func_([0-9A-F]{8}))__b\(spu_context\* ctx\) \{', re.M)
pecas = {}
for m in hdr.finditer(src):
    ini = m.end()
    fim = src.index(nl + '}' + nl, ini)
    corpo = src[ini:fim]
    promovida = src.rfind('/* promover_regs.py */', 0, m.start()) > src.rfind('#undef RELOAD', 0, m.start())
    linhas = [l.strip() for l in corpo.split(nl) if l.strip()]
    # última linha útil antes do SPILL final (pode haver uma cópia morta do salto)
    uteis = [l for l in linhas if l not in ('SPILL;',)]
    alvo = None
    if uteis:
        ult = uteis[-1]
        mm = re.match(r'^(?:_r\d+ = spu_link\(0x[0-9A-F]+\); )?\{ ctx->pc = 0x([0-9A-F]+); g_spu_trampoline_fn = ' + mod + r'_spu_func_([0-9A-F]{8}); \{ SPILL; return; \} \}$', ult)
        if mm and int(mm.group(1), 16) == int(mm.group(2), 16):
            alvo = int(mm.group(1), 16)
    ea = int(m.group(2), 16)
    pecas[ea] = dict(nome=m.group(1), promovida=promovida, alvo=alvo, linhas=len(linhas))

amostras = defaultdict(int)
if len(sys.argv) > 3 and not sys.argv[2].startswith('--'):
    base = 0x140000000
    syms = []
    for ln in open(sys.argv[3], encoding='utf-8', errors='replace'):
        mb = re.search(r'Preferred load address is ([0-9a-fA-F]+)', ln)
        if mb:
            base = int(mb.group(1), 16)
            continue
        ms = re.match(r'^\s*[0-9a-fA-F]{4}:[0-9a-fA-F]{8}\s+(\S+)\s+([0-9a-fA-F]{16})\s', ln)
        if ms and int(ms.group(2), 16) >= base:
            syms.append((int(ms.group(2), 16) - base, ms.group(1)))
    syms.sort()
    rv = [r for r, _ in syms]
    for ln in open(sys.argv[2]):
        p = ln.split()
        try:
            r, n = int(p[0], 16), int(p[1])
        except (ValueError, IndexError):
            continue
        i = bisect.bisect_right(rv, r) - 1
        if i >= 0:
            mm = re.match(mod + r'_spu_func_([0-9A-F]{8})', syms[i][1])
            if mm:
                amostras[int(mm.group(1), 16)] += n

# cadeias maximais: começa onde ninguém da lista salta para cá (ou em qualquer peça promovida)
entra = defaultdict(int)
for ea, p in pecas.items():
    if p['alvo'] in pecas:
        entra[p['alvo']] += 1
cadeias = []
for ea, p in pecas.items():
    if not p['promovida'] or not (p['alvo'] in pecas and pecas[p['alvo']]['promovida']):
        continue
    c = [ea]
    vistos = {ea}
    while True:
        q = pecas[c[-1]]
        nx = q['alvo']
        if nx is None or nx not in pecas or not pecas[nx]['promovida'] or nx in vistos:
            break
        c.append(nx)
        vistos.add(nx)
    cadeias.append(c)
# só as maximais (não contidas como sufixo de outra)
suf = set()
for c in cadeias:
    for k in range(1, len(c)):
        suf.add(tuple(c[k:]))
cadeias = [c for c in cadeias if tuple(c) not in suf]
cadeias.sort(key=lambda c: -sum(amostras[x] for x in c))
print(f'{mod}: {len(pecas)} peças, {sum(1 for p in pecas.values() if p["promovida"])} promovidas, {len(cadeias)} cadeias')
for c in (cadeias if '--todas' in sys.argv else cadeias[:25]):
    s = sum(amostras[x] for x in c)
    print(f'  {s:6d} amostras  {len(c)} peças  {sum(pecas[x]["linhas"] for x in c):5d} linhas  ' + ' '.join(f'{x:X}' for x in c))
