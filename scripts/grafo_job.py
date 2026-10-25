"""Grafo de chamadas de um módulo de job SPU traduzido (port/spu/<mod>/spu_recomp.c)
a partir de uma entrada: funções alcançáveis, tamanho (linhas), chamadas
diretas, desvios indiretos e plugins (spu_drain_call para fora do módulo).

uso: python grafo_job.py <dir do módulo> <entrada hex> [--arvore]"""
import re
import sys
from pathlib import Path

d = Path(sys.argv[1])
entrada = int(sys.argv[2], 16)
src = (d / 'spu_recomp.c').read_text(encoding='latin-1')
mod = d.name
re_hdr = re.compile(r'^(?:static )?(?:SPU_PN )?void (' + re.escape(mod) + r'_spu_func_([0-9A-F]{8})(?:__b)?)\(spu_context\* ctx\) \{', re.M)
funcs = {}
for m in re_hdr.finditer(src):
    ini = m.end()
    fim = src.find('\n}\n', ini)
    funcs[int(m.group(2), 16)] = (m.group(1), src[ini:fim])
re_call = re.compile(re.escape(mod) + r'_spu_func_([0-9A-F]{8})(?:__b)?\(ctx\)')
vistos, ordem, pilha = set(), [], [entrada]
while pilha:
    a = pilha.pop()
    if a in vistos or a not in funcs:
        continue
    vistos.add(a)
    ordem.append(a)
    for c in re_call.findall(funcs[a][1]):
        pilha.append(int(c, 16))
tot = 0
print(f'{len(ordem)} funções alcançáveis a partir de 0x{entrada:X} (de {len(funcs)} no módulo)')
for a in sorted(ordem):
    corpo = funcs[a][1]
    n = corpo.count('\n')
    tot += n
    chamadas = sorted({int(c, 16) for c in re_call.findall(corpo)} - {a})
    ind = corpo.count('spu_indirect_branch')
    if '--arvore' in sys.argv or n > 150:
        print(f'  0x{a:05X} {n:5d} linhas  chama {", ".join(f"{c:X}" for c in chamadas) or "-"}'
              + (f'  indiretos {ind}' if ind else ''))
print(f'total {tot} linhas traduzidas')
nao = sorted({int(c, 16) for a in ordem for c in re_call.findall(funcs[a][1])} - set(funcs))
if nao:
    print('chamadas sem função no arquivo:', ', '.join(f'{c:X}' for c in nao))
