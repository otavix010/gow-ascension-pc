"""Achatamento preguiçoso dos resultados de float da SPU (código traduzido).

Cada fa/fs/fm/fma/fms/fnms traduzido termina achatando o resultado: expoente 0
(denormal ou zero de qualquer sinal) vira +0, como a SPU faz. Para outra conta
de float isso não muda nada: com MXCSR.DAZ o hardware lê um denormal como zero,
e o sinal de um zero de entrada nunca chega ao resultado (todo resultado zero
sai +0 de novo); as comparações fcgt/fceq/fcmgt/fcmeq tratam expoente 0 como
zero. O achatamento só importa quando o valor SAI do float: gravação na LS,
operação de bits, shufb, conversão, frest/frsqest/fi, desvio, chamada, retorno.

Este script faz definições que alcançam cada uso (reaching definitions) no
grafo de cada função e troca spu_fm(...) por spu_fm_r(...) (resultado cru)
nas definições cujos usos alcançados são TODOS operandos de conta de float.

uso: python flush_preguicoso.py <dir do módulo> [--aplicar] [--fn HEX]
  sem --aplicar: só conta (quantas definições de float podem ficar cruas)
  --aplicar: grava spu_recomp.c (backup .sem_flushpreg na primeira vez)"""
import re
import shutil
import sys
from pathlib import Path

FLOAT = ('fa', 'fs', 'fm', 'fma', 'fms', 'fnms')
SEGUROS = FLOAT + ('fcgt', 'fceq', 'fcmgt', 'fcmeq')   # consumidores que aceitam valor cru
COPIAS_IMM = ('shlqbyi', 'rotqbyi', 'shlqbii', 'rotqbii', 'ori', 'ai', 'ahi', 'xori')  # op x, 0
COPIAS_XX = ('or', 'and')                                                         # op x, x
RE_FUNC = re.compile(r'^(?:static )?(?:SPU_PN )?void (\w+_spu_func_[0-9A-F]{8}(?:__b)?)\(spu_context\* ctx\) \{\n', re.M)
# registrador: ctx->gpr[N] (id N) ou, nos módulos com promoção, a local _rN
# (id 128+N -- são armazenamentos diferentes; a cópia entre eles é uso comum)
RE_REG = re.compile(r'ctx->gpr\[(\d+)\]|\b_r(\d+)\b')
RE_DEF = re.compile(r'^(ctx->gpr\[\d+\]|_r\d+) = (spu_(\w+)\((.*)\));$')
RE_UM = re.compile(r'(?:ctx->gpr\[(\d+)\]|_r(\d+))')


def rid(m):
    return int(m.group(1)) if m.group(1) is not None else 128 + int(m.group(2))


def regs(texto):
    return [rid(m) for m in RE_REG.finditer(texto)]
RE_LABEL = re.compile(r'^(loc_[0-9A-F]+):$')
RE_GOTO = re.compile(r'goto (loc_[0-9A-F]+);')


def args_de(chamada):
    """argumentos de nível superior de 'spu_op(a, b, c)'"""
    i = chamada.index('(')
    corpo = chamada[i + 1:-1]
    out, prof, ini = [], 0, 0
    for k, ch in enumerate(corpo):
        if ch == '(':
            prof += 1
        elif ch == ')':
            prof -= 1
        elif ch == ',' and prof == 0:
            out.append(corpo[ini:k].strip())
            ini = k + 1
    out.append(corpo[ini:].strip())
    return out


def analisar(linhas):
    """linhas: corpo da função (lista de str). Devolve o conjunto de índices de
    linha cujas definições de float podem ficar cruas, e o total de definições."""
    # 1) comandos: (idx, defs, usos_seguros, usos_inseguros, sucessores, escapa)
    n = len(linhas)
    rot = {}
    for i, l in enumerate(linhas):
        m = RE_LABEL.match(l.strip())
        if m:
            rot[m.group(1)] = i
    defs = [None] * n          # registrador definido (int) ou None
    seg = [()] * n             # registradores usados como operando seguro
    inseg = [()] * n           # registradores usados de outro jeito
    todos = [False] * n        # usa TODOS os registradores (chamada, retorno, desvio indireto)
    succ = [()] * n
    eh_float = [False] * n
    copia = [None] * n         # registrador de origem de uma cópia transparente
    for i, l in enumerate(linhas):
        t = l.strip()
        prox = (i + 1,)
        if not t or t.startswith('/*') or t.startswith('#') or RE_LABEL.match(t):
            succ[i] = prox
            continue
        m = RE_DEF.match(t)
        if m and '{' not in t:
            r, op = rid(RE_UM.fullmatch(m.group(1))), m.group(3)
            defs[i] = r
            a = args_de(m.group(2))
            # cópia de registrador (shlqbyi x,0 / ori x,0 / ai x,0 / or x,x ...):
            # transparente -- o valor cru segue cru se o destino só for usado em float
            if (op in COPIAS_IMM and len(a) == 2 and a[1] == '0' and RE_UM.fullmatch(a[0])) or \
                    (op in COPIAS_XX and len(a) == 2 and a[0] == a[1] and RE_UM.fullmatch(a[0])):
                copia[i] = rid(RE_UM.fullmatch(a[0]))
                succ[i] = prox
                continue
            # conversão para inteiro com escala 2^(173-I8) <= 2^126: um denormal
            # (|x| < 2^-126) ou -0 dá 0 do mesmo jeito que o +0
            if op in ('cflts', 'cfltu') and len(a) == 2 and RE_UM.fullmatch(a[0]) and \
                    re.fullmatch(r'\d+', a[1]) and int(a[1]) >= 47:
                seg[i] = (rid(RE_UM.fullmatch(a[0])),)
                succ[i] = prox
                continue
            if op in SEGUROS:
                s, u = [], []
                for x in a:
                    mm = RE_UM.fullmatch(x)
                    if mm:
                        s.append(rid(mm))
                    else:
                        u += regs(x)
                seg[i], inseg[i] = tuple(s), tuple(u)
                eh_float[i] = op in FLOAT
            else:
                inseg[i] = tuple(regs(m.group(2)))
            succ[i] = prox
            continue
        # qualquer outro comando: todos os registradores citados são uso inseguro
        inseg[i] = tuple(regs(t))
        gotos = RE_GOTO.findall(t)
        incond = t.startswith('goto ')
        if 'return;' in t or 'SPU_RET' in t or '(ctx);' in t or 'spu_drain_call' in t \
                or 'spu_indirect' in t or 'longjmp' in t or 'spu_halt' in t:
            todos[i] = True
        alvo = tuple(rot[g] for g in gotos if g in rot)
        if any(g not in rot for g in gotos):
            todos[i] = True
        if incond:
            succ[i] = alvo
        elif 'return;' in t and not t.startswith('if') and not t.startswith('{'):
            succ[i] = ()
        else:
            succ[i] = alvo + prox
        # definições dentro de comandos compostos (chamadas: ctx->gpr[0] = spu_link(...); {...})
        md = re.match(r'^(?:ctx->gpr\[(\d+)\]|_r(\d+)) = ', t)
        if md:
            defs[i] = rid(md)
    # 2) para cada registrador, "uso inseguro alcançável a partir daqui antes de
    #    redefinição" = vivo-inseguro (análise para trás, como liveness).
    #    liv[i] = conjunto de regs cujo valor atual, na entrada do comando i, tem
    #    algum uso inseguro adiante (antes de ser redefinido).
    TODOS = frozenset(range(256))
    # nó n = saída da função (cair do fim devolve ao chamador): tudo vivo
    for i in range(n):
        succ[i] = tuple(s if s < n else n for s in succ[i])
    liv_in = [frozenset()] * n + [TODOS]
    pred = [[] for _ in range(n)]
    for i in range(n):
        for s in succ[i]:
            if s < n:
                pred[s].append(i)
    trabalho = list(range(n - 1, -1, -1))
    na_fila = [True] * n
    while trabalho:
        i = trabalho.pop()
        na_fila[i] = False
        out = frozenset()
        for s in succ[i]:
            out |= liv_in[s]
        if todos[i]:
            novo = TODOS
        else:
            novo = out
            if defs[i] is not None:
                novo = novo - {defs[i]}
            novo = novo | frozenset(inseg[i])
            if copia[i] is not None and defs[i] in out:
                novo = novo | {copia[i]}
        if novo != liv_in[i]:
            liv_in[i] = novo
            for p in pred[i]:
                if not na_fila[p]:
                    na_fila[p] = True
                    trabalho.append(p)
    # 3) uma definição de float pode ficar crua se o registrador não está
    #    vivo-inseguro na SAÍDA do comando
    crus = set()
    total = 0
    for i in range(n):
        if not eh_float[i]:
            continue
        total += 1
        out = frozenset()
        for s in succ[i]:
            out |= liv_in[s]
        if defs[i] not in out:
            crus.add(i)
    return crus, total


def main():
    d = Path(sys.argv[1])
    so_fn = sys.argv[sys.argv.index('--fn') + 1].upper().rjust(8, '0') if '--fn' in sys.argv else None
    arq = d / 'spu_recomp.c'
    src = arq.read_text(encoding='latin-1')
    partes, pos = [], 0
    tot_def = tot_cru = 0
    for m in RE_FUNC.finditer(src):
        ini = m.end()
        fim = src.find('\n}\n', ini)
        if fim < 0:
            continue
        partes.append(src[pos:ini])
        corpo = src[ini:fim]
        if so_fn and so_fn not in m.group(1):
            partes.append(corpo)
            pos = fim
            continue
        linhas = corpo.split('\n')
        crus, total = analisar(linhas)
        tot_def += total
        tot_cru += len(crus)
        if so_fn:
            print(f'{m.group(1)}: {len(crus)}/{total} definições de float cruas')
        for i in crus:
            linhas[i] = re.sub(r'= spu_(fa|fs|fm|fma|fms|fnms)\(', r'= spu_\1_r(', linhas[i], count=1)
        partes.append('\n'.join(linhas))
        pos = fim
    partes.append(src[pos:])
    print(f'{d.name}: {tot_cru}/{tot_def} definições de float podem ficar cruas '
          f'({100.0 * tot_cru / max(tot_def, 1):.0f}%)')
    if '--aplicar' in sys.argv:
        bak = arq.with_name(arq.name + '.sem_flushpreg')
        if not bak.exists():
            shutil.copy2(arq, bak)
        arq.write_text(''.join(partes), encoding='latin-1')
        print('gravado', arq)


if __name__ == '__main__':
    main()
