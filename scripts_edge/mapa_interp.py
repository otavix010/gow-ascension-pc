"""Das linhas [interp-entry] de um log, acha o módulo de job (base LS 0x4000) ou o plugin de cada EA e diz se
a entrada já existe como função traduzida. uso: python -I mapa_interp.py <log> <raiz gow-edge>"""
import os
import re
import sys

log, raiz = sys.argv[1], sys.argv[2]
spu = os.path.join(raiz, 'port', 'spu')
jms = sorted(int(d[7:], 16) for d in os.listdir(spu) if d.startswith('gowajm_'))
pls = sorted(int(d[7:], 16) for d in os.listdir(spu) if d.startswith('gowapl_'))
linhas = open(log, encoding='utf-8', errors='replace').read().splitlines()
ult = max(i for i, l in enumerate(linhas) if re.search(r'\[interp-entry\] [0-9.]+ M instr', l))
cache = {}


def tem(mod, ls):
    p = os.path.join(spu, mod, 'spu_recomp.c')
    if p not in cache:
        cache[p] = open(p, encoding='latin-1').read()
    return f'{mod}_spu_func_{ls:08X}__b(spu_context* ctx) {{' in cache[p]


for l in linhas[ult + 1:]:
    m = re.search(r'w(\d+) +LS 0x([0-9A-F]+): +([0-9.]+)%.*ea=0x([0-9A-F]+)', l)
    if not m:
        break
    w, ls, pct, ea = int(m[1]), int(m[2], 16), float(m[3]), int(m[4], 16)
    if pct < 0.15 or not ea:
        continue
    achou = ''
    for b in reversed(jms):
        if b <= ea and ea - b + 0x4000 == ls:
            achou = f'gowajm_{b:08X} LS 0x{ls:05X} ' + ('(já traduzida)' if tem(f'gowajm_{b:08X}', ls) else 'FALTA')
            break
    if not achou:
        for b in reversed(pls):
            if b <= ea < b + 0x2000:
                achou = f'gowapl_{b:08X} +0x{ea - b:X}'
                break
    print(f'w{w:<3} {pct:5.2f}%  ea 0x{ea:08X}  {achou or "?"}')
