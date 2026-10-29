"""Para cada foto do plugin 0x74C580: confere o modelo e mostra o que foi escrito depois do fim da saída.
uso: python -I pl_cauda.py <pasta das fotos>"""
import glob
import os
import struct
import sys
import importlib.util

pasta = sys.argv[1]
spec = importlib.util.spec_from_file_location('m', os.path.join(os.path.dirname(__file__), 'pl_modelo_lib.py'))
m = importlib.util.module_from_spec(spec)
spec.loader.exec_module(m)

ks = sorted({int(os.path.basename(p).split('_')[2]) for p in glob.glob(os.path.join(pasta, 'pl_foto_*_0.bin'))})
for k in ks:
    r0, ls0 = m.carrega(os.path.join(pasta, f'pl_foto_{k}_0.bin'))
    _, ls1 = m.carrega(os.path.join(pasta, f'pl_foto_{k}_1.bin'))
    c = m.Chamada(r0, ls0)
    dif = sum(1 for v in range(c.n) if c.vertice(v) != ls1[c.r5 + 54 * v:c.r5 + 54 * v + 54])
    fim = c.r5 + 54 * c.n
    mud = [i for i in range(fim, fim + 160) if ls0[i] != ls1[i]]
    ult = max(mud) if mud else fim - 1
    # a saída "esperada" estendida: o modelo continua pelos vértices n, n+1 (lendo além dos fluxos)
    ext = b''.join(c.vertice(v) for v in range(c.n, c.n + 2))
    print(f'chamada {k:2d}: n={c.n:4d} ({"ímpar" if c.n & 1 else "par"}) r5&15={c.r5 & 15:2d} fim&15={fim & 15:2d} '
          f'modelo {c.n - dif}/{c.n}; escrito até fim+{ult - fim + 1}')
    if mud:
        a, b = fim, ult + 1
        print(f'   depois do fim: {ls1[a:b].hex()}')
        print(f'   modelo n, n+1: {ext[:b - a].hex()}')
        print(f'   antes:         {ls0[a:b].hex()}')
        print(f'   últimos 16 B:  {ls1[fim - 16:fim].hex()}')
