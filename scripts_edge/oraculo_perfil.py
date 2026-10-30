"""Quantas instruções cada trecho (de um rótulo ao seguinte) de uma função do EDGE executa nas fotos, pelo oráculo
(spu_interp_c.py), para escolher qual laço vale um nativo. Os núcleos /*VK*/ são pulados: o laço traduzido logo
depois faz as mesmas voltas e entra na conta dele.
uso: python -I oraculo_perfil.py <spu_recomp.c original> <função> <pasta das fotos>"""
import bisect
import collections
import glob
import importlib.util
import os
import re
import sys

aqui = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location('si', os.path.join(aqui, 'spu_interp_c.py'))
si = importlib.util.module_from_spec(spec); spec.loader.exec_module(si)

src, fn, fotos = sys.argv[1:4]
s = open(src, encoding='latin-1').read()
cab = f'static SPU_PN void {fn}(spu_context* ctx) {{\n'
i = s.index(cab) + len(cab)
corpo = s[i:s.index('\n}\n', i)]
linhas = []
for l in corpo.split('\n'):
    t = l.strip()
    if t.startswith('/*VK') or 'HALT-ASSERT' in t:   # as asserções de halt (heqi...) não disparam num job normal
        continue
    if t.startswith('SPU_TAILX('):
        linhas.append('SPU_RET(ctx);'); continue
    linhas.append(t)
prog, rot = si.compila('\n'.join(linhas))
rot.setdefault('loc_FIM', len(prog))
marcos = sorted((v, k) for k, v in rot.items())
idx = [v for v, _ in marcos]
conta = collections.Counter()


def roda_contando(prog, rot, maq, limite=10 ** 7):
    pc, n = 0, 0
    while pc < len(prog):
        n += 1
        if n > limite:
            raise RuntimeError('laço sem fim?')
        j = bisect.bisect_right(idx, pc) - 1
        conta[marcos[j][1] if j >= 0 else '(início)'] += 1
        ins = prog[pc]; pc += 1
        t = ins[0]
        if t == 'op':
            _, d, nome, args = ins
            maq.g[d] = list(si.OPS[nome](*[maq.arg(a) for a in args]))
        elif t == 'ld':
            a = maq.end(ins[2]); maq.g[ins[1]] = si.w4(bytes(maq.ls[a:a + 16]))
        elif t == 'st':
            a = maq.end(ins[1]); maq.ls[a:a + 16] = si.b16(maq.g[ins[2]])
        elif t == 'k':
            maq.g[ins[1]] = list(ins[2])
        elif t == 'if':
            _, r, bits, cmp, alvo = ins
            v = maq.g[r][0] & (si.M32 if bits == 32 else 0xFFFF)
            if (v != 0) == (cmp == '!='):
                pc = rot.get(alvo, len(prog))
        elif t == 'goto':
            pc = rot.get(ins[1], len(prog))
    return n


tot = 0
for p0 in sorted(glob.glob(os.path.join(fotos, 'pl_foto_*_0.bin'))):
    r0, ls0 = si.carrega(p0)
    tot += roda_contando(prog, rot, si.Maquina([list(r) for r in r0], ls0))
for nome, n in conta.most_common(15):
    print(f'{nome:16s} {n:9d}  {100.0 * n / tot:5.1f}%')
print(f'total {tot} instruções em {len(glob.glob(os.path.join(fotos, "pl_foto_*_0.bin")))} chamadas')
