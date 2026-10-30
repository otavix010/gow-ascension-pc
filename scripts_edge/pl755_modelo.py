"""Modelo do laço 0x30310 do plugin 0x755540 (posições comprimidas -> float4) contra o oráculo: em cada foto roda o
plugin até a entrada do laço (estado), depois até o fim, e compara a saída com o modelo:
  vértice i (12 bytes no fluxo, a partir de r27 - 48): 3 x u16 big-endian; v = (u16 & r33) + r31 (por componente);
  saída = (v0, v1, v2) * 2^(r34 - 32) por componente (r17 = -(32 - r34)) e 1.0, 2 vértices por volta, a partir de r36 + r18.
uso: python -I pl755_modelo.py <pasta das fotos>"""
import glob
import importlib.util
import os
import struct
import subprocess
import sys

aqui = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location('si', os.path.join(aqui, 'spu_interp_c.py'))
si = importlib.util.module_from_spec(spec); spec.loader.exec_module(si)
fotos = sys.argv[1]
pasta = 'gowapl_00755540'
raiz = os.path.join(aqui, '..')
s = open(os.path.join(raiz, 'port', 'spu', pasta, 'spu_recomp.c.sem_promocao'), encoding='latin-1').read()
corpo = ''
for fn in ('00030014', '00030310'):
    cab = f'void {pasta}_spu_func_{fn}(spu_context* ctx) {{\n'
    i = s.index(cab) + len(cab)
    b = s[i:s.index('\n}\n', i)]
    corpo += (f'loc_{int(fn, 16):08X}:\n' if f'loc_{int(fn, 16):08X}:' not in b else '') + b + '\n'
prog, rot = si.compila(corpo)
rot.setdefault('loc_FIM', len(prog))
codigo = bytes.fromhex(subprocess.run([sys.executable, '-I', os.path.join(aqui, 'elf_ea.py'), os.path.join(raiz, 'jogo', 'GOWA.elf'),
                                       '755540', '40'], capture_output=True, text=True).stdout.strip())
alvo = rot['loc_00030310']


def passo(m, pc):
    ins = prog[pc]; pc += 1
    t = ins[0]
    if t == 'op':
        _, d, nome, args = ins; m.g[d] = list(si.OPS[nome](*[m.arg(a) for a in args]))
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


def f(v, sh):
    return struct.unpack('>I', struct.pack('>f', v * 2.0 ** (sh - 32)))[0] if v else 0


tot = ok = 0
for p0 in sorted(glob.glob(os.path.join(fotos, 'pl_foto_*_0.bin')), key=lambda x: int(os.path.basename(x).split('_')[2])):
    k = int(os.path.basename(p0).split('_')[2])
    r0, ls0 = si.carrega(p0)
    m = si.Maquina([list(r) for r in r0], ls0)
    m.reloc = ls0.find(codigo) - 0x30000
    pc = 0
    while pc != alvo:
        pc = passo(m, pc)
    g = [list(r) for r in m.g]; ls_e = bytes(m.ls)
    while pc < len(prog):
        pc = passo(m, pc)
    n = (0x100000000 - g[19][0]) // 32 if g[19][0] else 0
    params = dict(r33=g[33], r31=g[31], r34=g[34], r17=g[17], r23=g[23], r24=g[24], r26=g[26], r28=g[28], r35=g[35])
    ent = (g[27][0] - 48) & 0x3FFFF
    out = (g[36][0] + g[18][0]) & 0x3FFFF
    bons = 0
    for i in range(2 * n):
        a = ent + 12 * i
        u = struct.unpack('>3H', ls_e[a:a + 6])
        esp = [f(si.sx(((u[c] & g[33][c]) + g[31][c]) & si.M32, 32), g[34][c]) for c in range(3)] + [g[35][3]]
        o = si.w4(bytes(m.ls[out + 16 * i:out + 16 * i + 16]))
        bons += o == esp
        if o != esp and bons == i and i < 3:
            print(f'  foto {k} vértice {i}: oráculo {[hex(x) for x in o]} modelo {[hex(x) for x in esp]}')
    tot += 2 * n; ok += bons
    print(f'foto {k}: {n} voltas, {bons}/{2 * n} vértices iguais; r34 {g[34]} r17 {g[17]} r23 {g[23]}')
print(f'total {ok}/{tot}')
