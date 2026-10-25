"""Constantes embutidas de um fragment program da RSX (o .fp.bin despejado por
GOWA_PS_DUMP_KEY), numeradas como o decompilador numera fp_constants[]: um slot
por instrução que lê constante, na ordem do microcódigo.
uso: fp_consts.py arquivo.fp.bin [slot ...]"""
import struct
import sys

FP_BRANCH = 1 << 31       # w2: instrução de fluxo (sem constante)
FP_END = 1                # w0: última instrução


def palavra(b, o):
    be = struct.unpack_from('>I', b, o)[0]
    return ((be << 16) | (be >> 16)) & 0xFFFFFFFF


def constantes(b):
    out, off = [], 0
    while off + 16 <= len(b):
        w0, w1, w2, w3 = (palavra(b, off + 4 * i) for i in range(4))
        off += 16
        if not (w2 & FP_BRANCH) and ((w1 & 3) == 2 or (w2 & 3) == 2 or (w3 & 3) == 2):
            k = [struct.unpack('<f', struct.pack('<I', palavra(b, off + 4 * i)))[0] for i in range(4)]
            out.append(k)
            off += 16
        if w0 & FP_END:
            break
    return out


if __name__ == '__main__':
    b = open(sys.argv[1], 'rb').read()
    ks = constantes(b)
    sel = [int(x) for x in sys.argv[2:]] or range(len(ks))
    for i in sel:
        if i < len(ks):
            print('c%-3d %s' % (i, '  '.join('%12.6g' % v for v in ks[i])))
    print(len(ks), 'constantes')
