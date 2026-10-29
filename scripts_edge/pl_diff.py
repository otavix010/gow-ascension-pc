"""Mostra registradores de entrada e as faixas da LS alteradas entre as fotos de um plugin.
uso: python -I pl_diff.py <pasta> <k> [regs separados por vírgula]"""
import os
import struct
import sys

pasta, k = sys.argv[1], int(sys.argv[2])
regs = [int(x) for x in (sys.argv[3] if len(sys.argv) > 3 else '1,2,3,4,5,6,7,8,23,44').split(',')]

def carrega(p):
    b = open(p, 'rb').read()
    return [b[i * 16:(i + 1) * 16] for i in range(128)], b[2048:]

r0, ls0 = carrega(os.path.join(pasta, f'pl_foto_{k}_0.bin'))
r1, ls1 = carrega(os.path.join(pasta, f'pl_foto_{k}_1.bin'))
for r in regs:
    w = struct.unpack('<4I', r0[r])
    print(f'r{r:<3d} entrada ' + ' '.join(f'{x:08X}' for x in w) + '   saída ' +
          ' '.join(f'{x:08X}' for x in struct.unpack('<4I', r1[r])))
mud = [i for i in range(0x40000) if ls0[i] != ls1[i]]
faixas = []
for i in mud:
    if faixas and i <= faixas[-1][1] + 16:
        faixas[-1][1] = i
    else:
        faixas.append([i, i])
print(f'{len(faixas)} faixas alteradas')
for a, b in faixas[:30]:
    print(f'  0x{a:05X}..0x{b:05X} ({b - a + 1} B)')
