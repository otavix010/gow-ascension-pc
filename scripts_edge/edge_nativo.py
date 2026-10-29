"""Liga as partes nativas do EDGE (port/edge/*.h) no código SPU traduzido (port/spu/...).

O código traduzido é gerado a cada geração, então os ganchos são aplicados depois dela: na entrada
de cada função substituída, `if (<nativo>(ctx)) { SPU_RET(ctx); }`. O nativo devolve 0 quando o
caso não é coberto, e a função traduzida segue como antes.

uso: python -I edge_nativo.py aplicar|tirar [raiz do port]"""
import os
import sys

GANCHOS = [
    # (pasta do código traduzido, função, cabeçalho nativo, função nativa)
    ('gowapl_0074C580', 'gowapl_0074C580_spu_func_00030000', 'pl_0074C580.h', 'pl_0074C580_nativo'),
    # a ponte dos plugins relocáveis (spu_interp.c) acha o 0x30020 pelo conteúdo e entra direto nele
    ('gowapl_0074C580', 'gowapl_0074C580_spu_func_00030020', 'pl_0074C580.h', 'pl_0074C580_nativo'),
]
MARCA = '/* GOWA_EDGE_NATIVO */'
INC = '#include "../../runtime/spu/spu_helpers.h"\n'


def arquivo(raiz, pasta):
    return os.path.join(raiz, 'port', 'spu', pasta, 'spu_recomp.c')


def main():
    modo = sys.argv[1]
    raiz = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(__file__), '..')
    if modo == 'tirar':
        for pasta in sorted({g[0] for g in GANCHOS}):
            p = arquivo(raiz, pasta)
            s = open(p, encoding='latin-1').read()
            n = s.count(MARCA)
            s = '\n'.join(l for l in s.split('\n') if MARCA not in l)
            open(p, 'w', encoding='latin-1', newline='').write(s)
            print(f'{pasta}: {n} linhas tiradas')
        return
    for pasta, fn, cab, nat in GANCHOS:
        p = arquivo(raiz, pasta)
        s = open(p, encoding='latin-1').read()
        linha_inc = f'#include "../../../port/edge/{cab}" {MARCA}\n'
        if linha_inc not in s:
            assert s.count(INC) == 1, (p, 'include dos helpers')
            s = s.replace(INC, INC + linha_inc)
        cabecalho = f'void {fn}(spu_context* ctx) {{\n'
        i = s.index(cabecalho)
        j = s.index('RELOAD;\n', i) + len('RELOAD;\n')
        gancho = f'        if ({nat}(ctx)) {{ SPU_RET(ctx); }} {MARCA}\n'
        if s[j:j + len(gancho)] == gancho:
            print(f'{fn}: já aplicado')
        else:
            s = s[:j] + gancho + s[j:]
            print(f'{fn}: aplicado')
        open(p, 'w', encoding='latin-1', newline='').write(s)


main()
