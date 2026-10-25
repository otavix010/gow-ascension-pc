"""Traduz as linhas "[hostprof-rip] 0x<RVA> <pct>%" (GOWA_HOSTPROF_RIP=1) para
função+deslocamento pelo map do linker e soma por função.

uso: python resolver_rip.py <log> [map]"""
import os as _os_raiz
RAIZ = _os_raiz.environ.get('GOWA_RAIZ') or _os_raiz.path.dirname(_os_raiz.path.dirname(_os_raiz.path.abspath(__file__)))  # raiz do projeto
import bisect
import re
import sys
from collections import defaultdict

log = sys.argv[1]
mapf = sys.argv[2] if len(sys.argv) > 2 else _os_raiz.path.join(RAIZ, r'port\build\gowa.map')

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

por_fn = defaultdict(float)
linhas = []
re_rip = re.compile(r'\[hostprof-rip\] 0x([0-9A-F]+)\s+([\d.]+)%')
for ln in open(log, encoding='utf-8', errors='replace'):
    m = re_rip.search(ln)
    if not m:
        continue
    rva, pct = int(m.group(1), 16), float(m.group(2))
    i = bisect.bisect_right(rvas, rva) - 1
    nome, ini = (syms[i][1], syms[i][0]) if i >= 0 else ('?', 0)
    por_fn[nome] += pct
    linhas.append((pct, f'{nome}+0x{rva - ini:X}', rva))
print('--- por função ---')
for nome, pct in sorted(por_fn.items(), key=lambda x: -x[1])[:25]:
    print(f'{pct:7.2f}  {nome}')
print('--- por instrução ---')
for pct, onde, rva in sorted(linhas, reverse=True)[:40]:
    print(f'{pct:6.2f}  0x{rva:08X}  {onde}')
