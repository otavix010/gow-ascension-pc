"""Registradores vivos num ponto de uma função traduzida do job (análise de vivacidade para trás, entre funções):
quais registradores alguém ainda lê depois de um rótulo -- o que um nativo de bloco precisa deixar como o original.
Modelo (conservador):
  - chamada direta de outra função do módulo: lê os vivos na entrada dela (calculados do mesmo jeito) e não mata
    nenhum registrador; chamada de cauda (SPU_TAILX): sai lendo os vivos na entrada do destino;
  - SPU_RET: lê r0, r1, r3, r4 e r80-r127 (retorno e preservados); desvio indireto, LUZGPU/NINHO e o que não se
    entende: lê todos;
  - os ganchos dos nativos (if (nativo(ctx)) goto ...) são desvios sem leitura: quando devolvem 0 o traduzido lê.
  - --ind=<função>: os desvios indiretos com volta (o ponteiro do descarregamento, r82+12 etc.) contam como chamada
    dessa função -- quem usa o resultado confere o ponteiro em tempo de execução.
  - --abi: os desvios indiretos que sobram seguem a ABI da SPU: chamada com volta lê r0-r10 e r80-r127; salto
    indireto sem volta (bi rN) é um retorno (lê o de SPU_RET e rN). Sem --abi leem todos.
uso: python -I vivos.py [--ind=<função>] [--abi] <spu_recomp.c> <função> <rótulo | +linha do arquivo> [...]"""
import re
import sys

TODOS = frozenset(range(128))
RET = frozenset([0, 1, 3, 4] + list(range(80, 128)))

IND, ABI = None, False
while sys.argv[1].startswith('--'):
    op = sys.argv.pop(1)
    if op.startswith('--ind='):
        IND = op[6:]
    elif op == '--abi':
        ABI = True
src = sys.argv[1]
texto = open(src, encoding='latin-1').read().split('\n')
funcs = {}
k = 0
while k < len(texto):
    m = re.match(r'^static SPU_PN void (\w+)\(spu_context\* ctx\) \{$', texto[k])
    if m:
        ini = k + 1
        while texto[k] != '}':
            k += 1
        funcs[m.group(1)] = (ini, k)   # linhas [ini, k) do arquivo, base 0
    k += 1


def pedacos(t):
    """divide uma linha em comandos no ';' de nível 0; um bloco {...} é um comando só"""
    out, prof, cur = [], 0, ''
    for ch in t:
        cur += ch
        if ch in '({':
            prof += 1
        elif ch in ')}':
            prof -= 1
            if ch == '}' and prof == 0:
                out.append(cur.strip()); cur = ''
        elif ch == ';' and prof == 0:
            out.append(cur.strip()); cur = ''
    if cur.strip():
        out.append(cur.strip())
    return [p.rstrip(';').strip() for p in out if p.strip(' ;')]


def regs(t):
    return set(int(r) for r in re.findall(r'ctx->gpr\[(\d+)\]', t))


memo, andamento = {}, set()


def vivos_entrada(fn):
    if fn in memo:
        return memo[fn]
    if fn in andamento or fn not in funcs:
        return TODOS
    r = frozenset(analisa(fn)[0][0])
    memo[fn] = r
    return r


def classifica(p):
    """-> lista de (usa, define, sucessores ('seg' = próximo, rótulo), saída lida ou None)"""
    m = re.match(r'^if \((.*)\) (.*)$', p, re.S)
    if m and m.group(1).count('(') == m.group(1).count(')'):
        cond, acao = m.group(1), m.group(2).strip()
        if acao.startswith('{') and acao.endswith('}'):
            acao = acao[1:-1].strip().rstrip(';')
        u = regs(cond) if not re.search(r'\w+_nativo|\bll_\w+|\bfl_\w+', cond) else set()
        sub = classifica(acao)
        res = []
        for (uu, dd, ss, ex) in sub:
            res.append((u | uu, set(), ss + ['seg'], ex))   # condicional: não mata nada, segue também
        return res if res else [(u, set(), ['seg'], None)]
    m = re.match(r'^goto (\w+)$', p)
    if m:
        return [(set(), set(), [m.group(1)], None)]
    m = re.match(r'^SPU_TAILX\(ctx, 0x\w+u, (\w+), \w+\)$', p)
    if m:
        return [(set(), set(), [], vivos_entrada(m.group(1)))]
    if p.startswith('SPU_RET(ctx)') or p == '{ SPU_RET(ctx); }':
        return [(set(), set(), [], RET)]
    m = re.match(r'^ctx->pc = ctx->gpr\[(\d+)\]\._u32\[0\]$', p)
    if m and ABI:
        return [(set(), set(), ['seg'], RET | {int(m.group(1))})]   # o 'return' seguinte fecha
    if '_tg' in p and IND and andamento_ordem and andamento_ordem[-1] == PRINCIPAL:
        return [(set(vivos_entrada(IND)), set(), ['seg'], None)]
    if '_tg' in p and ABI:
        return [(set(range(11)) | set(range(80, 128)), set(), ['seg'], None)]
    if '_tg' in p or 'LUZGPU' in p or 'NINHO' in p:
        return [(set(TODOS), set(), ['seg'], None)]
    if p == 'g_spu_trampoline_fn = spu_indirect_branch' and ABI:
        return [(set(), set(), ['seg'], None)]
    if 'spu_indirect_branch' in p or 'SPU_RET_REG' in p:
        return [(set(), set(), [], TODOS)]
    m = re.search(r'(\w+__b)\(ctx\)', p)
    if m and p.startswith('{'):
        return [(set(vivos_entrada(m.group(1))), set(), ['seg'], None)]
    m = re.match(r'^ctx->gpr\[(\d+)\] = (.*)$', p, re.S)
    if m:
        return [(regs(m.group(2)), {int(m.group(1))}, ['seg'], None)]
    if p.startswith(('/*', 'ctx->int_enable', 'ctx->pc')) or p in ('', ';'):
        return [(set(), set(), ['seg'], None)]
    if p == 'return' and ABI:
        return [(set(), set(), [], None)]
    if 'return' in p:
        return [(set(), set(), [], TODOS)]
    return [(regs(p), set(), ['seg'], None)]


cache = {}
andamento_ordem = []   # pilha das funções em análise; [0] é a pedida
PRINCIPAL = sys.argv[2] if len(sys.argv) > 2 else None
BIE = re.compile(r'^(?:if \(.*\) \{ )?ctx->int_enable = [01]; ctx->pc = ctx->gpr\[2\]\._u32\[0\]; g_spu_trampoline_fn = spu_indirect_branch; return;(?: \})?$')


def analisa(fn):
    """-> (vivos na entrada de cada comando, comandos, rótulo -> índice, linha do arquivo de cada comando)"""
    if fn in cache:
        return cache[fn]
    ini, fim = funcs[fn]
    andamento.add(fn)
    andamento_ordem.append(fn)
    cmds, rot, lin = [], {}, []
    for n in range(ini, fim):
        t = texto[n].strip()
        m = re.match(r'^(\w+):\s*(?:;)?\s*(/\*.*\*/)?$', t)
        if m:
            rot[m.group(1)] = len(cmds)
            continue
        if not t or t.startswith('/*'):
            continue
        if BIE.match(t) and len(cmds) == 1 and cmds[0][1] == {2} and cmds[0][0] == {2}:
            # bie/bid r2 logo depois do 'ai r2, r2, 8' da entrada de uma função alcançada por brsl r2: o destino é a
            # própria função + 8 (idioma de ligar/desligar interrupções)
            m = re.match(r'(\w+_func_)([0-9A-F]{8})(__b)$', fn)
            alvo = f'{m.group(1)}{int(m.group(2), 16) + 8:08X}{m.group(3)}' if m else None
            if alvo in funcs:
                cond = re.match(r'^if \((.*?)\) \{', t)
                u = regs(cond.group(1)) if cond else set()
                cmds.append((u, set(), ['seg'] if cond else [], vivos_entrada(alvo)))
                lin.append(n + 1)
                continue
        if 'HALT-ASSERT' in t:
            # asserção de halt: a SPU para, nenhum registrador é lido depois
            cond = re.match(r'^if \((.*?)\) \{', t)
            cmds.append((regs(cond.group(1)) if cond else set(), set(), ['seg'], None))
            lin.append(n + 1)
            continue
        for p in pedacos(t):
            for c in classifica(p):
                cmds.append(c)
                lin.append(n + 1)
    N = len(cmds)
    viv = [set() for _ in range(N + 1)]   # viv[N] = cair do fim (não acontece)
    mudou = True
    while mudou:
        mudou = False
        for i in range(N - 1, -1, -1):
            u, d, ss, ex = cmds[i]
            out = set(ex) if ex else set()
            for s in ss:
                j = i + 1 if s == 'seg' else rot.get(s)
                out |= viv[j] if j is not None else TODOS
            novo = u | (out - d)
            if novo != viv[i]:
                viv[i] = novo; mudou = True
    r = (viv, cmds, rot, lin)
    andamento_ordem.pop()
    andamento.discard(fn)
    cache[fn] = r
    return r


if __name__ == '__main__':
    fn = sys.argv[2]
    viv, cmds, rot, lin = analisa(fn)
    for alvo in sys.argv[3:]:
        if alvo.startswith('+'):
            n = int(alvo[1:])
            i = next(i for i, l in enumerate(lin) if l >= n)
        else:
            i = rot[alvo]
        v = sorted(viv[i])
        print(f'{alvo}: {len(v)} vivos: {v}')
