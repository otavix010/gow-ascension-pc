"""Modelo do laço 0x304A0 do plugin 0x74EF90 contra o oráculo: roda o plugin até a entrada do laço e até o fim,
e compara a saída com o modelo: posições como o 0x755540 (12 bytes por vértice, 3 x u16, v * 2^(r67-32)) e os dois
vetores como o 0x752710 (6 bytes depois da posição; contas de pl752_nativo_modelo.py). Mostra as regiões da LS que
o laço muda e quantos vetores batem por base de tabela de seleção candidata.
uso: python -I pl74e_modelo.py <pasta das fotos> [k]"""
import glob
import importlib.util
import os
import struct
import subprocess
import sys

aqui = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location('si', os.path.join(aqui, 'spu_interp_c.py'))
si = importlib.util.module_from_spec(spec); spec.loader.exec_module(si)
O = si.OPS
fotos = sys.argv[1]
so = int(sys.argv[2]) if len(sys.argv) > 2 else 0
pasta = 'gowapl_0074EF90'
raiz = os.path.join(aqui, '..')
s = open(os.path.join(raiz, 'port', 'spu', pasta, 'spu_recomp.c.sem_promocao'), encoding='latin-1').read()
corpo = ''
for fn in ('00030090', '000304A0'):
    cab = f'void {pasta}_spu_func_{fn}(spu_context* ctx) {{\n'
    i = s.index(cab) + len(cab)
    b = s[i:s.index('\n}\n', i)]
    corpo += (f'loc_{int(fn, 16):08X}:\n' if f'loc_{int(fn, 16):08X}:' not in b else '') + b + '\n'
prog, rot = si.compila(corpo)
rot.setdefault('loc_FIM', len(prog))
codigo = bytes.fromhex(subprocess.run([sys.executable, '-I', os.path.join(aqui, 'elf_ea.py'), os.path.join(raiz, 'jogo', 'GOWA.elf'),
                                       '74EF90', '40'], capture_output=True, text=True).stdout.strip())
alvo = rot['loc_000304A0']
K, B, UM = [0x46352E7C] * 4, [0xC6353150] * 4, [0x3F800000] * 4
R30 = [0x7FFFFFFF] * 3 + [0]
R32 = [0x80000000] * 3 + [0xFFFFFFFF]
R52 = [0x08080808] * 3 + [0]


def passo(m, pc):
    ins = prog[pc]; pc += 1
    t = ins[0]
    if t == 'op':
        _, d, nome, args = ins; m.g[d] = list(O[nome](*[m.arg(a) for a in args]))
    elif t == 'ld':
        a = m.end(ins[2]); m.g[ins[1]] = si.w4(bytes(m.ls[a:a + 16]))
    elif t == 'st':
        a = m.end(ins[1]); m.ls[a:a + 16] = si.b16(m.g[ins[2]])
    elif t == 'k':
        m.g[ins[1]] = list(ins[2])
    elif t == 'if':
        _, r, bits, cmp, al = ins
        v = m.g[r][0] & (si.M32 if bits == 32 else 0xFFFF)
        if (v != 0) == (cmp == '!='): pc = rot.get(al, len(prog))
    elif t == 'goto':
        pc = rot.get(ins[1], len(prog))
    return pc


def campos(b6):
    bits = int.from_bytes(b6, 'big')
    return ((bits >> 24) & 0xF, (bits >> 38) & 0x3FF, (bits >> 28) & 0x3FF), (bits & 0xF, (bits >> 14) & 0x3FF, (bits >> 4) & 0x3FF)


def raiz3(ab):
    q = O['fm'](ab, ab)
    sm = O['fa'](q, O['shlqbyi'](q, 4))
    u = O['fs'](UM, sm)
    return O['fm'](u, si.fi(u, si.frsqest(u)))


def sel_saida(c, ab, e, b):
    cc = O['selb'](c, e, R32)
    ctl = O['selb']([0, 0, 0, 0x0C0D0E0F], e, R30)
    if b: ctl = O['a'](ctl, R52)
    return O['shufb'](cc, ab, ctl)


def vetores(ls, a, b, tab):
    """vértices A (em a) e B (em b): os 2 vetores de cada (16 bytes cada), pela tabela de seleção em tab"""
    (sA1, aA1, bA1), (sA2, aA2, bA2) = campos(ls[a + 6:a + 12])
    (sB1, aB1, bB1), (sB2, aB2, bB2) = campos(ls[b + 6:b + 12])
    out = []
    for (sa, fa_, fb_, sb, fc, fd) in ((sA1, aA1, bA1, sB1, aB1, bB1), (sA2, aA2, bA2, sB2, aB2, bB2)):
        ab = O['fma'](K, [UM[0] | fa_, UM[0] | fb_, UM[0] | fc, UM[0] | fd], B)
        c = raiz3(ab)
        eA = si.w4(bytes(ls[(tab + 16 * sa) & 0x3FFF0:((tab + 16 * sa) & 0x3FFF0) + 16]))
        eB = si.w4(bytes(ls[(tab + 16 * sb) & 0x3FFF0:((tab + 16 * sb) & 0x3FFF0) + 16]))
        out.append((sel_saida(c, ab, eA, 0), sel_saida(c, ab, eB, 1)))
    return out


for p0 in sorted(glob.glob(os.path.join(fotos, 'pl_foto_*_0.bin')), key=lambda x: int(os.path.basename(x).split('_')[2])):
    k = int(os.path.basename(p0).split('_')[2])
    if so and k != so: continue
    r0, ls0 = si.carrega(p0)
    m = si.Maquina([list(r) for r in r0], ls0)
    m.reloc = ls0.find(codigo) - 0x30000
    pc = 0
    while pc != alvo:
        pc = passo(m, pc)
    g = [list(r) for r in m.g]; ls_e = bytes(m.ls)
    while pc < len(prog):
        pc = passo(m, pc)
    fim = bytes(m.ls)
    dif = [a for a in range(len(fim)) if fim[a] != ls_e[a]]
    reg = []
    for a in dif:
        if reg and a <= reg[-1][1] + 16: reg[-1][1] = a
        else: reg.append([a, a])
    off = g[35][0]
    n = (0x100000000 - off) // 32 if off else 0
    ent = (g[44][0] - 48) & 0x3FFFF
    pb = (g[78][0] + off) & 0x3FFFF
    bons = 0
    for i in range(2 * n):
        a = ent + 12 * i
        u = struct.unpack('>3H', ls_e[a:a + 6])
        esp = []
        for c in range(3):
            v = si.sx(((u[c] & g[68][c]) + g[63][c]) & si.M32, 32)
            esp.append(struct.unpack('>I', struct.pack('>f', v * 2.0 ** (g[67][c] - 32)))[0] if v else 0)
        esp.append(0x3F800000)
        bons += si.w4(fim[pb + 16 * i:pb + 16 * i + 16]) == esp
    print(f'foto {k}: {n} voltas; posições {bons}/{2 * n}; regiões mudadas ' +
          ', '.join(f'{x:#07x}..{y:#07x}' for x, y in reg[:6]))
    # vetores: as duas regiões depois das posições; testa bases de tabela
    v1 = pb + 32 * n
    v2 = v1 + 32 * n
    melhor = None
    for tab in range(0x3F200, 0x3F500, 16):
        acertos = 0
        for p in range(min(n, 8)):
            (a1, b1), (a2, b2) = vetores(ls_e, ent + 24 * p, ent + 24 * p + 12, tab)
            acertos += si.w4(fim[v1 + 32 * p:v1 + 32 * p + 16]) == a1
            acertos += si.w4(fim[v2 + 32 * p:v2 + 32 * p + 16]) == a2
        if not melhor or acertos > melhor[1]: melhor = (tab, acertos)
    print(f'   tabela {melhor[0]:#07x}: {melhor[1]}/{2 * min(n, 8)} vetores A iguais (v1 em {v1:#07x}, v2 em {v2:#07x})')


def modelo_ls(ls_e, g, ultimo):
    """a LS depois do laço pelo modelo; ultimo = se os vetores do último par são gravados"""
    ls = bytearray(ls_e)
    off = g[35][0]; n = (0x100000000 - off) // 32
    ent = (g[44][0] - 48) & 0x3FFFF
    pb = (g[78][0] + off) & 0x3FFFF
    v1, v2 = pb + 32 * n, pb + 64 * n
    tab = g[49][0]
    st = lambda a, v: ls.__setitem__(slice(a, a + 16), si.b16(v))
    # 1a volta: o lixo (os registradores de entrada, zero nas fotos) no fim de cada fluxo de vetores
    for a in (v1 + 32 * n, v1 + 32 * n + 16, v2 + 32 * n, v2 + 32 * n + 16):
        st(a, [0, 0, 0, 0])
    for i in range(2 * n):
        a = ent + 12 * i
        u = struct.unpack('>3H', ls_e[a:a + 6])
        esp = []
        for c in range(3):
            v = si.sx(((u[c] & g[68][c]) + g[63][c]) & si.M32, 32)
            esp.append(struct.unpack('>I', struct.pack('>f', v * 2.0 ** (g[67][c] - 32)))[0] if v else 0)
        st(pb + 16 * i, esp + [0x3F800000])
    for p in range(n if ultimo else n - 1):
        (a1, b1), (a2, b2) = vetores(ls_e, ent + 24 * p, ent + 24 * p + 12, tab)
        st(v1 + 32 * p, a1); st(v1 + 32 * p + 16, b1); st(v2 + 32 * p, a2); st(v2 + 32 * p + 16, b2)
    return bytes(ls)


if '--ls' in sys.argv:
    tot = {0: 0, 1: 0}
    for p0 in sorted(glob.glob(os.path.join(fotos, 'pl_foto_*_0.bin')), key=lambda x: int(os.path.basename(x).split('_')[2])):
        k = int(os.path.basename(p0).split('_')[2])
        r0, ls0 = si.carrega(p0)
        m = si.Maquina([list(r) for r in r0], ls0)
        m.reloc = ls0.find(codigo) - 0x30000
        pc = 0
        while pc != alvo: pc = passo(m, pc)
        g = [list(r) for r in m.g]; ls_e = bytes(m.ls)
        juncos = [g[r] for r in (39, 56, 50, 51, 52, 57, 58, 59)]
        while pc < len(prog): pc = passo(m, pc)
        fim = bytes(m.ls)
        for ult in (0, 1):
            mod = modelo_ls(ls_e, g, ult)
            d = [a for a in range(len(fim)) if fim[a] != mod[a]]
            tot[ult] += not d
            print(f'foto {k} último={ult}: {len(d)} bytes diferentes' + (f' (1o em {d[0]:#07x})' if d else '') +
                  ('' if ult else f'  regs do lixo {"zero" if all(x == [0, 0, 0, 0] for x in juncos) else "NÃO zero"}'))
    print('iguais (último 0/1):', tot)
