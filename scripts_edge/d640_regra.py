"""Deduz a regra de descarte da D640 pelas fotos (out\\d640): para cada triângulo de entrada, as
características candidatas, e qual combinação reproduz a lista compactada de saída.
uso: python -I d640_regra.py <pasta> [k ...]"""
import glob
import math
import os
import struct
import sys

pasta = sys.argv[1]
ks = [int(x) for x in sys.argv[2:]] or sorted({int(os.path.basename(p).split('_')[2]) for p in glob.glob(os.path.join(pasta, 'pl_foto_*_0.bin'))})
C = 0x107A0

def car(p):
    b = open(p, 'rb').read()
    return [b[i * 16:(i + 1) * 16] for i in range(128)], b[2048:]

def chamada(k):
    r0, ls0 = car(os.path.join(pasta, f'pl_foto_{k}_0.bin'))
    r1, ls1 = car(os.path.join(pasta, f'pl_foto_{k}_1.bin'))
    be = lambda ls, o: struct.unpack('>I', ls[o:o + 4])[0]
    h = lambda ls, o: struct.unpack('>H', ls[o:o + 2])[0]
    nin, nout = h(ls0, C + 0x190), h(ls1, C + 0x190)
    ib = be(ls0, C + 0x50) & 0x3FFF0
    slot = ls0[C] & 15
    pp = be(ls0, C + 0x10 + 4 * slot) & 0x3FFF0
    idx = [h(ls0, ib + 2 * i) for i in range(nin)]
    out = [h(ls1, ib + 2 * i) for i in range(nout)]
    pos = lambda v: (struct.unpack('>f', ls1[pp + 16 * v:pp + 16 * v + 4])[0],
                     struct.unpack('>f', ls1[pp + 16 * v + 4:pp + 16 * v + 8])[0],
                     be(ls1, pp + 16 * v + 8))
    return dict(k=k, nin=nin, nout=nout, idx=idx, out=out, pos=pos, ls0=ls0, ib=ib, ctx0=ls0[C:C + 16],
                ctx140=ls0[C + 0x140:C + 0x150], r80=struct.unpack('<I', r0[80][:4])[0])

def tris(c):
    return [tuple(c['idx'][3 * t:3 * t + 3]) for t in range(c['nin'] // 3)]

for k in ks:
    c = chamada(k)
    T = tris(c)
    kept_out = [tuple(c['out'][3 * t:3 * t + 3]) for t in range(c['nout'] // 3)]
    # a saída deve ser uma subsequência da entrada
    j = 0; kept = []
    for t in T:
        if j < len(kept_out) and t == kept_out[j]:
            kept.append(True); j += 1
        else:
            kept.append(False)
    sub = j == len(kept_out)
    print(f'chamada {k}: {len(T)} tri -> {len(kept_out)} | subsequência: {sub} | ctx0 {c["ctx0"].hex()} ctx140[13]={c["ctx140"][13]:02X} r80={c["r80"]}')
    st = {}
    for t, kp in zip(T, kept):
        pa, pb, pc = (c['pos'](v) for v in t)
        fand = pa[2] & pb[2] & pc[2]
        X = [math.trunc(p[0] + 0.5) for p in (pa, pb, pc)]
        Y = [math.trunc(p[1] + 0.5) for p in (pa, pb, pc)]
        zx = X[0] == X[1] == X[2]
        zy = Y[0] == Y[1] == Y[2]
        cr = (pb[0] - pa[0]) * (pc[1] - pa[1]) - (pb[1] - pa[1]) * (pc[0] - pa[0])
        chave = (bool(fand), zx or zy, 'cr>0' if cr > 0 else ('cr<0' if cr < 0 else 'cr=0'))
        st.setdefault(chave, [0, 0])[0 if kp else 1] += 1
    for ch, (a, b) in sorted(st.items()):
        print(f'   recorte_comum={ch[0]!s:5} area_zero={ch[1]!s:5} {ch[2]}: {a} ficaram, {b} saíram')
