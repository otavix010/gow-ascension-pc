# Grafo de chamadas/saltos do módulo traduzido gowajm_0072DF80 a partir da entrada (AD98), com o peso do perfil.
# Uso: python grafo_jm.py [perfil_simbolizado.txt]
import re, sys
s = open('port/spu/gowajm_0072DF80/spu_recomp.c', encoding='utf-8', errors='replace').read()
defs = [(m.start(), m.group(1)) for m in re.finditer(r'^static SPU_PN void gowajm_0072DF80_spu_func_([0-9A-F]{8})__b\(spu_context\* ctx\)\s*\{', s, re.M)]
g = {}
for i, (p, n) in enumerate(defs):
    corpo = s[p:defs[i + 1][0] if i + 1 < len(defs) else len(s)]
    refs = set(re.findall(r'gowajm_0072DF80_spu_func_([0-9A-F]{8})', corpo[60:])) - {n}
    g[n] = (corpo.count('\n'), sorted(refs), corpo.count('spu_indirect_branch'), corpo.count('SPU_CALL') + corpo.count('spu_drain_call'))
vis, pilha = set(), ['0000AD98']
while pilha:
    f = pilha.pop()
    if f in vis or f not in g: continue
    vis.add(f); pilha += g[f][1]
print(len(defs), 'funções;', len(vis), 'alcançáveis da entrada; linhas', sum(g[f][0] for f in vis))
ind = [f for f in vis if g[f][2]]
print('com desvio indireto:', ' '.join(f[4:] for f in sorted(ind)))
