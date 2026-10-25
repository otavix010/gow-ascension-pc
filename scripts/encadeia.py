"""Cadeias de funções de uma instrução (21/10): em vários trechos o tradutor partiu código reto
em uma função por instrução (os ajudantes de DMA com interrupção desligada do gerenciador de
jobs, 0x4030.. no 0x72DF80; laços da tarefa MeshProc), cada uma passando para a seguinte pelo
TRAMPOLIM -- um passo do dreno por instrução. Este passe troca, só nesses pontos, a
transferência pelo trampolim por um salto direto guardado:

    { ctx->pc = Y; g_spu_trampoline_fn = F_Y; return; }
 -> { ctx->pc = Y; if (spu_tail_ok(ctx, Y)) { F_Y__b(ctx); return; } g_spu_trampoline_fn = F_Y; return; }

spu_tail_ok (spu_context.h) devolve 0 com orçamento do passo esgotado (GOWA_SPU_TAIL, 32),
QUALQUER evento pendente (a entrega de interrupção e a contagem do atraso dela continuam no
dreno), destino na LS 0 ou entrada de troca de pilha -- aí segue o trampolim, como antes.
Pontos trocados:
  (a) transferência para Y = X + 4 dentro de uma função "elo" (X, até 3 comandos);
  (b) transferência para um elo, de qualquer função do módulo;
  (d) com --alvos: transferência para um dos destinos quentes da lista;
  (c) o idioma de liga/desliga interrupção `brsl $R, X` + (X: `ai $R,$R,8 ; bie/bid $R`): a
      chamada vira o efeito em linha (R += 8, int_enable) e o salto direto para X + 8.
Não toca código promovido (SPILL) nem outras transferências (o experimento de 18/10 com
todas as 29 mil pesou no cache de instruções).

uso: python encadeia.py <dir do módulo> aplicar|tirar|contar
     aplicar: backup em spu_recomp.c.sem_encadeia (uma vez); GOWA_SPU_TAIL=0 desliga em tempo
     de execução (spu_tail_ok devolve 0)."""
import re
import shutil
import sys
from pathlib import Path

d = Path(sys.argv[1])
modo = sys.argv[2]
# todas (21/10): TODA transferência estática do módulo vira SPU_TAILX (spu_context.h: só o
# orçamento do passo; os produtores de evento zeram o orçamento) -- o idioma de interrupção
# também; o destino LS 0 (fim de job) fica no trampolim
TODAS = modo == 'todas'
if TODAS:
    modo = 'aplicar'
# --alvos arq: nomes de função (um por linha) -- os destinos mais frequentes do trampolim
# (GOWA_TRAMP_STATS + tramp_nomes.py); toda transferência estática para eles também vira salto guardado
ALVOS = set()
if '--alvos' in sys.argv:
    ALVOS = {l.strip() for l in open(sys.argv[sys.argv.index('--alvos') + 1], encoding='utf-8') if l.strip()}
F = d / 'spu_recomp.c'
BAK = d / 'spu_recomp.c.sem_encadeia'
if modo == 'tirar':
    shutil.copy(BAK, F)          # data nova: o ninja recompila
    print('restaurado de', BAK)
    sys.exit(0)

s = F.read_text(encoding='latin-1')
nl = '\r\n' if '\r\n' in s else '\n'
mod = d.name
RF = re.compile(r'^(static SPU_PN )?void (' + mod + r'_spu_func_([0-9A-F]{8}))(__b)?\(spu_context\* ctx\) \{\r?\n', re.M)
funcs = {}       # endereço -> dict(nome, b (tem __b), ini, fim, corpo)
for m in RF.finditer(s):
    a = int(m.group(3), 16)
    fim = s.find(nl + '}' + nl, m.end())
    corpo = s[m.end():fim]
    ent = funcs.setdefault(a, dict(nome=m.group(2)))
    if m.group(4) or not m.group(1):
        # o corpo de verdade: a forma __b (SPU_PN) quando existe; a pública é só o embrulho
        if m.group(4) or 'b' not in ent:
            ent['corpo'] = corpo
            ent['ini'] = m.end()
            ent['fim'] = fim
            ent['b'] = bool(m.group(4))
RT = re.compile(r'\{ ctx->pc = 0x([0-9A-F]+); g_spu_trampoline_fn = (' + mod + r'_spu_func_([0-9A-F]{8})); return; \}')
RTS = re.compile(r'\{ ctx->pc = 0x([0-9A-F]+); g_spu_trampoline_fn = (' + mod + r'_spu_func_([0-9A-F]{8})); \{ SPILL; return; \} \}')
SPILL_TAMBEM = '--spill' in sys.argv


def comandos(corpo):
    out = []
    for t in corpo.splitlines():
        t = t.strip()
        if not t or t.startswith('/*'):
            continue
        out.append(t)
        if t.endswith('return; }') or t.endswith('return;'):
            break          # o resto é código morto depois do primeiro desvio incondicional
    return out


elo = set()
for a, f in funcs.items():
    if 'corpo' not in f:
        continue
    cs = comandos(f['corpo'])
    if 1 <= len(cs) <= 3:
        mm = RT.search(cs[-1])
        if mm and int(mm.group(1), 16) == a + 4 and int(mm.group(3), 16) == a + 4:
            elo.add(a)
# idioma de interrupção: X = { R += 8; int_enable = E; pc = R; indireto }
RI = re.compile(r'^ctx->gpr\[(\d+)\] = spu_ai\(ctx->gpr\[\1\], 8\);$')
RIE = re.compile(r'^ctx->int_enable = ([01]); ctx->pc = ctx->gpr\[(\d+)\]\._u32\[0\]; g_spu_trampoline_fn = spu_indirect_branch; return;$')
idioma = {}
for a, f in funcs.items():
    if 'corpo' not in f:
        continue
    cs = comandos(f['corpo'])
    if len(cs) == 2:
        m1, m2 = RI.match(cs[0]), RIE.match(cs[1])
        if m1 and m2 and m1.group(1) == m2.group(2) and (a + 8) in funcs:
            idioma[a] = (int(m1.group(1)), m2.group(1))


def alvo_b(y):
    f = funcs[y]
    return f['nome'] + ('__b' if f.get('b') else '')


def troca_simples(y):
    if TODAS:
        return f'SPU_TAILX(ctx, 0x{y:X}u, {alvo_b(y)}, {funcs[y]["nome"]});'
    return (f'{{ ctx->pc = 0x{y:X}; if (spu_tail_ok(ctx, 0x{y:X}u)) {{ {alvo_b(y)}(ctx); return; }} '
            f'g_spu_trampoline_fn = {funcs[y]["nome"]}; return; }}')


novo = []
pos = 0
n_a = n_b = n_c = n_d = 0
# percorre as funções em ordem de texto e troca dentro do corpo de cada uma
for a, f in sorted(funcs.items(), key=lambda kv: kv[1].get('ini', 0)):
    if 'corpo' not in f:
        continue
    corpo = f['corpo']
    eu_elo = a in elo

    def sub(mm):
        global n_a, n_b, n_d
        y = int(mm.group(1), 16)
        if int(mm.group(3), 16) != y or y not in funcs or 'corpo' not in funcs[y]:
            return mm.group(0)
        if eu_elo and y == a + 4:
            n_a += 1
            return troca_simples(y)
        if y in elo:
            n_b += 1
            return troca_simples(y)
        if funcs[y]['nome'] in ALVOS:
            n_d += 1
            return troca_simples(y)
        if TODAS and y != 0:
            n_d += 1
            return troca_simples(y)
        return mm.group(0)
    # (c) chamada do idioma de interrupção: "ctx->gpr[R] = spu_link(X); { ctx->pc = X; tramp = F_X; return; }"
    def sub_i(mm):
        global n_c
        r, x = int(mm.group(1)), int(mm.group(2), 16)
        if x not in idioma or idioma[x][0] != r or int(mm.group(3), 16) != x:
            return mm.group(0)
        e = idioma[x][1]
        y = x + 8
        n_c += 1
        return (f'ctx->gpr[{r}] = spu_link(0x{x:X}); ctx->gpr[{r}] = spu_ai(ctx->gpr[{r}], 8); ctx->int_enable = {e}; '
                + troca_simples(y))
    c2 = re.sub(r'ctx->gpr\[(\d+)\] = spu_link\(0x([0-9A-F]+)\); \{ ctx->pc = 0x\2; g_spu_trampoline_fn = ' + mod +
                r'_spu_func_([0-9A-F]{8}); return; \}', sub_i, corpo)
    c2 = RT.sub(sub, c2)
    # --spill (22/10): também a forma das funções promovidas (promover_regs.py), que grava os
    # registradores antes de voltar ao dreno -- o salto enxuto vem depois do SPILL
    if TODAS and SPILL_TAMBEM:
        def sub_s(mm):
            global n_d
            y = int(mm.group(1), 16)
            if int(mm.group(3), 16) != y or y not in funcs or 'corpo' not in funcs[y] or y == 0:
                return mm.group(0)
            n_d += 1
            return '{ SPILL; ' + troca_simples(y) + ' }'
        c2 = RTS.sub(sub_s, c2)
    if c2 != corpo:
        novo.append(s[pos:f['ini']])
        novo.append(c2)
        pos = f['fim']
novo.append(s[pos:])
print(f'{mod}: {len(elo)} elos, {len(idioma)} ajudantes de interrupção; trocas: (a) {n_a}, (b) {n_b}, (c) {n_c}, (d) {n_d}')
if modo == 'aplicar':
    if not BAK.exists():
        shutil.copy2(F, BAK)
    F.write_text(''.join(novo), encoding='latin-1')
    print('aplicado; backup', BAK.name)
