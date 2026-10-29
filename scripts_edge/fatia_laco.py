"""Fatia de dependências do laço de descarte (loc_0000D92C da D640): quais instruções alimentam as
gravações (ST) e o contador, e por onde entram as contas de float (a decisão).
uso: python -I fatia_laco.py <spu_recomp.c original>"""
import re
import sys
import importlib.util
import os

spec = importlib.util.spec_from_file_location('si', os.path.join(os.path.dirname(__file__), 'spu_interp_c.py'))
si = importlib.util.module_from_spec(spec); spec.loader.exec_module(si)
s = open(sys.argv[1], encoding='latin-1').read()
i = s.index('static SPU_PN void gowajm_0072DF80_spu_func_0000D640__b(spu_context* ctx) {')
f = s[i:s.index('\n}\n', i)]
bloco = f[f.index('loc_0000D92C:'):f.index('loc_0000DB70:')]
prog, rot = si.compila(bloco)
REG = re.compile(r'ctx->gpr\[(\d+)\]')

def usos(ins):
    t = ins[0]
    if t == 'op': return {int(x) for a in ins[3] for x in REG.findall(a)}
    if t == 'ld': return {int(x) for x in REG.findall(ins[2])}
    if t == 'st': return {int(x) for x in REG.findall(ins[1])} | {ins[2]}
    if t == 'if': return {ins[1]}
    return set()

def defs(ins):
    return {ins[1]} if ins[0] in ('op', 'ld', 'k') else set()

FLOAT = {'fs', 'fa', 'fm', 'fm_r', 'fma', 'fma_r', 'fms', 'fnms', 'fcgt', 'cflts'}
corpo = [x for x in prog if x[0] != 'goto']
n = len(corpo)
# fatia para trás, com dependência entre voltas (duas passadas pelo corpo)
vivos = set()
for ins in corpo:
    if ins[0] in ('st', 'if'): vivos |= usos(ins)
marc = [False] * n
for _ in range(6):
    for j in range(n - 1, -1, -1):
        ins = corpo[j]
        if ins[0] in ('st', 'if') or defs(ins) & vivos:
            if not marc[j]:
                marc[j] = True
            vivos |= usos(ins)
# quais instruções dependem (para frente) de float
sujo = set()
for _ in range(6):
    for ins in corpo:
        u = usos(ins)
        if (ins[0] == 'op' and ins[2] in FLOAT) or (u & sujo):
            sujo |= defs(ins)
print(f'{n} instruções no corpo; {sum(marc)} na fatia das gravações/contador')
for j, ins in enumerate(corpo):
    if not marc[j]: continue
    dep_f = bool(usos(ins) & sujo) or (ins[0] == 'op' and ins[2] in FLOAT)
    if ins[0] == 'op': txt = f'r{ins[1]} = {ins[2]}(' + ', '.join(re.sub(r'ctx->gpr\[(\d+)\]', r'r\1', a.strip()) for a in ins[3]) + ')'
    elif ins[0] == 'ld': txt = f'r{ins[1]} = LD(' + re.sub(r'ctx->gpr\[(\d+)\]\._u32\[0\]', r'r\1', ins[2]) + ')'
    elif ins[0] == 'st': txt = 'ST(' + re.sub(r'ctx->gpr\[(\d+)\]\._u32\[0\]', r'r\1', ins[1]) + f', r{ins[2]})'
    elif ins[0] == 'if': txt = f'if r{ins[1]} {ins[3]} 0'
    else: txt = str(ins)
    print(('  [DECISÃO] ' if dep_f else '            ') + txt)
