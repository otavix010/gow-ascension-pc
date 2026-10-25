"""Executa um fragment program do RSX (gravado por GOWA_FP_UCODE_DUMP) para um
pixel, com entradas dadas, e imprime cada instrução com o resultado: para
achar onde o shader traduzido diverge do esperado.

uso: python fp_emula.py fp.bin constantes.txt entradas.json
  constantes.txt: a linha [fpconst] do log (c0=(...) c1=(...) ...), na ordem dos slots
  entradas.json : {"in": {"4": [x,y,z,w], ...}, "tex": {"0": [r,g,b,a], ...},
                   "gamma": {"0": 7}, "exp": [unidades]}
  (TEX devolve o valor fixo da unidade; gamma aplica sRGB->linear nos canais)"""
import json
import math
import re
import struct
import sys

C = 'xyzw'


def word(b, i):
    be = struct.unpack_from('>I', b, i)[0]
    return ((be << 16) | (be >> 16)) & 0xFFFFFFFF


def srgb2lin(v):
    return v / 12.92 if v <= 0.04045 else ((v + 0.055) / 1.055) ** 2.4


def lin2srgb(v):
    v = min(max(v, 0.0), 1.0)
    return v * 12.92 if v < 0.0031308 else 1.055 * v ** (1 / 2.4) - 0.055


b = open(sys.argv[1], 'rb').read()
consts = [[float(x) for x in m.group(1).split()] for m in re.finditer(r'c\d+=\(([^)]*)\)', open(sys.argv[2]).read())]
cfg = json.load(open(sys.argv[3]))
R = [[0.0] * 4 for _ in range(64)]
H = [[0.0] * 4 for _ in range(64)]
CC = [[0.0] * 4, [0.0] * 4]
slot = 0


def rd(w, absb, kv):
    t = w & 3
    idx = (w >> 2) & 63
    half = (w >> 8) & 1
    if t == 0:
        v = (H if half else R)[idx]
    elif t == 1:
        v = cfg['in'].get(str(INP), [0, 0, 0, 0])
    elif t == 2:
        v = kv
    else:
        v = [0, 0, 0, 0]
    sw = [(w >> (9 + 2 * i)) & 3 for i in range(4)]
    o = [v[s] for s in sw]
    if absb:
        o = [abs(x) for x in o]
    if (w >> 17) & 1:
        o = [-x for x in o]
    return o


off = 0
n = 0
outs = {}
while off + 16 <= len(b):
    w0, w1, w2, w3 = (word(b, off + 4 * i) for i in range(4))
    op = (w0 >> 24) & 63
    INP = (w0 >> 13) & 15
    unit = (w0 >> 17) & 15
    has_k = any(((w >> 0) & 3) == 2 for w in (w1, w2, w3)) and not (w2 & (1 << 31))
    kv = [0, 0, 0, 0]
    if has_k:
        kv = consts[slot] if slot < len(consts) else [0, 0, 0, 0]
        slot += 1
    a = rd(w1, (w1 >> 29) & 1, kv)
    bb = rd(w2, (w2 >> 18) & 1, kv)
    c = rd(w3, 0, kv)
    res = None
    name = str(op)
    if op == 0x01: res, name = a, 'MOV'
    elif op == 0x02: res, name = [a[i] * bb[i] for i in range(4)], 'MUL'
    elif op == 0x03: res, name = [a[i] + bb[i] for i in range(4)], 'ADD'
    elif op == 0x04: res, name = [a[i] * bb[i] + c[i] for i in range(4)], 'MAD'
    elif op == 0x05: res, name = [sum(a[i] * bb[i] for i in range(3))] * 4, 'DP3'
    elif op == 0x06: res, name = [sum(a[i] * bb[i] for i in range(4))] * 4, 'DP4'
    elif op == 0x08: res, name = [min(a[i], bb[i]) for i in range(4)], 'MIN'
    elif op == 0x09: res, name = [max(a[i], bb[i]) for i in range(4)], 'MAX'
    elif op == 0x0A: res, name = [1.0 if a[i] < bb[i] else 0.0 for i in range(4)], 'SLT'
    elif op == 0x0B: res, name = [1.0 if a[i] >= bb[i] else 0.0 for i in range(4)], 'SGE'
    elif op == 0x0D: res, name = [1.0 if a[i] > bb[i] else 0.0 for i in range(4)], 'SGT'
    elif op == 0x10: res, name = [x - math.floor(x) for x in a], 'FRC'
    elif op == 0x11: res, name = [math.floor(x) for x in a], 'FLR'
    elif op == 0x1A: res, name = [1.0 / a[0] if a[0] else float('inf')] * 4, 'RCP'
    elif op == 0x1B: res, name = [1.0 / math.sqrt(abs(a[0])) if a[0] else float('inf')] * 4, 'RSQ'
    elif op == 0x1C: res, name = [2.0 ** a[0]] * 4, 'EX2'
    elif op == 0x1D: res, name = [math.log2(a[0]) if a[0] > 0 else float('-inf')] * 4, 'LG2'
    elif op == 0x3A: res, name = [x / bb[0] if bb[0] else float('inf') for x in a], 'DIV'
    elif op in (0x17, 0x18, 0x2F, 0x31):
        t = list(cfg['tex'].get(str(unit), [1, 1, 1, 1]))
        g = cfg.get('gamma', {}).get(str(unit), 0)
        if not ((w0 >> 21) & 1):
            for i in range(4):
                if (g >> i) & 1:
                    t[i] = srgb2lin(t[i])
        if (w0 >> 21) & 1 and unit in cfg.get('exp', []):
            t = [x * 2 - 1 for x in t]
        res, name = t, 'TEX%u' % unit
    # condição de execução
    exec_ok = [True] * 4
    if (w1 >> 18) & 7 != 7:
        ccr = CC[(w1 >> 31) & 1]
        csw = [(w1 >> (21 + 2 * i)) & 3 for i in range(4)]
        lt, eq, gt = (w1 >> 18) & 1, (w1 >> 19) & 1, (w1 >> 20) & 1
        for i in range(4):
            v = ccr[csw[i]]
            exec_ok[i] = bool((lt and v < 0) or (eq and v == 0) or (gt and v > 0))
    if res is not None:
        sc = (w2 >> 28) & 7
        mul = {0: 1, 1: 2, 2: 4, 3: 8, 5: 0.5, 6: 0.25, 7: 0.125}.get(sc, 1)
        res = [x * mul for x in res]
        if (w0 >> 31) & 1:
            res = [min(max(x, 0.0), 1.0) for x in res]
        mask = [(w0 >> (9 + i)) & 1 for i in range(4)]
        if not any(mask):
            mask = [1, 1, 1, 1]
        if not (w0 >> 30) & 1:
            dst = (H if (w0 >> 7) & 1 else R)[(w0 >> 1) & 63]
            for i in range(4):
                if mask[i] and exec_ok[i]:
                    dst[i] = res[i]
        if (w0 >> 8) & 1:
            ccw = CC[(w1 >> 30) & 1]
            for i in range(4):
                if mask[i] and exec_ok[i]:
                    ccw[i] = res[i]
        dname = ('h' if (w0 >> 7) & 1 else 'r') + str((w0 >> 1) & 63)
        print(f'{n:3d} {name:5s} {"--" if (w0 >> 30) & 1 else dname}.{"".join(C[i] for i in range(4) if mask[i])} = '
              + ' '.join('%.4g' % x for x in res) + ('  [cond ' + ''.join('1' if e else '0' for e in exec_ok) + ']' if not all(exec_ok) else ''))
    n += 1
    off += 32 if has_k else 16
    if w0 & 1:
        break
print('h0 =', ['%.4g' % x for x in H[0]], ' sRGB:', ['%.3f' % lin2srgb(x) for x in H[0][:3]])
print('h4 =', ['%.4g' % x for x in H[4]], ' sRGB:', ['%.3f' % lin2srgb(x) for x in H[4][:3]])
print('r0 =', ['%.4g' % x for x in R[0]])
