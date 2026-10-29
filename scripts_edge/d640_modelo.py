"""Modelo exato da regra de descarte da D640 (conta da SPU: arredonda para zero a cada passo, fms fundido),
conferido contra as fotos. uso: python -I d640_modelo.py <pasta> [escala da grade: 1 ou 2]"""
import collections
import glob
import os
import struct
import sys
from fractions import Fraction

pasta = sys.argv[1]
ESC = Fraction(int(sys.argv[2])) if len(sys.argv) > 2 else Fraction(1)
C = 0x107A0

def car(p):
    b = open(p, 'rb').read()
    return [b[i * 16:(i + 1) * 16] for i in range(128)], b[2048:]

def f_exato(w):
    e = (w >> 23) & 0xFF
    if e == 0:
        return Fraction(0)
    v = Fraction((w & 0x7FFFFF) | 0x800000) * Fraction(2) ** (e - 150)
    return -v if w >> 31 else v

def rz(x):
    """arredonda para zero na grade do float da SPU (denormal -> 0; satura)"""
    if x == 0:
        return Fraction(0)
    s = -1 if x < 0 else 1
    a = abs(x)
    e = a.numerator.bit_length() - a.denominator.bit_length()
    while Fraction(2) ** e > a: e -= 1
    while Fraction(2) ** (e + 1) <= a: e += 1
    if e + 127 <= 0:
        return Fraction(0)
    if e + 127 > 255:
        return s * (Fraction(2) ** 128 - Fraction(2) ** 104)
    m = int(a / Fraction(2) ** (e - 23))
    return s * Fraction(m) * Fraction(2) ** (e - 23)

def trunc(x):
    return int(x)   # Fraction -> int trunca para zero

def regra(P):
    """P = [(xw, yw, flags)] em palavras; True = fica"""
    F = [p[2] & 0xFF for p in P]
    A = F[0] & F[1] & F[2]
    if A & 0xFC:
        return False
    x = [f_exato(p[0]) for p in P]
    y = [f_exato(p[1]) for p in P]
    X = [trunc(rz(v * ESC + Fraction(1, 2))) for v in x]
    Y = [trunc(rz(v * ESC + Fraction(1, 2))) for v in y]
    if X[0] == X[1] == X[2] or Y[0] == Y[1] == Y[2]:
        return False
    ex1, ey1 = rz(x[1] - x[0]), rz(y[1] - y[0])
    ex2, ey2 = rz(x[2] - x[0]), rz(y[2] - y[0])
    cr = rz(ex1 * ey2 - rz(ey1 * ex2))
    return cr >= 0

tot = collections.Counter()
for p0 in sorted(glob.glob(os.path.join(pasta, 'pl_foto_*_0.bin'))):
    k = int(os.path.basename(p0).split('_')[2])
    r0, ls0 = car(p0)
    r1, ls1 = car(os.path.join(pasta, f'pl_foto_{k}_1.bin'))
    be = lambda ls, o: struct.unpack('>I', ls[o:o + 4])[0]
    h = lambda ls, o: struct.unpack('>H', ls[o:o + 2])[0]
    nin, nout = h(ls0, C + 0x190), h(ls1, C + 0x190)
    ib = be(ls0, C + 0x50) & 0x3FFF0
    pp = be(ls0, C + 0x10 + 4 * (ls0[C] & 15)) & 0x3FFF0
    idx = [h(ls0, ib + 2 * i) for i in range(nin)]
    out = [h(ls1, ib + 2 * i) for i in range(nout)]
    modelo = []
    for t in range(nin // 3):
        tri = idx[3 * t:3 * t + 3]
        P = [(be(ls1, pp + 16 * v), be(ls1, pp + 16 * v + 4), be(ls1, pp + 16 * v + 8)) for v in tri]
        if regra(P):
            modelo += tri
    ok = modelo == out
    tot['ok' if ok else 'dif'] += 1
    print(f'chamada {k:2d}: {nin // 3} tri, real {nout // 3}, modelo {len(modelo) // 3} -> {"IGUAL" if ok else "DIFERENTE"}')
print(dict(tot))
