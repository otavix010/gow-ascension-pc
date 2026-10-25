"""Estágios do software pipelining de um laço SPU traduzido (um só bloco: o
rótulo desvia para si mesmo). Para cada comando do corpo, o estágio = quantas
iterações atrás foi lido (da LS) o item que ele processa: um valor calculado
na mesma iteração a partir das leituras está no estágio 0; um valor que usa um
registrador carregado da iteração anterior herda o estágio dele + 1.

Por ponto de definição (os registradores são reaproveitados):
  - indução: r = ai(x, imm) / cópia de x, com x vindo de outra indução
    (ponteiros += 96, índice += 16, contador += 1) -> estágio '-';
  - invariantes (sem definição no corpo) -> '-';
  - recorrências (acumuladores: dependem de si mesmos da iteração anterior)
    -> 'REC', estágio calculado só pelos outros operandos.

uso: python estagios_laco.py <dir do módulo> <função hex> <rótulo hex> [--fonte arquivo]"""
import re
import sys
from pathlib import Path

d, fn, rot = Path(sys.argv[1]), sys.argv[2].upper().rjust(8, '0'), sys.argv[3].upper().rjust(8, '0')
arq = sys.argv[sys.argv.index('--fonte') + 1] if '--fonte' in sys.argv else d / 'spu_recomp.c'
src = Path(arq).read_text(encoding='latin-1')
m = re.search(r'^(?:static )?(?:SPU_PN )?void \w+_spu_func_' + fn + r'(?:__b)?\(spu_context\* ctx\) \{\n', src, re.M)
corpo = src[m.end():src.find('\n}\n', m.end())]
i = corpo.find(f'loc_{rot}:')
j = corpo.find(f'goto loc_{rot};', i)
fim = corpo.find('\n', j)
cmds = [t.strip() for t in corpo[i:fim].split('\n')[1:] if t.strip() and not t.strip().startswith('/*')]
R = re.compile(r'ctx->gpr\[(\d+)\]')


def parse(t):
    mm = re.match(r'ctx->gpr\[(\d+)\] = (.*);$', t)
    if mm:
        return int(mm.group(1)), [int(x) for x in R.findall(mm.group(2))], mm.group(2)
    return None, [int(x) for x in R.findall(t)], t


P = [parse(t) for t in cmds]
N = len(P)
ultima = {}
for k, (w, us, _) in enumerate(P):
    if w is not None:
        ultima[w] = k
# definição que alcança cada uso: ('c', k) no corpo antes, ('p', k) da iteração anterior, None invariante
alcanca = []
cur = {}
for k, (w, us, _) in enumerate(P):
    a = []
    for u in us:
        if u in cur:
            a.append(('c', cur[u]))
        elif u in ultima:
            a.append(('p', ultima[u]))
        else:
            a.append(None)
    alcanca.append(a)
    if w is not None:
        cur[w] = k
# indução por ponto de definição (ponto fixo decrescente)
cand = set()
for k, (w, us, rhs) in enumerate(P):
    if w is not None and (re.match(r'spu_ai\(ctx->gpr\[\d+\], -?\d+\)$', rhs) or
                          re.match(r'spu_(ori|shlqbyi|rotqbyi)\(ctx->gpr\[\d+\], 0\)$', rhs)):
        cand.add(k)
mudou = True
while mudou:
    mudou = False
    for k in list(cand):
        a = alcanca[k][0]
        if a is None or a[1] not in cand:
            cand.discard(k)
            mudou = True
inducao = cand
# dependência de cada comando (ignorando indução e invariantes)
dep = []
for k in range(N):
    dep.append([a for a in alcanca[k] if a is not None and a[1] not in inducao])
# recorrência: k alcança a si mesmo por um caminho que cruza a borda do laço
def chega(k):
    vistos, pilha = set(), [(k, False)]
    while pilha:
        x, cruzou = pilha.pop()
        for tipo, y in dep[x]:
            c2 = cruzou or tipo == 'p'
            if y == k and c2:
                return True
            if (y, c2) not in vistos:
                vistos.add((y, c2))
                pilha.append((y, c2))
    return False
recorr = {k for k in range(N) if k not in inducao and chega(k)}
estagio = [None] * N
for passo in range(8):
    for k, (w, us, rhs) in enumerate(P):
        if k in inducao:
            continue
        st = 0 if rhs.startswith('spu_ls_read128') else None
        for tipo, y in dep[k]:
            if k in recorr and y in recorr:
                continue
            s = estagio[y]
            if s is None:
                continue
            s = s + (1 if tipo == 'p' else 0)
            st = s if st is None else max(st, s)
        estagio[k] = st
print(f'laço loc_{rot}: {N} comandos; {len(inducao)} de indução, {len(recorr)} em recorrências')
for k in range(N):
    marca = 'REC' if k in recorr else ('ind' if k in inducao else '   ')
    s = '-' if estagio[k] is None else str(estagio[k])
    txt = re.sub(r'ctx->gpr\[(\d+)\]', r'r\1', cmds[k]).replace('._u32[0]', '')
    print(f'{s:>2} {marca} {txt[:120]}')
