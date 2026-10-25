"""Núcleo vetorial de um NINHO de laços do código SPU traduzido (21/10).

Diferente do vetorizar_laco.py (que junta 4 iterações de um laço interno), aqui cada
faixa de 128 bits do zmm faz UMA ITERAÇÃO DO LAÇO EXTERNO inteira, com os laços
internos dentro: as 4 faixas andam juntas nos laços internos porque a contagem deles
não depende da faixa (provado aqui pela análise de uniformidade: todo desvio do corpo
depende só de invariantes e contadores; o gerador recusa se não for assim).

Memória:
  * leitura com endereço uniforme (igual nas 4 faixas) fora do rascunho: broadcast;
  * leitura com endereço por faixa: 4 leituras de 16 bytes (vk_ld4);
  * RASCUNHO (bases dadas em --rascunho; ponteiros derivados por ai/cópia): privado por
    faixa -- um zmm por quadword (as 4 faixas no mesmo endereço = um acesso só);
    iniciado com o conteúdo da LS e, no fim, a faixa 3 (última iteração) volta à LS;
  * gravação por faixa: adiada para o fim do grupo e feita na ordem das iterações
    (faixa 0 inteira, depois 1, 2, 3) -- a mesma ordem do laço;
  * a independência entre leituras e gravações das iterações de um grupo é conferida
    em tempo de execução por regiões (perfil do ninho, ver PERFIS).
Float: as operações vk acumulam a máscara de alcance estendido; grupo marcado é
refeito pela transcrição literal, uma iteração por vez (nada foi gravado na LS: as
gravações por faixa ficam para depois da conferência e o rascunho é privado).

uso: python vetorizar_ninho.py <dir do módulo> <função> <início> <saída>
         [--fonte arq] [--gancho]
     início = rótulo do laço externo (alvo do desvio de volta), saída = rótulo depois
     dele. --gancho: grava o .inc, inclui no spu_recomp.c e põe a chamada antes do
     rótulo de início (o caminho de entrada; o desvio de volta não passa por ela).
Variáveis: GOWA_NINHO=0 desliga, GOWA_NINHO_CHECK=N confere 1 de cada N chamadas
contra a transcrição (LS inteira + 128 registradores), GOWA_NINHO_STATS=1."""
import re
import sys
from pathlib import Path

d = Path(sys.argv[1])
fn = sys.argv[2].upper().rjust(8, '0')
ini = sys.argv[3].upper().rjust(8, '0')
fim = sys.argv[4].upper().rjust(8, '0')
fonte = Path(sys.argv[sys.argv.index('--fonte') + 1]) if '--fonte' in sys.argv else d / 'spu_recomp.c.sem_flushpreg'
gancho = '--gancho' in sys.argv
CONSERTO = '--sem-conserto' not in sys.argv   # conta exata por palavra em vez de refazer o grupo
UNICO = '--unico' in sys.argv   # uma versão só: o conserto vetorial em linha no corpo A (sem B)
DBG = '--dbg' in sys.argv   # diagnóstico: mostra a primeira conta que acende o alcance estendido
mod = d.name

# ---------------------------------------------------------------- perfis
# O que o gerador não deduz sozinho: as bases do rascunho e a conferência de regiões
# (C), escrita para o ninho. O gerador confere que todo acesso do corpo cai numa das
# classes que a conferência cobre (BASES_*).
PERFIS = {
    ('000073E0', '000079B0'): dict(
        # rascunho: dois blocos de 96 bytes por luz (r105 e r106 + 96*k; r94/r95 cópias)
        rascunho={94, 95, 105, 106},
        # leituras por faixa: fluxo de registros (r103 + constante) e as coletas por
        # deslocamento (palavra 3 dos registros) nas posições (r77) e nos registros (r76)
        bases_ld_faixa={103, 77, 76},
        # leituras uniformes: os três vetores de luzes (r99/r97/r98 + 16*k)
        bases_ld_unif={99, 97, 98},
        # gravações por faixa: os três fluxos de saída + deslocamento
        bases_st_faixa={73, 74, 75},
        # registradores de deslocamento: escritos uma vez por iteração, rotqbyi 12 de
        # um registro lido de r103 + c
        desloc={79, 86, 87, 88, 89, 90, 91, 92},
        regioes=r'''
/* regiões do ninho 73E0/79B0 (n = r83 luzes, T iterações de 8 registros):
 *   leituras: fluxo [r103, r103 + 128T); posições r77 + desl; registros r76 + desl;
 *             luzes r99/r97/r98 + [0, 16(n+2));
 *   rascunho (privado): r106 e r105 + [0, 96(n+2));
 *   gravações por faixa: r73/r74/r75 + desl.
 * desl = palavra 3 dos 8T registros. Gravações não podem tocar nenhuma leitura nem o
 * rascunho; o rascunho não pode tocar nenhuma leitura (é privado durante o grupo). */
typedef struct { uint32_t lo, hi; } NOME_reg;
static int NOME_sobrepoe(NOME_reg a, NOME_reg b) { return a.lo < b.hi && b.lo < a.hi; }
static int NOME_regioes(const spu_context* ctx, uint32_t T, uint32_t n, uint32_t* cbase, NOME_reg* c1, NOME_reg* c2) {
    const uint8_t* ls = ctx->ls_store;
#define W0(r) (ctx->gpr[r]._u32[0])
    if (n > 46) return 2;
    if (W0(94) != W0(105) || W0(95) != W0(106)) return 2;
    const uint32_t s0 = W0(103);
    if ((s0 & 15u) || s0 >= SPU_LS_SIZE || s0 + 128u * T > SPU_LS_SIZE) return 3;
    uint32_t omin = 0xFFFFFFFFu, omax = 0;
    for (uint32_t i = 0; i < 8u * T; i++) {
        uint32_t w; memcpy(&w, ls + s0 + 16u * i + 12u, 4); w = SPU_BSWAP32(w);
        omin = w < omin ? w : omin; omax = w > omax ? w : omax;
    }
    if (omax >= SPU_LS_SIZE) return 4;
    static const int bd[5] = {77, 76, 73, 74, 75};
    NOME_reg r[5];
    for (int k = 0; k < 5; k++) {
        const uint32_t b = W0(bd[k]);
        if (b >= SPU_LS_SIZE || b + omax + 16u > SPU_LS_SIZE) return 5;
        r[k].lo = (b + omin) & ~15u; r[k].hi = ((b + omax) & ~15u) + 16u;
    }
    NOME_reg le[6];
    le[0] = r[0]; le[1] = r[1]; le[2].lo = s0; le[2].hi = s0 + 128u * T;
    static const int bl[3] = {99, 97, 98};
    for (int k = 0; k < 3; k++) {
        const uint32_t b = W0(bl[k]) & ~15u;
        if (b + 16u * (n + 2) > SPU_LS_SIZE) return 6;
        le[3 + k].lo = b; le[3 + k].hi = b + 16u * (n + 2);
    }
    c1->lo = W0(106) & ~15u; c1->hi = c1->lo + 96u * (n + 2);
    c2->lo = W0(105) & ~15u; c2->hi = c2->lo + 96u * (n + 2);
    if (c1->hi > SPU_LS_SIZE || c2->hi > SPU_LS_SIZE) return 6;
    for (int g = 2; g < 5; g++) {
        for (int k = 0; k < 6; k++) if (NOME_sobrepoe(r[g], le[k])) return 7;
        if (NOME_sobrepoe(r[g], *c1) || NOME_sobrepoe(r[g], *c2)) return 7;
    }
    for (int k = 0; k < 6; k++) if (NOME_sobrepoe(*c1, le[k]) || NOME_sobrepoe(*c2, le[k])) return 8;
    *cbase = c1->lo < c2->lo ? c1->lo : c2->lo;
    const uint32_t topo = c1->hi > c2->hi ? c1->hi : c2->hi;
    if (topo - *cbase > 1024u * 16u) return 9;
    return 0;
#undef W0
}
'''),
}
perfil = PERFIS.get((fn, ini))
if perfil is None:
    raise SystemExit(f'sem perfil para {fn}/{ini}')

# ---------------------------------------------------------------- leitura
src = fonte.read_text(encoding='latin-1')
m = re.search(r'^(?:static )?(?:SPU_PN )?void \w+_spu_func_' + fn + r'(?:__b)?\(spu_context\* ctx\) \{\r?\n', src, re.M)
corpo = src[m.end():]
a = corpo.find(f'loc_{ini}:')
b = corpo.find(f'loc_{fim}:', a)
assert a >= 0 and b > a, 'rótulos não achados'
cru = [t.strip() for t in corpo[a:b].splitlines()]

RR = r'ctx->gpr\[(\d+)\]'
S = []     # comandos
for t in cru:
    if not t or t.startswith('/*'):
        continue
    mm = re.fullmatch(r'loc_([0-9A-F]{8}):', t)
    if mm:
        S.append(dict(tipo='rot', rot=mm.group(1)))
        continue
    mm = re.fullmatch(r'if \(' + RR + r'\._u32\[0\] (==|!=) 0\) goto loc_([0-9A-F]{8});', t)
    if mm:
        S.append(dict(tipo='if', c=int(mm.group(1)), cmp=mm.group(2), alvo=mm.group(3), txt=t))
        continue
    mm = re.fullmatch(r'spu_ls_write128\(ctx, (.*), ' + RR + r'\);', t)
    if mm:
        S.append(dict(tipo='st', addr=mm.group(1), s=int(mm.group(2)), txt=t))
        continue
    mm = re.fullmatch(RR + r' = spu_(\w+)\((.*)\);', t)
    if mm:
        dd, op, ar = int(mm.group(1)), mm.group(2), mm.group(3)
        c = dict(tipo='op', d=dd, op=op, txt=t)
        if op == 'ls_read128':
            assert ar.startswith('ctx, ')
            c['addr'] = ar[5:]
            c['args'] = []
        else:
            c['args'] = [x.strip() for x in ar.split(',')]
        S.append(c)
        continue
    raise SystemExit('comando não reconhecido: ' + t)
N = len(S)
rotulo = {c['rot']: i for i, c in enumerate(S) if c['tipo'] == 'rot'}
assert S[0]['tipo'] == 'rot' and S[0]['rot'] == ini
assert S[-1]['tipo'] == 'if' and S[-1]['alvo'] == ini, 'o ninho tem de terminar no desvio de volta'
for i, c in enumerate(S[:-1]):
    if c['tipo'] == 'if' and c['alvo'] == ini:
        raise SystemExit('desvio de volta no meio do corpo')
    if c['tipo'] == 'if' and c['alvo'] not in rotulo:
        raise SystemExit('desvio para fora do ninho: ' + c['txt'])


def parse_end(e):
    """'ctx->gpr[A]._u32[0] + ctx->gpr[B]._u32[0]' | '... + 0x70' -> (A, B|None, const)"""
    mm = re.fullmatch(RR + r'\._u32\[0\] \+ ' + RR + r'\._u32\[0\]', e)
    if mm:
        return int(mm.group(1)), int(mm.group(2)), 0
    mm = re.fullmatch(RR + r'\._u32\[0\] \+ (-?0x[0-9A-Fa-f]+|-?\d+)', e)
    if mm:
        return int(mm.group(1)), None, int(mm.group(2), 0)
    raise SystemExit('endereço não reconhecido: ' + e)


for c in S:
    if 'addr' in c:
        c['end'] = parse_end(c['addr'])


def regs_lidos(c):
    out = []
    if c['tipo'] == 'op':
        for x in c['args']:
            mm = re.fullmatch(RR, x)
            if mm:
                out.append(int(mm.group(1)))
    if 'end' in c:
        out.append(c['end'][0])
        if c['end'][1] is not None:
            out.append(c['end'][1])
    if c['tipo'] == 'st':
        out.append(c['s'])
    if c['tipo'] == 'if':
        out.append(c['c'])
    return out


escritos = sorted({c['d'] for c in S if c['tipo'] == 'op'})
usados = sorted({r for c in S for r in regs_lidos(c)} | set(escritos))
invariantes = sorted(set(usados) - set(escritos))


def succ(i):
    c = S[i]
    if c['tipo'] == 'if':
        if c['alvo'] == ini:
            return []                       # fim da iteração
        return [i + 1, rotulo[c['alvo']]]
    return [i + 1] if i + 1 < N else []


COPIA = {('ori', '0'), ('shlqbyi', '0'), ('rotqbyi', '0'), ('ai', '0')}


def eh_copia(c):
    a_ = c['args']
    return len(a_) == 2 and re.fullmatch(RR, a_[0]) and (c['op'], a_[1]) in COPIA


# ---------------------------------------------------------------- caminhos
# Desvios cujo registrador é invariante decidem igual em toda iteração da chamada:
# cada combinação é uma classe de caminho. Por classe: escritos "com certeza" desde o
# início da iteração (interseção nas junções, laços até o ponto fixo); um registrador
# lido sem estar escrito com certeza e que a classe escreve em algum lugar vem da
# iteração anterior = carregado. Só induções (x + passo) são aceitas.
fixos = sorted({(c['c'], c['cmp']) for c in S if c['tipo'] == 'if' and c['c'] in invariantes})
carregados = set()
for mascara in range(1 << len(fixos)):
    decide = {f: bool(mascara >> k & 1) for k, f in enumerate(fixos)}   # True = desvia

    def succ_c(i):
        c = S[i]
        if c['tipo'] == 'if' and (c['c'], c['cmp']) in decide:
            return [rotulo[c['alvo']]] if decide[(c['c'], c['cmp'])] else [i + 1]
        return succ(i)
    alc = set()
    pilha = [0]
    while pilha:
        i = pilha.pop()
        if i in alc or i >= N:
            continue
        alc.add(i)
        pilha.extend(succ_c(i))
    esc_classe = {S[i]['d'] for i in alc if S[i]['tipo'] == 'op'}
    TODOS = frozenset(range(128))
    dw = {i: TODOS for i in alc}
    dw[0] = frozenset()
    entrada = {0: frozenset()}
    mudou = True
    while mudou:
        mudou = False
        for i in sorted(alc):
            preds_in = entrada.get(i)
            if preds_in is None:
                continue
            c = S[i]
            sai = preds_in | ({c['d']} if c['tipo'] == 'op' else set())
            for j in succ_c(i):
                if j not in alc:
                    continue
                novo = sai if j not in entrada else (entrada[j] & sai)
                if entrada.get(j) != novo:
                    entrada[j] = frozenset(novo)
                    mudou = True
    for i in alc:
        for r in regs_lidos(S[i]):
            if r not in entrada[i] and r in esc_classe:
                carregados.add(r)

# induções: valor no fim da iteração = início + passo, por cópias e ai (todas as classes
# passam pelo mesmo código de atualização; conferido no caminho reto do último bloco)
passos = {}
for r in sorted(carregados):
    simb = {}

    def val(x):
        return simb.get(x, (x, 0))
    for c in S:
        if c['tipo'] != 'op':
            continue
        if eh_copia(c):
            simb[c['d']] = val(int(re.fullmatch(RR, c['args'][0]).group(1)))
        elif c['op'] == 'ai' and re.fullmatch(RR, c['args'][0]):
            b_, k_ = val(int(re.fullmatch(RR, c['args'][0]).group(1)))
            simb[c['d']] = (b_, k_ + int(c['args'][1], 0)) if b_ is not None else (None, 0)
        else:
            simb[c['d']] = (None, 0)
    fimv = val(r)
    if fimv[0] != r or fimv[1] == 0:
        raise SystemExit(f'r{r} é carregado entre iterações externas e não é indução ({fimv})')
    passos[r] = fimv[1]
# a simulação acima é textual: a indução tem de ser escrita uma vez só, fora dos laços internos
lacos0 = [(rotulo[c['alvo']], i) for i, c in enumerate(S) if c['tipo'] == 'if' and c['alvo'] != ini and rotulo[c['alvo']] <= i]
for r in passos:
    ws = [i for i, c in enumerate(S) if c['tipo'] == 'op' and c['d'] == r]
    if len(ws) != 1 or any(lo <= ws[0] <= hi for lo, hi in lacos0):
        raise SystemExit(f'indução r{r} escrita {len(ws)} vezes ou dentro de laço interno')

# saída do laço externo: rC = clgt(rI, rX) com rX indução e rI invariante (o padrão do 73E0)
ult = S[-1]
defc = [c for c in S if c['tipo'] == 'op' and c['d'] == ult['c']][-1:]
assert defc and defc[0]['op'] == 'clgt' and ult['cmp'] == '!=', 'saída do laço externo não reconhecida'
assert all(c['tipo'] != 'rot' for c in S[S.index(defc[0]):]), 'saída do laço externo não reconhecida'
lim_r = int(re.fullmatch(RR, defc[0]['args'][0]).group(1))
cnt_r = int(re.fullmatch(RR, defc[0]['args'][1]).group(1))
assert lim_r in invariantes and cnt_r in passos and passos[cnt_r] > 0, 'saída do laço externo não reconhecida'
pos_clgt = S.index(defc[0])
assert all(not (c['tipo'] == 'op' and c['d'] == cnt_r) for c in S[pos_clgt:]), 'contador muda depois do teste'
assert sum(1 for c in S if c['tipo'] == 'op' and c['d'] == cnt_r) == 1, 'contador escrito mais de uma vez'
assert [c for c in S if c['tipo'] == 'op' and c['d'] == cnt_r][0]['op'] == 'ai'
assert S.index([c for c in S if c['tipo'] == 'op' and c['d'] == cnt_r][0]) < pos_clgt

# ---------------------------------------------------------------- uniformidade
# NU = registradores que podem diferir entre faixas; SD = ponteiros do rascunho
# (base + constante); TX = derivados do rascunho por outras contas (não podem ser
# endereço). Ponto fixo sobre o grafo (o desvio de volta encerra a iteração).
NU0 = frozenset(carregados)
SD0 = frozenset(perfil['rascunho'])
estado = {0: (NU0, SD0, frozenset())}


def transf(c, st):
    nu, sd, tx = set(st[0]), set(st[1]), set(st[2])
    if c['tipo'] != 'op':
        return frozenset(nu), frozenset(sd), frozenset(tx)
    dd = c['d']
    lidos = regs_lidos(c)
    nu.discard(dd); sd.discard(dd); tx.discard(dd)
    if c['op'] == 'ls_read128':
        base, off, k = c['end']
        rasc = base in st[1]
        if rasc or any(x in st[0] for x in lidos):
            nu.add(dd)
    else:
        if any(x in st[0] for x in lidos):
            nu.add(dd)
        if (eh_copia(c) or c['op'] == 'ai') and lidos and lidos[0] in st[1]:
            sd.add(dd)
        elif any(x in st[1] or x in st[2] for x in lidos):
            tx.add(dd)
    return frozenset(nu), frozenset(sd), frozenset(tx)


mudou = True
while mudou:
    mudou = False
    for i in range(N):
        if i not in estado:
            continue
        sai = transf(S[i], estado[i])
        for j in succ(i):
            if j >= N:
                continue
            if j not in estado:
                novo = sai
            else:
                e = estado[j]
                novo = (e[0] | sai[0], e[1] & sai[1], e[2] | sai[2] | (e[1] ^ sai[1]))
            if estado.get(j) != novo:
                estado[j] = novo
                mudou = True

# classificação dos acessos e conferência das classes do perfil
for i, c in enumerate(S):
    nu, sd, tx = estado[i]
    if c['tipo'] == 'if' and c['alvo'] != ini and c['c'] in nu:
        raise SystemExit('desvio não uniforme: ' + c['txt'])
    if 'end' not in c:
        continue
    base, off, k = c['end']
    if base in tx or (off is not None and (off in tx or off in sd)):
        raise SystemExit('endereço derivado do rascunho por conta não suportada: ' + c['txt'])
    if base in sd:
        if off is not None:
            raise SystemExit('rascunho com deslocamento em registrador: ' + c['txt'])
        c['cls'] = 'rasc'
    elif base in nu or (off is not None and off in nu):
        c['cls'] = 'faixa'
    else:
        c['cls'] = 'unif'
    if c['tipo'] == 'st' and c['cls'] == 'unif':
        raise SystemExit('gravação uniforme fora do rascunho (não suportada): ' + c['txt'])
    if c['cls'] == 'faixa':
        bases = perfil['bases_ld_faixa'] if c['tipo'] == 'op' else perfil['bases_st_faixa']
        if base not in bases:
            raise SystemExit('acesso por faixa fora do perfil: ' + c['txt'])
        if off is not None and off not in perfil['desloc']:
            raise SystemExit('deslocamento fora do perfil: ' + c['txt'])
        if off is None and base not in passos:
            raise SystemExit('acesso por faixa com base não indutiva: ' + c['txt'])
    if c['cls'] == 'unif' and c['tipo'] == 'op' and base not in perfil['bases_ld_unif']:
        raise SystemExit('leitura uniforme fora do perfil: ' + c['txt'])
# deslocamentos: em cada acesso por faixa, a definição que chega (a última antes, no
# texto; o acesso fora de laço interno) é um rotqbyi 12 de um registro lido de
# (indução + constante) no mesmo bloco
lacos_t = [(rotulo[c['alvo']], i) for i, c in enumerate(S) if c['tipo'] == 'if' and c['alvo'] != ini and rotulo[c['alvo']] <= i]
for i, c in enumerate(S):
    if c.get('cls') != 'faixa' or c['end'][1] is None:
        continue
    r = c['end'][1]
    assert not any(lo <= i <= hi for lo, hi in lacos_t), c['txt']
    defs = [j for j in range(i) if S[j]['tipo'] == 'op' and S[j]['d'] == r]
    assert defs, c['txt']
    dj = S[defs[-1]]
    assert dj['op'] == 'rotqbyi' and dj['args'][1] == '12', c['txt']
    src_r = int(re.fullmatch(RR, dj['args'][0]).group(1))
    ant = [j for j in range(defs[-1]) if S[j]['tipo'] == 'op' and S[j]['d'] == src_r]
    assert ant and S[ant[-1]]['op'] == 'ls_read128' and S[ant[-1]]['end'][0] in passos and S[ant[-1]]['end'][1] is None, c['txt']
    assert all(S[j]['tipo'] != 'rot' for j in range(ant[-1], defs[-1])), c['txt']
    assert not any(lo <= defs[-1] <= hi for lo, hi in lacos_t), c['txt']
# gravações por faixa: fora dos laços internos (o buffer adiado tem tamanho fixo)
lacos = [(rotulo[c['alvo']], i) for i, c in enumerate(S) if c['tipo'] == 'if' and c['alvo'] != ini and rotulo[c['alvo']] <= i]
for i, c in enumerate(S):
    if c['tipo'] == 'st' and c['cls'] == 'faixa' and any(lo <= i <= hi for lo, hi in lacos):
        raise SystemExit('gravação por faixa dentro de laço interno: ' + c['txt'])

# ---------------------------------------------------------------- achatamento preguiçoso
# A regra do scripts/flush_preguicoso.py: um resultado de float que só vai para outras
# contas de float ou comparações de float (com DAZ, e as comparações vk tratam expoente 0
# como 0), também através de cópias, sai cru (sem o +0 do expoente 0) -- se não chegar ao
# fim da iteração (entregue ao código traduzido / à próxima). Definições que chegam por
# ponto fixo no grafo do corpo.
FLOAT = {'fa', 'fs', 'fm', 'fma', 'fms', 'fnms'}
SEG = FLOAT | {'fcgt', 'fceq', 'fcmgt', 'fcmeq'}
RD = {0: {}}
mudou = True
while mudou:
    mudou = False
    for i in range(N):
        if i not in RD:
            continue
        c = S[i]
        sai = RD[i]
        if c['tipo'] == 'op':
            sai = dict(sai)
            sai[c['d']] = frozenset([i])
        for j in succ(i):
            if j >= N:
                continue
            if j not in RD:
                novo = dict(sai)
            else:
                novo = dict(RD[j])
                for r, ks in sai.items():
                    novo[r] = novo.get(r, frozenset()) | ks
            if RD.get(j) != novo:
                RD[j] = novo
                mudou = True
fim_rd = dict(RD[N - 1])          # o último comando (desvio de volta) não escreve
fim_vivos = set().union(*fim_rd.values()) if fim_rd else set()
usos = {i: [] for i in range(N)}
for i in range(N):
    if i in RD:
        for r in regs_lidos(S[i]):
            for k in RD[i].get(r, ()):
                usos[k].append(i)


def seguro(k, vis):
    if k in fim_vivos:
        return False
    for i in usos[k]:
        c = S[i]
        if c['tipo'] == 'op' and c['op'] in SEG:
            continue
        if c['tipo'] == 'op' and eh_copia(c) and i not in vis and seguro(i, vis | {k}):
            continue
        return False
    return True


CRU = {k for k in range(N) if S[k]['tipo'] == 'op' and S[k]['op'] in FLOAT and seguro(k, frozenset())} if '--sem-cru' not in sys.argv else set()
nfl = sum(1 for c in S if c['tipo'] == 'op' and c['op'] in FLOAT)
print(f'float: {nfl} contas, {len(CRU)} cruas')

# ---------------------------------------------------------------- geração
nome = f'gowa_ninho_{mod}_{fn}_{ini}'
VEC2 = {'fcgt', 'fceq', 'fcmgt', 'fcmeq', 'and', 'or', 'xor', 'andc', 'nor', 'nand', 'orc',
        'a', 'sf', 'ceq', 'cgt', 'clgt', 'fi', 'clgth', 'ceqh', 'cgth', 'rotm', 'rotqby', 'shl',
        'rotqmbybi', 'rotqmbi', 'rotma', 'rothm', 'rot', 'sfh', 'ah'}
VEC3 = {'selb', 'shufb'}
VEC1 = {'frsqest', 'frest', 'cntb', 'gbh', 'gb'}
VECI = {'ai', 'sfi', 'andi', 'ori', 'xori', 'ceqi', 'cgti', 'clgti', 'andbi', 'shli', 'rotmi', 'roti',
        'rotqbii', 'shlqbii', 'rotqbyi', 'shlqbyi', 'rotqmbyi', 'cflts', 'cfltu', 'csflt', 'cuflt'}
CONST = {'il', 'ila', 'ilhu', 'fsmbi', 'ls_const128'}
out = []
o = out.append
nst = sum(1 for c in S if c['tipo'] == 'st' and c['cls'] == 'faixa')
cont = {}
for c in S:
    if 'cls' in c:
        k = ('ld_' if c['tipo'] == 'op' else 'st_') + c['cls']
        cont[k] = cont.get(k, 0) + 1
o(f'/* Gerado por scripts/vetorizar_ninho.py -- {mod} func_{fn} ninho loc_{ini}..loc_{fim}. Não editar. */')
o('#include "spu_vk.h"')
o('#include "spu_vk_exato.h"')
o('#include <stdio.h>')
o('#include <stdlib.h>')
o('void gowa_fndump(const void* gpr, const void* ls, unsigned fn, int fase);   /* gowa_offline.cpp */')
o(f'/* {N} comandos; induções {passos}; invariantes {len(invariantes)}; escritos {len(escritos)};')
o(f' * acessos: {cont}; saída: r{cnt_r} += {passos[cnt_r]} enquanto r{lim_r} > r{cnt_r} (sem sinal) */')
o('')
# referência: uma iteração externa literal
o(f'static __attribute__((noinline, cold)) void {nome}_ref1(spu_context* ctx) {{')
for c in S[:-1]:
    if c['tipo'] == 'rot':
        o(f'R_{c["rot"]}:;')
    elif c['tipo'] == 'if':
        o('        ' + c['txt'].replace('goto loc_', 'goto R_'))
    else:
        o('        ' + c['txt'])
o('}')
o('')
o(perfil['regioes'].replace('NOME', nome))
o(f'static inline vk {nome}_ld1(const uint8_t* ls, uint32_t a) {{')
o('    return vk__bswap32(_mm512_broadcast_i32x4(_mm_load_si128((const __m128i*)(ls + (a & (SPU_LS_MASK & ~0xFu))))));')
o('}')
o(f'static inline void {nome}_end4(vk v, uint32_t a[4]) {{')
o('    const __m128i w = _mm_and_si128(_mm512_castsi512_si128(_mm512_permutexvar_epi32(_mm512_setr_epi32(0, 4, 8, 12, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0), v)),')
o('                                    _mm_set1_epi32((int)(SPU_LS_MASK & ~0xFu)));')
o('    a[0] = (uint32_t)_mm_cvtsi128_si32(w); a[1] = (uint32_t)_mm_extract_epi32(w, 1);')
o('    a[2] = (uint32_t)_mm_extract_epi32(w, 2); a[3] = (uint32_t)_mm_extract_epi32(w, 3);')
o('}')
if DBG:
    o('static int ndbg;')
    o('static void nh_dbg(int i, const char* t, __mmask16 m, vk r, const vk* in, int nin) {')
    o('    uint32_t w[16]; _mm512_storeu_si512(w, r);')
    o('    fprintf(stderr, "[dbg] comando %d: %s  máscara %04X\\n", i, t, (unsigned)m);')
    o('    for (int k = 0; k < 16; k++) if (m >> k & 1) {')
    o('        fprintf(stderr, "   faixa %d palavra %d: saída %08X entradas", k / 4, k % 4, w[k]);')
    o('        for (int j = 0; j < nin; j++) { uint32_t x[16]; _mm512_storeu_si512(x, in[j]); fprintf(stderr, " %08X", x[k]); }')
    o('        fprintf(stderr, "\\n"); }')
    o('}')
o('static inline uint32_t vk_w0(vk v) { return (uint32_t)_mm_cvtsi128_si32(_mm512_castsi512_si128(v)); }')
o('static inline __m128i vk_bs128(__m128i x) { return _mm_shuffle_epi8(x, _mm_setr_epi8(3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12)); }')
XS = {'fa': ('x', 'spu__fa_any(A[k], B[k])'), 'fs': ('x', 'spu__fa_any(A[k], B[k] ^ 0x80000000u)'),
      'fm': ('x', 'spu__fm_any(A[k], B[k])'), 'fma': ('xx', 'spu__fma_any(A[k], B[k], C[k], 0u, 0u)'),
      'fms': ('xx', 'spu__fma_any(A[k], B[k], C[k], 0u, 0x80000000u)'), 'fnms': ('xx', 'spu__fma_any(A[k], B[k], C[k], 0x80000000u, 0u)')}
for _op, (_n, _ex) in XS.items():
    _tres = _n == 'xx'
    o(f'static __attribute__((noinline, cold)) vk {nome}_x{_op}(vk r, __mmask16 m, vk a, vk b' + (', vk c' if _tres else '') + ') {')
    o('    uint32_t R[16], A[16], B[16]' + (', C[16]' if _tres else '') + ';')
    o('    _mm512_storeu_si512(R, r); _mm512_storeu_si512(A, a); _mm512_storeu_si512(B, b);' + (' _mm512_storeu_si512(C, c);' if _tres else ''))
    o(f'    for (int k = 0; k < 16; k++) if (m >> k & 1) R[k] = {_ex};')
    o('    return _mm512_loadu_si512(R);')
    o('}')
o('')
o(f'/* rascunho privado: um zmm (4 faixas) por quadword; por thread */')
o(f'static __declspec(thread) uint8_t {nome}_privb[1024 * 64 + 64];')
o('')


def R_(x):
    return f'r{x}'


def arg(x):
    mm = re.fullmatch(RR, x)
    return R_(int(mm.group(1))) if mm else None


def end_vk(e):
    base, off, k = e
    if off is not None:
        return f'_mm512_add_epi32({R_(base)}, {R_(off)})'
    return f'vk_ai({R_(base)}, {k})'


def end_w0(e):
    base, off, k = e
    if off is not None:
        return f'(vk_w0({R_(base)}) + vk_w0({R_(off)}))'
    return f'(vk_w0({R_(base)}) + (uint32_t)({k}))'


def expr_op(c):
    op, a_ = c['op'], c['args']
    if op in CONST:
        return f'vk_bc(spu_{op}({", ".join(a_)}))'
    if op == 'iohl':
        return f'_mm512_or_si512({arg(a_[0])}, _mm512_set1_epi32((int)(({a_[1]}) & 0xFFFF)))'
    if eh_copia(c):
        return arg(a_[0])
    if op in FLOAT:
        return f'vk_{op}{"_r" if S.index(c) in CRU else ""}({", ".join(arg(x) for x in a_)}, &bad)'
    if op in VEC2 and len(a_) == 2 and all(arg(x) for x in a_):
        return f'vk_{op}({arg(a_[0])}, {arg(a_[1])})'
    if op in VEC3 and len(a_) == 3 and all(arg(x) for x in a_):
        return f'vk_{op}({arg(a_[0])}, {arg(a_[1])}, {arg(a_[2])})'
    if op in VEC1 and len(a_) == 1:
        return f'vk_{op}({arg(a_[0])})'
    if op in VECI and len(a_) == 2 and arg(a_[0]) and re.fullmatch(r'-?(0x[0-9A-Fa-f]+|\d+)', a_[1]):
        return f'vk_{op}({arg(a_[0])}, {a_[1]})'
    # sem forma vetorial: faixa a faixa com o auxiliar de 128 bits
    sys.stderr.write(f'aviso: faixa a faixa: {c["txt"]}\n')
    fx = []
    for L in range(4):
        fx.append(f'spu_{op}(' + ', '.join(f'vk_lane({arg(x)}, {L})' if arg(x) else x for x in a_) + ')')
    return 'vk_join(' + ', '.join(fx) + ')'


def emite_locais(g_expr):
    """locais vk = registradores do ctx (broadcast); induções do grupo g_expr"""
    o('    uint8_t* const ls = ctx->ls_store;')
    o(f'    vk* const priv = (vk*)(((uintptr_t){nome}_privb + 63) & ~(uintptr_t)63);')
    for r in usados:
        o(f'    vk {R_(r)} = vk_bc(ctx->gpr[{r}]);')
    for r, p in sorted(passos.items()):
        o(f'    vk g{r} = _mm512_add_epi32(vk_bc(ctx->gpr[{r}]), _mm512_add_epi32(_mm512_set1_epi32((int)(4u * {p}u * ({g_expr}))), _mm512_setr_epi32(0, 0, 0, 0, {p}, {p}, {p}, {p}, {2 * p}, {2 * p}, {2 * p}, {2 * p}, {3 * p}, {3 * p}, {3 * p}, {3 * p})));')
    o(f'    vk sv[{max(nst, 1)}]; uint32_t sa[{max(nst, 1)}][4];')


def emite_corpo(modo):
    """o corpo de uma iteração externa (as 4 faixas). modo 'A': as contas de float acumulam
    `bad` (nenhuma chamada); 'B': conserto por palavra logo depois de cada conta"""
    k_st = 0
    for r in sorted(passos):
        o(f'        {R_(r)} = g{r};')
    for c in S[:-1]:
        t = c['tipo']
        if t == 'rot':
            o(f'V_{c["rot"]}:;')
            continue
        if t == 'if':
            o(f'        if (vk_w0({R_(c["c"])}) {c["cmp"]} 0) goto V_{c["alvo"]};')
            continue
        if t == 'st':
            if c['cls'] == 'rasc':
                o(f'        priv[(({end_w0(c["end"])} - cbase) >> 4) & 1023] = {R_(c["s"])};')
            else:
                o(f'        sv[{k_st}] = {R_(c["s"])}; {nome}_end4({end_vk(c["end"])}, sa[{k_st}]);')
                k_st += 1
            continue
        if c['op'] == 'ls_read128':
            if c['cls'] == 'rasc':
                o(f'        {R_(c["d"])} = priv[(({end_w0(c["end"])} - cbase) >> 4) & 1023];')
            elif c['cls'] == 'unif':
                o(f'        {R_(c["d"])} = {nome}_ld1(ls, {end_w0(c["end"])});')
            else:
                o(f'        {{ uint32_t _a[4]; {nome}_end4({end_vk(c["end"])}, _a); {R_(c["d"])} = vk_ld4(ls, _a[0], _a[1], _a[2], _a[3]); }}')
            continue
        if modo == 'A' and DBG and c['op'] in FLOAT:
            ins = ', '.join(arg(x) for x in c['args'])
            o(f'        {{ const __mmask16 _b0 = bad; {R_(c["d"])} = {expr_op(c)}; if (bad != _b0 && ndbg < 12) {{ ndbg++; nh_dbg({S.index(c)}, "{c["txt"]}", bad & ~_b0, {R_(c["d"])}, (vk[]){{ {ins} }}, {len(c["args"])}); }} }}')
        elif modo == 'B' and c['op'] in FLOAT:
            # palavras de alcance estendido/saturado: a conta exata por palavra (a mesma dos
            # auxiliares de 128 bits) numa função fria -- o grupo segue vetorial
            ins = [arg(x) for x in c['args']]
            o(f'        {{ __mmask16 _m = 0; vk _t = vk_{c["op"]}{"_r" if S.index(c) in CRU else ""}({", ".join(ins)}, &_m);')
            o(f'          if (__builtin_expect(_m != 0, 0)) {{ _t = _mm512_mask_mov_epi32(_t, _m, vkx_{c["op"]}({", ".join(ins)})); (*consertos)++; }}')
            o(f'          {R_(c["d"])} = _t; }}')
        else:
            o(f'        {R_(c["d"])} = {expr_op(c)};')


def emite_grava(ind):
    o(f'{ind}/* gravações na ordem das iterações (só as faixas válidas) */')
    for L in range(4):
        o(f'{ind}{"" if L == 0 else f"if (nv > {L}) "}for (int k = 0; k < {nst}; k++) _mm_store_si128((__m128i*)(ls + sa[k][{L}]), vk_bs128(vk_x(sv[k], {L})));')


def emite_entrega(ind):
    o(f'{ind}/* estado no fim da última iteração (última faixa válida) e o rascunho dela na LS */')
    o(f'{ind}const int ul = (int)nv - 1;')
    for r in escritos:
        o(f'{ind}ctx->gpr[{r}] = vk_lane({R_(r)}, ul);')
    o(f'{ind}for (uint32_t a = c1.lo; a < c1.hi; a += 16) _mm_store_si128((__m128i*)(ls + a), vk_bs128(vk_x(priv[((a - cbase) >> 4) & 1023], ul)));')
    o(f'{ind}for (uint32_t a = c2.lo; a < c2.hi; a += 16) _mm_store_si128((__m128i*)(ls + a), vk_bs128(vk_x(priv[((a - cbase) >> 4) & 1023], ul)));')


o(f'/* A: os grupos a partir do 0 (o último com só nv_ult faixas válidas: as outras calculam lixo,')
o(f' * só leem -- endereços mascarados --, não gravam nem entregam estado), as contas de float')
o(f' * acumulando a máscara de alcance estendido -- NENHUMA chamada no laço (no ABI do Windows os')
o(f' * zmm não sobrevivem a chamadas: até uma fria nunca feita derrama o laço inteiro, ~25% aqui).')
o(f' * Devolve o primeiro grupo marcado (nada dele gravado) ou `grupos` (estado entregue). */')
o(f'static uint32_t {nome}_A(spu_context* ctx, uint32_t grupos, uint32_t nv_ult, uint32_t cbase, NOME_reg c1, NOME_reg c2, long long* consertos) {{'.replace('NOME', nome))
emite_locais('0u')
o('    uint32_t nv = 4;')
o('    for (uint32_t g = 0; g < grupos; g++) {')
o('        nv = g + 1 == grupos ? nv_ult : 4u;')
o('        __mmask16 bad = 0;')
emite_corpo('B' if UNICO else 'A')
o('        if (bad) return g;')
emite_grava('        ')
for r, p in sorted(passos.items()):
    o(f'        g{r} = vk_ai(g{r}, {4 * p});')
o('    }')
emite_entrega('    ')
o('    return grupos;')
o('}')
o('')
o(f'/* B: os grupos de g0 até o fim, com o conserto por palavra em linha (vkx_*, sem chamada); grava')
o(f' * e entrega o estado. Mais lenta que a A (~20%: o código de conserto pesa no laço), só depois')
o(f' * do primeiro grupo com alcance estendido. */')
o(f'static __attribute__((noinline)) void {nome}_B(spu_context* ctx, uint32_t g0, uint32_t grupos, uint32_t nv_ult, uint32_t cbase, NOME_reg c1, NOME_reg c2, long long* consertos) {{'.replace('NOME', nome))
emite_locais('g0')
o('    uint32_t nv = 4;')
o('    for (uint32_t g = g0; g < grupos; g++) {')
o('        nv = g + 1 == grupos ? nv_ult : 4u;')
emite_corpo('B')
emite_grava('        ')
for r, p in sorted(passos.items()):
    o(f'        g{r} = vk_ai(g{r}, {4 * p});')
o('    }')
emite_entrega('    ')
o('}')
o('')
o(f'/* faz todas as iterações: o rascunho privado começa com a LS; A até o primeiro grupo com alcance')
o(f' * estendido, e daí em diante B (o alcance estendido vem dos parâmetros das luzes, iguais na')
o(f' * chamada inteira). ruins = grupos feitos pela B. */')
o(f'static int {nome}_nucleo(spu_context* ctx, uint32_t grupos, uint32_t nv_ult, uint32_t cbase, NOME_reg c1, NOME_reg c2, long long* ruins, long long* consertos) {{'.replace('NOME', nome))
o('    uint8_t* const ls = ctx->ls_store;')
o(f'    vk* const priv = (vk*)(((uintptr_t){nome}_privb + 63) & ~(uintptr_t)63);')
o('    for (uint32_t a = c1.lo; a < c1.hi; a += 16) priv[((a - cbase) >> 4) & 1023] = ' + f'{nome}_ld1(ls, a);')
o('    for (uint32_t a = c2.lo; a < c2.hi; a += 16) priv[((a - cbase) >> 4) & 1023] = ' + f'{nome}_ld1(ls, a);')
o(f'    uint32_t g = {nome}_A(ctx, grupos, nv_ult, cbase, c1, c2, consertos);')
o(f'    if (g < grupos) {{ {nome}_B(ctx, g, grupos, nv_ult, cbase, c1, c2, consertos); *ruins += grupos - g; }}')
o('    return 1;')
o('}')
o('')
o(f'/* chamado na entrada do laço externo (antes de loc_{ini}). Devolve 1 se fez todas as')
o(f' * iterações (o código traduzido segue em loc_{fim}); 0 = continua em loc_{ini} com as')
o(f' * que sobraram (estado já avançado). */')
o(f'int {nome}(spu_context* ctx) {{')
o('    static int liga = -1, conf = 0, est = 0;')
o('    if (liga < 0) { const char* e = getenv("GOWA_NINHO"); liga = e ? atoi(e) : 1;')
o('                    const char* c = getenv("GOWA_NINHO_CHECK"); conf = c ? atoi(c) : 0;')
o('                    est = getenv("GOWA_NINHO_STATS") ? 1 : 0; }')
o('    if (!liga) return 0;')
o('    static volatile long long nc, it_tot, it_feitas, ruins, consertos, recusa[10];')
o(f'    const uint32_t lim = ctx->gpr[{lim_r}]._u32[0], c0 = ctx->gpr[{cnt_r}]._u32[0];')
o(f'    if (lim > 0x100000u || c0 > lim) {{ recusa[1]++; return 0; }}')
o(f'    uint32_t T = (lim - c0 + {passos[cnt_r] - 1}u) / {passos[cnt_r]}u; if (T == 0) T = 1;')
o('    nc++; it_tot += T;')
o('    if (est && (nc & 0x3FFF) == 0)')
o(f'        fprintf(stderr, "[ninho] {fn}/{ini}: %lld chamadas, %.1f iter/chamada, %.1f%% vetoriais, grupos pela B (conserto) %lld, palavras consertadas %lld; recusas: poucas %lld, regiões %lld/%lld/%lld/%lld/%lld/%lld/%lld/%lld\\n",')
o('                nc, (double)it_tot / nc, 100.0 * it_feitas / (it_tot ? it_tot : 1), ruins, consertos, recusa[0], recusa[2], recusa[3], recusa[4], recusa[5], recusa[6], recusa[7], recusa[8], recusa[9]);')
o('    if (T < 2) { recusa[0]++; return 0; }')
o(f'    const uint32_t n = ctx->gpr[83]._u32[0];')
o(f'    uint32_t cbase = 0; {nome}_reg c1, c2;')
o(f'    const int rz = {nome}_regioes(ctx, T, n, &cbase, &c1, &c2);')
o('    if (rz) { recusa[rz]++; return 0; }')
o(f'    gowa_fndump(ctx->gpr, ctx->ls_store, 0x{int(ini, 16) & 0xFFFF:04X}u, 0);   /* GOWA_FNDUMP: estado de entrada (banco de teste) */')
o('    const uint32_t grupos = (T + 3u) / 4u, nv_ult = T - 4u * (grupos - 1u);')
o('    long long rr = 0, xx = 0;')
o('    if (conf > 0) {')
o('        static volatile long ncham; static volatile long nconf, ndif;')
o('        if ((ncham++ % conf) == 0) {')
o('            spu_context* ref = (spu_context*)malloc(sizeof(spu_context));')
o('            if (!ref) return 0;')
o('            memcpy(ref, ctx, sizeof(spu_context));')
o(f'            {nome}_nucleo(ctx, grupos, nv_ult, cbase, c1, c2, &rr, &xx);')
o(f'            for (uint32_t k = 0; k < T; k++) {nome}_ref1(ref);')
o('            int dif = 0;')
o('            for (int q = 0; q < 128; q++)')
o('                if (memcmp(&ref->gpr[q], &ctx->gpr[q], 16)) {')
o('                    if (ndif < 8) fprintf(stderr, "[ninho] DIFERENÇA r%d: %08X %08X %08X %08X x ref %08X %08X %08X %08X (T=%u n=%u)\\n", q,')
o('                                          ctx->gpr[q]._u32[0], ctx->gpr[q]._u32[1], ctx->gpr[q]._u32[2], ctx->gpr[q]._u32[3],')
o('                                          ref->gpr[q]._u32[0], ref->gpr[q]._u32[1], ref->gpr[q]._u32[2], ref->gpr[q]._u32[3], T, n);')
o('                    dif = 1; }')
o('            for (uint32_t a = 0; a < SPU_LS_SIZE; a += 16)')
o('                if (memcmp(ref->ls_store + a, ctx->ls_store + a, 16)) {')
o('                    if (ndif < 8) fprintf(stderr, "[ninho] DIFERENÇA LS 0x%05X (T=%u n=%u)\\n", a, T, n);')
o('                    dif = 1; break; }')
o('            nconf++;')
o('            if (dif) {')
o(f'                if (ndif++ < 8) fprintf(stderr, "[ninho] DIFERENÇA {mod} {fn}/{ini} (T=%u n=%u) -- ninho desligado\\n", T, n);')
o('                memcpy(ctx->gpr, ref->gpr, sizeof ctx->gpr);')
o('                memcpy(ctx->ls_store, ref->ls_store, SPU_LS_SIZE);')
o('                liga = 0;')
o(f'            }} else if ((nconf & (nconf - 1)) == 0) fprintf(stderr, "[ninho] {fn}/{ini}: %ld conferências iguais (última: T=%u n=%u, grupos refeitos %lld)\\n", nconf, T, n, rr);')
o('            free(ref);')
o('            ruins += rr; consertos += xx; it_feitas += T;')
o('            return 1;')
o('        }')
o('    }')
o(f'    {nome}_nucleo(ctx, grupos, nv_ult, cbase, c1, c2, &rr, &xx);')
o('    ruins += rr; consertos += xx; it_feitas += T;')
o('    return 1;')
o('}')

texto = '\n'.join(out) + '\n'
arq = d / (f'ninho_{fn}_{ini}' + ('_dbg' if DBG else '') + '.inc')
arq.write_text(texto, encoding='latin-1')
print(f'{arq.name}: {N} comandos, induções {passos}, invariantes {len(invariantes)}, escritos {len(escritos)}, acessos {cont}')

if gancho and not DBG:
    alvo = d / 'spu_recomp.c'
    s = alvo.read_text(encoding='latin-1')
    nl = '\r\n' if '\r\n' in s else '\n'
    marca = f'/*NINHO {fn}/{ini}*/'
    if marca not in s:
        m2 = re.search(r'^(?:static )?(?:SPU_PN )?void \w+_spu_func_' + fn + r'(?:__b)?\(spu_context\* ctx\) \{\r?\n', s, re.M)
        p = s.find(f'loc_{ini}:', m2.end())
        assert p > 0
        lin = (f'        {marca} {{ extern int {nome}(spu_context*); if ({nome}(ctx)) goto loc_{fim}; }}' + nl)
        s = s[:p] + lin + s[p:]
        inc = f'#include "ninho_{fn}_{ini}.inc"'
        ult_inc = list(re.finditer(r'^#include "vk_[^"]+\.inc"\r?\n', s, re.M))
        if ult_inc:
            q = ult_inc[-1].end()
            s = s[:q] + inc + nl + s[q:]
        else:
            s = s + nl + inc + nl
        alvo.write_text(s, encoding='latin-1')
        print('gancho posto')
    else:
        print('gancho já existe')
