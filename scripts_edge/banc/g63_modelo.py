"""Modelo do laço 0x63B4 do General6 (0x727200), pelo traço simbólico do corpo (16 voltas de 4 amostras):
  V = os 4 vetores r88, r85, r83, r82; a cada amostra V <- M1 V (M1 = colunas r99, r107, r98, r108; c = lane)
     V'.c = fma(r108.c, V.w, fma(r98.c, V.z, fma(r107.c, V.y, r99.c * V.x)))
  W = M2 V (colunas r104, r103, r102, r101, a mesma forma); ponto da oitava o, coordenada c (x, y, z):
     p.c = fma(W82.c, A3.c, fma(W83.c, A2.c, fma(W85.c, A1.c, W88.c * A0.c)))
     (A0..A3) = (r122, r123, r124, r121), (r120, r117, r118, r119), (r115, r116, r113, r114), (r112, r110, r111, r109)
  saída[s] = (((entrada[s] + N1) + N2) + N3) + N4, gravada no lugar (r125 + 4s; r125 += 16 por volta)
  meia-precisão: a = saída * K0[s] (pilha 0x8E0), f = fma(a, K1.x, K2.x) (0x8D0, 0x8C0), limitada a [r105.x, r106.x],
     h = (f * E.x (0x9C0)) * 2^-112; half = ((h & 0x80000000) | (h << 3)) >> 16
     grava 8 bytes em r100 + 8s: r127.x, r127.y | half (r100 += 32 por volta); contador 0x9B0 += 1 (as 4 palavras)
Laço até o contador (pilha 0x9B0) chegar ao 0x870. Confere com as fotos de entrada e saída do laço (LS inteira).

uso: python -I g63_modelo.py <pasta das fotos>"""
import glob
import os
import struct
import sys

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import spu_interp_c as O
from g6_modelo import ruido, s1

M32 = 0xFFFFFFFF
OITAVAS = ((122, 123, 124, 121), (120, 117, 118, 119), (115, 116, 113, 114), (112, 110, 111, 109))


def matvec(r, cols, V):   # cols = (a, b, c, d): fma(d.c, V.w, fma(c.c, V.z, fma(b.c, V.y, a.c * V.x)))
    a, b, c, d = (r[k] for k in cols)
    return [s1('fma', d[i], V[3], s1('fma', c[i], V[2], s1('fma', b[i], V[1], s1('fm', a[i], V[0])))) for i in range(4)]


def be(ls, a): return struct.unpack('>I', bytes(ls[a:a + 4]))[0]
def pbe(ls, a, v): ls[a:a + 4] = struct.pack('>I', v & M32)


def laco(r, ls):
    """executa o laço sobre (regs de entrada, LS bytearray); devolve os regs que mudam"""
    r = [list(x) for x in r]
    sp = r[1][0]
    V = {k: list(r[k]) for k in (88, 85, 83, 82)}
    while True:
        r125, r100 = r[125][0], r[100][0]
        for s in range(4):
            W = {k: matvec(r, (104, 103, 102, 101), V[k]) for k in V}
            acc = be(ls, r125 + 4 * s)
            for o, (a0, a1, a2, a3) in enumerate(OITAVAS):
                p = [s1('fma', W[82][c], r[a3][c], s1('fma', W[83][c], r[a2][c],
                     s1('fma', W[85][c], r[a1][c], s1('fm', W[88][c], r[a0][c])))) for c in range(3)]
                acc = s1('fa', acc, ruido(ls, *p))
            pbe(ls, r125 + 4 * s, acc)
            a = s1('fm', acc, be(ls, sp + 0x8E0 + 4 * s))
            f = s1('fma', a, be(ls, sp + 0x8D0), be(ls, sp + 0x8C0))
            if O.OPS['fcgt']([f] * 4, [r[106][0]] * 4)[0]: f = r[106][0]
            if not O.OPS['fcgt']([f] * 4, [r[105][0]] * 4)[0]: f = r[105][0]
            h = s1('fm', s1('fm', be(ls, sp + 0x9C0), f), 0x07800000)
            half = ((h & 0x80000000) | ((h << 3) & M32)) >> 16
            pbe(ls, r100 + 8 * s, r[127][0])
            pbe(ls, r100 + 8 * s + 4, r[127][1] | half)
            V = {k: matvec(r, (99, 107, 98, 108), V[k]) for k in V}
        for i in range(4): pbe(ls, sp + 0x9B0 + 4 * i, be(ls, sp + 0x9B0 + 4 * i) + 1)
        r[125] = [(x + 16) & M32 for x in r[125]]
        r[100] = [(x + 32) & M32 for x in r[100]]
        if be(ls, sp + 0x9B0) == be(ls, sp + 0x870): break
    for k in V: r[k] = V[k]
    # depois do laço (antes do trampolim para 0x6A90): 0x9F0 += 32 * n e 0x860 += 16 * n (n = 0x870)
    n = be(ls, sp + 0x870)
    for i in range(4):
        pbe(ls, sp + 0x9F0 + 4 * i, be(ls, sp + 0x9F0 + 4 * i) + ((be(ls, sp + 0x870 + 4 * i) << 5) & M32))
        pbe(ls, sp + 0x860 + 4 * i, be(ls, sp + 0x860 + 4 * i) + ((be(ls, sp + 0x870 + 4 * i) << 4) & M32))
    return r


def main():
    pasta = sys.argv[1]
    ok = dif = 0
    for p0 in sorted(glob.glob(os.path.join(pasta, 'pl_foto_*_0.bin')), key=lambda x: int(x.split('_')[-2])):
        r0, ls0 = O.carrega(p0)
        r1, ls1 = O.carrega(p0.replace('_0.bin', '_1.bin'))
        ls = bytearray(ls0)
        r = laco(r0, ls)
        sp = r0[1][0]
        # a LS: tudo igual, menos a pilha que a chamada do ruído e o laço usam como rascunho (0xB70 e abaixo do sp)
        difs = [a for a in range(0, len(ls), 4) if ls[a:a + 4] != ls1[a:a + 4] and not (sp - 0x400 <= a < sp) and a != sp + 0xB70]
        regs = [k for k in (88, 85, 83, 82) if r[k] != r1[k]]
        if not difs and not regs: ok += 1
        else:
            dif += 1
            if dif <= 4:
                print('difere', os.path.basename(p0), 'regs', regs, 'LS', [hex(a) for a in difs[:8]], len(difs))
                for a in difs[:4]: print('   ', hex(a), 'modelo', ls[a:a + 4].hex(), 'foto', ls1[a:a + 4].hex(), 'entrada', ls0[a:a + 4].hex())
    print(f'modelo x fotos: {ok} laços iguais, {dif} diferentes')


if __name__ == '__main__':
    main()
