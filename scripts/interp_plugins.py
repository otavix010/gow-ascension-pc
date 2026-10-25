"""De onde vem cada entrada interpretada do [interp-entry] (GOWA_INTERP_ENTRY_LOG=1, com os
32 bytes inteiros, spu_interp.c 21/10): o código dos plugins do EDGE é copiado para um LS
diferente a cada job e a mesma rotina de biblioteca aparece em vários plugins, então o ea do
log (primeira cópia na memória) não diz qual é. Aqui: procura os 32 bytes em cada fragmento de
out/plugins_getcode.txt (ea, tamanho, LS onde caiu, workload); vale o fragmento cujo LS de
carga põe a entrada exatamente nos bytes achados (LS - LS de carga = deslocamento); sem isso,
lista todos os fragmentos que têm os bytes. Diz também se o deslocamento já é uma função da
tabela port/gowa_plugins.inc (então caiu no interpretador por outro motivo: não folha, etc).

uso: python interp_plugins.py out/<log> [--todas]"""
import os as _os_raiz
RAIZ = _os_raiz.environ.get('GOWA_RAIZ') or _os_raiz.path.dirname(_os_raiz.path.dirname(_os_raiz.path.abspath(__file__)))  # raiz do projeto
import re
import struct
import sys
from collections import defaultdict

R = RAIZ
d = open(R + '/jogo/GOWA.elf', 'rb').read()
phoff = struct.unpack_from('>Q', d, 0x20)[0]
phnum = struct.unpack_from('>H', d, 0x38)[0]
phsz = struct.unpack_from('>H', d, 0x36)[0]
segs = []
for i in range(phnum):
    p = phoff + i * phsz
    t = struct.unpack_from('>I', d, p)[0]
    off, va, _pa, fsz = struct.unpack_from('>QQQQ', d, p + 8)
    if t == 1 and fsz:
        segs.append((off, va, fsz))


def mem(ea, n):
    for off, va, fsz in segs:
        if va <= ea and ea + n <= va + fsz:
            return d[off + ea - va:off + ea - va + n]
    return b''


frags = defaultdict(set)   # (ea, sz) -> {(ls, wid)}
for ln in open(R + '/out/plugins_getcode.txt'):
    ea, sz, ls, wid = ln.split()
    frags[(int(ea, 16), int(sz, 16))].add((int(ls, 16), int(wid)))
tab = defaultdict(set)     # ea do fragmento -> deslocamentos registrados
for ln in open(R + '/port/gowa_plugins.inc', encoding='utf-8'):
    m = re.search(r'gowapl_([0-9A-F]{8})_spu_func_([0-9A-F]{8})', ln)
    if m and ln.lstrip().startswith('{'):
        tab[int(m.group(1), 16)].add(int(m.group(2), 16) - 0x30000)
for f in ('gowa_plugins_w11.inc',):
    try:
        for ln in open(R + '/port/' + f, encoding='utf-8'):
            m = re.search(r'gowapl_([0-9A-F]{8})_spu_func_([0-9A-F]{8})', ln)
            if m and ln.lstrip().startswith('{'):
                tab[int(m.group(1), 16)].add(int(m.group(2), 16) - 0x30000)
    except OSError:
        pass

linhas = [ln for ln in open(sys.argv[1], encoding='utf-8', errors='replace') if ln.startswith('[interp-entry]   w')]
if '--todas' not in sys.argv:
    # só o último relatório (cada um é um bloco de até 40 linhas após o cabeçalho)
    blocos, cur = [], []
    for ln in open(sys.argv[1], encoding='utf-8', errors='replace'):
        if ln.startswith('[interp-entry] ') and 'M instr' in ln:
            if cur:
                blocos.append(cur)
            cur = [ln]
        elif ln.startswith('[interp-entry]   w'):
            cur.append(ln)
    if cur:
        blocos.append(cur)
    if blocos:
        print(blocos[-1][0].strip())
        linhas = blocos[-1][1:]
soma = defaultdict(float)
for ln in linhas:
    m = re.match(r'\[interp-entry\]\s+w(\d+)\s+LS 0x([0-9A-F]+):\s+([\d.]+)% \((\d+) entradas, (\d+) instr/entrada\) ea=0x([0-9A-F]+) ([0-9A-F]{64})', ln)
    if not m:
        continue
    w, ls, pct, n, ipe, ea0, hx = int(m.group(1)), int(m.group(2), 16), float(m.group(3)), int(m.group(4)), int(m.group(5)), int(m.group(6), 16), bytes.fromhex(m.group(7))
    exato, cand = [], []
    for (fe, fs), cargas in frags.items():
        raw = mem(fe, fs)
        k = raw.find(hx)
        while k >= 0:
            cand.append((fe, k))
            for lsc, wid in cargas:
                if lsc + k == ls:
                    exato.append((fe, k, wid))
            k = raw.find(hx, k + 4)
    if exato:
        fe, k, wid = exato[0]
        reg = 'REGISTRADA' if k in tab.get(fe, ()) else 'nova'
        txt = f'plugin 0x{fe:08X} +0x{k:X} ({reg})'
        soma[(fe, k, reg)] += pct
    elif cand:
        txt = 'ambíguo: ' + ', '.join(f'0x{fe:08X}+0x{k:X}{"*" if k in tab.get(fe, ()) else ""}' for fe, k in cand[:5])
        for fe, k in cand[:1]:
            soma[(fe, k, 'ambíguo')] += pct
    else:
        txt = f'fora dos plugins (ea=0x{ea0:08X})'
        soma[(ea0, ls, 'fora w%d' % w)] += pct
    print(f'w{w:<2} LS 0x{ls:05X} {pct:5.2f}% {n:6d}x {ipe:6d} instr  {txt}')
print('--- por entrada')
for (fe, k, reg), p in sorted(soma.items(), key=lambda x: -x[1]):
    print(f'  {p:5.2f}%  0x{fe:08X} +0x{k:X}  {reg}')
