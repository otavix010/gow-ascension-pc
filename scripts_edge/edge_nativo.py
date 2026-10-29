"""Liga as partes nativas do EDGE (port/edge/*.h) no código SPU traduzido (port/spu/...).

O código traduzido é gerado a cada geração, então os ganchos são aplicados depois dela. Dois tipos:
- 'entrada': na entrada da função, `if (<nativo>(ctx)) { SPU_RET(ctx); }`;
- 'antes': logo antes de um rótulo (roda uma vez, antes do laço), `if (<nativo>(ctx)) goto <destino>;`.
O nativo devolve 0 quando o caso não é coberto, e o código traduzido segue como antes.

uso: python -I edge_nativo.py aplicar|tirar [raiz do port]"""
import os
import sys

GANCHOS = [
    # plugin 0x74C580 (empacotamento dos vértices); a ponte dos plugins relocáveis (spu_interp.c) acha o
    # 0x30020 pelo conteúdo e entra direto nele, então as duas entradas
    dict(tipo='entrada', pasta='gowapl_0074C580', fn='gowapl_0074C580_spu_func_00030000', cab='pl_0074C580.h', nat='pl_0074C580_nativo'),
    dict(tipo='entrada', pasta='gowapl_0074C580', fn='gowapl_0074C580_spu_func_00030020', cab='pl_0074C580.h', nat='pl_0074C580_nativo'),
    # plugin 0x752710 (descompressão de posição e vetores): a partir da entrada do laço, depois da preparação
    dict(tipo='entrada', pasta='gowapl_00752710', fn='gowapl_00752710_spu_func_000302E8', cab='pl_00752710.h', nat='pl_00752710_nativo'),
    # D640 do job 0x72DF80: o laço de descarte de triângulos
    dict(tipo='antes', pasta='gowajm_0072DF80', fn='gowajm_0072DF80_spu_func_0000D640__b', cab='fn_0000D640.h',
         nat='fn_0000D640_laco_nativo', rotulo='loc_0000D92C', destino='loc_0000DB70'),
]
MARCA = '/* GOWA_EDGE_NATIVO */'
INC = '#include "../../runtime/spu/spu_helpers.h"\n'


def arquivo(raiz, pasta):
    return os.path.join(raiz, 'port', 'spu', pasta, 'spu_recomp.c')


def corpo_da_funcao(s, fn):
    for cab in (f'static SPU_PN void {fn}(spu_context* ctx) {{\n', f'void {fn}(spu_context* ctx) {{\n'):
        if cab in s:
            i = s.index(cab) + len(cab)
            return i, s.index('\n}\n', i)
    raise ValueError('função não achada: ' + fn)


def main():
    modo = sys.argv[1]
    raiz = sys.argv[2] if len(sys.argv) > 2 else os.path.join(os.path.dirname(__file__), '..')
    if modo == 'tirar':
        for pasta in sorted({g['pasta'] for g in GANCHOS}):
            p = arquivo(raiz, pasta)
            s = open(p, encoding='latin-1').read()
            n = s.count(MARCA)
            s = '\n'.join(l for l in s.split('\n') if MARCA not in l)
            open(p, 'w', encoding='latin-1', newline='').write(s)
            print(f'{pasta}: {n} linhas tiradas')
        return
    for g in GANCHOS:
        p = arquivo(raiz, g['pasta'])
        s = open(p, encoding='latin-1').read()
        linha_inc = f'#include "../../../port/edge/{g["cab"]}" {MARCA}\n'
        if linha_inc not in s:
            if INC in s:
                s = s.replace(INC, INC + linha_inc, 1)
            else:   # o módulo inclui os helpers pelo spu_recomp.h
                inc2 = '#include "spu_recomp.h"\n'
                assert s.count(inc2) == 1, (p, 'include')
                s = s.replace(inc2, inc2 + linha_inc, 1)
        i, fim = corpo_da_funcao(s, g['fn'])
        corpo = s[i:fim]
        if g['tipo'] == 'entrada':
            j = corpo.index('RELOAD;\n') + len('RELOAD;\n') if 'RELOAD;\n' in corpo else 0
            gancho = f'        if ({g["nat"]}(ctx)) {{ SPU_RET(ctx); }} {MARCA}\n'
            if corpo[j:j + len(gancho)] == gancho:
                print(f'{g["fn"]}: já aplicado'); continue
            corpo = corpo[:j] + gancho + corpo[j:]
        else:
            rot = f'{g["rotulo"]}:\n'
            assert corpo.count(rot) == 1, (g['fn'], g['rotulo'])
            gancho = f'        if ({g["nat"]}(ctx)) goto {g["destino"]}; {MARCA}\n'
            if gancho in corpo:
                print(f'{g["fn"]} antes de {g["rotulo"]}: já aplicado'); continue
            corpo = corpo.replace(rot, gancho + rot)
        s = s[:i] + corpo + s[fim:]
        open(p, 'w', encoding='latin-1', newline='').write(s)
        print(f'{g["fn"]}: aplicado ({g["tipo"]})')


main()
