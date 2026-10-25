"""Para um laço de uma função SPU traduzida (rótulo que desvia para si mesmo):
quais registradores atravessam iterações (lidos antes de escritos no corpo,
e escritos no corpo), quais são só de entrada (lidos, nunca escritos) e quais
são temporários; e a cadeia de cada carregado (o que o define no fim do corpo).

uso: python laco_carregados.py <dir do módulo> <função hex> <rótulo hex>"""
import re
import sys
from pathlib import Path

d, fn, rot = Path(sys.argv[1]), sys.argv[2].upper().rjust(8, '0'), sys.argv[3].upper().rjust(8, '0')
src = (d / 'spu_recomp.c').read_text(encoding='latin-1')
m = re.search(r'^(?:static )?(?:SPU_PN )?void \w+_spu_func_' + fn + r'(?:__b)?\(spu_context\* ctx\) \{\n', src, re.M)
corpo = src[m.end():src.find('\n}\n', m.end())]
i = corpo.find(f'loc_{rot}:')
j = corpo.find(f'goto loc_{rot};', i)
fim = corpo.find('\n', j)
laco = [l.strip() for l in corpo[i:fim].split('\n')[1:]]
lidos_antes, escritos, lidos = set(), set(), set()
defs = {}
for t in laco:
    if t.startswith('/*') or not t:
        continue
    mm = re.match(r'ctx->gpr\[(\d+)\] = (.*);$', t)
    rhs = mm.group(2) if mm else t
    for r in re.findall(r'ctx->gpr\[(\d+)\]', rhs):
        r = int(r)
        lidos.add(r)
        if r not in escritos:
            lidos_antes.add(r)
    if mm:
        w = int(mm.group(1))
        escritos.add(w)
        defs[w] = rhs
carregados = sorted(lidos_antes & escritos)
entrada = sorted(lidos_antes - escritos)
temporarios = sorted(escritos - lidos_antes)
print(f'laço loc_{rot}: {len(laco)} linhas')
print('carregados entre iterações:', carregados)
print('só entrada (invariantes):', entrada)
print('temporários:', temporarios)
print('--- definição (última) de cada carregado ---')
for r in carregados:
    print(f'  r{r} = {defs[r][:110]}')
