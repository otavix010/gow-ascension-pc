"""Casa a saída do plugin 0x74C580 (passo 54) com os fluxos de entrada, a partir das fotos da LS
(GOWA_PL_FOTO). uso: python -I pl_layout.py <foto_0.bin> <foto_1.bin> [vértices a mostrar]"""
import struct
import sys

def carrega(p):
    b = open(p, 'rb').read()
    regs = [b[i * 16:(i + 1) * 16] for i in range(128)]
    return regs, b[2048:]

r0, ls0 = carrega(sys.argv[1])
r1, ls1 = carrega(sys.argv[2])
nmostra = int(sys.argv[3]) if len(sys.argv) > 3 else 3
pref = lambda r: struct.unpack('<I', r[0:4])[0]   # _u32[0] = palavra preferida (host little-endian)
r3, r4, r5 = pref(r0[3]), pref(r0[4]), pref(r0[5])
print(f'r3=0x{r3:05X} r4={r4} r5=0x{r5:05X}')
be32 = lambda ls, o: struct.unpack('>I', ls[o:o + 4])[0]
bef = lambda ls, o: struct.unpack('>f', ls[o:o + 4])[0]

# trechos alterados
mud = [i for i in range(0x40000) if ls0[i] != ls1[i]]
faixas = []
for i in mud:
    if faixas and i <= faixas[-1][1] + 16:
        faixas[-1][1] = i
    else:
        faixas.append([i, i])
for a, b in faixas:
    print(f'alterado 0x{a:05X}..0x{b:05X} ({b - a + 1} B)')

ids = list(ls0[r3 + 0x160:r3 + 0x168])
ptr = {}
for k, i in enumerate(ids):
    if i:
        ptr[i] = be32(ls0, r3 + 0x10 + 4 * k)
print('atributos:', {i: hex(p) for i, p in ptr.items()})

def half(h):
    return struct.unpack('>e', struct.pack('>H', h))[0]

for v in range(nmostra):
    print(f'--- vértice {v}')
    for i, p in ptr.items():
        f = [bef(ls0, p + 16 * v + 4 * c) for c in range(4)]
        w = [be32(ls0, p + 16 * v + 4 * c) for c in range(4)]
        print(f'  attr {i:2d}: ' + ' '.join(f'{x:12.6g}' for x in f) + '  | ' + ' '.join(f'{x:08X}' for x in w))
    o = ls1[r5 + 54 * v:r5 + 54 * v + 54]
    print('  saída:', ' '.join(o[j:j + 2].hex() for j in range(0, 54, 2)))
    print('  como half:', ' '.join(f'{half(struct.unpack(">H", o[j:j + 2])[0]):.4g}' for j in range(0, 54, 2)))
    print('  como float:', ' '.join(f'{struct.unpack(">f", o[j:j + 4])[0]:.6g}' for j in range(0, 52, 4)))
