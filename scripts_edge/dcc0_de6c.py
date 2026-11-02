"""Quantas vezes cada foto da DCC0 passa pelos laços DE6C/DEF8/DFF0 (desempacotar índices), pelo oráculo, e o estado
na primeira entrada do DE6C. uso: python -I dcc0_de6c.py <spu_recomp.c original> <pasta das fotos>"""
import glob
import importlib.util
import os
import sys

aqui = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location('si', os.path.join(aqui, 'spu_interp_c.py'))
si = importlib.util.module_from_spec(spec); spec.loader.exec_module(si)
src, fotos = sys.argv[1], sys.argv[2]
fn = 'gowajm_0072DF80_spu_func_0000DCC0__b'
s = open(src, encoding='latin-1').read()
cab = f'static SPU_PN void {fn}(spu_context* ctx) {{' + chr(10)
i = s.index(cab) + len(cab)
corpo = s[i:s.index(chr(10) + '}' + chr(10), i)]
linhas = [('SPU_RET(ctx);' if l.strip().startswith('SPU_TAILX(') else l.strip()) for l in corpo.split(chr(10))
          if not l.strip().startswith('/*VK') and 'HALT-ASSERT' not in l and 'GOWA_EDGE_NATIVO' not in l]
prog, rot = si.compila(chr(10).join(linhas))
alvos = {rot[r]: r for r in ('loc_0000DE6C', 'loc_0000DEF8', 'loc_0000DFF0', 'loc_0000E0E4', 'loc_0000E0CC')}
for p0 in sorted(glob.glob(os.path.join(fotos, 'pl_foto_*_0.bin')), key=lambda x: int(os.path.basename(x).split('_')[2])):
    r0, ls0 = si.carrega(p0)
    m = si.Maquina([list(r) for r in r0], ls0)
    pc, cont, passos, primeira = 0, {}, 0, None
    while pc < len(prog) and passos < 2000000:
        if pc in alvos:
            cont[alvos[pc]] = cont.get(alvos[pc], 0) + 1
            if alvos[pc] == 'loc_0000DE6C' and primeira is None:
                primeira = {r: list(m.g[r]) for r in range(2, 80)}
        ins = prog[pc]; pc += 1; passos += 1
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
        elif t == 'ret':
            break
        else:
            break
    print(os.path.basename(p0), cont, 'passos', passos, 'largura r13' if primeira else '', [hex(w) for w in primeira[13]] if primeira else '')
