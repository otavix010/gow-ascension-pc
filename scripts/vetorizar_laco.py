"""Gera um núcleo nativo AVX-512 para um laço SPU traduzido (um só bloco: o
rótulo desvia para si mesmo): QUATRO ITERAÇÕES POR INSTRUÇÃO, exato bit a bit.

Como (por ponto de definição, SSA):
  - cada registrador de cada iteração vira uma faixa de 128 bits de um zmm
    (faixa l = iteração 4g+l do grupo g);
  - uso na mesma iteração ('c'): a mesma faixa;
  - uso do valor da iteração anterior ('p', software pipelining): as faixas
    deslocadas de uma ([anterior.3, atual.0, atual.1, atual.2], vk_prev);
  - recorrências (acumuladores, indução: componentes fortemente conexos do
    grafo de dependências com aresta 'p'): calculadas faixa a faixa, em ordem,
    com os próprios auxiliares de 128 bits -- a soma continua na ordem original;
  - leituras da LS por faixa; gravações guardadas e feitas só no fim do grupo,
    em ordem de iteração;
  - o grupo é descartado (e o laço volta ao código traduzido dali em diante)
    se alguma conta de float caiu no alcance estendido (o caminho exato dos
    auxiliares), ou se uma leitura do grupo pode depender de uma gravação do
    mesmo grupo;
  - sempre sobra pelo menos uma iteração para o código traduzido, que termina
    o laço com o estado entregue (registradores lidos antes de escritos no
    corpo = indução, acumuladores, valores do pipelining).

Saída: <dir>/vk_<FUNÇÃO>_<RÓTULO>.inc com
  int gowa_vk_<mod>_<fn>_<rot>(spu_context* ctx)  -- núcleo; devolve iterações feitas
  e uma referência (o corpo traduzido literal, numa cópia do contexto) para a
  conferência GOWA_VK_CHECK=N (a cada N chamadas compara registradores
  entregues e a LS inteira; diferença -> [vk] DIFERENÇA e desliga o núcleo).
--gancho: insere a chamada antes do rótulo do laço no spu_recomp.c.

uso: python vetorizar_laco.py <dir do módulo> <função hex> <rótulo hex> [--fonte arq] [--gancho]"""
import re
import sys
from pathlib import Path

d, fn, rot = Path(sys.argv[1]), sys.argv[2].upper().rjust(8, '0'), sys.argv[3].upper().rjust(8, '0')
arq = Path(sys.argv[sys.argv.index('--fonte') + 1]) if '--fonte' in sys.argv else d / 'spu_recomp.c'
VKX = '--vkx' in sys.argv   # 21/10: contas de float consertadas em linha (spu_vk_exato.h) em vez de refazer o grupo
src = arq.read_text(encoding='latin-1')
mod = d.name
m = re.search(r'^(?:static )?(?:SPU_PN )?void \w+_spu_func_' + fn + r'(?:__b)?\(spu_context\* ctx\) \{\n', src, re.M)
corpo = src[m.end():src.find('\n}\n', m.end())]
i = corpo.find(f'loc_{rot}:')
j = corpo.find(f'goto loc_{rot};', i)
assert corpo.count(f'goto loc_{rot};') == 1, 'o rótulo do laço tem outras entradas'
fim = corpo.find('\n', j)
linhas = [t.strip() for t in corpo[i:fim].split('\n')[1:]]
linhas = [t for t in linhas if t and not t.startswith('/*')]
desvio = linhas[-1]
mb = re.fullmatch(r'if \(ctx->gpr\[(\d+)\]\._u32\[0\] (==|!=) 0\) goto loc_' + rot + ';', desvio)
if mb:
    reg_desvio = int(mb.group(1))
    desvio_nz = mb.group(2) == '!='
    cmds = linhas[:-1]
else:
    # saída antes do fim: A; if (X ==|!= 0) goto SAÍDA; B; goto topo. As iterações que o
    # núcleo faz não saem (a última fica sempre com o código traduzido): corpo = A + B,
    # com o teste invertido (sai em X == 0 <=> continua em X != 0)
    assert desvio == f'goto loc_{rot};', 'desvio final inesperado: ' + desvio
    RS = r'if \(ctx->gpr\[(\d+)\]\._u32\[0\] (==|!=) 0\) goto loc_[0-9A-F]{8};'
    saidas = [k for k, t in enumerate(linhas[:-1]) if re.fullmatch(RS, t)]
    assert len(saidas) == 1, 'laço com mais de uma saída'
    me = re.fullmatch(RS, linhas[saidas[0]])
    reg_desvio = int(me.group(1))
    desvio_nz = me.group(2) == '=='
    depois = linhas[saidas[0] + 1:-1]
    assert not any(re.match(r'ctx->gpr\[' + str(reg_desvio) + r'\] = ', t) for t in depois), \
        'o registrador da saída muda depois dela'
    cmds = linhas[:saidas[0]] + depois
R = re.compile(r'ctx->gpr\[(\d+)\]')
# ---------------------------------------------------------------- parse
P = []
for t in cmds:
    mm = re.fullmatch(r'ctx->gpr\[(\d+)\] = spu_(\w+)\((.*)\);', t)
    ms = re.fullmatch(r'spu_ls_write128\(ctx, (.*), ctx->gpr\[(\d+)\]\);', t)
    if mm:
        w, op, args = int(mm.group(1)), mm.group(2), mm.group(3)
        op = re.sub(r'_r$', '', op)     # formas cruas: mesmos bits depois de achatados
        if op == 'ls_read128':
            P.append(dict(w=w, op='ld', addr=args[len('ctx, '):], us=[int(x) for x in R.findall(args)]))
        else:
            a = [x.strip() for x in args.split(',')]
            P.append(dict(w=w, op=op, args=a, us=[int(x) for x in R.findall(args)]))
    elif ms:
        P.append(dict(w=None, op='st', addr=ms.group(1), val=int(ms.group(2)),
                      us=[int(x) for x in R.findall(ms.group(1))] + [int(ms.group(2))]))
    else:
        raise SystemExit('comando não suportado: ' + t)
N = len(P)
ultima = {}
for k, c in enumerate(P):
    if c['w'] is not None:
        ultima[c['w']] = k
cur = {}
for k, c in enumerate(P):
    c['ref'] = {}
    for u in set(c['us']):
        c['ref'][u] = ('c', cur[u]) if u in cur else (('p', ultima[u]) if u in ultima else None)
    if c['w'] is not None:
        cur[c['w']] = k
fim_def = dict(cur)                      # definição que vale no fim da iteração
# ---------------------------------------------------------------- SCC (Tarjan)
adj = {k: set() for k in range(N)}       # y -> k (k depende de y)
for k, c in enumerate(P):
    for u, r in c['ref'].items():
        if r is not None:
            adj[r[1]].add(k)
# gravação antes de leitura na mesma iteração: aresta de ordem gravação -> leitura (a
# leitura sai depois e recebe o valor gravado onde o endereço bate), só se a leitura não
# alcançar a gravação -- senão a aresta fecharia um ciclo (recorrência pela memória) e o
# par fica para a checagem de conflito do grupo


def alcanca(a, b):
    vis, pilha_ = {a}, [a]
    while pilha_:
        x = pilha_.pop()
        if x == b:
            return True
        for y in adj[x]:
            if y not in vis:
                vis.add(y)
                pilha_.append(y)
    return False


for kl in range(N):
    if P[kl]['op'] == 'ld':
        for ks_ in range(kl):
            if P[ks_]['op'] == 'st' and not alcanca(kl, ks_):
                adj[ks_].add(kl)
idx, low, pilha, na, comp = {}, {}, [], set(), {}
cont = [0]
ncomp = [0]
sys.setrecursionlimit(10000)


def tarjan(v):
    idx[v] = low[v] = cont[0]; cont[0] += 1
    pilha.append(v); na.add(v)
    for w in adj[v]:
        if w not in idx:
            tarjan(w)
            low[v] = min(low[v], low[w])
        elif w in na:
            low[v] = min(low[v], idx[w])
    if low[v] == idx[v]:
        while True:
            w = pilha.pop(); na.discard(w)
            comp[w] = ncomp[0]
            if w == v:
                break
        ncomp[0] += 1


for v in range(N):
    if v not in idx:
        tarjan(v)
membros = {}
for k, cc in comp.items():
    membros.setdefault(cc, []).append(k)
seq = set()
for cc, ks in membros.items():
    if len(ks) > 1 or any(k in adj[k] for k in ks):
        seq.update(ks)
# ordem topológica dos componentes (Tarjan dá a ordem inversa)
ordem_comp = sorted(membros, key=lambda cc: -cc)
# ---------------------------------------------------------------- contagem de iterações
# expressão C (no início do laço, com ctx) do número de iterações que o laço vai fazer
reg_teste = reg_desvio                   # o registrador que o desvio lê (antes de seguir cópias)
kdes = fim_def[reg_desvio]
cdef = P[kdes]
# o desvio pode testar uma cópia (ori x,0 ...) do valor do contador: segue até ele
COPIA = {('ori', '0'), ('shlqbyi', '0'), ('rotqbyi', '0'), ('ai', '0')}
while cdef['op'] not in ('ld', 'st') and len(cdef.get('args', [])) == 2 and (cdef['op'], cdef['args'][-1]) in COPIA \
        and R.fullmatch(cdef['args'][0]):
    u0 = int(R.fullmatch(cdef['args'][0]).group(1))
    r0 = cdef['ref'].get(u0)
    if not r0 or r0[0] != 'c':
        break
    kdes = r0[1]
    cdef = P[kdes]
    reg_desvio = cdef['w']


def passo_contador(r, k_uso):
    """se o valor de r lido pelo comando k_uso vem de 'r = ai(r, s)' na mesma iteração
    (contador que anda s por iteração), devolve s"""
    ref = P[k_uso]['ref'].get(r)
    if not ref or ref[0] != 'c':
        return None
    c = P[ref[1]]
    if c['op'] != 'ai' or c['w'] != r or not R.fullmatch(c['args'][0]) or int(R.fullmatch(c['args'][0]).group(1)) != r:
        return None
    if P[ref[1]]['ref'][r] != ('p', ref[1]):
        return None
    return int(c['args'][1])


expr_iters = None
if not desvio_nz and cdef['op'] == 'ceq':
    # brz em ceq(lim, cont) com cont += 1: iterações = lim - cont
    ra, rb = [int(R.fullmatch(x).group(1)) for x in cdef['args']]
    for rl, rc in ((ra, rb), (rb, ra)):
        if passo_contador(rc, kdes) == 1 and rl not in ultima:
            expr_iters = f'ctx->gpr[{rl}]._u32[0] - ctx->gpr[{rc}]._u32[0]'
            break
elif desvio_nz and cdef['op'] == 'ai' and R.fullmatch(cdef['args'][0]):
    # brnz em X = Y + s, com Y recebendo X no fim da iteração (o próprio X, ou uma
    # cópia dele): X_t = Y0 + s*t, sai no primeiro t com X_t == 0 (só se exato)
    ry = int(R.fullmatch(cdef['args'][0]).group(1))
    s = int(cdef['args'][1])
    ref = cdef['ref'][ry]
    ok = False
    if ref and ref[0] == 'p' and s != 0:
        ky = ref[1]
        if ky == kdes:
            ok = True
        else:
            cy = P[ky]
            if cy['op'] in ('ori', 'shlqbyi', 'rotqbyi', 'ai') and len(cy['args']) == 2 and cy['args'][1] == '0' \
                    and R.fullmatch(cy['args'][0]) and cy['ref'].get(int(R.fullmatch(cy['args'][0]).group(1))) == ('c', kdes):
                ok = True
    if ok:
        dist = f'(0u - ctx->gpr[{ry}]._u32[0])' if s > 0 else f'ctx->gpr[{ry}]._u32[0]'
        k = abs(s)
        expr_iters = f'(({dist} % {k}u) ? 0u : {dist} / {k}u)'
elif desvio_nz and cdef['op'] == 'cgt':
    # brnz em cgt(lim, cont) com cont += s (s > 0), comparação com sinal depois do passo:
    # iterações = o menor t >= 1 com cont0 + s*t >= lim
    ra, rb = [R.fullmatch(x) for x in cdef['args']]
    if ra and rb:
        rl, rc = int(ra.group(1)), int(rb.group(1))
        s = passo_contador(rc, kdes)
        if s and s > 0 and rl not in ultima:
            expr_iters = (f'(uint32_t)({{ const int64_t _d = (int64_t)(int32_t)ctx->gpr[{rl}]._u32[0] - (int32_t)ctx->gpr[{rc}]._u32[0]; '
                          f'_d <= {s} ? 1 : (_d + {s - 1}) / {s}; }})')
elif desvio_nz and cdef['op'] == 'cgti' and R.fullmatch(cdef['args'][0]):
    # brnz em cgti(X, imm), X um contador: a cadeia de definições de X (ai imediato,
    # a com um invariante, cópias) volta a X depois de cruzar uma iteração. Com sinal:
    # X_1 = entrada + inc1, X_t = X_1 + passo*(t-1), continua enquanto X_t > imm;
    # passo < 0 (confere em tempo de execução, senão 0 = núcleo não roda)
    rx, imm = int(R.fullmatch(cdef['args'][0]).group(1)), int(cdef['args'][1])
    ref0 = cdef['ref'].get(rx)
    inc = [[], []]      # termos antes (0) e depois (1) de cruzar a iteração
    lado, k_at, ent, ok = 0, None, None, False
    if ref0 and ref0[0] == 'c':
        k_at = ref0[1]
        for _ in range(64):
            c = P[k_at]
            args = c.get('args', [])
            if c['op'] == 'ai' and len(args) == 2 and R.fullmatch(args[0]):
                inc[lado].append(f'(uint32_t){int(args[1])}')
                u = int(R.fullmatch(args[0]).group(1))
            elif c['op'] in ('ori', 'shlqbyi', 'rotqbyi') and len(args) == 2 and args[1] == '0' and R.fullmatch(args[0]):
                u = int(R.fullmatch(args[0]).group(1))
            elif c['op'] == 'a' and len(args) == 2 and all(R.fullmatch(x) for x in args):
                ua, ub = [int(R.fullmatch(x).group(1)) for x in args]
                if c['ref'].get(ub) is None and c['ref'].get(ua) is not None:
                    u, uinv = ua, ub
                elif c['ref'].get(ua) is None and c['ref'].get(ub) is not None:
                    u, uinv = ub, ua
                else:
                    break
                inc[lado].append(f'ctx->gpr[{uinv}]._u32[0]')
            else:
                break
            r = c['ref'].get(u)
            if r is None:
                break
            if r[0] == 'p':
                if lado == 1:
                    break
                lado, ent = 1, u
            k_at = r[1]
            if lado == 1 and k_at == ref0[1]:
                ok = True
                break
    if ok:
        x1 = ' + '.join([f'ctx->gpr[{ent}]._u32[0]'] + inc[0])
        ps = ' + '.join(inc[0] + inc[1]) or '0u'
        expr_iters = (f'(uint32_t)({{ const int64_t _x = (int32_t)({x1}), _s = (int32_t)({ps}); '
                      f'_s >= 0 ? 0 : _x <= {imm} ? 1 : 1 + (_x - ({imm}) + (-_s) - 1) / (-_s); }})')
if expr_iters is None:
    raise SystemExit('contador do laço não reconhecido: ' + desvio + ' / ' + str(cdef))
# registradores entregues ao código traduzido: lidos antes de escritos no corpo
entregues = sorted({u for c in P for u, r in c['ref'].items() if r is not None and r[0] == 'p'})
invariantes = sorted({u for c in P for u, r in c['ref'].items() if r is None})
# ---------------------------------------------------------------- geração
FLOAT = {'fa': 2, 'fs': 2, 'fm': 2, 'fma': 3, 'fms': 3, 'fnms': 3}
# achatamento preguiçoso dentro do núcleo (a regra do scripts/flush_preguicoso.py): um
# resultado de float que só vai para outras contas de float, comparações de float ou
# cflts/cfltu com I8 >= 47 (também através de cópias), e que não é o valor entregue de
# nenhum registrador no fim, sai cru (sem o +0 do expoente 0). Gravações, leituras e o
# resto dos comandos são usos inseguros.
usos = {k: [] for k in range(N)}
for kk, c in enumerate(P):
    for u, r in c['ref'].items():
        if r is not None:
            usos[r[1]].append(kk)
ent_defs = {fim_def[u] for u in entregues if u in fim_def}
SEG = set(FLOAT) | {'fcgt', 'fceq', 'fcmgt', 'fcmeq'}


def eh_copia(c):
    a_ = c.get('args', [])
    return c['op'] not in ('ld', 'st') and len(a_) == 2 and bool(R.fullmatch(a_[0])) and         ((c['op'], a_[1]) in COPIA or (c['op'] in ('or', 'and') and a_[0] == a_[1]))


def so_float(k, vis):
    if k in ent_defs:
        return False
    for kk in usos[k]:
        c = P[kk]
        if c['op'] in SEG:
            continue
        if c['op'] in ('cflts', 'cfltu') and re.fullmatch(r'\d+', c['args'][1]) and int(c['args'][1]) >= 47:
            continue
        if eh_copia(c) and kk not in vis and so_float(kk, vis | {k}):
            continue
        return False
    return True


cru = {k for k in range(N) if P[k]['op'] in FLOAT and so_float(k, frozenset())}
VEC2 = {'fcgt', 'fceq', 'fcmgt', 'fcmeq', 'and', 'or', 'xor', 'andc', 'nor', 'nand', 'orc',
        'a', 'sf', 'ceq', 'cgt', 'clgt', 'fi', 'clgth', 'ceqh', 'cgth', 'rotm', 'rotqby', 'shl',
        'rotqmbybi', 'rotqmbi', 'rotma', 'rothm', 'rot', 'sfh', 'ah'}
VEC3 = {'selb', 'shufb'}
VEC1 = {'frsqest', 'frest', 'cntb', 'gbh', 'gb'}
VECI = {'ai', 'sfi', 'andi', 'ori', 'xori', 'ceqi', 'cgti', 'clgti', 'andbi', 'shli', 'rotmi', 'roti',
        'rotqbii', 'shlqbii', 'rotqbyi', 'shlqbyi', 'rotqmbyi', 'cflts', 'cfltu', 'csflt', 'cuflt'}
COPIA = {('ori', '0'), ('shlqbyi', '0'), ('rotqbyi', '0'), ('ai', '0')}
nome = f'gowa_vk_{mod}_{fn}_{rot}'
V = lambda k: f'v{k}'                    # vetor (4 iterações) da definição k
PV = lambda k: f'p{k}'                   # o mesmo do grupo anterior (faixa 3 = última iteração)
precisa_prev = sorted({r[1] for c in P for u, r in c['ref'].items() if r is not None and r[0] == 'p'})
out = []
o = out.append
o(f'/* Gerado por scripts/vetorizar_laco.py -- {mod} func_{fn} laço loc_{rot}. Não editar. */')
o('#include "spu_vk.h"')
if VKX:
    o('#include "spu_vk_exato.h"')
o('#include <stdio.h>')
o('#include <stdlib.h>')
o(f'/* {N} comandos; {len(seq)} em recorrências/indução (faixa a faixa); entregues: {entregues} */')
o('')
# referência: o corpo traduzido literal
o(f'static void {nome}_ref(spu_context* ctx, uint32_t iters) {{')
o('    for (uint32_t it = 0; it < iters; it++) {')
for t in cmds:
    o('        ' + t)
o('    }')
o('}')
o('')


QV = lambda y: f'q{y}'                    # __m128i: faixa 3 (última iteração) do grupo anterior


def entrada(k, u):
    """expressão vetorial (vk) do valor do registrador u lido pelo comando k"""
    r = P[k]['ref'][u]
    if r is None:
        return f'_mm512_broadcast_i32x4(i{u})'
    tipo, y = r
    if tipo == 'c':
        return V(y)
    return f'vk_prevq({V(y)}, {QV(y)})'


def entrada_x(k, u, L):
    """expressão __m128i do valor de u lido pelo comando k na faixa L (L constante)"""
    r = P[k]['ref'][u]
    if r is None:
        return f'i{u}'
    tipo, y = r
    mesmo = y in seq and comp[y] == comp[k]
    if tipo == 'c':
        return f'vk_u2x(t{y}_{L})' if mesmo else f'vk_x({V(y)}, {L})'
    if L == 0:
        return QV(y)
    return f'vk_u2x(t{y}_{L - 1})' if mesmo else f'vk_x({V(y)}, {L - 1})'


def expr_vetor(k):
    c = P[k]
    op = c['op']
    a = c['args']

    def arg(x):
        mm = R.fullmatch(x)
        return entrada(k, int(mm.group(1))) if mm else None
    if (op, a[-1]) in COPIA and len(a) == 2 and R.fullmatch(a[0]):
        return arg(a[0])
    if op in FLOAT:
        if VKX:   # conserto em linha pelo caminho exato vetorial (laços onde o alcance estendido é a regra)
            ar_ = ', '.join(arg(x) for x in a)
            return (f'({{ __mmask16 _m = 0; vk _t = vk_{op}{"_r" if k in cru else ""}({ar_}, &_m); '
                    f'if (__builtin_expect(_m != 0, 0)) _t = _mm512_mask_mov_epi32(_t, _m, vkx_{op}({ar_})); _t; }})')
        return f'vk_{op}{"_r" if k in cru else ""}({", ".join(arg(x) for x in a)}, &bad)'
    if op in VEC2 and len(a) == 2 and all(R.fullmatch(x) for x in a):
        return f'vk_{op}({arg(a[0])}, {arg(a[1])})'
    if op in VEC3 and len(a) == 3 and all(R.fullmatch(x) for x in a):
        return f'vk_{op}({arg(a[0])}, {arg(a[1])}, {arg(a[2])})'
    if op in VEC1 and len(a) == 1:
        return f'vk_{op}({arg(a[0])})'
    if op in VECI and len(a) == 2 and R.fullmatch(a[0]) and re.fullmatch(r'-?\d+', a[1]):
        return f'vk_{op}({arg(a[0])}, {a[1]})'
    return None


def expr_faixa(k, L):
    """o comando k na faixa L (constante) com o auxiliar de 128 bits; devolve u128"""
    c = P[k]
    ar = []
    for x in c['args']:
        mm = R.fullmatch(x)
        ar.append(f'vk_x2u({entrada_x(k, int(mm.group(1)), L)})' if mm else x)
    return f'spu_{c["op"]}({", ".join(ar)})'


def addr_faixa(k, L):
    """endereço de leitura/gravação do comando k na faixa L"""
    e = P[k]['addr']

    def troca(mm):
        ex = f'(uint32_t)_mm_cvtsi128_si32({entrada_x(k, int(mm.group(1)), L)})'
        if ex not in acache:
            acache[ex] = f'a{len(acache)}'
            o(f'        const uint32_t {acache[ex]} = {ex};')
        return acache[ex]
    return re.sub(r'ctx->gpr\[(\d+)\]\._u32\[0\]', troca, e)




def eh_copia(k):
    c = P[k]
    return c['op'] not in ('ld', 'st') and len(c['args']) == 2 and (c['op'], c['args'][-1]) in COPIA and R.fullmatch(c['args'][0])


def forma_scc(cc):
    """('ind', k, y, s): r = ai(r_ant, s) + cópias; ('acc', k, pos, y): um acumulador
    (op com UM operando = valor do próprio componente na iteração anterior) + cópias;
    ('gen',) caso contrário. y = definição alvo da referência à iteração anterior."""
    ks = sorted(membros[cc])
    ops = [k for k in ks if not eh_copia(k)]
    if len(ops) != 1:
        return ('gen',)
    k = ops[0]
    c = P[k]
    for kc in ks:
        if kc != k:
            u = int(R.fullmatch(P[kc]['args'][0]).group(1))
            r = P[kc]['ref'][u]
            if not r or r[0] != 'c' or r[1] not in ks:
                return ('gen',)
    pos_mem = []
    for i, x in enumerate(c['args']):
        mm = R.fullmatch(x)
        if mm:
            r = c['ref'][int(mm.group(1))]
            if r and r[1] in ks:
                pos_mem.append((i, r))
    if len(pos_mem) != 1 or pos_mem[0][1][0] != 'p':
        return ('gen',)
    i, r = pos_mem[0]
    if c['op'] == 'ai' and i == 0 and re.fullmatch(r'-?\d+', c['args'][1]):
        return ('ind', k, r[1], int(c['args'][1]))
    if c['op'] in ('fa', 'fs', 'fma', 'fms', 'fnms', 'a') and all(R.fullmatch(x) for x in c['args']):
        return ('acc', k, i, r[1])
    return ('gen',)


desc_memo = {}


def desc(k):
    if k not in desc_memo:
        vis, pilha = set(), [k]
        while pilha:
            x = pilha.pop()
            for y in adj[x]:
                if y not in vis:
                    vis.add(y)
                    pilha.append(y)
        desc_memo[k] = vis
    return desc_memo[k]


formas = {cc: (forma_scc(cc) if sorted(membros[cc])[0] in seq else ('vec',)) for cc in ordem_comp}
posc = {cc: i for i, cc in enumerate(ordem_comp)}


# passo por iteração do valor de cada definição (None = desconhecido): indução, cópias,
# ai/a com invariante. Endereço de leitura/gravação = soma dos termos -> A + s*iteração.
def passo_def(y, vis=frozenset()):
    if y in vis:
        return None
    f_ = formas.get(comp[y])
    if f_ and f_[0] == 'ind':
        return f_[3]
    c = P[y]
    a_ = c.get('args', [])
    if c['op'] in ('ld', 'st'):
        return None
    def passo_reg(u):
        r = c['ref'].get(u)
        return 0 if r is None else passo_def(r[1], vis | {y})
    if len(a_) == 2 and R.fullmatch(a_[0]) and ((c['op'], a_[1]) in COPIA or (c['op'] == 'ai' and re.fullmatch(r'-?\d+', a_[1]))):
        return passo_reg(int(R.fullmatch(a_[0]).group(1)))
    if c['op'] == 'a' and len(a_) == 2 and all(R.fullmatch(x) for x in a_):
        p1, p2 = [passo_reg(int(R.fullmatch(x).group(1))) for x in a_]
        return None if p1 is None or p2 is None else p1 + p2
    return None


def passo_end(k):
    tot = 0
    for t in P[k]['addr'].split('+'):
        t = t.strip()
        mm = re.fullmatch(r'ctx->gpr\[(\d+)\]\._u32\[0\]', t)
        if mm:
            r = P[k]['ref'].get(int(mm.group(1)))
            ps = 0 if r is None else passo_def(r[1])
            if ps is None:
                return None
            tot += ps
        elif not re.fullmatch(r'(0x[0-9a-fA-F]+|\d+)', t):
            return None
    return tot


_passos = {passo_end(k) for k in range(N) if P[k]['op'] in ('ld', 'st')}
if '--dbg' in sys.argv:
    print('passos:', [(k, P[k]['op'], P[k]['addr'], passo_end(k)) for k in range(N) if P[k]['op'] in ('ld', 'st')])
# todas as leituras e gravações com o mesmo passo, múltiplo de 16: a diferença entre os
# endereços de uma gravação e de uma leitura é a mesma em todas as iterações da chamada
passo_comum = _passos.pop() if len(_passos) == 1 and None not in _passos else None
if passo_comum is not None and passo_comum % 16:
    passo_comum = None
usado = set()
plano = {}
for i, cc in enumerate(ordem_comp):
    if cc in usado:
        continue
    f = formas[cc]
    if f[0] == 'acc':
        chave = (P[f[1]]['op'], f[2])
        lote = [cc]
        dl = set().union(*[desc(m) for m in membros[cc]])
        for j in range(i + 1, len(ordem_comp)):
            if len(lote) == 4:
                break
            x = ordem_comp[j]
            if x in usado:
                continue
            if any(m in dl for m in membros[x]):
                break
            fx = formas[x]
            if fx[0] == 'acc' and (P[fx[1]]['op'], fx[2]) == chave:
                lote.append(x)
                for m in membros[x]:
                    dl |= desc(m)
        for x in lote:
            usado.add(x)
        plano.setdefault(max(posc[x] for x in lote), []).append(('lote', lote))
    else:
        usado.add(cc)
        plano.setdefault(i, []).append(('um', cc))
nst = sum(1 for c in P if c['op'] == 'st')
nld = sum(1 for c in P if c['op'] == 'ld')
o(f'static int {nome}_nucleo(spu_context* ctx, int* motivo) {{')
o(f'    const uint32_t iters = {expr_iters};')
o('    if (iters <= 4 || iters > 0x10000) return 0;')
o('    const uint32_t grupos = (iters - 1) / 4;')
o('    uint8_t* const ls = SPU_LS_BASE(ctx);')
for u in invariantes:
    o(f'    const __m128i i{u} = vk_u2x(ctx->gpr[{u}]);')
for y in precisa_prev:
    o(f'    __m128i {QV(y)} = vk_u2x(ctx->gpr[{P[y]["w"]}]);')
o('    uint32_t g = 0;')
o('    int par_k = 0; uint32_t par_e = 0; (void)par_k; (void)par_e;')
o('    int seguro = 0;   /* conflito impossível no resto da chamada (decidido no grupo 0) */')
o(f'    uint32_t sta[{max(nst, 1)}][4]; vk stv[{max(nst, 1)}];')
o(f'    uint32_t lda[{max(nld, 1)}][4];')
o('    for (; g < grupos; g++) {')
o('        __mmask16 bad = 0;')
ini_grupo = len(out)
ist = 0
ild = 0
mapa_st = {}
mapa_ld = {}
pendentes = []   # (gravação, leitura) da mesma iteração sem encaminhamento
acache = {}


def emite_lote(lote):
    """acumuladores em lote: até 4 por zmm, contribuições transpostas, 4 passos em ordem"""
    fs_ = [formas[x] for x in lote]
    op = P[fs_[0][1]]['op']
    pos = fs_[0][2]
    n_ = len(lote)
    tag = f'L{fs_[0][1]}'
    qs = [QV(f[3]) for f in fs_] + ['_mm_setzero_si128()'] * (4 - n_)
    o(f'        vk {tag}_acc = vk_joinx({", ".join(qs)});')
    nargs = len(P[fs_[0][1]]['args'])
    for p_ in range(nargs):
        if p_ == pos:
            continue
        xs = []
        for f in fs_:
            u = int(R.fullmatch(P[f[1]]['args'][p_]).group(1))
            xs.append(entrada(f[1], u))
        xs += ['_mm512_setzero_si512()'] * (4 - n_)
        o(f'        vk {tag}_o{p_}_0, {tag}_o{p_}_1, {tag}_o{p_}_2, {tag}_o{p_}_3;')
        o(f'        vk_t4({", ".join(xs)}, &{tag}_o{p_}_0, &{tag}_o{p_}_1, &{tag}_o{p_}_2, &{tag}_o{p_}_3);')
    for L in range(4):
        ar = [f'{tag}_acc' if p_ == pos else f'{tag}_o{p_}_{L}' for p_ in range(nargs)]
        extra = ', &bad' if op in FLOAT else ''
        if VKX and op in FLOAT:
            o(f'        {{ __mmask16 _m = 0; vk _t = vk_{op}({", ".join(ar)}, &_m); if (__builtin_expect(_m != 0, 0)) _t = _mm512_mask_mov_epi32(_t, _m, vkx_{op}({", ".join(ar)})); {tag}_acc = _t; }} const vk {tag}_s{L} = {tag}_acc;')
        else:
            o(f'        {tag}_acc = vk_{op}({", ".join(ar)}{extra}); const vk {tag}_s{L} = {tag}_acc;')
    o(f'        vk {tag}_r0, {tag}_r1, {tag}_r2, {tag}_r3;')
    o(f'        vk_t4({tag}_s0, {tag}_s1, {tag}_s2, {tag}_s3, &{tag}_r0, &{tag}_r1, &{tag}_r2, &{tag}_r3);')
    for j, x in enumerate(lote):
        for k in sorted(membros[x]):
            o(f'        const vk {V(k)} = {tag}_r{j};')


def emite_st(k):
    global ist
    c = P[k]
    mapa_st[k] = ist
    for L in range(4):
        o(f'        sta[{ist}][{L}] = ({addr_faixa(k, L)}) & (SPU_LS_MASK & ~0xFu);')
    o(f'        stv[{ist}] = vk__bswap32({entrada(k, c["val"])});')
    ist += 1
    feito.add(k)


feito = set()      # comandos já emitidos (para adiantar gravações antes de leituras)
itens = [it for i in sorted(plano) for it in plano[i]]
for tipo_it, alvo in itens:
    if tipo_it == 'lote':
        emite_lote(alvo)
        for x in alvo:
            feito.update(membros[x])
        continue
    cc = alvo
    ks = sorted(membros[cc])
    feito.update(ks)
    if formas[cc][0] == 'ind':
        _, kai, y, passo = formas[cc]
        o(f'        const vk {V(kai)} = vk_passo({QV(y)}, {passo});')
        for k in ks:
            if k != kai:
                o(f'        const vk {V(k)} = {V(kai)};')
        continue
    if ks[0] in seq:
        # componente sequencial: faixa a faixa (desenrolado), em ordem do corpo
        for L in range(4):
            for k in ks:
                c = P[k]
                if c['op'] in ('ld', 'st'):
                    raise SystemExit('leitura/gravação dentro de recorrência: não suportado')
                mm = R.fullmatch(c['args'][0]) if c['args'] else None
                if (c['op'], c['args'][-1]) in COPIA and len(c['args']) == 2 and mm:
                    o(f'        const u128 t{k}_{L} = vk_x2u({entrada_x(k, int(mm.group(1)), L)});')
                else:
                    o(f'        const u128 t{k}_{L} = {expr_faixa(k, L)};')
        for k in ks:
            o(f'        const vk {V(k)} = vk_joinx(vk_u2x(t{k}_0), vk_u2x(t{k}_1), vk_u2x(t{k}_2), vk_u2x(t{k}_3));')
        continue
    k = ks[0]
    c = P[k]
    if c['op'] == 'ld':
        mapa_ld[k] = ild
        for L in range(4):
            o(f'        lda[{ild}][{L}] = ({addr_faixa(k, L)}) & (SPU_LS_MASK & ~0xFu);')
        # gravações anteriores da mesma iteração (ficam para o fim do grupo): onde o
        # endereço bate, a faixa recebe o valor gravado -- na ordem do corpo, a última vence
        # (a ordem de emissão é a dos componentes, não a do corpo: uma gravação anterior
        # no corpo que ainda não foi emitida fica para a checagem de conflito do grupo)
        for kk in range(k):
            if P[kk]['op'] == 'st' and kk not in mapa_st and                     all(r is None or (r[1] in feito and r[1] != k) for r in P[kk]['ref'].values()):
                emite_st(kk)    # adiantada: as entradas já existem
        ants = [mapa_st[kk] for kk in range(k) if P[kk]['op'] == 'st' and kk in mapa_st]
        pendentes.extend((kk, k) for kk in range(k) if P[kk]['op'] == 'st' and kk not in mapa_st)
        cq = '' if ants else 'const '
        o(f'        {cq}vk {V(k)} = vk_ld4(ls, lda[{ild}][0], lda[{ild}][1], lda[{ild}][2], lda[{ild}][3]);')
        for a_st in ants:
            o(f'        {V(k)} = _mm512_mask_mov_epi32({V(k)}, vk_mesmo_end(sta[{a_st}], lda[{ild}]), vk__bswap32(stv[{a_st}]));')
        ild += 1
        continue
    if c['op'] == 'st':
        if k not in mapa_st:
            emite_st(k)
        continue
    e = expr_vetor(k)
    if e is None:
        o(f'        const vk {V(k)} = vk_joinx({", ".join("vk_u2x(" + expr_faixa(k, L) + ")" for L in range(4))});')
    else:
        o(f'        const vk {V(k)} = {e};')
# o valor da última iteração de cada v{y} sai logo depois do último uso de v{y}/q{y}
# (n{y}, xmm) -- assim o zmm morre cedo; q{y} só muda depois das conferências do grupo
ult = {}
for y in precisa_prev:
    rx = re.compile(r'\b(v|q)' + str(y) + r'\b')
    for idx in range(ini_grupo, len(out)):
        if rx.search(out[idx]):
            ult[y] = idx
for idx in sorted(set(ult.values()), reverse=True):
    ys = [y for y in precisa_prev if ult.get(y) == idx]
    out[idx + 1:idx + 1] = [f'        const __m128i n{y} = vk_x({V(y)}, 3);' for y in ys]
o('        if (bad) { *motivo = 1; break; }   /* alcance estendido: o código traduzido faz este grupo */')
if nst and nld:
    # gravação da iteração j x leitura da iteração L > j, endereço a endereço
    o(f'        if (!seguro) {{   int conflito = vk_conflito(sta, {nst}, lda, {nld});')
    # gravação antes de leitura na mesma iteração: encaminhada na leitura quando a
    # gravação já tinha sido emitida; as outras, conferidas aqui
    if pendentes:
        # uma comparação de 128 bits por par (as 4 iterações de uma vez)
        o('            __mmask8 pm = 0;')
        for kst, kld in pendentes:
            o(f'            pm |= _mm_cmpeq_epi32_mask(_mm_loadu_si128((const __m128i*)sta[{mapa_st[kst]}]), _mm_loadu_si128((const __m128i*)lda[{mapa_ld[kld]}]));')
        o('            if (pm) { conflito = 1; par_k = 1; }')
    o('            if (conflito) { *motivo = 2; break; } }')
    if passo_comum is not None:
        # mesma indução em todos os endereços: gravação a na iteração j = S + p*j, leitura b
        # na iteração L = B + p*L; colidem só se S - B = p*(L - j), L - j em 1..3 (ou 0 nos
        # pares da mesma iteração sem encaminhamento) -- a diferença não muda na chamada
        m_ = 0x3FFF0
        ks_ = sorted({(passo_comum * d_) & m_ for d_ in (1, 2, 3)})
        o('        if (!g) {')
        o('            seguro = 1;')
        o(f'            for (int a = 0; a < {nst}; a++) for (int b = 0; b < {nld}; b++) {{')
        o('                const uint32_t d = (sta[a][0] - lda[b][0]) & 0x3FFF0u;')
        o('                if (' + ' || '.join(f'd == 0x{k_:X}u' for k_ in ks_) + ') seguro = 0;')
        o('            }')
        for kst, kld in pendentes:
            o(f'            if (((sta[{mapa_st[kst]}][0] - lda[{mapa_ld[kld]}][0]) & 0x3FFF0u) == 0) seguro = 0;')
        o('        }')
    elif all(passo_end(k) is not None for k in mapa_st) and all(passo_end(k) is not None for k in mapa_ld) and '--sem-par' not in sys.argv:
        # 21/10: decisão POR PAR, uma vez por chamada (no fim do grupo 0, com os endereços
        # crus da faixa 0; vale dali em diante). Endereço = A + p*iteração.
        #  - mesmo passo p: a diferença crua D = S - B não muda; a gravação da iteração j e a
        #    leitura da L = j + d só caem no mesmo quadword se |D - p*d| < 16 (d = 1..3; e
        #    d = 0 nos pares da mesma iteração sem encaminhamento);
        #  - passos diferentes: os intervalos da chamada inteira (com 16 de folga) não se
        #    tocam (e não dão a volta na LS).
        # Cobre o caso da saída com passo que não é múltiplo de 16 (registros de 108 B do
        # plugin 30600), onde o passo_comum não se aplica.
        o('        if (!g) {')
        o('            int ok = 1;')
        for a in sorted(mapa_st):
            o(f'            const uint32_t rs{mapa_st[a]} = ({addr_faixa(a, 0)}) & SPU_LS_MASK;')
        for b in sorted(mapa_ld):
            o(f'            const uint32_t rl{mapa_ld[b]} = ({addr_faixa(b, 0)}) & SPU_LS_MASK;')
        o('            const int64_t nit = (int64_t)iters - 1;')
        pend_set = set(pendentes)
        for a in sorted(mapa_st):
            for b in sorted(mapa_ld):
                pa, pb = passo_end(a), passo_end(b)
                sa_, sb_ = f'rs{mapa_st[a]}', f'rl{mapa_ld[b]}'
                if pa == pb:
                    ds = [1, 2, 3] + ([0] if (a, b) in pend_set else [])
                    conds = ' || '.join(f'(D - ({pa * d_})) > -16 && (D - ({pa * d_})) < 16' for d_ in ds)
                    o(f'            {{ const int32_t D = (int32_t)(({sa_} - {sb_}) << 14) >> 14; if ({conds}) ok = 0; }}')
                else:
                    o(f'            {{ const int64_t s0 = {sa_}, s1 = s0 + ({pa}) * nit, b0 = {sb_}, b1 = b0 + ({pb}) * nit;')
                    o('              const int64_t slo = s0 < s1 ? s0 : s1, shi = (s0 < s1 ? s1 : s0) + 16, blo = b0 < b1 ? b0 : b1, bhi = (b0 < b1 ? b1 : b0) + 16;')
                    o('              if (slo < 0 || blo < 0 || shi > SPU_LS_SIZE || bhi > SPU_LS_SIZE || (slo < bhi + 16 && blo < shi + 16)) ok = 0; }')
        o('            seguro = ok;')
        o('        }')
if nst:
    # gravações em ordem de iteração, e dentro dela na ordem do corpo
    ordem_st = [mapa_st[k] for k in range(N) if k in mapa_st]
    for L in range(4):
        for a in ordem_st:
            o(f'        _mm_store_si128((__m128i*)(ls + sta[{a}][{L}]), vk_x(stv[{a}], {L}));')
for y in precisa_prev:
    o(f'        {QV(y)} = n{y};')
o('    }')
if nst and nld:
    # diagnóstico (GOWA_VK_CONFDBG) fora do laço: chamadas lá dentro custam derramamentos
    o('    if (*motivo == 2) {')
    o(f'        static int vz, vp; vk_conflito_mostra("{fn}/{rot}", &vz, sta, {nst}, lda, {nld});')
    if pendentes:
        o('        static const int pares[][4] = {' + ', '.join(f'{{{mapa_st[a_]}, {mapa_ld[b_]}, {a_}, {b_}}}' for a_, b_ in pendentes) + '};')
        o(f'        if (par_k) for (unsigned q = 0; q < {len(pendentes)}u; q++) for (int L = 0; L < 4; L++)')
        o('            if (sta[pares[q][0]][L] == lda[pares[q][1]][L]) vk_par_mostra("' + f'{fn}/{rot}' + '", &vp, pares[q][2], pares[q][3], sta[pares[q][0]][L]);')
    o('    }')
o('    if (!g) return 0;')
o('    /* estado no fim da última iteração feita: o código traduzido continua dali */')
for u in entregues:
    o(f'    ctx->gpr[{u}] = vk_x2u({QV(fim_def[u])});')
o('    return (int)(g * 4);')
o('}')
o('')
# grupo com alcance estendido: a transcrição literal (exata) faz essas 4 iterações a partir
# do estado entregue e o núcleo continua; a última iteração fica sempre com o traduzido
o(f'static __attribute__((noinline, cold)) void {nome}_ref4(spu_context* ctx) {{ {nome}_ref(ctx, 4); }}')
o(f'static int {nome}_corre(spu_context* ctx, int* motivo) {{')
o('    int tot = 0;')
o('    for (;;) {')
o('        int mt = 0;')
o(f'        tot += {nome}_nucleo(ctx, &mt);')
o('        if (mt) *motivo = mt;')
o('        if (mt != 1) break;')
o(f'        const uint32_t resta = {expr_iters};')
o('        if (resta <= 4 || resta > 0x10000) break;')
o(f'        {nome}_ref4(ctx);')
o('        tot += 4;')
o('    }')
o('    return tot;')
o('}')
o('')
# invólucro com conferência
o(f'int {nome}(spu_context* ctx) {{')
o('    static int liga = -1, conf = 0; static volatile long ncham;')
o('    if (liga < 0) { const char* e = getenv("GOWA_VK"); liga = e ? atoi(e) : 1;')
o('                    const char* c = getenv("GOWA_VK_CHECK"); conf = c ? atoi(c) : 0; }')
o('    if (!liga) return 0;')
o('    if (conf > 0 && (ncham++ % conf) == 0) {')
o('        spu_context* ref = (spu_context*)malloc(sizeof(spu_context));')
o('        if (!ref) return 0;')
o('        memcpy(ref, ctx, sizeof(spu_context));')
o(f'        const uint32_t it_e = {expr_iters};')
o(f'        int mt = 0; const int feitas = {nome}_corre(ctx, &mt);')
o(f'        if (feitas) {nome}_ref(ref, (uint32_t)feitas);')
o('        int dif = memcmp(ref->ls_store, ctx->ls_store, SPU_LS_SIZE) != 0;')
o(f'        static const int ent[] = {{{", ".join(str(u) for u in entregues)}}};')
o('        for (unsigned q = 0; q < sizeof ent / sizeof ent[0]; q++)')
o('            if (memcmp(&ref->gpr[ent[q]], &ctx->gpr[ent[q]], 16)) {')
o(f'                fprintf(stderr, "[vk] DIFERENÇA {mod} {fn}/{rot} r%d: %08X %08X %08X %08X x ref %08X %08X %08X %08X (iter %d)\\n", ent[q],')
o('                        ctx->gpr[ent[q]]._u32[0], ctx->gpr[ent[q]]._u32[1], ctx->gpr[ent[q]]._u32[2], ctx->gpr[ent[q]]._u32[3],')
o('                        ref->gpr[ent[q]]._u32[0], ref->gpr[ent[q]]._u32[1], ref->gpr[ent[q]]._u32[2], ref->gpr[ent[q]]._u32[3], feitas);')
o('                dif = 1; }')
o('        static volatile long nconf, ndif;')
o('        nconf++;')
o('        if (dif) {')
o(f'            if (ndif++ < 8) fprintf(stderr, "[vk] DIFERENÇA {mod} {fn}/{rot} (LS ou registradores), %d iterações -- núcleo desligado\\n", feitas);')
o('            memcpy(ctx->gpr, ref->gpr, sizeof ctx->gpr);   /* fica o resultado do código traduzido */')
o('            memcpy(ctx->ls_store, ref->ls_store, SPU_LS_SIZE);')
o('            liga = 0;')
o(f'        }} else if ((nconf & (nconf - 1)) == 0) fprintf(stderr, "[vk] {fn}/{rot}: %ld conferências iguais (última: %d iterações)\\n", nconf, feitas);')
o('        if (it_e > 4 && it_e <= 0x10000) {   /* a contagem bate com a saída de verdade? */')
o('            uint32_t tot = (uint32_t)feitas; int saiu = 0;')
o(f'            while (tot < it_e + 8) {{ {nome}_ref(ref, 1); tot++; if ({"" if desvio_nz else "!"}(ref->gpr[{reg_teste}]._u32[0] != 0) == 0) {{ saiu = 1; break; }} }}')
o('            static volatile long nconta;')
o('            if (!saiu || tot != it_e) {')
o(f'                if (nconta++ < 8) fprintf(stderr, "[vk] CONTAGEM {mod} {fn}/{rot}: fórmula %u, laço %u%s -- núcleo desligado\\n", it_e, tot, saiu ? "" : "+");')
o('                liga = 0;')
o('            }')
o('        }')
o('        free(ref);')
o('        return feitas;')
o('    }')
o('    static int est = -1; if (est < 0) est = getenv("GOWA_VK_STATS") ? 1 : 0;')
o('    if (est) {')
o(f'        static volatile long long nc, it_tot, it_feitas, hist[6], sai[3];')
o(f'        const uint32_t iters = {expr_iters};')
o(f'        int mt = 0; const int feitas = {nome}_corre(ctx, &mt);')
o('        nc++; it_tot += iters; it_feitas += feitas; sai[mt]++;')
o('        hist[iters <= 4 ? 0 : iters <= 8 ? 1 : iters <= 16 ? 2 : iters <= 32 ? 3 : iters <= 64 ? 4 : 5]++;')
o('        if ((nc & 0xFFFFF) == 0)')
o(f'            fprintf(stderr, "[vk] {fn}/{rot}: %lld chamadas, %.1f iter/chamada, %.0f%% feitas no núcleo; <=4:%lld <=8:%lld <=16:%lld <=32:%lld <=64:%lld mais:%lld; saídas: alcance %lld, conflito %lld\\n",')
o('                    nc, (double)it_tot / nc, 100.0 * it_feitas / (it_tot ? it_tot : 1), hist[0], hist[1], hist[2], hist[3], hist[4], hist[5], sai[1], sai[2]);')
o('        return feitas;')
o('    }')
o(f'    int mt = 0; return {nome}_corre(ctx, &mt);')
o('}')
saida = d / f'vk_{fn}_{rot}.inc'
saida.write_text('\n'.join(out) + '\n', encoding='latin-1')
print(f'{saida}: {N} comandos, {len(seq)} sequenciais, {nld} leituras, {nst} gravações; {len(cru)} contas de float cruas de {sum(1 for c in P if c["op"] in FLOAT)}; entregues {entregues}')
print('invariantes:', invariantes)
if '--gancho' in sys.argv:
    alvo = d / 'spu_recomp.c'
    s = alvo.read_text(encoding='latin-1')
    marca = f'/*VK {fn}/{rot}*/'
    if marca not in s:
        mf = re.search(r'^(?:static )?(?:SPU_PN )?void \w+_spu_func_' + fn + r'(?:__b)?\(spu_context\* ctx\) \{\n', s, re.M)
        p = s.find(f'loc_{rot}:', mf.end())
        corpo_f = s[mf.end():p]
        mdecl = re.search(r'^\s*u128 (_r\d+(?:, _r\d+)*);', corpo_f, re.M)
        if mdecl and 'RELOAD;' in corpo_f:
            # código promovido (promover_regs.py): registradores em locais _rN.
            # A contagem é testada com as locais; só então descarrega, roda e recarrega.
            locais = {int(x[2:]) for x in mdecl.group(1).split(', ')}
            pre = re.sub(r'ctx->gpr\[(\d+)\]', lambda mm: f'_r{mm.group(1)}' if int(mm.group(1)) in locais else mm.group(0), expr_iters)
            gancho = (f'        {marca} {{ extern int {nome}(spu_context*); const uint32_t _vi = {pre}; '
                      f'if (_vi > 4 && _vi <= 0x10000) {{ SPILL; if ({nome}(ctx)) RELOAD; }} }}\n')
        else:
            gancho = f'        {marca} {{ extern int {nome}(spu_context*); {nome}(ctx); }}\n'
        ini = s.rfind('\n', 0, p) + 1
        s = s[:ini] + gancho + s[ini:]
        if f'#include "vk_{fn}_{rot}.inc"' not in s:
            s = s.rstrip('\n') + f'\n#include "vk_{fn}_{rot}.inc"\n'
        alvo.write_text(s, encoding='latin-1')
        print('gancho inserido em', alvo)
