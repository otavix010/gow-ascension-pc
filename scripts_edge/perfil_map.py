"""Resolve o perfil da reprodução (GOWA_CONF_PERF, <pasta>/perfil.txt: "<rva hex> <amostras>")
com o gowa.map do mesmo build: amostras por função e por grupo.

uso: python perfil_map.py <perfil.txt> <gowa.map> [n]"""
import bisect
import collections
import re
import sys

perfil, mapa = sys.argv[1], sys.argv[2]
n = int(sys.argv[3]) if len(sys.argv) > 3 else 40
BASE = 0x140000000
sim = []
for ln in open(mapa, encoding='utf-8', errors='replace'):
    m = re.match(r'\s*0001:[0-9a-fA-F]+\s+(\S+)\s+([0-9a-fA-F]{16})\s', ln)
    if m:
        sim.append((int(m.group(2), 16) - BASE, m.group(1)))
sim.sort()
ends = [a for a, _ in sim]
por = collections.Counter()
tot = 0
for ln in open(perfil):
    a, c = ln.split()
    a, c = int(a, 16), int(c)
    tot += c
    if a == 0xFFFFFFFF:
        por['(DLL: memcpy do CRT, ntdll...)'] += c
        continue
    i = bisect.bisect_right(ends, a) - 1
    por[sim[i][1] if i >= 0 else '?'] += c


def grupo(s):
    if re.match(r'gowa_(vk|ninho)_', s):
        return 'núcleos vetoriais/ninho'
    m = re.match(r'(gowajm_[0-9A-F]+|gowapl_[0-9A-F]+|gowanat_\w+?)_', s)
    if m:
        return 'SPU traduzida ' + m.group(1)
    if s.startswith(('spu_', 'mfc_', 'gowa_conf', 'yz_')):
        return 'runtime SPU'
    return 'outros (' + s[:24] + ')'


g = collections.Counter()
for s, c in por.items():
    g[grupo(s)] += c
print('%d amostras' % tot)
for s, c in g.most_common(12):
    print('%6.2f%%  %s' % (100.0 * c / tot, s))
print()
for s, c in por.most_common(n):
    print('%6.2f%%  %s' % (100.0 * c / tot, s))
