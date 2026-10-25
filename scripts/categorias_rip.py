"""Divide as amostras de GOWA_HOSTPROF_RIP=2 (arquivo "rva n") por categoria e
por função, pelo .map do MESMO executável.

uso: python categorias_rip.py <rip.txt> <map> [N funções]"""
import bisect
import re
import sys
from collections import defaultdict

rip, mapf = sys.argv[1], sys.argv[2]
top = int(sys.argv[3]) if len(sys.argv) > 3 else 40
base = 0x140000000
syms = []
re_base = re.compile(r'Preferred load address is ([0-9a-fA-F]+)')
re_sym = re.compile(r'^\s*[0-9a-fA-F]{4}:[0-9a-fA-F]{8}\s+(\S+)\s+([0-9a-fA-F]{16})\s')
for ln in open(mapf, encoding='utf-8', errors='replace'):
    m = re_base.search(ln)
    if m:
        base = int(m.group(1), 16)
        continue
    m = re_sym.match(ln)
    if m:
        va = int(m.group(2), 16)
        if va >= base:
            syms.append((va - base, m.group(1)))
syms.sort()
rvas = [r for r, _ in syms]


def nome(rva):
    i = bisect.bisect_right(rvas, rva) - 1
    return syms[i][1] if i >= 0 else '?'


def categoria(n):
    if n.startswith('gowajm_0072DF80'):
        return 'job 0x72DF80 (montador de comandos)'
    if n.startswith('gowajm_'):
        return 'outros jobs (gowajm_*)'
    if n.startswith('gowapl_'):
        return 'plugins (gowapl_*)'
    if n.startswith('gowapm_') or n.startswith('gowatspm'):
        return 'policy modules (gowapm_*)'
    if re.match(r'spu_[0-9a-f]{16}', n):
        return 'tarefas SPU (spu_<sha>)'
    if 'interp' in n:
        return 'runtime SPU: interpretador'
    if n.startswith(('spu_', 'mfc_', 'sys_spu', 'spurs')):
        return 'runtime SPU: ' + n.split('+')[0]
    if n.startswith('?func_') or n.startswith('func_'):
        return 'PPU traduzido'
    if n.startswith(('rsx', 'ld_', 'gcm', 'sink', 'fetch', 'get_pso', 'texture', 'live_')):
        return 'RSX'
    return 'outros: ' + n


tot = 0
porcat = defaultdict(int)
porfn = defaultdict(int)
for ln in open(rip):
    if ln.startswith('#'):
        continue
    r, n = ln.split()
    n = int(n)
    nm = nome(int(r, 16))
    tot += n
    porfn[nm] += n
    c = categoria(nm)
    if c.startswith('runtime SPU: '):
        porcat['runtime SPU (total)'] += n
    porcat[c] += n
print(f'{tot} amostras')
for c, n in sorted(porcat.items(), key=lambda x: -x[1])[:30]:
    print(f'{100.0 * n / tot:6.2f}%  {c}')
print('--- funções ---')
for f, n in sorted(porfn.items(), key=lambda x: -x[1])[:top]:
    print(f'{100.0 * n / tot:6.2f}%  {f}')
