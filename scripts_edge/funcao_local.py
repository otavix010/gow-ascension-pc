"""E4 (01/11): transcreve as funções FOLHA PURAS do job traduzido (sem chamadas, sem canais da SPU, sem desvio
indireto, sem nativo próprio) com os registradores em variáveis locais em vez de ctx->gpr: as mesmas operações
(spu_*), na mesma ordem e com os mesmos desvios, então o resultado é idêntico bit a bit. Cada uma vira
fl_<endereço>(ctx), que faz a função inteira, devolve os registradores escritos ao contexto em cada saída e
devolve 1. GOWA_FUNCAO_LOCAL=1 liga (padrão desligado até o A/B).

uso: python -I funcao_local.py <spu_recomp.c> <saída .h> <prefixo das funções>  -> imprime os endereços gerados"""
import re
import sys

MARCA = '/* GOWA_EDGE_NATIVO */'
# fluxo que não volta pelo SPU_RET (salto de cauda, parada, retorno cru): a função fica de fora
PROIBIDO = ('SPU_TAILX', 'return;', 'spu_halt', 'RELOAD', 'spu_stop', 'VK ', 'ctx->status')
LS_OK = ('spu_ls_read128(', 'spu_ls_write128(', 'spu_ls_const128(', 'spu_ls_read128_kcx(', 'SPU_RET(')
ROT = r'loc_[0-9A-F]{8}(?:_fim)?'


def funcoes(s, prefixo):
    for m in re.finditer(r'^static SPU_PN void (' + prefixo + r'_spu_func_([0-9A-F]{8})__b)\(spu_context\* ctx\) \{\n', s, re.M):
        i = m.end()
        yield m.group(1), m.group(2), s[i:s.index('\n}\n', i)]


def regs(t):
    return list(map(int, re.findall(r'ctx->gpr\[(\d+)\]', t)))


def puro(t):
    """a linha só usa o ctx para a LS (um helper que lesse ctx->gpr por dentro veria os registradores velhos)"""
    sem_gpr = re.sub(r'ctx->gpr\[\d+\]', 'G', t)
    return all(sem_gpr[:k.start()].endswith(LS_OK) for k in re.finditer(r'\bctx\b', sem_gpr))


def converte(nome, corpo):
    """devolve as linhas C da função com registradores locais, ou None. As linhas com efeito (chamadas, canais,
    desvio indireto, ganchos nativos) ficam como estão, cercadas de sincronização: os locais escritos voltam ao ctx
    antes e todos são relidos depois."""
    # os próprios ganchos desta passada (regeração depois de aplicar) não contam; a chamada direta também sai (a
    # chamada completa chama o nativo pelo gancho de entrada do chamado)
    corpo = chr(10).join(l for l in corpo.split(chr(10)) if not (MARCA in l and ('fl_' in l or 'nat_dir_' in l)))
    if any(p in corpo for p in PROIBIDO):
        return None
    lidos, escritos, saida, efeitos = [], [], [], 0
    def le(rs):
        for r in rs:
            if r not in lidos and r not in escritos: lidos.append(r)
    for l in corpo.split(chr(10)):
        t = l.replace(MARCA, '').strip()
        if not t or re.fullmatch(r'/\*.*?\*/;?', t):
            continue
        if 'SPU_RET' in t and t != 'SPU_RET(ctx);':
            return None   # gancho de entrada com retorno (a função tem nativo próprio)
        if t == 'SPU_RET(ctx);':
            saida.append(('ret', None)); continue
        if re.fullmatch(ROT + ':', t):
            saida.append(('rot', t)); continue
        if re.fullmatch('goto ' + ROT + ';', t):
            saida.append(('st', t)); continue
        m = re.fullmatch(r'if \((.*)\) goto (' + ROT + ');', t)
        if m and puro(m.group(1)):
            le(regs(m.group(1))); saida.append(('st', t)); continue
        if m:   # condição com efeito (gancho nativo): sincroniza antes e relê nos dois caminhos
            efeitos += 1; saida.append(('se_efeito', (m.group(1), m.group(2)))); continue
        mm = re.fullmatch(r'ctx->gpr\[(\d+)\] = (.*);', t)
        if mm and puro(mm.group(2)) and '{' not in t:
            le(regs(mm.group(2)))
            if int(mm.group(1)) not in escritos: escritos.append(int(mm.group(1)))
            saida.append(('st', t)); continue
        if t.startswith('spu_ls_write128(') and puro(t):
            le(regs(t)); saida.append(('st', t)); continue
        if 'goto' in t or 'return' in t:
            return None
        efeitos += 1; saida.append(('efeito', t))
    if not saida or saida[-1][0] == 'rot' or (saida[-1][0] == 'st' and not saida[-1][1].startswith('goto ')):
        return None   # a função tem de terminar em retorno ou desvio
    todos = sorted(set(lidos) | set(escritos))
    loc = lambda t: re.sub(r'ctx->gpr\[(\d+)\]', r'g\1', t).replace('loc_', 'fl_')
    vai = ' '.join(f'ctx->gpr[{r}] = g{r};' for r in sorted(escritos))
    vem = ' '.join(f'g{r} = ctx->gpr[{r}];' for r in todos)
    out = [f'static int {nome}(spu_context* ctx)', '{', '    if (!gowa_funcao_local_on()) return 0;']
    if todos:
        out.append('    u128 ' + ', '.join(f'g{r} = ctx->gpr[{r}]' for r in todos) + ';')
    for tipo, t in saida:
        if tipo == 'ret':
            out.append('    { ' + vai + ' return 1; }')
        elif tipo == 'rot':
            out.append(loc(t) + ' ;')
        elif tipo == 'efeito':
            out.append('    ' + vai); out.append('    ' + t); out.append('    ' + vem)
        elif tipo == 'se_efeito':
            out.append('    ' + vai)
            out.append(f'    if ({t[0]}) {{ {vem} goto {loc(t[1])}; }}')
            out.append('    ' + vem)
        else:
            out.append('    ' + loc(t))
    out.append('}')
    return out, efeitos


def main():
    src, saida, prefixo = sys.argv[1:4]
    modo_geral = len(sys.argv) > 4 and sys.argv[4] == 'geral'   # também as funções com chamadas e efeitos
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
        if c and (c[1] == 0 or modo_geral):
            out += c[0] + ['']
            feitos.append((fn, end))
    # os nativos chamados pelas linhas com efeito são definidos depois, noutros cabeçalhos: declarações antecipadas
    nomes = sorted(set(re.findall(r'((?:ll|fn|pl)_[0-9A-Za-z_]+)\(ctx\)', chr(10).join(out))))
    k = out.index('static int gowa_funcao_local_on(void)')
    out[k:k] = [f'static int {n}(spu_context* ctx);' for n in nomes]
    out += ['#endif', '']
    open(saida, 'w', encoding='utf-8', newline='\n').write('\n'.join(out))
    for fn, end in feitos:
        print(fn, end)


if __name__ == '__main__':
    main()
