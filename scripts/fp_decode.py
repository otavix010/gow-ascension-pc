"""Decodifica um fragment program do RSX gravado por GOWA_FP_UCODE_DUMP (as
palavras com as metades de 16 bits trocadas, como o rsx_fp_read_word lê):
opcode, destino/máscara, escrita de condição, fontes com swizzle e a constante
embutida quando há. Para conferir o decompilador instrução a instrução.

uso: python fp_decode.py fp_XXXXXXXX.bin"""
import struct
import sys

OPS = {0x00: 'NOP', 0x01: 'MOV', 0x02: 'MUL', 0x03: 'ADD', 0x04: 'MAD', 0x05: 'DP3', 0x06: 'DP4',
       0x07: 'DST', 0x08: 'MIN', 0x09: 'MAX', 0x0A: 'SLT', 0x0B: 'SGE', 0x0C: 'SLE', 0x0D: 'SGT',
       0x0E: 'SNE', 0x0F: 'SEQ', 0x10: 'FRC', 0x11: 'FLR', 0x12: 'KIL', 0x13: 'PK4', 0x14: 'UP4',
       0x15: 'DDX', 0x16: 'DDY', 0x17: 'TEX', 0x18: 'TXP', 0x19: 'TXD', 0x1A: 'RCP', 0x1B: 'RSQ',
       0x1C: 'EX2', 0x1D: 'LG2', 0x1E: 'LIT', 0x1F: 'LRP', 0x20: 'STR', 0x21: 'SFL', 0x22: 'COS',
       0x23: 'SIN', 0x24: 'PK2', 0x25: 'UP2', 0x26: 'POW', 0x27: 'PKB', 0x28: 'UPB', 0x29: 'PK16',
       0x2A: 'UP16', 0x2B: 'BEM', 0x2C: 'PKG', 0x2D: 'UPG', 0x2E: 'DP2A', 0x2F: 'TXL', 0x31: 'TXB',
       0x33: 'TEXBEM', 0x34: 'TXPBEM', 0x35: 'BEMLUM', 0x36: 'REFL', 0x37: 'TIMESWTEX', 0x38: 'DP2',
       0x39: 'NRM', 0x3A: 'DIV', 0x3B: 'DIVSQ', 0x3C: 'LIF', 0x3D: 'FENCT', 0x3E: 'FENCB',
       0x40: 'BRK', 0x41: 'CAL', 0x42: 'IFE', 0x43: 'LOOP', 0x44: 'REP', 0x45: 'RET'}
C = 'xyzw'


def word(b, i):
    be = struct.unpack_from('>I', b, i)[0]
    return ((be << 16) | (be >> 16)) & 0xFFFFFFFF


def src(w, absb):
    t = w & 3
    idx = (w >> 2) & 63
    half = (w >> 8) & 1
    sw = ''.join(C[(w >> (9 + 2 * i)) & 3] for i in range(4))
    neg = '-' if (w >> 17) & 1 else ''
    name = {0: ('h' if half else 'r') + str(idx), 1: 'in', 2: 'c', 3: '?'}[t]
    s = f'{neg}{name}.{sw}'
    return f'|{s}|' if absb else s, t


b = open(sys.argv[1], 'rb').read()
off = 0
n = 0
while off + 16 <= len(b):
    w0, w1, w2, w3 = (word(b, off + 4 * i) for i in range(4))
    op = (w0 >> 24) & 63
    if w2 & (1 << 31):
        op |= 0x40   # branch group
    dst = ('h' if (w0 >> 7) & 1 else 'r') + str((w0 >> 1) & 63)
    mask = ''.join(C[i] for i in range(4) if (w0 >> (9 + i)) & 1) or 'xyzw'
    if (w0 >> 30) & 1:
        dst = '--'
    cond = ' +cc%d' % ((w1 >> 30) & 1) if (w0 >> 8) & 1 else ''
    inp = (w0 >> 13) & 15
    unit = (w0 >> 17) & 15
    s0, t0 = src(w1, (w1 >> 29) & 1)
    s1, t1 = src(w2, (w2 >> 18) & 1)
    s2, t2 = src(w3, 0)
    exec_ = ''
    if (w1 >> 18) & 7 != 7:
        exec_ = ' if(cc%d.%s %s)' % ((w1 >> 31) & 1, ''.join(C[(w1 >> (21 + 2 * i)) & 3] for i in range(4)),
                                     {1: 'LT', 2: 'EQ', 4: 'GT', 3: 'LE', 5: 'NE', 6: 'GE', 0: 'FL'}.get((w1 >> 18) & 7, '?'))
    sat = '_sat' if (w0 >> 31) & 1 else ''
    k = ''
    has_k = 2 in (t0, t1, t2) and not (w2 & (1 << 31))
    if has_k and off + 32 <= len(b):
        kw = [word(b, off + 16 + 4 * i) for i in range(4)]
        k = ' K=(' + ' '.join('%g' % struct.unpack('<f', struct.pack('<I', x))[0] for x in kw) + ')'
    print(f'{n:3d} {OPS.get(op, hex(op)):6s}{sat} {dst}.{mask}{cond}{exec_}  {s0}, {s1}, {s2}  in={inp} tex={unit}{k}')
    n += 1
    off += 32 if has_k else 16
    if w0 & 1:
        break
