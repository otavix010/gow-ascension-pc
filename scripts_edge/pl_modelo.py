"""Modelo do plugin 0x74C580 (empacotamento de vértices, passo 54) conferido contra as fotos da LS.
uso: python -I pl_modelo.py <foto_0.bin> <foto_1.bin>"""
import struct
import sys
from fractions import Fraction

def carrega(p):
    b = open(p, 'rb').read()
    return [b[i * 16:(i + 1) * 16] for i in range(128)], b[2048:]

r0, ls0 = carrega(sys.argv[1])
_, ls1 = carrega(sys.argv[2])
pref = lambda r: struct.unpack('<I', r[0:4])[0]
r3, r4, r5 = pref(r0[3]), pref(r0[4]), pref(r0[5])
be32 = lambda o: struct.unpack('>I', ls0[o:o + 4])[0]

def f32_exato(w):
    """valor exato de um float da SPU (sem inf/nan: expoente 255 é número; denormal = 0)"""
    s = -1 if w >> 31 else 1
    e = (w >> 23) & 0xFF
    m = w & 0x7FFFFF
    if e == 0:
        return Fraction(0)
    return s * Fraction(m | 0x800000) * Fraction(2) ** (e - 150)

def para_f32_rz(x):
    """Fraction -> float da SPU arredondando para zero (denormal vira 0; satura no máximo)"""
    if x == 0:
        return 0
    s = 0x80000000 if x < 0 else 0
    x = abs(x)
    e = x.numerator.bit_length() - x.denominator.bit_length()
    while Fraction(2) ** e > x: e -= 1
    while Fraction(2) ** (e + 1) <= x: e += 1
    eb = e + 127
    if eb <= 0:
        return s
    if eb > 255:
        return s | 0x7FFFFFFF
    m = int(x / Fraction(2) ** (e - 23))   # trunca
    return s | (eb << 23) | (m & 0x7FFFFF)

def fma_rz(a, b, c):
    return para_f32_rz(f32_exato(a) * f32_exato(b) + f32_exato(c))

def cflts(w, i8):
    v = f32_exato(w) * Fraction(2) ** (173 - i8)
    n = int(v)   # trunca para zero
    return max(-2 ** 31, min(2 ** 31 - 1, n))

def half(w):
    a = (w >> 16) & 0x7FFF
    h = ((((w - 0x38000000) & 0xFFFFFFFF) >> 13) & 0x7FFF) | ((w >> 16) & 0x8000)
    if a >= 0x4780:
        h = 0x7BFF | ((w >> 16) & 0x8000)
    if a < 0x3800:
        h = 0
    return h

S4 = (0x3F7FE000, 0x3F7FE000, 0x3F7FC000)
B4 = (0xBA000000, 0xBA000000, 0xBA800000)

ids = list(ls0[r3 + 0x160:r3 + 0x168])
ptr = {i: be32(r3 + 0x10 + 4 * k) for k, i in enumerate(ids) if i}

def vertice(v):
    w = {i: [be32(p + 16 * v + 4 * c) for c in range(4)] for i, p in ptr.items()}
    out = b''.join(struct.pack('>I', x) for x in w[1][:3])
    for c in range(4):
        y = fma_rz(w[3][c], 0x46FFFF00, 0xBF000000)
        out += struct.pack('>h', cflts(y, 157) >> 16)
    f = [(cflts(fma_rz(w[4][c], S4[c], B4[c]), 142) & 0xFFFFFFFF) for c in range(3)]
    pk = ((f[0] >> 21) & 0x7FF) | (((f[1] >> 21) & 0x7FF) << 11) | (((f[2] >> 22) & 0x3FF) << 22)
    out += struct.pack('>I', pk)
    out += b''.join(struct.pack('>I', x) for x in w[27][:3])
    for i in (28, 29, 30):
        out += b''.join(struct.pack('>H', half(x)) for x in w[i][:3])
    return out

dif = 0
for v in range(r4):
    m = vertice(v)
    real = ls1[r5 + 54 * v:r5 + 54 * v + 54]
    if m != real:
        dif += 1
        if dif <= 5:
            print(f'vértice {v}:\n  modelo {m.hex()}\n  real   {real.hex()}')
print(f'{r4 - dif} de {r4} vértices iguais')
fim = r5 + 54 * r4
print(f'saída 0x{r5:05X}..0x{fim:05X}; mudou além disso até 0x{max(i for i in range(0x40000) if ls0[i] != ls1[i] and i < 0x3F000):05X}')

extra = ls1[fim:fim + 16]
print('16 B depois do fim:', extra.hex(), '| antes:', ls0[fim:fim + 16].hex())
print('modelo do vértice', r4, ':', vertice(r4)[:20].hex())
print('modelo do vértice', r4 + 1, ':', vertice(r4 + 1)[:20].hex())
