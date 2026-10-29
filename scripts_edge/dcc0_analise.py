"""Olha as fotos da DCC0 (entrada _0, saída _1): registradores de entrada, cabeçalho do fluxo e as regiões do LS
que a função escreveu. uso: python -I dcc0_analise.py out\\dcc0 [k ...]"""
import glob
import os
import struct
import sys

pasta = sys.argv[1]
ks = [int(x) for x in sys.argv[2:]]


def carrega(p):
    b = open(p, 'rb').read()
    return [list(struct.unpack('<4I', b[i * 16:(i + 1) * 16])) for i in range(128)], b[2048:]


for p0 in sorted(glob.glob(os.path.join(pasta, 'pl_foto_*_0.bin')), key=lambda x: int(os.path.basename(x).split('_')[2])):
    k = int(os.path.basename(p0).split('_')[2])
    if ks and k not in ks:
        continue
    r0, ls0 = carrega(p0)
    r1, ls1 = carrega(p0.replace('_0.bin', '_1.bin'))
    r3, r4, r5 = r0[3][0], r0[4][0], r0[5][0]
    hdr = ls0[r4:r4 + 16]
    print(f'== foto {k}: r3={r3:05X} r4={r4:05X} r5={r5:08X} r17={r0[17]} r18={[hex(x) for x in r0[18]]}')
    print('   cabeçalho r4:', hdr.hex(' ', 2))
    # regiões escritas
    reg = []
    i = 0
    n = len(ls0)
    while i < n:
        if ls0[i] != ls1[i]:
            j = i
            while j < n and (ls0[j] != ls1[j] or (j + 16 < n and ls0[j:j + 16] != ls1[j:j + 16])):
                j += 1
            reg.append((i, j))
            i = j
        else:
            i += 1
    for a, b in reg[:6]:
        print(f'   escrito {a:05X}..{b:05X} ({b - a} bytes): {ls1[a:a + 32].hex(" ", 2)}')
    print('   saída r3=', hex(r1[3][0]), 'r4=', hex(r1[4][0]))
