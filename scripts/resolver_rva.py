"""Traduz as linhas "[hostprof] ... rt RVA 0x..." de um log para nomes de função usando o
map do linker (port/build/gowa.map, gerado com /MAP) e soma por função entre as threads.

uso: python resolver_rva.py <log> [map]"""
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

tot = defaultdict(float)
re_rt = re.compile(r'\[hostprof\]\s+([\d.]+)%\s+rt RVA 0x([0-9A-F]+)')
for ln in open(log, encoding='utf-8', errors='replace'):
    m = re_rt.search(ln)
    if not m:
        continue
    pct, rva = float(m.group(1)), int(m.group(2), 16)
    i = bisect.bisect_right(rvas, rva) - 1
    nome = syms[i][1] if i >= 0 else '?'
    tot[nome] += pct
for nome, pct in sorted(tot.items(), key=lambda x: -x[1])[:30]:
    print(f'{pct:7.1f}  {nome}')
