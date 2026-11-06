"""Modelo do 0x4930 do General6 (0x727200): ruído de Perlin melhorado em 4 pontos (r3..r6 = x, y, z, _),
saída r3 = o ruído de cada ponto. Contas de float com a semântica da SPU (OPS do spu_interp_c.py: arredonda
para zero, denormal vira +0), na ordem que o traço (g6_traco.py --traco) mostrou. Confere com as fotos.

uso: python -I g6_modelo.py <pasta das fotos>"""
import glob
import os
import struct
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import spu_interp_c as O

F = O.OPS
UM, ZERO = 0x3F800000, 0


def s1(op, *a):   # conta de float num valor escalar (palavra de 32 bits) pela semântica da SPU
    return F[op](*[[x] * 4 for x in a])[0]


def piso(x):
    """x - (0,99999994 se negativo), truncado pelos bits (a mesma sequência inteira do original); devolve
    (parte inteira truncada em float, fração)"""
    neg = 0x3F7FFFFF if x >> 31 else 0
    t = s1('fs', x, neg)
    e = (t >> 23) & 255
    n = (127 - e) & O.M32
    m = O.rotm(0x007FFFFF, n) if O.sx(n, 32) > -31 else 0
    if O.sx(n - 1, 32) >= 0:   # |t| < 1: some tudo
        m = O.M32
    tr = t & ~m & O.M32
    return tr, s1('fs', x, tr)


def fade(t):
    return s1('fm', s1('fm', s1('fm', t, t), t), s1('fma', t, s1('fms', t, 0x40C00000, 0x41700000), 0x41200000))


def ruido(ls, x, y, z):
    p = struct.unpack('>512I', ls[0x8680:0x8680 + 2048])
    g = [struct.unpack('>4I', ls[0x8580 + 16 * i:0x8590 + 16 * i]) for i in range(16)]
    (tx, fx), (ty, fy), (tz, fz) = piso(x), piso(y), piso(z)
    X, Y, Z = (O.cflts([w] * 4, 173)[0] & 255 for w in (tx, ty, tz))
    u, v, w = fade(fx), fade(fy), fade(fz)
    A = p[X] + Y; AA = p[A] + Z; AB = p[A + 1] + Z
    B = p[X + 1] + Y; BA = p[B] + Z; BB = p[B + 1] + Z
    M1 = 0xBF800000   # -1

    def d(a, o):   # a + o (o = 0 ou -1)
        return s1('fa', a, o)

    def grad(h, ox, oy, oz):
        G = g[p[h] & 15] if h < 512 else None
        return s1('fa', s1('fa', s1('fm', d(fx, ox), G[0]), s1('fm', d(fy, oy), G[1])), s1('fm', d(fz, oz), G[2]))

    def lerp(t, a, b):
        return s1('fma', s1('fs', b, a), t, a)

    return lerp(w, lerp(v, lerp(u, grad(AA, 0, 0, 0), grad(BA, M1, 0, 0)),
                        lerp(u, grad(AB, 0, M1, 0), grad(BB, M1, M1, 0))),
                lerp(v, lerp(u, grad(AA + 1, 0, 0, M1), grad(BA + 1, M1, 0, M1)),
                        lerp(u, grad(AB + 1, 0, M1, M1), grad(BB + 1, M1, M1, M1))))


def main():
    pasta = sys.argv[1]
    ok = dif = 0
    for p0 in sorted(glob.glob(os.path.join(pasta, 'pl_foto_*_0.bin'))):
        r0, ls0 = O.carrega(p0)
        r1, _ = O.carrega(p0.replace('_0.bin', '_1.bin'))
        for k in range(4):
            pt = r0[3 + k]
            got = ruido(ls0, pt[0], pt[1], pt[2])
            if got == r1[3][k]: ok += 1
            else:
                dif += 1
                if dif <= 5: print('difere', os.path.basename(p0), k, hex(got), hex(r1[3][k]))
    print(f'modelo x fotos: {ok} pontos iguais, {dif} diferentes')


if __name__ == '__main__':
    main()
