"""Transcreve um laço SPU traduzido de um bloco só (o rótulo desvia para si mesmo no fim) com os registradores em
variáveis locais em vez de ctx->gpr: as mesmas operações (spu_*), na mesma ordem, então o resultado é idêntico bit a
bit. O ganho é tirar o vai-e-volta de cada registrador pela memória do contexto (o compilador não pode mantê-los em
registradores no código traduzido, porque toda escrita na LS passa pelo mesmo ctx). O núcleo faz o laço inteiro,
devolve os registradores escritos ao contexto e devolve 1: o gancho (edge_nativo.py, tipo 'laco_local') salta então
para o rótulo <rótulo>_fim, logo depois do desvio do laço. GOWA_LACO_LOCAL=0 desliga todos (A/B).

uso: python -I laco_local.py <spu_recomp.c> <função> <rótulo> <saída .h> <nome da função C>"""
import re
import sys


def corpo_do_laco(s, fn, rot):
    i = s.index(f'static SPU_PN void {fn}(spu_context* ctx) {{')
    f = s[i:s.index('\n}\n', i)]
    j = f.index(f'{rot}:\n')
    corpo = []
    for l in f[j:].split('\n')[1:]:
        t = l.strip()
        if f'goto {rot};' in t:
            return corpo, t
        if not t or t.startswith('/*'):
            continue
        assert not re.fullmatch(r'loc_[0-9A-F]{8}:', t) and 'goto' not in t, 'o laço não é um bloco só: ' + t
        corpo.append(t)
    raise ValueError('desvio do laço não achado')


def main():
    src, fn, rot, saida, nome = sys.argv[1:6]
    corpo, desvio = corpo_do_laco(open(src, encoding='latin-1').read(), fn, rot)
    m = re.fullmatch(r'if \((ctx->gpr\[\d+\]\._u(?:32|16)\[0\] (?:!=|==) 0)\) goto ' + rot + ';', desvio)
    assert m, 'desvio do laço inesperado: ' + desvio
    cond = m.group(1)
    lidos, escritos = [], []
    for t in corpo + [cond]:
        mm = re.match(r'ctx->gpr\[(\d+)\] = (.*);$', t)
        rhs = mm.group(2) if mm else t
        for r in map(int, re.findall(r'ctx->gpr\[(\d+)\]', rhs)):
            if r not in escritos and r not in lidos:
                lidos.append(r)
        if mm and int(mm.group(1)) not in escritos:
            escritos.append(int(mm.group(1)))
        assert mm or t.startswith('spu_ls_write128(') or t is cond or t == '/* branch hint (ignored) */;', \
            'linha não reconhecida: ' + t
        # o ctx só pode ir para a LS (um helper que lesse ctx->gpr por dentro veria os registradores velhos)
        sem_gpr = re.sub(r'ctx->gpr\[\d+\]', 'G', t)
        assert all(sem_gpr[:k.start()].endswith(('spu_ls_read128(', 'spu_ls_write128(', 'spu_ls_const128('))
                   for k in re.finditer(r'\bctx\b', sem_gpr)), 'ctx fora da LS: ' + t
    todos = sorted(set(lidos) | set(escritos))
    loc = lambda t: re.sub(r'ctx->gpr\[(\d+)\]', r'g\1', t)
    guarda = 'GOWA_' + re.sub(r'\W', '_', nome).upper() + '_H'
    out = [f'/* GOWA: {fn} {rot} com registradores locais (gerado por scripts_edge/laco_local.py; não editar).',
           ' * Mesmas operações e ordem do código traduzido: idêntico bit a bit. Faz o laço inteiro e devolve 1 (o',
           f' * gancho salta para {rot}_fim, depois do desvio); 0 = nada feito (desligado). */',
           f'#ifndef {guarda}', f'#define {guarda}',
           '#ifndef GOWA_LACO_LOCAL_ON_DEF', '#define GOWA_LACO_LOCAL_ON_DEF',
           '#include <stdlib.h>',
           'static int gowa_laco_local_on(void)   /* GOWA_LACO_LOCAL=0 desliga todos (A/B) */', '{',
           '    static int on = -1;',
           '    if (on < 0) { const char* e = getenv("GOWA_LACO_LOCAL"); on = !(e && *e == \'0\'); }',
           '    return on;', '}', '#endif',
           f'static int {nome}(spu_context* ctx)', '{',
           '    if (!gowa_laco_local_on()) return 0;',
           '    u128 ' + ', '.join(f'g{r} = ctx->gpr[{r}]' for r in todos) + ';',
           '    do {']
    out += ['        ' + loc(t) for t in corpo if not t.startswith('/*')]
    out += [f'    }} while ({loc(cond)});']
    out += [f'    ctx->gpr[{r}] = g{r};' for r in escritos]
    out += ['    return 1;', '}', '#endif', '']
    open(saida, 'w', encoding='utf-8', newline='\n').write('\n'.join(out))
    print(f'{saida}: {len(corpo)} linhas, {len(lidos)} lidos antes de escritos, {len(escritos)} escritos')


if __name__ == '__main__':
    main()
