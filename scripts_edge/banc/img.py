import os, struct, sys
d = open(os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', '..', 'jogo', 'GOWA.elf'), 'rb').read()
phoff = struct.unpack_from('>Q', d, 0x20)[0]; phnum = struct.unpack_from('>H', d, 0x38)[0]; phsz = struct.unpack_from('>H', d, 0x36)[0]
segs = []
for i in range(phnum):
    p = phoff + i * phsz
    t = struct.unpack_from('>I', d, p)[0]
    off, va, _pa, fsz = struct.unpack_from('>QQQQ', d, p + 8)
    if t == 1 and fsz: segs.append((off, va, fsz))
def va2f(v):
    for off, va, fsz in segs:
        if va <= v < va + fsz: return off + (v - va)
ea = int(sys.argv[1], 16); n = int(sys.argv[2], 16) if len(sys.argv) > 2 else 0x1000
f = va2f(ea)
open(sys.argv[3] if len(sys.argv) > 3 else f'pl_{ea:08X}.bin', 'wb').write(d[f:f + n])
print('ok', hex(ea), hex(f))
