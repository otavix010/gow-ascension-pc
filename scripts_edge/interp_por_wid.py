"""Instruções interpretadas por workload (M por 10 s), na última janela [interp-entry] de cada log."""
import re
import sys

for log in sys.argv[1:]:
    ls = open(log, encoding='utf-8', errors='replace').read().splitlines()
    ult = max(i for i, l in enumerate(ls) if re.search(r'\[interp-entry\] [0-9.]+ M instr', l))
    tot = float(re.search(r'\] ([0-9.]+) M instr', ls[ult])[1])
    por = {}
    for l in ls[ult + 1:]:
        m = re.search(r'w(\d+) +LS 0x[0-9A-F]+: +([0-9.]+)%', l)
        if not m:
            break
        por[int(m[1])] = por.get(int(m[1]), 0) + float(m[2]) * tot / 100
    top = ', '.join(f'w{w} {v:.1f}' for w, v in sorted(por.items(), key=lambda x: -x[1])[:8])
    print(f'{log.split(chr(92))[-1]}: total {tot:.1f} M/10 s (top 40 somam {sum(por.values()):.1f}); {top}')
