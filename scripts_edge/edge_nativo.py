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
    # E550 (skinning): o laço EC08 menos a última volta; entra antes do núcleo vk dele e salta para o laço
    dict(tipo='antes_vk', pasta='gowajm_0072DF80', fn='gowajm_0072DF80_spu_func_0000E550__b', cab='fn_0000E550_ec08.h',
         nat='fn_0000E550_ec08_nativo', vk='/*VK 0000E550/0000EC08*/', destino='loc_0000EC08'),
    # DCC0/E3C4: fn_0000DCC0_e3c4.h é exato (300/300, ordem de leitura do original com a saída no lugar do fluxo),
    # mas escalar: um desvio imprevisível por triângulo o deixa mais lento (4,2%) que o laço local (~2-3,5%); fora
    # dict(tipo='antes', pasta='gowajm_0072DF80', fn='gowajm_0072DF80_spu_func_0000DCC0__b', cab='fn_0000DCC0_e3c4.h',
    #      nat='fn_0000DCC0_e3c4_nativo', rotulo='loc_0000E3C4', destino='loc_0000E3C4_fim', fim='loc_0000E3C4'),
    # F538 (transformação dos vértices pela matriz): o laço F620 inteiro; antes do vk dele, saltando para depois do laço
    dict(tipo='antes_vk', pasta='gowajm_0072DF80', fn='gowajm_0072DF80_spu_func_0000F538__b', cab='fn_0000F538.h',
         nat='fn_0000F538_nativo', vk='/*VK 0000F538/0000F620*/', destino='loc_0000F620_fim', fim='loc_0000F620'),
    # A880 (normais: produto vetorial e normalização): port/edge/fn_0000A880.h é exato (300/300), mas empata com o vk
    # na reprodução (30/10: 2,3–3,0% x 2,3% no cap. 16, no ruído); fica fora até medir ganho
    # dict(tipo='antes_vk', pasta='gowajm_0072DF80', fn='gowajm_0072DF80_spu_func_0000A880__b', cab='fn_0000A880.h',
    #      nat='fn_0000A880_nativo', vk='/*VK 0000A880/0000A950*/', destino='loc_0000A950_fim', fim='loc_0000A950'),
]
# laços de um bloco só com os registradores em variáveis locais (laco_local.py: o cabeçalho port/edge/ll_*.h é gerado
# aqui, do próprio módulo): o núcleo faz o laço inteiro e salta para <rótulo>_fim, depois do desvio; vem antes do vk
LACOS_LOCAIS = [
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_0000DCC0__b', 'loc_0000E3C4'),   # DCC0: reconstrução dos triângulos
    # 30/10: os outros laços de um bloco (8 linhas ou mais) sem vk na função; as funções que começam no próprio rótulo
    # (entrada direta) também
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_0000A950__b', 'loc_0000A950'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_00007040__b', 'loc_000071C8'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_000071C8__b', 'loc_000071C8'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_00007BB4__b', 'loc_00007BB4'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_0000EC08__b', 'loc_0000EC08'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_0000F24C__b', 'loc_0000F24C'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_0000EE28__b', 'loc_0000EE28'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_00007ED8__b', 'loc_00007ED8'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_00006AC8__b', 'loc_00006DE0'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_0000F620__b', 'loc_0000F620'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_0000BBC8__b', 'loc_0000BD54'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_0000BD54__b', 'loc_0000BD54'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_0000D4DC__b', 'loc_0000D4DC'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_0000E3C4__b', 'loc_0000E3C4'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_0000E210__b', 'loc_0000E210'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_000073E0__b', 'loc_000083E8'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_000083E8__b', 'loc_000083E8'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_0000CC98__b', 'loc_0000CE60'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_0000CE60__b', 'loc_0000CE60'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_000053B0__b', 'loc_00005720'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_00005720__b', 'loc_00005720'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_000073E0__b', 'loc_00007670'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_00007670__b', 'loc_00007670'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_000073E0__b', 'loc_00007828'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_00007828__b', 'loc_00007828'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_00004A54__b', 'loc_00004CA0'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_00004CA0__b', 'loc_00004CA0'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_0000DCC0__b', 'loc_0000E134'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_0000E134__b', 'loc_0000E134'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_000073E0__b', 'loc_000075D8'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_000075D8__b', 'loc_000075D8'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_0000E804__b', 'loc_0000E804'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_00009990__b', 'loc_00009A00'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_00009A00__b', 'loc_00009A00'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_00009AB8__b', 'loc_00009B90'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_00009B90__b', 'loc_00009B90'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_0000DCC0__b', 'loc_0000E30C'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_0000E30C__b', 'loc_0000E30C'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_0000AFF0__b', 'loc_0000B100'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_0000B100__b', 'loc_0000B100'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_00005D90__b', 'loc_00005DA8'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_00005DA8__b', 'loc_00005DA8'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_00009090__b', 'loc_000096E0'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_000096E0__b', 'loc_000096E0'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_00004A54__b', 'loc_00004EB8'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_00004EB8__b', 'loc_00004EB8'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_00004F88__b', 'loc_000050B0'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_000050B0__b', 'loc_000050B0'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_00009090__b', 'loc_00009620'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_00009AB8__b', 'loc_0000A490'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_00009AB8__b', 'loc_0000A5A0'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_00009AB8__b', 'loc_0000A6C0'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_00009AB8__b', 'loc_0000A7C8'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_0000A490__b', 'loc_0000A490'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_0000A5A0__b', 'loc_0000A5A0'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_0000A6C0__b', 'loc_0000A6C0'),
    ('gowajm_0072DF80', 'gowajm_0072DF80_spu_func_0000A7C8__b', 'loc_0000A7C8'),
    # 30/10: nos laços que já têm vk, o vk ganha (A880/A950, D250/D4DC, F538/F620 pioram com o local; E210 e os do
    # E550 empatam): ficam sem o local
]
for _pasta, _fn, _rot in LACOS_LOCAIS:
    _id = _fn.split('_spu_func_')[1].replace('__b', '') + '_' + _rot[4:]
    GANCHOS.append(dict(tipo='laco_local', pasta=_pasta, fn=_fn, rotulo=_rot, cab=f'll_{_id}.h', nat=f'll_{_id}'))
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
        if g['tipo'] == 'laco_local':
            sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
            import laco_local
            sys.argv = ['laco_local', p, g['fn'], g['rotulo'], os.path.join(raiz, 'port', 'edge', g['cab']), g['nat']]
            laco_local.main()
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
        elif g['tipo'] == 'antes_vk':
            k = corpo.index(g['vk'])
            k = corpo.rindex('\n', 0, k) + 1
            gancho = f'        if ({g["nat"]}(ctx)) goto {g["destino"]}; {MARCA}\n'
            if gancho in corpo:
                print(f'{g["fn"]} antes do vk: já aplicado'); continue
            corpo = corpo[:k] + gancho + corpo[k:]
            if g.get('fim'):   # rótulo depois do desvio do laço, para o nativo que faz o laço inteiro
                d = corpo.index(f'goto {g["fim"]};', k + len(gancho))
                d = corpo.index('\n', d) + 1
                corpo = corpo[:d] + f'{g["fim"]}_fim: {MARCA}\n' + corpo[d:]
        elif g['tipo'] == 'laco_local':
            rot = f'{g["rotulo"]}:\n'
            assert corpo.count(rot) == 1, (g['fn'], g['rotulo'])
            gancho = f'        if ({g["nat"]}(ctx)) goto {g["rotulo"]}_fim; {MARCA}\n'
            if gancho in corpo:
                print(f'{g["fn"]} {g["rotulo"]} local: já aplicado'); continue
            k = corpo.index(rot)
            ant = corpo.rindex('\n', 0, k - 1) + 1   # a linha do vk, se houver, fica depois do gancho
            if '/*VK ' in corpo[ant:k]:
                k = ant
            corpo = corpo[:k] + gancho + corpo[k:]
            d = corpo.index(f'goto {g["rotulo"]};', k + len(gancho) + len(rot))
            d = corpo.index('\n', d) + 1
            corpo = corpo[:d] + f'{g["rotulo"]}_fim: {MARCA}\n' + corpo[d:]
        else:
            rot = f'{g["rotulo"]}:\n'
            assert corpo.count(rot) == 1, (g['fn'], g['rotulo'])
            gancho = f'        if ({g["nat"]}(ctx)) goto {g["destino"]}; {MARCA}\n'
            if gancho in corpo:
                print(f'{g["fn"]} antes de {g["rotulo"]}: já aplicado'); continue
            k = corpo.index(rot)
            if g.get('fim'):   # o nativo faz o laço inteiro: entra antes do vk (se houver) e sai depois do desvio
                ant = corpo.rindex('\n', 0, k - 1) + 1
                if '/*VK ' in corpo[ant:k]:
                    k = ant
            corpo = corpo[:k] + gancho + corpo[k:]
            if g.get('fim'):
                d = corpo.index(f'goto {g["fim"]};', k + len(gancho) + len(rot))
                d = corpo.index('\n', d) + 1
                corpo = corpo[:d] + f'{g["fim"]}_fim: {MARCA}\n' + corpo[d:]
        s = s[:i] + corpo + s[fim:]
        open(p, 'w', encoding='latin-1', newline='').write(s)
        print(f'{g["fn"]}: aplicado ({g["tipo"]})')


main()
