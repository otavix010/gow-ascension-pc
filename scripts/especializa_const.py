"""Especialização guardada de uma função SPU promovida por valores de registradores que chegam
constantes (22/10). Ex.: a 0x4F00 do 0x727200 usa 60x como máscara de shufb o r16, que a 0x4930
(três peças antes, outra função) define com `ila 0x10203`; e 4 máscaras `cwd` sobre o r1 (pilha,
sempre alinhada em 16). Separadas em funções, o compilador não vê as constantes e cada shufb vira
a sequência genérica (~7 instruções) em vez de um vpermi2b/pshufb só.

Gera <X>__k = cópia de <X>__b com, logo depois do RELOAD, os registradores fixados nos valores
(e o r1 com os 4 bits baixos zerados), e põe na entrada de <X>__b:
    if (__builtin_expect(<os valores conferem>, 1)) { __attribute__((musttail)) return <X>__k(ctx); }
Fora do caso, a função original segue igual. Backup <arquivo>.sem_const_esp na primeira vez;
cada aplicação parte dele.

uso: python especializa_const.py port/spu/<mód> <EA> [<EA> ...] --reg 16=00010203:00010203:00010203:00010203 [--r1]
     python especializa_const.py port/spu/<mód> --tirar
(--reg R=w0:w1:w2:w3 em hex, as 4 palavras do registrador; --r1: exige r1 alinhado em 16)"""
import re
import shutil
import sys
from pathlib import Path

argv = sys.argv[1:]
d = Path(argv[0])
f = d / 'spu_recomp.c'
bak = f.with_name(f.name + '.sem_const_esp')
if '--tirar' in argv:
    if bak.exists():
        shutil.copy2(bak, f)
        print('restaurado', f)
    sys.exit(0)
if not bak.exists():
    shutil.copy2(f, bak)
src = bak.read_bytes().decode('latin-1')
nl = '\r\n' if '\r\n' in src else '\n'
mod = d.name
regs = {}
for i, a in enumerate(argv):
    if a == '--reg':
        r, v = argv[i + 1].split('=')
        regs[int(r)] = [int(x, 16) for x in v.split(':')]
r1 = '--r1' in argv
eas = [int(a, 16) for a in argv[1:] if re.fullmatch(r'[0-9A-Fa-f]{3,8}', a) and argv[argv.index(a) - 1] not in ('--reg',)]

for ea in eas:
    nome = f'{mod}_spu_func_{ea:08X}'
    h = f'static SPU_PN void {nome}__b(spu_context* ctx) {{'
    i = src.index(h)
    a = src.rindex('#define SPILL', 0, i)
    fim = src.index(nl + '}' + nl, i) + len(nl + '}' + nl)
    und = src.index('#undef RELOAD', fim) + len('#undef RELOAD')
    bloco = src[a:und]                       # defines + função + undefs
    corpo = src[i:fim]
    linhas = corpo.split(nl)
    assert linhas[2].strip() == 'RELOAD;', nome
    usados = set(int(x) for x in re.findall(r'\b_r(\d+)\b', corpo))
    fix = []
    for r, w in regs.items():
        if r in usados:
            fix.append(f'        _r{r} = spu_il_words(0x{w[0]:08X}u, 0x{w[1]:08X}u, 0x{w[2]:08X}u, 0x{w[3]:08X}u);   /* especializa_const.py */')
    if r1 and 1 in usados:
        fix.append('        _r1._u32[0] &= ~15u;   /* especializa_const.py: pilha alinhada (conferido na entrada) */')
    # clone
    clone_linhas = [linhas[0].replace(f'{nome}__b(', f'{nome}__k(')] + linhas[1:3] + fix + linhas[3:]
    # rótulos do clone: loc_ -> kloc_ (gotos ficam dentro dele)
    clone = nl.join(clone_linhas)
    clone = re.sub(r'\bloc_([0-9A-F]{8})\b', r'kloc_\1', clone)
    # guarda na entrada da original (depois do RELOAD)
    conds = []
    for r, w in regs.items():
        if r in usados:
            conds.append(f'_r{r}._u32[0] == 0x{w[0]:08X}u && _r{r}._u32[1] == 0x{w[1]:08X}u && '
                         f'_r{r}._u32[2] == 0x{w[2]:08X}u && _r{r}._u32[3] == 0x{w[3]:08X}u')
    if r1 and 1 in usados:
        conds.append('(_r1._u32[0] & 15u) == 0')
    if not conds:
        print(f'{nome}: nenhum dos registradores é usado; pulada')
        continue
    guarda = (f'        if (__builtin_expect({" && ".join(conds)}, 1)) {{ __attribute__((musttail)) return {nome}__k(ctx); }}'
              '   /* especializa_const.py */')
    nova = nl.join(linhas[:3] + [guarda] + linhas[3:])
    defs = src[a:i]
    undefs = src[fim:und]
    novo_bloco = defs + clone + undefs + nl + nl + defs + nova + undefs
    src = src[:a] + novo_bloco + src[und:]
    k = src.index('#include "spu_recomp.h"')
    k = src.index(nl, k) + len(nl)
    src = src[:k] + f'static SPU_PN void {nome}__k(spu_context* ctx);' + nl + src[k:]
    print(f'{nome}: clone __k com {len(fix)} fixações; guarda: {len(conds)} condições')
if 'spu_il_words' not in src.split('#include "spu_recomp.h"')[0] and 'static inline u128 spu_il_words' not in src:
    k = src.index('#include "spu_recomp.h"')
    k = src.index(nl, k) + len(nl)
    src = (src[:k] + 'static inline u128 spu_il_words(uint32_t a, uint32_t b, uint32_t c, uint32_t d) '
           '{ u128 r; r._u32[0] = a; r._u32[1] = b; r._u32[2] = c; r._u32[3] = d; return r; }   /* especializa_const.py */'
           + nl + src[k:])
f.write_bytes(src.encode('latin-1'))
