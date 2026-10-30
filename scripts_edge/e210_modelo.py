"""Modelo do laço E210 da DCC0 contra o oráculo (spu_interp_c.py). Regra: N = r19 voltas, cada uma com uma máscara
u16 (r23 anda 2 por volta); para os 16 índices da volta, do bit alto para o baixo: 1 = o próximo índice explícito do
fluxo (u16 em r8, anda 2), 0 = o próximo sequencial (contador = halfword 0 de r21). Software pipeline: grava N-1
blocos de 32 bytes em r17 (a volta 0 grava um bloco provisório no mesmo lugar do bloco 0).
uso: python -I e210_modelo.py <spu_recomp.c original> <pasta das fotos>"""
import glob
import importlib.util
import os
import struct
import sys

aqui = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location('si', os.path.join(aqui, 'spu_interp_c.py'))
si = importlib.util.module_from_spec(spec); spec.loader.exec_module(si)
src, fotos = sys.argv[1], sys.argv[2]
fn = 'gowajm_0072DF80_spu_func_0000DCC0__b'
s = open(src, encoding='latin-1').read()
cab = f'static SPU_PN void {fn}(spu_context* ctx) {{\n'
i = s.index(cab) + len(cab)
corpo = s[i:s.index('\n}\n', i)]
linhas = [('SPU_RET(ctx);' if l.strip().startswith('SPU_TAILX(') else l.strip()) for l in corpo.split('\n')
          if not l.strip().startswith('/*VK') and 'HALT-ASSERT' not in l]
prog, rot = si.compila('\n'.join(linhas))
alvo = rot['loc_0000E210']
saida = next(k for k in range(alvo, len(prog)) if prog[k][0] == 'if' and prog[k][4] == 'loc_0000E210') + 1
M = 0x3FFFF


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


def mascara(ls, r25, r23, jeito):
    if jeito == 'direto':
        a = (r25 + r23) & M
        return (ls[a] << 8) | ls[(a + 1) & M]
    q = (r25 + r23) & M & ~15
    o = r23 & 15
    return (ls[q + o] << 8) | ls[q + ((o + 1) & 15)]


def modelo(g, ls, jeito):
    ls = bytearray(ls)
    n = g[19][0]
    r25, r23, r8, r17 = g[25][0], g[23][0], g[8][0] & M, g[17][0] & M
    base = g[21][0] >> 16
    for v in range(n - 1):
        mk = mascara(ls, r25, r23 + 2 * v, jeito)
        out = []
        for t in range(16):
            if (mk >> (15 - t)) & 1:
                out.append(struct.unpack('>H', bytes(ls[r8:r8 + 2]))[0]); r8 = (r8 + 2) & M
            else:
                out.append(base & 0xFFFF); base += 1
        ls[r17 + 32 * v:r17 + 32 * v + 32] = struct.pack('>16H', *out)
    return ls, r17 + 32 * (n - 1), r8, base


for k in sorted({int(os.path.basename(f).split('_')[2]) for f in glob.glob(os.path.join(fotos, 'pl_foto_*_0.bin'))}):
    r0, ls0 = si.carrega(os.path.join(fotos, f'pl_foto_{k}_0.bin'))
    m = si.Maquina([list(r) for r in r0], ls0)
    pc = 0; passos = 0
    while pc != alvo and pc < len(prog) and passos < 5_000_000:
        pc = passo(m, pc); passos += 1
    if pc != alvo:
        print(f'foto {k}: não chega na E210'); continue
    g0 = [list(r) for r in m.g]; ls_e = bytes(m.ls)
    while pc != saida:
        pc = passo(m, pc)
    res = []
    for jeito in ('direto', 'alinhado'):
        ls_m, r17m, r8m, basem = modelo(g0, ls_e, jeito)
        dif = [a for a in range(len(ls_m)) if ls_m[a] != m.ls[a]]
        res.append(f'{jeito}: LS {"igual" if not dif else f"{len(dif)} bytes diferentes (1o {dif[0]:#x})"}, '
                   f'r10 {r17m:#x}/{m.g[10][0] & M:#x} r8 {r8m:#x}/{m.g[8][0] & M:#x} base {basem}/{m.g[21][0] >> 16}')
    print(f'foto {k}: N {g0[19][0]} r25 {g0[25][0]:#x} | ' + ' | '.join(res))
