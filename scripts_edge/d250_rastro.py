"""Rastro do laço D4DC da D250 pelo oráculo numa foto: registradores na entrada do laço e as gravações de cada volta.
uso: python -I d250_rastro.py <spu_recomp.c original> <pasta das fotos> <k> [regs...]"""
import importlib.util
import os
import sys

aqui = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location('si', os.path.join(aqui, 'spu_interp_c.py'))
si = importlib.util.module_from_spec(spec); spec.loader.exec_module(si)
src, fotos, k = sys.argv[1], sys.argv[2], int(sys.argv[3])
regs = [int(x) for x in sys.argv[4:]]
fn = 'gowajm_0072DF80_spu_func_0000D250__b'
s = open(src, encoding='latin-1').read()
cab = f'static SPU_PN void {fn}(spu_context* ctx) {{\n'
i = s.index(cab) + len(cab)
corpo = s[i:s.index('\n}\n', i)]
linhas = [l.strip() for l in corpo.split('\n') if not l.strip().startswith('/*VK') and 'HALT-ASSERT' not in l]
prog, rot = si.compila('\n'.join(linhas))
alvo = rot['loc_0000D4DC']
r0, ls0 = si.carrega(os.path.join(fotos, f'pl_foto_{k}_0.bin'))
m = si.Maquina([list(r) for r in r0], ls0)
pc, volta = 0, -1
while pc < len(prog):
    if pc == alvo:
        volta += 1
        if volta == 0:
            for r in regs:
                print(f'r{r:<3}', ' '.join(f'{w:08X}' for w in m.g[r]))
    ins = prog[pc]
    if ins[0] == 'st' and volta >= 0:
        a = m.end(ins[1])
        print(f'volta {volta}: st r{ins[2]} -> {a:#07x}:', ' '.join(f'{w:08X}' for w in m.g[ins[2]]))
    if ins[0] == 'ld' and volta >= 0 and volta < 2:
        a = m.end(ins[2])
        print(f'volta {volta}: ld r{ins[1]} <- {a:#07x}:', ' '.join(f'{w:08X}' for w in si.w4(bytes(m.ls[a:a + 16]))))
    t = ins[0]; pc += 1
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
print('voltas', volta + 1)
