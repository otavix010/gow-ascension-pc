"""Modelo da versão nativa do plugin 0x752710 (a partir da entrada do laço 0x302E8, depois da preparação
0x30020), conferido contra a LS inteira das fotos de saída. Usa as operações exatas do oráculo.
uso: python -I pl752_nativo_modelo.py <pasta das fotos> [troca: 0/1 = qual registrador de entrada é o vértice A]"""
import glob
import importlib.util
import os
import subprocess
import sys

aqui = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location('si', os.path.join(aqui, 'spu_interp_c.py'))
si = importlib.util.module_from_spec(spec); spec.loader.exec_module(si)
O = si.OPS
raiz = os.path.join(aqui, '..')
fotos = sys.argv[1]
K, B, UM = [0x46352E7C] * 4, [0xC6353150] * 4, [0x3F800000] * 4
R30 = [0x7FFFFFFF] * 3 + [0]
R32 = [0x80000000] * 3 + [0xFFFFFFFF]
R52 = [0x08080808] * 3 + [0]

def pre_302e8(r0, ls0, reloc):
    """roda a preparação 0x30020 pelo oráculo e para na entrada do laço (o gancho nativo fica ali)"""
    s = open(os.path.join(raiz, 'port', 'spu', 'gowapl_00752710', 'spu_recomp.c.sem_promocao'), encoding='latin-1').read()
    cab = 'void gowapl_00752710_spu_func_00030020(spu_context* ctx) {\n'
    i = s.index(cab) + len(cab)
    corpo = s[i:s.index('\n}\n', i)]
    prog, rot = si.compila(corpo)
    rot.setdefault('loc_000302E8', len(prog))
    m = si.Maquina([list(r) for r in r0], ls0)
    m.reloc = reloc
    si.roda(prog, rot, m)
    return m

def campos(reg18):
    bits = int.from_bytes(reg18[12:18], 'big')
    v1 = ((bits >> 24) & 0xF, (bits >> 38) & 0x3FF, (bits >> 28) & 0x3FF)
    v2 = (bits & 0xF, (bits >> 14) & 0x3FF, (bits >> 4) & 0x3FF)
    return v1, v2

def raiz3(ab):   # r13 = (aA, bA, aB, bB) -> r15 (as 4 faixas, com as de lixo)
    q = O['fm'](ab, ab)
    sm = O['fa'](q, O['shlqbyi'](q, 4))
    u = O['fs'](UM, sm)
    return O['fm'](u, si.fi(u, si.frsqest(u)))

def sel_saida(c, ab, e, b):
    cc = O['selb'](c, e, R32)
    ctl = O['selb']([0, 0, 0, 0x0C0D0E0F], e, R30)
    if b: ctl = O['a'](ctl, R52)
    return O['shufb'](cc, ab, ctl)

def nativo(m, troca):
    g = m.g
    ls = m.ls
    r5 = g[5][0]
    P = r5 // 32
    inp = (g[35][0] - 72) & 0x3FFFF
    b0, b1, b2 = (g[61][0] - r5) & 0x3FFFF, (g[50][0] - r5) & 0x3FFFF, (g[51][0] - r5) & 0x3FFFF
    r40 = g[40][0]
    ent = lambda sel: si.w4(bytes(ls[(r40 + 16 * sel) & 0x3FFF0:((r40 + 16 * sel) & 0x3FFF0) + 16]))
    st = lambda a, v: ls.__setitem__(slice(a & 0x3FFF0, (a & 0x3FFF0) + 16), si.b16(v))
    # 1a volta do original: as posições do par 0 e os vetores "do par -1" (registradores de entrada)
    r18 = g[7][0]
    eA, eB = (g[38], g[39]) if not troca else (g[39], g[38])
    st(b0, list(si.w4(bytes(ls[inp:inp + 12]) + bytes(4)))[:3] + [0x3F800000])
    st(b0 + 16, list(si.w4(bytes(ls[inp + 18:inp + 30]) + bytes(4)))[:3] + [0x3F800000])
    st(g[50][0] + r18, sel_saida(g[15], g[13], eA, 0))
    st(g[58][0] + r18, sel_saida(g[15], g[13], eB, 1))
    fA, fB = (g[44], g[46]) if not troca else (g[46], g[44])
    st(g[51][0] + r18, sel_saida(g[45], g[47], fA, 0))
    st(g[59][0] + r18, sel_saida(g[45], g[47], fB, 1))
    for p in range(P):
        pa, pb = inp + 36 * p, inp + 36 * p + 18
        if p:
            st(b0 + 32 * p, si.w4(bytes(ls[pa:pa + 12]) + bytes(4))[:3] + [0x3F800000])
            st(b0 + 32 * p + 16, si.w4(bytes(ls[pb:pb + 12]) + bytes(4))[:3] + [0x3F800000])
        (sA1, aA1, bA1), (sA2, aA2, bA2) = campos(ls[pa:pa + 18])
        (sB1, aB1, bB1), (sB2, aB2, bB2) = campos(ls[pb:pb + 18])
        for (sa, fa_, fb_, sb, fc, fd, base) in ((sA1, aA1, bA1, sB1, aB1, bB1, b1), (sA2, aA2, bA2, sB2, aB2, bB2, b2)):
            ab = O['fma'](K, [UM[0] | fa_, UM[0] | fb_, UM[0] | fc, UM[0] | fd], B)
            c = raiz3(ab)
            st(base + 32 * p, sel_saida(c, ab, ent(sa), 0))
            st(base + 32 * p + 16, sel_saida(c, ab, ent(sb), 1))
    g[1] = O['ai'](g[1], 224)
    g[3] = [0, 0, 0, 0]

troca = int(sys.argv[2]) if len(sys.argv) > 2 else 0
codigo = bytes.fromhex(subprocess.run([sys.executable, '-I', os.path.join(aqui, 'elf_ea.py'), os.path.join(raiz, 'jogo', 'GOWA.elf'),
                                       '752710', '40'], capture_output=True, text=True).stdout.strip())
ok = 0; tot = 0
for p0 in sorted(glob.glob(os.path.join(fotos, 'pl_foto_*_0.bin')), key=lambda x: int(os.path.basename(x).split('_')[2])):
    k = int(os.path.basename(p0).split('_')[2])
    r0, ls0 = si.carrega(p0)
    r1, ls1 = si.carrega(p0.replace('_0.bin', '_1.bin'))
    m = pre_302e8(r0, ls0, ls0.find(codigo) - 0x30000)
    nativo(m, troca)
    dl = [j for j in range(0x40000) if m.ls[j] != ls1[j]]
    dr = [j for j in (1, 3) if m.g[j] != r1[j]]
    tot += 1; ok += not dl and not dr
    print(f'chamada {k:2d}: LS dif {len(dl)}' + (f' (1a em 0x{dl[0]:05X}, últ 0x{dl[-1]:05X})' if dl else '') + f' | r1/r3 dif {dr}')
print(f'{ok} de {tot} iguais')
