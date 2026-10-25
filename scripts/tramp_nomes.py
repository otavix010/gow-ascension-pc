"""Traduz as linhas [tramp] de um log com GOWA_TRAMP_STATS (destinos do trampolim do dreno,
"rva 0x...") para nomes de função pelo .map do MESMO executável, e soma por módulo e por
"cadeia" (funções de uma instrução encadeadas pelo trampolim).

uso: python tramp_nomes.py <log> <map>"""
import bisect
import re
import sys
from collections import defaultdict

log, mapf = sys.argv[1], sys.argv[2]
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
rvas = [s[0] for s in syms]
tot = None
linhas = []
for ln in open(log, encoding='utf-8', errors='replace'):
    m = re.search(r'\[tramp\] (\d+) transferencias', ln)
    if m:
        tot = int(m.group(1))
    m = re.search(r'\[tramp\]\s+([\d.]+)%\s+rva 0x([0-9A-Fa-f]+)', ln)
    if m:
        pct, rva = float(m.group(1)), int(m.group(2), 16)
        i = bisect.bisect_right(rvas, rva) - 1
        nome = syms[i][1] if i >= 0 else '?'
        linhas.append((pct, nome, rva - (syms[i][0] if i >= 0 else 0)))
print(f'{tot} transferências na janela' if tot else 'total não achado')
por_mod = defaultdict(float)
for pct, nome, off in linhas:
    print(f'{pct:6.2f}%  {nome}+0x{off:X}')
    mm = re.match(r'(gow\w+?_[0-9A-F]{8})_spu_func_([0-9A-F]{8})', nome)
    por_mod[mm.group(1) if mm else nome] += pct
print('--- por módulo (dos 40 primeiros)')
for k, v in sorted(por_mod.items(), key=lambda x: -x[1]):
    print(f'{v:6.2f}%  {k}')
