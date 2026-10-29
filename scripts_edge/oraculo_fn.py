"""Oráculo de uma função inteira do módulo do EDGE (do começo até o SPU_RET ou a chamada de cauda final), a
partir das fotos de entrada e saída (sonda_fn.py). As chamadas de núcleo vetorial (/*VK ...*/) são puladas:
o laço traduzido logo depois faz as mesmas voltas.
uso: python -I oraculo_fn.py <spu_recomp.c original> <função> <pasta das fotos> [k ...]"""
import glob
import importlib.util
import os
import re
import sys

aqui = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location('si', os.path.join(aqui, 'spu_interp_c.py'))
si = importlib.util.module_from_spec(spec); spec.loader.exec_module(si)

src, fn, fotos = sys.argv[1], sys.argv[2], sys.argv[3]
ks = [int(x) for x in sys.argv[4:]]
s = open(src, encoding='latin-1').read()
cab = f'static SPU_PN void {fn}(spu_context* ctx) {{\n'
i = s.index(cab) + len(cab)
corpo = s[i:s.index('\n}\n', i)]
linhas = []
for l in corpo.split('\n'):
    t = l.strip()
    if t.startswith('/*VK'):
        continue
    if t.startswith('SPU_TAILX('):
        linhas.append('SPU_RET(ctx);'); continue
    m = re.fullmatch(r'if \((.+)\) SPU_TAILX\(.+\);', t)
    if m:
        raise ValueError('saída condicional por cauda não suportada: ' + t)
    linhas.append(t)
prog, rot = si.compila('\n'.join(linhas))
rot.setdefault('loc_FIM', len(prog))
ok = dif = 0
for p0 in sorted(glob.glob(os.path.join(fotos, 'pl_foto_*_0.bin')), key=lambda x: int(os.path.basename(x).split('_')[2])):
    k = int(os.path.basename(p0).split('_')[2])
    if ks and k not in ks: continue
    r0, ls0 = si.carrega(p0)
    r1, ls1 = si.carrega(p0.replace('_0.bin', '_1.bin'))
    m = si.Maquina([list(r) for r in r0], ls0)
    n = si.roda(prog, rot, m)
    dr = [j for j in range(128) if m.g[j] != r1[j]]
    dl = [j for j in range(0x40000) if m.ls[j] != ls1[j]]
    bom = not dr and not dl
    ok += bom; dif += not bom
    print(f'chamada {k:2d}: {n} instruções | regs dif {dr[:8]} | LS dif {len(dl)}' + (f' (1a em 0x{dl[0]:05X})' if dl else ''))
print(f'{ok} iguais, {dif} diferentes')
