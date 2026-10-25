"""Analisa os despejos de GOWA_FNDUMP (out/fndump/<fn>_<k>_in|out.bin): registradores
de argumento, regiões da LS que mudaram (em quadwords, relativas ao sp e aos
ponteiros dos argumentos) e os floats envolvidos.

uso: python fndump_analisa.py 73E0 [k ...] [--floats]"""
import os as _os_raiz
RAIZ = _os_raiz.environ.get('GOWA_RAIZ') or _os_raiz.path.dirname(_os_raiz.path.dirname(_os_raiz.path.abspath(__file__)))  # raiz do projeto
import glob
import os
import struct
import sys

D = _os_raiz.path.join(RAIZ, r'out\fndump')
fn = sys.argv[1].upper().rjust(4, '0')
ks = [int(x) for x in sys.argv[2:] if x.isdigit()]
mostra_floats = '--floats' in sys.argv


def le(path):
    b = open(path, 'rb').read()
    gpr = [struct.unpack_from('<4I', b, 16 * i) for i in range(128)]
    ls = b[2048:2048 + 0x40000]
    return gpr, ls


def palavra(ls, a):
    return struct.unpack_from('>I', ls, a)[0]


def f32(w):
    return struct.unpack('<f', struct.pack('<I', w))[0]


pares = sorted(glob.glob(os.path.join(D, f'{fn}_*_in.bin')))
for p_in in pares:
    k = int(os.path.basename(p_in).split('_')[1])
    if ks and k not in ks:
        continue
    p_out = p_in.replace('_in.bin', '_out.bin')
    if not os.path.exists(p_out):
        continue
    gi, li = le(p_in)
    go, lo = le(p_out)
    sp = gi[1][0]
    print(f'=== chamada {k}: sp=0x{sp:05X}')
    for r in range(3, 15):
        w = gi[r]
        print(f'  r{r:<3d} {w[0]:08X} {w[1]:08X} {w[2]:08X} {w[3]:08X}   ({f32(w[0]):.6g})')
    print(f'  saída r3 {go[3][0]:08X} {go[3][1]:08X} {go[3][2]:08X} {go[3][3]:08X}')
    # regiões alteradas
    mud = [q for q in range(0, 0x40000, 16) if li[q:q + 16] != lo[q:q + 16]]
    reg = []
    for q in mud:
        if reg and q == reg[-1][1]:
            reg[-1][1] = q + 16
        else:
            reg.append([q, q + 16])
    ptrs = {f'r{r}': gi[r][0] for r in range(3, 15) if 0x1000 <= gi[r][0] < 0x40000}
    print(f'  {len(mud)} quadwords mudaram em {len(reg)} regiões:')
    for a, b in reg:
        rel = []
        if sp - 0x1000 <= a < sp + 0x100:
            rel.append(f'sp{a - sp:+#x}')
        for n, v in ptrs.items():
            if v <= a < v + 0x2000:
                rel.append(f'{n}+0x{a - v:X}')
        print(f'    0x{a:05X}..0x{b:05X} ({(b - a) // 16:4d} qw) {" ".join(rel)}')
        if mostra_floats and (b - a) <= 0x200:
            for q in range(a, b, 16):
                ws = [palavra(lo, q + 4 * i) for i in range(4)]
                print(f'        {q:05X}: ' + ' '.join(f'{f32(w):11.5g}' for w in ws))
