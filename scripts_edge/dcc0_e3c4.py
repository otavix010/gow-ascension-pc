"""Estado da DCC0 na entrada do laço E3C4 (reconstrução dos triângulos), pelo oráculo, a partir das fotos da DCC0:
registradores do laço, as tabelas em r23 (0xBC00+r79) e r26 (0xBD00+r79), as palavras de controle e a saída.
uso: python -I dcc0_e3c4.py <spu_recomp.c original> <pasta das fotos> [k]"""
import glob
import importlib.util
import os
import struct
import sys

aqui = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location('si', os.path.join(aqui, 'spu_interp_c.py'))
si = importlib.util.module_from_spec(spec); spec.loader.exec_module(si)
src, fotos = sys.argv[1], sys.argv[2]
k = int(sys.argv[3]) if len(sys.argv) > 3 else 10
fn = 'gowajm_0072DF80_spu_func_0000DCC0__b'
s = open(src, encoding='latin-1').read()
cab = f'static SPU_PN void {fn}(spu_context* ctx) {{\n'
i = s.index(cab) + len(cab)
corpo = s[i:s.index('\n}\n', i)]
linhas = [('SPU_RET(ctx);' if l.strip().startswith('SPU_TAILX(') else l.strip()) for l in corpo.split('\n')
          if not l.strip().startswith('/*VK') and 'HALT-ASSERT' not in l]
prog, rot = si.compila('\n'.join(linhas))
alvo = rot['loc_0000E3C4']
r0, ls0 = si.carrega(os.path.join(fotos, f'pl_foto_{k}_0.bin'))
m = si.Maquina([list(r) for r in r0], ls0)
# roda até chegar ao laço pela primeira vez
pc = 0
while pc != alvo:
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
g = m.g
for r in (3, 5, 7, 10, 11, 12, 13, 17, 18, 19, 20, 21, 22, 23, 24, 25, 26, 27, 28, 29, 30, 31, 32, 33, 34, 35, 36, 37, 53, 58, 79):
    print(f'r{r:<3}', ' '.join(f'{w:08X}' for w in g[r]))
ls = m.ls
t23, t26 = g[23][0] & 0x3FFF0, g[26][0] & 0x3FFF0
print('tabela r23:'); [print('  ', ls[t23 + 16 * e:t23 + 16 * e + 16].hex(' ')) for e in range(16)]
print('tabela r26:'); [print('  ', ls[t26 + 16 * e:t26 + 16 * e + 16].hex(' ')) for e in range(16)]
c = (g[29][0] - 2) & 0x3FFFF
print('controle (u16 a partir de r29-2):', ls[c:c + 32].hex(' ', 2))
print('índices novos em r20:', ls[g[20][0] & 0x3FFF0:(g[20][0] & 0x3FFF0) + 48].hex(' ', 2))
print('saída em r17:', hex(g[17][0]), 'voltas r21:', g[21][0])
