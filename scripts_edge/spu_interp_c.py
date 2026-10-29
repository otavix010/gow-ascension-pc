"""Interpretador das linhas do código SPU traduzido (spu_recomp.c) para um trecho entre dois rótulos,
com a semântica exata da SPU. Serve de oráculo para a versão nativa: roda a partir de uma foto da LS e
dos registradores (sonda_rotulos.py) e compara com a foto do fim.

uso: python -I spu_interp_c.py <spu_recomp.c original> <função> <rótulo início> <rótulo fim> <pasta das fotos>"""
import glob
import os
import re
import struct
import sys
from fractions import Fraction

M32 = 0xFFFFFFFF

# ---------------------------------------------------------------- valores: 4 palavras (big-endian lógico)
def b16(v):
    return b''.join(struct.pack('>I', w) for w in v)

def w4(b):
    return list(struct.unpack('>4I', b))

def sx(x, bits):
    x &= (1 << bits) - 1
    return x - (1 << bits) if x >> (bits - 1) else x

def por_palavra(f):
    return lambda a, b: [f(x, y) & M32 for x, y in zip(a, b)]

def halfs(v):
    return [(w >> 16) & 0xFFFF for w in v for _ in (0,)] and [h for w in v for h in ((w >> 16) & 0xFFFF, w & 0xFFFF)]

def de_halfs(h):
    return [((h[2 * i] & 0xFFFF) << 16) | (h[2 * i + 1] & 0xFFFF) for i in range(4)]

# ---------------------------------------------------------------- float da SPU
def fx(w):
    e = (w >> 23) & 0xFF
    if e == 0:
        return Fraction(0)
    v = Fraction((w & 0x7FFFFF) | 0x800000) * Fraction(2) ** (e - 150)
    return -v if w >> 31 else v

def fw(x):
    """Fraction -> palavra, arredondando para zero (denormal -> 0, satura no máximo)"""
    if x == 0:
        return 0
    s = 0x80000000 if x < 0 else 0
    a = abs(x)
    e = a.numerator.bit_length() - a.denominator.bit_length()
    while Fraction(2) ** e > a: e -= 1
    while Fraction(2) ** (e + 1) <= a: e += 1
    if e + 127 <= 0:
        return 0
    if e + 127 > 255:
        return s | 0x7FFFFFFF
    m = int(a / Fraction(2) ** (e - 23))
    return s | ((e + 127) << 23) | (m & 0x7FFFFF)

def fop(f):
    return lambda *v: [fw(f(*(fx(x[i]) for x in v))) for i in range(4)]

def cflts(a, i8):
    out = []
    for w in a:
        v = fx(w) * Fraction(2) ** (173 - i8)
        n = int(v)
        out.append(max(-2 ** 31, min(2 ** 31 - 1, n)) & M32)
    return out

# ---------------------------------------------------------------- operações
def shufb(a, b, c):
    ab = b16(a) + b16(b)
    cc = b16(c)
    r = bytearray(16)
    for i, k in enumerate(cc):
        if k & 0xC0 == 0x80: r[i] = 0
        elif k & 0xE0 == 0xC0: r[i] = 0xFF
        elif k & 0xE0 == 0xE0: r[i] = 0x80
        else: r[i] = ab[k & 0x1F]
    return w4(bytes(r))

def rot_bytes(a, n):
    b = b16(a); n &= 15
    return w4(b[n:] + b[:n])

def shl_bytes(a, n):
    b = b16(a)
    return w4((b[n:] + b'\0' * 16)[:16]) if n < 16 else [0] * 4

def shr_bytes(a, n):
    b = b16(a)
    return w4((b'\0' * n + b)[:16]) if n < 16 else [0] * 4

def rotm(w, n):   # deslocamento lógico à direita por (-n) & 63
    s = (-n) & 63
    return (w >> s) if s < 32 else 0

def rotma(w, n):
    s = (-n) & 63
    return (sx(w, 32) >> min(s, 31)) & M32

def fsmbi(imm):
    return w4(bytes(0xFF if (imm >> (15 - i)) & 1 else 0 for i in range(16)))

def fcmp_gt(a, b):
    return [M32 if fx(x) > fx(y) else 0 for x, y in zip(a, b)]

FRSQEST_LUT = [
    0x350160, 0x34E954, 0x2F993D, 0x2F993D, 0x2AA523, 0x2AA523, 0x26190D, 0x26190D,
    0x21E4F9, 0x21E4F9, 0x1E00E9, 0x1E00E9, 0x1A5CD9, 0x1A5CD9, 0x16F8CB, 0x16F8CB,
    0x13CCC0, 0x13CCC0, 0x10CCB3, 0x10CCB3, 0x0E00AA, 0x0E00AA, 0x0B58A1, 0x0B58A1,
    0x08D498, 0x08D498, 0x067491, 0x067491, 0x043089, 0x043089, 0x020C83, 0x020C83,
    0x7FFDF4, 0x7FD1DE, 0x7859C8, 0x783DBA, 0x71559C, 0x71559C, 0x6AE57C, 0x6AE57C,
    0x64F561, 0x64F561, 0x5F7149, 0x5F7149, 0x5A4D33, 0x5A4D33, 0x55811F, 0x55811F,
    0x51050F, 0x51050F, 0x4CC8FE, 0x4CC8FE, 0x48D0F0, 0x48D0F0, 0x4510E4, 0x4510E4,
    0x4180D7, 0x4180D7, 0x3E24CC, 0x3E24CC, 0x3AF4C3, 0x3AF4C3, 0x37E8BA, 0x37E8BA]

def frsqest(a):   # spu_frsqest_ref do runtime
    out = []
    for w in a:
        e = (w >> 23) & 0xFF
        rexp = 0xFF if e == 0 else 190 - (e + 1) // 2
        out.append(((rexp << 23) | (FRSQEST_LUT[(w >> 18) & 0x3F] & 0x7FFFFF)) & M32)
    return out

def fi(a, b):     # spu_fi_ref do runtime
    out = []
    for ya, est in zip(a, b):
        sinal = est & 0x80000000
        e = (est >> 23) & 0xFF
        base = (est >> 10) & 0x1FFF
        passo = est & 0x3FF
        y = ya & 0x7FFFF
        if e == 0:
            out.append(0); continue
        m = (((1 << 13) | base) << 19) - passo * y
        if m < (1 << 32):
            m <<= 1
            e -= 1
            if e == 0:
                out.append(0); continue
        out.append(sinal | (e << 23) | ((m >> 9) & 0x7FFFFF))
    return out

def ctl_insercao(pos, tam):
    c = bytearray(range(0x10, 0x20))
    for k in range(tam):
        c[pos + k] = {1: [3], 2: [2, 3], 4: [0, 1, 2, 3]}[tam][k]
    return w4(bytes(c))

def por_meia(f):
    return lambda a, b: de_halfs([f(x, y) & 0xFFFF for x, y in zip(halfs(a), halfs(b))])

def meia_imm(f):
    return lambda a, i: de_halfs([f(x, i) & 0xFFFF for x in halfs(a)])

def rot_h(h, n):
    n &= 15
    return ((h << n) | (h >> (16 - n))) & 0xFFFF if n else h

OPS = {
    'sf': por_palavra(lambda x, y: y - x),
    'sfh': por_meia(lambda x, y: y - x),
    'sfhi': meia_imm(lambda h, i: sx(i, 10) - h),
    'ah': por_meia(lambda x, y: x + y),
    'andhi': meia_imm(lambda h, i: h & (sx(i, 10) & 0xFFFF)),
    'orhi': meia_imm(lambda h, i: h | (sx(i, 10) & 0xFFFF)),
    'cgth': por_meia(lambda x, y: 0xFFFF if sx(x, 16) > sx(y, 16) else 0),
    'cgthi': meia_imm(lambda h, i: 0xFFFF if sx(h, 16) > sx(i, 10) else 0),
    'clgt': por_palavra(lambda x, y: M32 if x > y else 0),
    'fsmb': lambda a: w4(bytes(0xFF if (a[0] >> (15 - k)) & 1 else 0 for k in range(16))),
    'mpy': por_palavra(lambda x, y: sx(x & 0xFFFF, 16) * sx(y & 0xFFFF, 16)),
    'mpyhh': por_palavra(lambda x, y: sx(x >> 16, 16) * sx(y >> 16, 16)),
    'mpyi': lambda a, i: [(sx(w & 0xFFFF, 16) * sx(i, 10)) & M32 for w in a],
    'rot': por_palavra(lambda x, n: ((x << (n & 31)) | (x >> ((32 - (n & 31)) & 31))) if n & 31 else x),
    'roth': por_meia(lambda h, n: rot_h(h, n)),
    'rothm': por_meia(lambda h, n: (h >> ((-n) & 31)) if ((-n) & 31) < 16 else 0),
    'rothma': por_meia(lambda h, n: (sx(h, 16) >> min((-n) & 31, 15))),
    'rotqmbyi': lambda a, n: shr_bytes(a, (-n) & 31),
    'shl': por_palavra(lambda x, n: (x << (n & 63)) if (n & 63) < 32 else 0),
    'shlhi': meia_imm(lambda h, n: (h << (n & 31)) if (n & 31) < 16 else 0),
    'cbd': lambda a, i: ctl_insercao((a[0] + i) & 15, 1),
    'chd': lambda a, i: ctl_insercao((a[0] + i) & 14, 2),
    'cwd': lambda a, i: ctl_insercao((a[0] + i) & 12, 4),
    'clz': lambda a: [32 - w.bit_length() for w in a],
    'gbb': lambda a: [sum(((b & 1) << (15 - k)) for k, b in enumerate(b16(a))), 0, 0, 0],
    'rotm': lambda a, b: [rotm(w, n) for w, n in zip(a, b)],
    'frsqest': frsqest,
    'fi': fi,
    'fa_r': fop(lambda x, y: x + y),
    'fs_r': fop(lambda x, y: x - y),
    'fms_r': fop(lambda x, y, z: x * y - z),
    'shufb': shufb,
    'ori': lambda a, i: [(w | (sx(i, 10) & M32)) & M32 for w in a],
    'orbi': lambda a, i: w4(bytes(x | (i & 0xFF) for x in b16(a))),
    'andbi': lambda a, i: w4(bytes(x & (i & 0xFF) for x in b16(a))),
    'shlqbyi': lambda a, n: shl_bytes(a, n & 31),
    'rotqbyi': lambda a, n: rot_bytes(a, n),
    'rotqby': lambda a, b: rot_bytes(a, b[0]),
    'rotqmby': lambda a, b: shr_bytes(a, (-b[0]) & 31),
    'a': por_palavra(lambda x, y: x + y),
    'ai': lambda a, i: [(w + sx(i, 10)) & M32 for w in a],
    'sfi': lambda a, i: [(sx(i, 10) - w) & M32 for w in a],
    'and': por_palavra(lambda x, y: x & y),
    'or': por_palavra(lambda x, y: x | y),
    'xor': por_palavra(lambda x, y: x ^ y),
    'andc': por_palavra(lambda x, y: x & ~y),
    'andi': lambda a, i: [w & (sx(i, 10) & M32) for w in a],
    'ilhu': lambda i: [(i & 0xFFFF) << 16] * 4,
    'iohl': lambda a, i: [w | (i & 0xFFFF) for w in a],
    'ila': lambda i: [i & 0x3FFFF] * 4,
    'il': lambda i: [sx(i, 16) & M32] * 4,
    'ilh': lambda i: [((i & 0xFFFF) << 16) | (i & 0xFFFF)] * 4,
    'ceqi': lambda a, i: [M32 if w == (sx(i, 10) & M32) else 0 for w in a],
    'ceq': por_palavra(lambda x, y: M32 if x == y else 0),
    'ceqbi': lambda a, i: w4(bytes(0xFF if x == (i & 0xFF) else 0 for x in b16(a))),
    'ceqhi': lambda a, i: de_halfs([0xFFFF if h == (sx(i, 10) & 0xFFFF) else 0 for h in halfs(a)]),
    'cgt': por_palavra(lambda x, y: M32 if sx(x, 32) > sx(y, 32) else 0),
    'cgti': lambda a, i: [M32 if sx(w, 32) > sx(i, 10) else 0 for w in a],
    'clgti': lambda a, i: [M32 if w > (sx(i, 10) & M32) else 0 for w in a],
    'clgtbi': lambda a, i: w4(bytes(0xFF if x > (i & 0xFF) else 0 for x in b16(a))),
    'roti': lambda a, n: [((w << (n & 31)) | (w >> ((32 - (n & 31)) & 31))) & M32 if n & 31 else w for w in a],
    'rotmi': lambda a, n: [rotm(w, n) for w in a],
    'rotma': lambda a, b: [rotma(w, n) for w, n in zip(a, b)],
    'rothmi': lambda a, n: de_halfs([(h >> ((-n) & 31)) if ((-n) & 31) < 16 else 0 for h in halfs(a)]),
    'shli': lambda a, n: [(w << (n & 63)) & M32 if (n & 63) < 32 else 0 for w in a],
    'selb': lambda a, b, c: [(x & ~z | y & z) & M32 for x, y, z in zip(a, b, c)],
    'xsbh': lambda a: de_halfs([sx(h & 0xFF, 8) & 0xFFFF for h in halfs(a)]),
    'fsmbi': fsmbi,
    'fsm': lambda a: [M32 if (a[0] >> (3 - i)) & 1 else 0 for i in range(4)],
    'gb': lambda a: [sum(((a[i] & 1) << (3 - i)) for i in range(4)), 0, 0, 0],
    'mpyu': por_palavra(lambda x, y: (x & 0xFFFF) * (y & 0xFFFF)),
    'mpyhhu': por_palavra(lambda x, y: (x >> 16) * (y >> 16)),
    'mpyui': lambda a, i: [((w & 0xFFFF) * (i & 0x3FF)) & M32 for w in a],
    'cg': por_palavra(lambda x, y: 1 if x + y > M32 else 0),
    'addx': lambda a, b, c: [(x + y + (z & 1)) & M32 for x, y, z in zip(a, b, c)],
    'ahi': lambda a, i: de_halfs([(h + sx(i, 10)) & 0xFFFF for h in halfs(a)]),
    'cntb': lambda a: w4(bytes(bin(x).count('1') for x in b16(a))),
    'fs': fop(lambda x, y: x - y),
    'fa': fop(lambda x, y: x + y),
    'fm_r': fop(lambda x, y: x * y),
    'fm': fop(lambda x, y: x * y),
    'fma_r': fop(lambda x, y, z: x * y + z),
    'fma': fop(lambda x, y, z: x * y + z),
    'fms': fop(lambda x, y, z: x * y - z),
    'fnms': fop(lambda x, y, z: z - x * y),
    'fcgt': fcmp_gt,
    'cflts': cflts,
}

# ---------------------------------------------------------------- parser das linhas
R_REG = r'ctx->gpr\[(\d+)\]'

class Maquina:
    def __init__(self, regs, ls):
        self.g = regs
        self.ls = bytearray(ls)

    reloc = 0

    def end(self, e):
        e = e.strip()
        m = re.fullmatch(r'\(uint32_t\)\((0x[0-9A-Fa-f]+) \+ g_spu_hot_reloc\)', e)
        if m:
            return (int(m.group(1), 16) + self.reloc) & 0x3FFF0
        tot = 0
        for t in e.split('+'):
            t = t.strip()
            m = re.fullmatch(R_REG + r'\._u32\[0\]', t)
            if m: tot += self.g[int(m.group(1))][0]
            else: tot += int(t, 0)
        return tot & 0x3FFF0

    def arg(self, t):
        t = t.strip()
        m = re.fullmatch(R_REG, t)
        if m: return self.g[int(m.group(1))]
        return int(t, 0)

def dividir_args(s):
    out, prof, cur = [], 0, ''
    for ch in s:
        if ch == ',' and prof == 0:
            out.append(cur); cur = ''
            continue
        prof += ch == '('
        prof -= ch == ')'
        cur += ch
    out.append(cur)
    return out

def compila(bloco):
    """lista de (tipo, ...) e mapa de rótulos"""
    prog, rot = [], {}
    for l in bloco.split('\n'):
        l = l.strip()
        if not l or l.startswith('/*'):
            continue
        m = re.fullmatch(r'(loc_[0-9A-F]+):', l)
        if m:
            rot[m.group(1)] = len(prog); continue
        m = re.fullmatch(r'if \(' + R_REG + r'\._u(32|16)\[0\] (==|!=) 0\) goto (loc_[0-9A-F]+);', l)
        if m:
            prog.append(('if', int(m.group(1)), int(m.group(2)), m.group(3), m.group(4))); continue
        m = re.fullmatch(r'goto (loc_[0-9A-F]+);', l)
        if m:
            prog.append(('goto', m.group(1))); continue
        # passagem para a função seguinte por trampolim (o corpo dela vem em seguida no programa)
        m = re.fullmatch(r'\{ ctx->pc = 0x([0-9A-F]+); g_spu_trampoline_fn = [A-Za-z0-9_]+; return; \}', l)
        if m:
            prog.append(('goto', 'loc_%08X' % int(m.group(1), 16))); continue
        if l == 'SPU_RET(ctx);':
            prog.append(('goto', 'loc_FIM')); continue
        if l in ('}',) or l.startswith('void ') or l.startswith('static '):
            continue
        m = re.fullmatch(r'spu_ls_write128\(ctx, (.+), ' + R_REG + r'\);', l)
        if m:
            prog.append(('st', m.group(1), int(m.group(2)))); continue
        m = re.fullmatch(R_REG + r' = spu_ls_read128\(ctx, (.+)\);', l)
        if m:
            prog.append(('ld', int(m.group(1)), m.group(2))); continue
        m = re.fullmatch(R_REG + r' = spu_ls_const128\(ctx, 0x[0-9A-F]+, (0x[0-9A-F]+)u, (0x[0-9A-F]+)u, (0x[0-9A-F]+)u, (0x[0-9A-F]+)u\);', l)
        if m:
            prog.append(('k', int(m.group(1)), [int(m.group(i), 16) for i in range(2, 6)])); continue
        m = re.fullmatch(R_REG + r' = spu_([a-z0-9_]+)\((.*)\);', l)
        if m:
            prog.append(('op', int(m.group(1)), m.group(2), dividir_args(m.group(3)))); continue
        raise ValueError('linha não reconhecida: ' + l)
    return prog, rot

def roda(prog, rot, maq, limite=10 ** 7):
    pc, n = 0, 0
    while pc < len(prog):
        n += 1
        if n > limite: raise RuntimeError('laço sem fim?')
        ins = prog[pc]; pc += 1
        t = ins[0]
        if t == 'op':
            _, d, nome, args = ins
            f = OPS.get(nome)
            if f is None: raise ValueError('operação sem semântica: ' + nome)
            maq.g[d] = list(f(*[maq.arg(a) for a in args]))
        elif t == 'ld':
            a = maq.end(ins[2]); maq.g[ins[1]] = w4(bytes(maq.ls[a:a + 16]))
        elif t == 'st':
            a = maq.end(ins[1]); maq.ls[a:a + 16] = b16(maq.g[ins[2]])
        elif t == 'k':
            maq.g[ins[1]] = list(ins[2])
        elif t == 'if':
            _, r, bits, cmp, alvo = ins
            v = maq.g[r][0] & (M32 if bits == 32 else 0xFFFF)
            if (v != 0) == (cmp == '!='): pc = rot.get(alvo, len(prog))
        elif t == 'goto':
            pc = rot.get(ins[1], len(prog))
    return n

def carrega(p):
    b = open(p, 'rb').read()
    regs = [list(struct.unpack('<4I', b[i * 16:(i + 1) * 16])) for i in range(128)]
    return regs, b[2048:]

def main():
    src, fn, r_in, r_out, pasta = sys.argv[1:6]
    s = open(src, encoding='latin-1').read()
    i = s.index(f'static SPU_PN void {fn}(spu_context* ctx) {{')
    f = s[i:s.index('\n}\n', i)]
    bloco = f[f.index(f'{r_in}:'):f.index(f'{r_out}:')]
    prog, rot = compila(bloco)
    ok = dif = 0
    for p0 in sorted(glob.glob(os.path.join(pasta, 'pl_foto_*_0.bin')), key=lambda x: int(os.path.basename(x).split('_')[2])):
        k = int(os.path.basename(p0).split('_')[2])
        r0, ls0 = carrega(p0)
        r1, ls1 = carrega(p0.replace('_0.bin', '_1.bin'))
        maq = Maquina([list(r) for r in r0], ls0)
        n = roda(prog, rot, maq)
        dr = [j for j in range(128) if maq.g[j] != r1[j]]
        dl = [j for j in range(0x40000) if maq.ls[j] != ls1[j]]
        bom = not dr and not dl
        ok += bom; dif += not bom
        print(f'passagem {k:2d}: {n} instruções | regs dif {dr[:8]} | LS dif {len(dl)}' + (f' (1a em 0x{dl[0]:05X})' if dl else ''))
    print(f'{ok} iguais, {dif} diferentes')

if __name__ == '__main__':
    main()
