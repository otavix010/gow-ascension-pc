"""Modelo do trecho loc_00008B28 -> loc_00008D50 da 0x8930 (job 0x72DF80) contra as fotos da sonda (sonda_rotulos.py).

O laço 8B90 compacta registros de 8 bytes: para cada entrada i < cnt (cnt = byte em r35, relido a cada volta),
e = LS[r33 + i]; e == 0xFF pula. Senão os bytes 8..15 do registro (origem r27 = r35 + 8i) vão para o destino r28,
byte a byte em ordem; destino+8 = r32 (byte baixo), destino+13 = e; o destino só avança (+8, r34 += 1,
r32 += byte 12 do registro) se não for (byte 11 - 27) & 0xFF <= 3 com o bit 4 de r36 desligado.
uso: python -I f8930_modelo.py <pasta das fotos>"""
import glob
import importlib.util
import os
import sys

aqui = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location('si', os.path.join(aqui, 'spu_interp_c.py'))
si = importlib.util.module_from_spec(spec); spec.loader.exec_module(si)
M = 0x3FFFF


def modelo(g, ls):
    g = [list(r) for r in g]; ls = bytearray(ls)
    b = lambda a: ls[a & M]
    def put(a, v): ls[a & M] = v & 0xFF
    r1 = g[1][0]
    put(0x10A30 + (r1 & 15), g[6][0])
    put((g[37][0] & ~15) + ((r1 + 1) & 15), b((g[31][0] & ~15) + 1))
    put((g[37][0] & ~15) + ((r1 + 2) & 15), g[6][0])
    g[8][0] = 0; g[9][0] = 0
    cnt = b(g[35][0] & ~15)
    if cnt == 0:
        return g, ls, 0
    r36 = g[36][0] & 4
    r33, r35 = g[33][0], g[35][0]
    i, r27, r28, r32, r34 = 0, r35, (0xC9A0 + g[126][0]) & 0xFFFFFFFF, 0, 0
    voltas = 0
    while True:
        voltas += 1
        a = (i + r33) & 0xFFFFFFFF
        if b(a) != 0xFF:
            for k in range(8, 16):
                put(r28 + k, b(r27 + k))
            tipo = (b(r27 + 11) - 27) & 0xFF
            put(r28 + 8, r32)
            put(r28 + 13, b(a))
            fica = not (tipo <= 3 and r36 == 0)
            c = b(r27 + 12)
            if fica:
                r28 = (r28 + 8) & 0xFFFFFFFF; r32 = (r32 + c) & 0xFFFFFFFF; r34 = (r34 + 1) & 0xFFFFFFFF
            cnt = b(r35 & ~15)
        i += 1; r27 = (r27 + 8) & 0xFFFFFFFF
        if not (cnt > i):
            break
    g[8][0] = r34; g[9][0] = r32
    g[28][0] = r28; g[32][0] = r32; g[34][0] = r34; g[29][0] = i; g[27][0] = r27
    return g, ls, voltas


def main():
    pasta = sys.argv[1]
    ok = 0; ks = sorted({int(os.path.basename(f).split('_')[2]) for f in glob.glob(os.path.join(pasta, 'pl_foto_*_0.bin'))})
    for k in ks:
        r0, ls0 = si.carrega(os.path.join(pasta, f'pl_foto_{k}_0.bin'))
        r1, ls1 = si.carrega(os.path.join(pasta, f'pl_foto_{k}_1.bin'))
        g, ls, v = modelo(r0, ls0)
        dif = [a for a in range(len(ls1)) if ls[a] != ls1[a]]
        regs = [n for n in (8, 9, 27, 28, 29, 32, 34) if (g[n][0] & 0xFFFFFFFF) != (r1[n][0] & 0xFFFFFFFF)]
        mud = sum(1 for a in range(len(ls1)) if ls0[a] != ls1[a])
        bom = not dif and not regs
        ok += bom
        print(f'foto {k}: voltas {v}, bytes mudados {mud}, LS dif {len(dif)}{" " + hex(dif[0]) if dif else ""}, regs dif {regs}')
    print(f'{ok}/{len(ks)} iguais')


main()
