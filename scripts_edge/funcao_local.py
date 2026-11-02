"""E4 (01/11): transcreve as funções FOLHA PURAS do job traduzido (sem chamadas, sem canais da SPU, sem desvio
indireto, sem nativo próprio) com os registradores em variáveis locais em vez de ctx->gpr: as mesmas operações
(spu_*), na mesma ordem e com os mesmos desvios, então o resultado é idêntico bit a bit. Cada uma vira
fl_<endereço>(ctx), que faz a função inteira, devolve os registradores escritos ao contexto em cada saída e
devolve 1. GOWA_FUNCAO_LOCAL=1 liga (padrão desligado até o A/B).

uso: python -I funcao_local.py <spu_recomp.c> <saída .h> <prefixo das funções>  -> imprime os endereços gerados"""
import re
import sys

MARCA = '/* GOWA_EDGE_NATIVO */'
PROIBIDO = ('spu_drain_call', 'spu_rdch', 'spu_wrch', 'spu_indirect_branch', 'HALT', 'int_enable', 'SPU_TAILX',
            'spu_link', 'VK ', 'GOWA_EDGE_NATIVO', 'ctx->pc', 'ctx->status', 'spu_halt', 'RELOAD', 'spu_stop')


def funcoes(s, prefixo):
    for m in re.finditer(r'^static SPU_PN void (' + prefixo + r'_spu_func_([0-9A-F]{8})__b)\(spu_context\* ctx\) \{\n', s, re.M):
        i = m.end()
        yield m.group(1), m.group(2), s[i:s.index('\n}\n', i)]


def converte(nome, corpo):
    """devolve as linhas C da função local, ou None se não for folha pura"""
    # os próprios ganchos desta passada (regeração depois de aplicar) não contam
    corpo = chr(10).join(l for l in corpo.split(chr(10)) if not (MARCA in l and ('fl_' in l or 'nat_dir_' in l)))
    if any(p in corpo for p in PROIBIDO):
        return None
    linhas = [l.strip() for l in corpo.split('\n')]
    lidos, escritos, saida = [], [], []
    for t in linhas:
        if not t or t.startswith('/*'):
            continue
        sem_gpr = re.sub(r'ctx->gpr\[\d+\]', 'G', t)
        # o ctx só pode ir para a LS (um helper que lesse ctx->gpr por dentro veria os registradores velhos)
        for k in re.finditer(r'\bctx\b', sem_gpr):
            if not sem_gpr[:k.start()].endswith(('spu_ls_read128(', 'spu_ls_write128(', 'spu_ls_const128(',
                                                 'spu_ls_read128_kcx(', 'SPU_RET(')):
                return None
        if t == 'SPU_RET(ctx);':
            saida.append(('ret', None))
            continue
        if re.fullmatch(r'loc_[0-9A-F]{8}:', t):
            saida.append(('rot', t))
            continue
        m = re.fullmatch(r'goto (loc_[0-9A-F]{8});', t)
        if m:
            saida.append(('st', t))
            continue
        m = re.fullmatch(r'if \((.*)\) goto (loc_[0-9A-F]{8});', t)
        if m:
            for r in map(int, re.findall(r'ctx->gpr\[(\d+)\]', m.group(1))):
                if r not in lidos: lidos.append(r)
            saida.append(('st', t))
            continue
        mm = re.fullmatch(r'ctx->gpr\[(\d+)\] = (.*);', t)
        if mm:
            for r in map(int, re.findall(r'ctx->gpr\[(\d+)\]', mm.group(2))):
                if r not in lidos: lidos.append(r)
            if int(mm.group(1)) not in escritos: escritos.append(int(mm.group(1)))
            saida.append(('st', t))
            continue
        if t.startswith('spu_ls_write128('):
            for r in map(int, re.findall(r'ctx->gpr\[(\d+)\]', t)):
                if r not in lidos: lidos.append(r)
            saida.append(('st', t))
            continue
        return None   # linha não reconhecida
    if not saida or saida[-1][0] not in ('ret', 'st') or (saida[-1][0] == 'st' and not saida[-1][1].startswith('goto ')):
        return None   # a função tem de terminar em retorno ou desvio
    todos = sorted(set(lidos) | set(escritos))
    loc = lambda t: re.sub(r'ctx->gpr\[(\d+)\]', r'g\1', t).replace('loc_', 'fl_')
    out = [f'static int {nome}(spu_context* ctx)', '{', '    if (!gowa_funcao_local_on()) return 0;']
    if todos:
        out.append('    u128 ' + ', '.join(f'g{r} = ctx->gpr[{r}]' for r in todos) + ';')
    volta = ' '.join(f'ctx->gpr[{r}] = g{r};' for r in sorted(escritos)) + ' return 1;'
    for tipo, t in saida:
        if tipo == 'ret':
            out.append('    { ' + volta + ' }')
        elif tipo == 'rot':
            out.append(loc(t) + ' ;')
        else:
            out.append('    ' + loc(t))
    out.append('}')
    return out


def main():
    src, saida, prefixo = sys.argv[1:4]
    s = open(src, encoding='latin-1').read()
    out = ['/* GOWA (E4): funções folha puras do job com registradores locais (gerado por scripts_edge/funcao_local.py;',
           ' * não editar). Mesmas operações, ordem e desvios do código traduzido: idêntico bit a bit. */',
           '#ifndef GOWA_FL_' + prefixo.upper() + '_H', '#define GOWA_FL_' + prefixo.upper() + '_H', '#include <stdlib.h>',
           '/* GOWA_FUNCAO_LOCAL=1 liga (padrão desligado até o A/B no AVX2) */',
           'static int gowa_funcao_local_on(void)', '{', '    static int on = -1;',
           '    if (on < 0) { const char* e = getenv("GOWA_FUNCAO_LOCAL"); on = e && *e == \'1\'; }', '    return on;', '}']
    feitos = []
    for fn, end, corpo in funcoes(s, prefixo):
        c = converte(f'fl_{end}', corpo)
        if c:
            out += c + ['']
            feitos.append((fn, end))
    out += ['#endif', '']
    open(saida, 'w', encoding='utf-8', newline='\n').write('\n'.join(out))
    for fn, end in feitos:
        print(fn, end)


if __name__ == '__main__':
    main()
