"""Modelo do plugin 0x752710 (descompressão de posição + 2 vetores unitários "smallest three") conferido
contra as fotos (out\\pl752). Por vértice, 18 bytes: posição float3 + 48 bits = 2 x (seletor 4 + 2 x 10).
Cada componente: fma(1,0 com os 10 bits na mantissa, 11595,62, -11596,33); o terceiro =
fm(1 - s, fi(1 - s, frsqest(1 - s))), s = fa(fm(a, a), fm(b, b)). O seletor diz onde entra o terceiro e os
sinais. Este script aprende o mapa seletor -> arranjo pelos dados e confere se é consistente.
uso: python -I pl752_modelo.py <pasta das fotos>"""
import collections
import glob
import importlib.util
import os
import struct
import sys

aqui = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location('si', os.path.join(aqui, 'spu_interp_c.py'))
si = importlib.util.module_from_spec(spec); spec.loader.exec_module(si)
K, B, UM = 0x46352E7C, 0xC6353150, 0x3F800000

def comp(f10):
    return si.OPS['fma']([UM | f10] * 4, [K] * 4, [B] * 4)[0]

def terceiro(a, b):
    aa = si.OPS['fm']([a] * 4, [a] * 4)[0]
    bb = si.OPS['fm']([b] * 4, [b] * 4)[0]
    s = si.OPS['fa']([aa] * 4, [bb] * 4)[0]
    u = si.OPS['fs']([UM] * 4, [s] * 4)[0]
    e = si.frsqest([u] * 4)[0]
    return si.OPS['fm']([u] * 4, si.fi([u] * 4, [e] * 4))[0]

C = 0x107A0
mapa = collections.defaultdict(collections.Counter)
erros = 0
for p0 in sorted(glob.glob(os.path.join(sys.argv[1], 'pl_foto_*_0.bin'))):
    r0, ls0 = si.carrega(p0)
    r1, ls1 = si.carrega(p0.replace('_0.bin', '_1.bin'))
    inp, n = r0[4][0] & 0x3FFF0, r0[5][0]
    base = None
    be = lambda ls, o: struct.unpack('>I', ls[o:o + 4])[0]
    # os 3 fluxos de saída: os ponteiros que o plugin grava no ctx (+0x50/+0x60): o 1o é a posição
    ptrs = [be(ls1, C + 0x10 + 4 * k) for k in range(8)]
    ids = list(ls1[C + 0x160:C + 0x168])
    pos_k = {i: ptrs[k] for k, i in enumerate(ids) if i}
    for v in range(n):
        reg = ls0[inp + 18 * v:inp + 18 * v + 18]
        bits = int.from_bytes(reg[12:18], 'big')
        vet = [((bits >> 24) & 0xF, (bits >> 38) & 0x3FF, (bits >> 28) & 0x3FF), (bits & 0xF, (bits >> 14) & 0x3FF, (bits >> 4) & 0x3FF)]
        for j, (sel, fa_, fb_) in enumerate(vet):
            a, b = comp(fa_), comp(fb_)
            c = terceiro(a, b)
            p = pos_k[3 if j == 0 else 4] & 0x3FFF0
            out = [be(ls1, p + 16 * v + 4 * k) for k in range(4)]
            # o arranjo: onde estão a, b, c (com sinal) e o w
            arr = []
            for w in out[:3]:
                if w == a: arr.append('a')
                elif w == b: arr.append('b')
                elif w == c: arr.append('c')
                elif w == c ^ 0x80000000: arr.append('-c')
                elif w == a ^ 0x80000000: arr.append('-a')
                elif w == b ^ 0x80000000: arr.append('-b')
                else: arr.append('?')
            arr.append({0x3F800000: '+1', 0xBF800000: '-1'}.get(out[3], '%08X' % out[3]))
            mapa[(j, sel)][tuple(arr)] += 1
for (j, sel), cont in sorted(mapa.items()):
    print(f'vetor {j + 1} seletor {sel:X}: ' + ', '.join(f'{list(a)} x{n}' for a, n in cont.most_common()))
