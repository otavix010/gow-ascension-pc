"""Rastro do laço E210 da DCC0 pelo oráculo (spu_interp_c.py): por volta, a máscara u16 em r25+r23, os ponteiros
(r8 = fluxo de índices explícitos, r17 = saída) e os 16 índices u16 gravados; para deduzir a regra do laço.
uso: python -I e210_rastro.py <spu_recomp.c original> <pasta das fotos> [foto] [voltas]"""
import glob
import importlib.util
import os
import struct
import sys

aqui = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location('si', os.path.join(aqui, 'spu_interp_c.py'))
si = importlib.util.module_from_spec(spec); spec.loader.exec_module(si)
src, fotos = sys.argv[1], sys.argv[2]
qual = int(sys.argv[3]) if len(sys.argv) > 3 else None
nv = int(sys.argv[4]) if len(sys.argv) > 4 else 6
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


def h(r):
    return ' '.join(f'{x:08X}' for x in r)


ks = sorted({int(os.path.basename(f).split('_')[2]) for f in glob.glob(os.path.join(fotos, 'pl_foto_*_0.bin'))})
for k in ks if qual is None else [qual]:
    r0, ls0 = si.carrega(os.path.join(fotos, f'pl_foto_{k}_0.bin'))
    m = si.Maquina([list(r) for r in r0], ls0)
    pc = 0; passos = 0
    while pc != alvo and pc < len(prog) and passos < 5_000_000:
        pc = passo(m, pc); passos += 1
    if pc != alvo:
        print(f'foto {k}: não chega na E210'); continue
    print(f'=== foto {k}: r19 (voltas) {m.g[19][0]}  r25 {m.g[25][0]:#x} r23 {m.g[23][0]:#x} r8 {m.g[8][0]:#x} '
          f'r17 {m.g[17][0]:#x} r21 {h(m.g[21])} r13 {h(m.g[13])} r20 {h(m.g[20])}')
    for nome in (22, 27, 29, 30, 31, 32, 33, 34, 53, 58, 11, 18, 24, 5, 9, 26):
        print(f'   r{nome}: {h(m.g[nome])}')
    volta = 0
    while pc != saida and pc < len(prog):
        if pc == alvo:
            a = (m.g[25][0] + m.g[23][0]) & 0x3FFFF
            mask = struct.unpack('>H', bytes(m.ls[a:a + 2]))[0]
            r8 = m.g[8][0] & 0x3FFFF; r17 = m.g[17][0] & 0x3FFFF
            explicito = struct.unpack('>8H', bytes(m.ls[r8:r8 + 16]))
            if volta < nv:
                print(f' volta {volta}: mask {mask:016b} r8 {r8:#x} fluxo {explicito} r17 {r17:#x} r21 {m.g[21][0]:08X}')
            volta += 1
            ant17 = r17
        pc = passo(m, pc)
        if pc == alvo or pc == saida:
            if volta <= nv:
                print(f'    gravou {struct.unpack(">16H", bytes(m.ls[ant17:ant17 + 32]))}')
    print(f'   fim: voltas {volta}, r17 {m.g[17][0]:#x}, r8 {m.g[8][0]:#x}, r23 {m.g[23][0]:#x}, r21 {h(m.g[21])}')
    if qual is None and k != ks[0]:
        pass
