"""Registradores lidos antes de escritos no corpo de um laço traduzido (do rótulo até o desvio de volta), e os
escritos nele. uso: python -I vivos_laco.py <spu_recomp.c> <função> <rótulo do laço>"""
import re
import sys

src, fn, rot = sys.argv[1:4]
s = open(src, encoding='latin-1').read()
i = s.index(f'static SPU_PN void {fn}(spu_context* ctx) {{')
f = s[i:s.index('\n}\n', i)]
j = f.index(f'{rot}:\n')
corpo = []
for l in f[j:].split('\n')[1:]:
    corpo.append(l)
    if f'goto {rot};' in l:
        break
lidos, escritos = [], set()
for l in corpo:
    t = l.strip()
    m = re.match(r'ctx->gpr\[(\d+)\] = (.*);$', t)
    rhs = m.group(2) if m else t
    for r in re.findall(r'ctx->gpr\[(\d+)\]', rhs):
        r = int(r)
        if r not in escritos and r not in lidos:
            lidos.append(r)
    if m:
        escritos.add(int(m.group(1)))
print('vivos na entrada:', sorted(lidos))
print('escritos no corpo:', sorted(escritos))
