"""Execução simbólica por palavra de um trecho do código SPU traduzido (spu_recomp.c):
cada registrador é 4 expressões (uma por palavra de 32 bits); shufb/selb com
máscara constante alinhada em palavras viram permutação/seleção; as contas viram
nós de expressão. Mostra, para as gravações na LS e os registradores pedidos, a
fórmula de cada palavra -- para reescrever a função em C++ com a mesma conta.

uso: python simb_spu.py <dir do módulo> <função hex> <rótulo inicial> <rótulo final>
        [--const rN=w0,w1,w2,w3 ...] [--mostra r3,r5] [--fonte arq]
     executa do rótulo inicial (inclusive) até o rótulo final (exclusive), sem
     seguir desvios (um caminho reto; os 'if' viram notas)."""
import re
import sys
from pathlib import Path

d, fn = Path(sys.argv[1]), sys.argv[2].upper().rjust(8, '0')
ini, fim = sys.argv[3].upper().rjust(8, '0'), sys.argv[4].upper().rjust(8, '0')
fonte = Path(sys.argv[sys.argv.index('--fonte') + 1]) if '--fonte' in sys.argv else d / 'spu_recomp.c.sem_promocao'
mostra = []
if '--mostra' in sys.argv:
    mostra = [int(x[1:]) for x in sys.argv[sys.argv.index('--mostra') + 1].split(',')]
src = fonte.read_text(encoding='latin-1')
m = re.search(r'^(?:static )?(?:SPU_PN )?void \w+_spu_func_' + fn + r'(?:__b)?\(spu_context\* ctx\) \{\n', src, re.M)
corpo = src[m.end():src.find('\n}\n', m.end())]
a = corpo.find(f'loc_{ini}:') if ini != '00000000' else 0
b = corpo.find(f'loc_{fim}:', a + 1) if fim != 'FFFFFFFF' else len(corpo)
linhas = [t.strip() for t in corpo[a:b].split('\n')]
linhas = [t for t in linhas if t and not t.startswith('/*') and not t.startswith('loc_')]

# ------------------------------------------------------------------ expressões
nomes = {}          # expressão -> nome curto (subexpressões compartilhadas)
defs = []           # (nome, texto)


def E(op, *args):
    return (op,) + tuple(args)


def eh_const(x):
    return isinstance(x, int)


def txt(x):
    if eh_const(x):
        return f'0x{x:08X}'
    if isinstance(x, str):
        return x
    if x in nomes:
        return nomes[x]
    s = f'{x[0]}(' + ', '.join(txt(y) for y in x[1:]) + ')'
    if len(s) > 60:
        nm = f't{len(nomes)}'
        nomes[x] = nm
        defs.append((nm, s))
        return nm
    return s


R = {}   # reg -> [w0, w1, w2, w3]


def reg(n):
    if n not in R:
        R[n] = [f'r{n}.{i}' for i in range(4)]
    return R[n]


for c in [x for x in sys.argv if x.startswith('--const')]:
    pass
argv = sys.argv
i = 0
while i < len(argv):
    if argv[i] == '--const':
        rn, vs = argv[i + 1].split('=')
        R[int(rn[1:])] = [int(v, 16) for v in vs.split(',')]
        i += 2
    else:
        i += 1


def args_de(s):
    return [x.strip() for x in s.split(',')]


RE_R = re.compile(r'ctx->gpr\[(\d+)\]$')


def val(x):
    mm = RE_R.fullmatch(x)
    if mm:
        return reg(int(mm.group(1)))
    v = int(x, 0)
    return [v & 0xFFFFFFFF] * 4


def endereco(texto):
    """endereço de uma leitura/gravação: só a palavra 0 dos registradores conta"""
    termos = []
    for t in texto.split('+'):
        t = t.strip()
        mm = re.fullmatch(r'ctx->gpr\[(\d+)\]\._u32\[0\]', t)
        if mm:
            termos.append(reg(int(mm.group(1)))[0])
        else:
            termos.append(int(t, 0) & 0xFFFFFFFF)
    cs = sum(x for x in termos if eh_const(x)) & 0xFFFFFFFF
    ss = [x for x in termos if not eh_const(x)]
    if not ss:
        return cs
    base = ss[0] if len(ss) == 1 else E('add', *ss)
    return base if cs == 0 else E('add', base, cs)


def bytes_de(w4):
    return w4


def shufb(a_, b_, c_):
    if not all(eh_const(x) for x in c_):
        return [E('shufb', a_[i], b_[i], c_[i]) for i in range(4)]
    out = []
    for i in range(4):
        cw = c_[i]
        bs = [(cw >> (24 - 8 * k)) & 0xFF for k in range(4)]
        if all(x < 0x80 for x in bs) and bs[0] % 4 == 0 and bs == [bs[0], bs[0] + 1, bs[0] + 2, bs[0] + 3]:
            idx = bs[0] // 4
            out.append(a_[idx] if idx < 4 else b_[idx - 4])
        elif all(x in (0x80,) or (0xC0 <= x < 0xE0) or x >= 0xE0 for x in bs):
            out.append(0 if bs[0] < 0xE0 else (0xFFFFFFFF if bs[0] < 0xE0 + 0x20 else 0x80808080))
        else:
            out.append(E('bytes', *[a_[x // 4] if x < 16 else b_[(x - 16) // 4] for x in bs if x < 32], cw))
    return out


def selb(a_, b_, m_):
    out = []
    for i in range(4):
        if eh_const(m_[i]) and m_[i] in (0, 0xFFFFFFFF):
            out.append(b_[i] if m_[i] else a_[i])
        else:
            out.append(E('selb', a_[i], b_[i], m_[i]))
    return out


def lanes(op, *vs):
    return [E(op, *[v[i] for v in vs]) for i in range(4)]


def fsmbi(imm):
    out = []
    for i in range(4):
        w = 0
        for k in range(4):
            bit = (imm >> (15 - (4 * i + k))) & 1
            w = (w << 8) | (0xFF if bit else 0)
        out.append(w)
    return out


grav = []
notas = []
for t in linhas:
    mm = re.fullmatch(r'ctx->gpr\[(\d+)\] = spu_(\w+)\((.*)\);', t)
    ms = re.fullmatch(r'spu_ls_write128\(ctx, (.*), ctx->gpr\[(\d+)\]\);', t)
    if ms:
        grav.append((endereco(ms.group(1)), list(reg(int(ms.group(2))))))
        continue
    if not mm:
        notas.append(t)
        continue
    w, op, s = int(mm.group(1)), mm.group(2), mm.group(3)
    op = re.sub(r'_r$', '', op)
    a_ = args_de(s)
    if op == 'ls_read128':
        ad = endereco(s[len('ctx, '):])
        R[w] = [E('LD', ad, i) for i in range(4)]
    elif op == 'ls_const128':
        R[w] = [int(x.rstrip('u'), 16) for x in a_[2:6]]
    elif op in ('il', 'ila'):
        v = int(a_[0], 0) & (0xFFFFFFFF if op == 'il' else 0x3FFFF)
        if op == 'il' and v & 0x8000:
            v |= 0xFFFF0000
        R[w] = [v & 0xFFFFFFFF] * 4
    elif op == 'ilhu':
        R[w] = [(int(a_[0], 0) & 0xFFFF) << 16] * 4
    elif op == 'iohl':
        R[w] = [(x | (int(a_[1], 0) & 0xFFFF)) if eh_const(x) else E('or', x, int(a_[1], 0)) for x in val(a_[0])]
    elif op == 'fsmbi':
        R[w] = fsmbi(int(a_[0], 0))
    elif op in ('ori', 'shlqbyi', 'rotqbyi', 'ai') and len(a_) == 2 and a_[1] == '0':
        R[w] = list(val(a_[0]))
    elif op == 'rotqbyi' and int(a_[1], 0) % 4 == 0:
        k = (int(a_[1], 0) // 4) % 4
        v = val(a_[0])
        R[w] = [v[(i + k) % 4] for i in range(4)]
    elif op == 'shlqbyi' and int(a_[1], 0) % 4 == 0:
        k = int(a_[1], 0) // 4
        v = val(a_[0])
        R[w] = [v[i + k] if i + k < 4 else 0 for i in range(4)]
    elif op == 'shufb':
        R[w] = shufb(val(a_[0]), val(a_[1]), val(a_[2]))
    elif op == 'selb':
        R[w] = selb(val(a_[0]), val(a_[1]), val(a_[2]))
    elif op in ('ai', 'andi', 'ori', 'xori', 'cgti', 'ceqi', 'clgti', 'shli', 'rotmi', 'roti', 'andbi', 'cflts', 'cfltu', 'csflt', 'cuflt', 'mpyi', 'mpyui', 'ahi'):
        v = val(a_[0])
        k = int(a_[1], 0)
        if op == 'ai' and all(eh_const(x) for x in v):
            R[w] = [(x + k) & 0xFFFFFFFF for x in v]
        else:
            R[w] = [E(op, x, k) for x in v]
    else:
        R[w] = lanes(op, *[val(x) for x in a_])

print(f'# {len(linhas)} comandos de loc_{ini} a loc_{fim}')
for t in notas:
    print('# nota:', t)
saida = []
for ad, v in grav:
    saida.append(f'GRAVA [{txt(ad)}] = ' + ' | '.join(txt(x) for x in v))
for n in mostra:
    saida.append(f'r{n} = ' + ' | '.join(txt(x) for x in reg(n)))
for nm, s in defs:
    print(f'{nm} = {s}')
for s in saida:
    print(s)
