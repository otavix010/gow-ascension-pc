"""Mapa de uma função SPU traduzida (port/spu/<mod>/spu_recomp.c): blocos
(rótulos loc_*), quantas operações cada um tem, desvios (goto/laços), chamadas,
e os acessos à LS (base + deslocamento) -- para entender o algoritmo antes de
reescrever em nativo.

uso: python cfg_funcao.py <dir do módulo> <endereço hex> [--ls]"""
import re
import sys
from collections import Counter
from pathlib import Path

d = Path(sys.argv[1])
alvo = sys.argv[2].upper().rjust(8, '0')
src = (d / 'spu_recomp.c').read_text(encoding='latin-1')
m = re.search(r'^(?:static )?(?:SPU_PN )?void \w+_spu_func_' + alvo + r'(?:__b)?\(spu_context\* ctx\) \{\n', src, re.M)
corpo = src[m.end():src.find('\n}\n', m.end())]
blocos = []          # (rotulo, linhas)
atual, linhas = 'entrada', []
for ln in corpo.split('\n'):
    t = ln.strip()
    mm = re.match(r'^(loc_[0-9A-F]+):', t)
    if mm:
        blocos.append((atual, linhas))
        atual, linhas = mm.group(1), []
        continue
    linhas.append(t)
blocos.append((atual, linhas))
ordem = [b for b, _ in blocos]
for nome, ls in blocos:
    ops = Counter()
    for t in ls:
        for op in re.findall(r'spu_([a-z0-9_]+)\(', t):
            ops[op] += 1
    n = sum(ops.values())
    gotos = []
    for t in ls:
        for g in re.findall(r'goto (loc_[0-9A-F]+);', t):
            atras = g in ordem and ordem.index(g) <= ordem.index(nome)
            cond = t.startswith('if')
            gotos.append(f'{"<-" if atras else "->"}{g[4:]}{"?" if cond else ""}')
    calls = re.findall(r'_spu_func_([0-9A-F]{8})(?:__b)?\(ctx\)', ' '.join(ls))
    fl = sum(ops[o] for o in ('fa', 'fs', 'fm', 'fma', 'fnms', 'fms', 'frest', 'frsqest', 'fi', 'fcgt', 'fceq', 'fcmgt'))
    print(f'{nome:14s} ops={n:4d} float={fl:3d} ls_r={ops["ls_read128"]:3d} ls_w={ops["ls_write128"]:3d} shufb={ops["shufb"]:3d}'
          + (f'  chama {",".join(c[4:] for c in calls)}' if calls else '') + (f'  {" ".join(gotos)}' if gotos else ''))
    if '--ls' in sys.argv:
        bases = Counter()
        for t in ls:
            for b, o in re.findall(r'spu_ls_(?:read|write)128\(ctx, ctx->gpr\[(\d+)\]\._u32\[0\] \+ (-?0x[0-9A-F]+|ctx->gpr\[\d+\]\._u32\[0\])\)', t):
                bases[f'r{b}+{o}'] += 1
        if bases:
            print('               LS:', ', '.join(f'{k}x{v}' if v > 1 else k for k, v in sorted(bases.items())))
