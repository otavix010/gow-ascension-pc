"""Fotos consecutivas da LS de um job (out/fntrace/seq_<n>_<fn>.bin, GOWA_FNTRACE_LS=*): para cada
trecho entre duas entradas de estágio, as faixas de LS escritas (unidas com folga de 64 B), com o
tamanho -- o mapa de quem escreve onde no job.

uso: python ls_seq.py [pasta] [--faixas N]"""
import os as _os_raiz
RAIZ = _os_raiz.environ.get('GOWA_RAIZ') or _os_raiz.path.dirname(_os_raiz.path.dirname(_os_raiz.path.abspath(__file__)))  # raiz do projeto
import glob
import os
import re
import sys

d = sys.argv[1] if len(sys.argv) > 1 and not sys.argv[1].startswith('--') else _os_raiz.path.join(RAIZ, r'out/fntrace')
nf = int(sys.argv[sys.argv.index('--faixas') + 1]) if '--faixas' in sys.argv else 6
arqs = sorted(glob.glob(os.path.join(d, 'seq_*.bin')))
ant = None
for a in arqs:
    m = re.search(r'seq_(\d+)_([0-9A-F]+)\.bin', a)
    ls = open(a, 'rb').read()[2048:]
    if ant is not None:
        la, fa = ant
        fx = []
        i = 0
        n = len(ls)
        while i < n:
            if la[i] == ls[i]:
                i += 1
                continue
            s = i
            k = i
            j = i
            while j < n and j - k < 64:
                if la[j] != ls[j]:
                    k = j
                j += 1
            fx.append((s, k + 1))
            i = k + 1
        fx = [f for f in fx if f[0] < 0x3F000]   # fora a pilha
        tot = sum(e - s for s, e in fx)
        if tot:
            print(f'{fa:>4s} -> {m.group(2):>4s}: {tot:6d} B em {len(fx):3d} faixas: ' +
                  ' '.join(f'{s:05X}+{e - s:X}' for s, e in sorted(fx, key=lambda f: -(f[1] - f[0]))[:nf]))
    ant = (ls, m.group(2))
