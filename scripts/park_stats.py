"""Estatística das liberações de park da RSX (GOWA_PARK_LOG) numa janela do log:
quantas, quanto cada uma esperou (média, mediana, p90, soma) e por endereço.

uso: python park_stats.py LOG [inicio_ms] [fim_ms]"""
import re
import sys

log = sys.argv[1]
ini = int(sys.argv[2]) if len(sys.argv) > 2 else 175000
fim = int(sys.argv[3]) if len(sys.argv) > 3 else 10 ** 12
v = []
offs = {}
pat = re.compile(r'\[park\] t=(\d+)ms off=0x([0-9A-F]+) -> 0x([0-9A-F]+) waited (\d+)us')
for ln in open(log, encoding='utf-8', errors='replace'):
    m = pat.search(ln)
    if m and ini <= int(m.group(1)) < fim:
        w = int(m.group(4))
        v.append(w)
        offs.setdefault(m.group(2), []).append(w)
if not v:
    sys.exit('nenhum park na janela')
v.sort()
n = len(v)
print(f'{n} parks; média {sum(v) / n:.0f} us, mediana {v[n // 2]}, p90 {v[int(n * .9)]}, '
      f'máx {v[-1]}, soma {sum(v) / 1000:.0f} ms')
for o, l in sorted(offs.items(), key=lambda x: -sum(x[1]))[:8]:
    print(f'  off 0x{o}: {len(l)} parks, soma {sum(l) / 1000:.0f} ms, média {sum(l) / len(l):.0f} us')
