"""Convenção de chamada interna nas funções SPU traduzidas (port/spu/*/spu_recomp.c).

Cada função F vira um corpo `static SPU_PN void F__b(spu_context*)` (SPU_PN =
__attribute__((preserve_none)) no clang, ver spu_context.h) e as chamadas
diretas F(ctx) dentro do mesmo arquivo passam a chamar F__b. O nome F continua
existindo com a convenção padrão, como um invólucro que chama F__b: é ele que
vai para os ponteiros (trampolim, tabelas, registro de funções).

Por quê: na convenção do Windows x64 a função chamada salva rbx, rbp, rsi, rdi,
r12-r15 e xmm6-xmm15 (10 stores + 10 loads de 16 bytes, 8 push/pop) em toda
chamada. O estado da SPU fica no ctx, então quase nada atravessa uma chamada; com
preserve_none, a chamada direta entre funções traduzidas não salva nada.

uso: python spu_cc_interno.py aplicar|tirar [diretorio ...]
     (backup <arquivo>.sem_pn; o tirar restaura com data nova)
"""
import os as _os_raiz
RAIZ = _os_raiz.environ.get('GOWA_RAIZ') or _os_raiz.path.dirname(_os_raiz.path.dirname(_os_raiz.path.abspath(__file__)))  # raiz do projeto
import glob
import os
import re
import shutil
import sys
import time

R = RAIZ
DEF = re.compile(r'^void (\w+_spu_func_[0-9A-Fa-f]+)\(spu_context\* ctx\) \{$', re.M)


def aplicar(c):
    bak = c + '.sem_pn'
    src = open(c, encoding='utf-8').read()
    if 'SPU_PN void' in src:
        print(f'{c}: já aplicado')
        return
    shutil.copyfile(c, bak)
    nomes = DEF.findall(src)
    if not nomes:
        return
    conj = set(nomes)
    # chamadas diretas: NOME(ctx) -- definições têm "(spu_context* ctx)", ponteiros não têm parênteses
    chamada = re.compile(r'\b(\w+_spu_func_[0-9A-Fa-f]+)\(ctx\)')
    n_ch = 0

    def troca(m):
        nonlocal n_ch
        if m.group(1) in conj:
            n_ch += 1
            return m.group(1) + '__b(ctx)'
        return m.group(0)
    src = chamada.sub(troca, src)
    src = DEF.sub(lambda m: f'static SPU_PN void {m.group(1)}__b(spu_context* ctx) {{', src)
    proto = '\n/* spu_cc_interno.py: corpos com convenção interna (SPU_PN), invólucros no fim */\n' + \
        ''.join(f'static SPU_PN void {n}__b(spu_context* ctx);\n' for n in nomes)
    k = src.find('#include "spu_recomp.h"')
    if k < 0:
        raise SystemExit(f'{c}: sem #include "spu_recomp.h"')
    k = src.index('\n', k) + 1
    src = src[:k] + proto + src[k:]
    src += '\n/* spu_cc_interno.py: invólucros com a convenção padrão (ponteiros, tabelas) */\n' + \
        ''.join(f'void {n}(spu_context* ctx) {{ {n}__b(ctx); }}\n' for n in nomes)
    open(c, 'w', encoding='utf-8').write(src)
    print(f'{os.path.basename(os.path.dirname(c))}: {len(nomes)} funções, {n_ch} chamadas diretas')


def tirar(c):
    bak = c + '.sem_pn'
    if not os.path.exists(bak):
        return
    shutil.copyfile(bak, c)          # copyfile: data nova, o ninja recompila
    os.utime(c, (time.time(), time.time()))
    os.remove(bak)
    print(f'{c}: restaurado')


def main():
    modo = sys.argv[1]
    dirs = sys.argv[2:] or sorted(glob.glob(os.path.join(R, 'port', 'spu', '*')))
    for d in dirs:
        c = os.path.join(d, 'spu_recomp.c')
        if os.path.isfile(c):
            (aplicar if modo == 'aplicar' else tirar)(c)


if __name__ == '__main__':
    main()
