"""Funde cadeias de funções SPU promovidas (promover_regs.py) que se emendam por
transferência estática incondicional: X1 termina saltando para X2, X2 para X3... Cada
salto passava pelo trampolim -- grava os registradores escritos (SPILL), volta ao dreno,
chamada indireta, a próxima relê todos os que usa (RELOAD). Aqui as peças viram uma
função só (<X1>__m): locais = união, RELOAD = união dos usados (todo local começa com o
valor de ctx->gpr), SPILL = união dos escritos, e todo salto estático para uma peça da
cadeia (o da emenda, e também os condicionais/de volta) vira `goto`. Saídas para fora da
cadeia ficam como estão (com o SPILL da união).

As funções originais ficam (podem ser entradas de outros caminhos); só passam a usar a
fundida: o envoltório público de X1 e as chamadas diretas a X1__b. Backup em
<arquivo>.sem_fundir na primeira aplicação; cada aplicação parte dele.

uso:
  python fundir_cadeia.py port/spu/<mód> 4930,4AB0,4E90,4E9C,4F00 [outra,cadeia ...]
  python fundir_cadeia.py port/spu/<mód> --auto <rip.txt> <mapa> [--min N] [--max-linhas L]
      (as cadeias do acha_cadeias.py com pelo menos N amostras, padrão 100)
  python fundir_cadeia.py port/spu/<mód> --tirar
Exemplo (21/10): 0x727200, a rotina chamada 4x por volta do laço 0x63B4:
    python fundir_cadeia.py port/spu/gowajm_00727200 4930,4AB0,4E90,4E9C,4F00"""
import re
import shutil
import subprocess
import sys
from pathlib import Path

argv = sys.argv[1:]
d = Path(argv[0])
f = d / 'spu_recomp.c'
bak = f.with_name(f.name + '.sem_fundir')
if '--tirar' in argv:
    if bak.exists():
        shutil.copy2(bak, f)
        print('restaurado', f)
    sys.exit(0)
if not bak.exists():
    shutil.copy2(f, bak)
src = bak.read_bytes().decode('latin-1')
nl = '\r\n' if '\r\n' in src else '\n'
mod = d.name


def opt(nome, padrao):
    return type(padrao)(argv[argv.index(nome) + 1]) if nome in argv else padrao


if '--auto' in argv:
    i = argv.index('--auto')
    rip, mapa = argv[i + 1], argv[i + 2]
    minimo, max_linhas = opt('--min', 100), opt('--max-linhas', 4000)
    out = subprocess.run([sys.executable, '-I', str(Path(__file__).with_name('acha_cadeias.py')), str(d), rip, mapa, '--todas'],
                         capture_output=True, text=True, encoding='utf-8', errors='replace').stdout
    cadeias = []
    for ln in out.splitlines():
        m = re.match(r'\s*(\d+) amostras\s+\d+ \S+\s+(\d+) linhas\s+(.*)$', ln)
        if m and int(m.group(1)) >= minimo and int(m.group(2)) <= max_linhas:
            cadeias.append([int(x, 16) for x in m.group(3).split()])
else:
    cadeias = [[int(x, 16) for x in a.split(',')] for a in argv[1:] if not a.startswith('--')]


def peca(src, ea):
    nome = f'{mod}_spu_func_{ea:08X}'
    h = f'static SPU_PN void {nome}__b(spu_context* ctx) {{'
    i = src.index(h)
    a = src.rindex('#define SPILL', 0, i)
    r = src.index('#define RELOAD', a)
    spill = src[a:src.index(nl, a)]
    reload = src[r:src.index(nl, r)]
    assert src.index(nl, r) + len(nl) == i, f'{nome}: defines fora do lugar'
    assert 'promover_regs.py' in spill, f'{nome}: não promovida'
    fim = src.index(nl + '}' + nl, i)
    corpo = src[i + len(h) + len(nl):fim + len(nl)].split(nl)[:-1]
    assert corpo[0].strip().startswith('u128 ') and corpo[1].strip() == 'RELOAD;' and corpo[-1].strip() == 'SPILL;', nome
    escritos = re.findall(r'ctx->gpr\[(\d+)\] = _r\1;', spill)
    usados = re.findall(r'_r(\d+) = ctx->gpr\[\1\];', reload)
    return nome, [int(x) for x in escritos], [int(x) for x in usados], corpo[2:-1]


def fundir(src, cadeia):
    pecas = [peca(src, ea) for ea in cadeia]
    esc = sorted({r for p in pecas for r in p[1]})
    uso = sorted({r for p in pecas for r in p[2]} | set(esc))
    nome0 = pecas[0][0]
    fund = f'{nome0}__m'
    if f'void {fund}(' in src:
        return src, f'{fund}: já existe, pulada'
    membros = {ea: f'{mod}_spu_func_{ea:08X}' for ea in cadeia}
    n_goto = 0

    def troca(m):
        nonlocal n_goto
        alvo = int(m.group(1), 16)
        if alvo in membros and m.group(2) == membros[alvo]:
            n_goto += 1
            return f'goto m_{alvo:08X};'
        return m.group(0)
    linhas = []
    for k, (nome, _e, _u, corpo) in enumerate(pecas):
        linhas.append(f'm_{cadeia[k]:08X}:;')
        for c in corpo:
            linhas.append(re.sub(r'\{ ctx->pc = 0x([0-9A-F]+); g_spu_trampoline_fn = (\w+); \{ SPILL; return; \} \}', troca, c))
    txt = ['#define SPILL do { ' + ' '.join(f'ctx->gpr[{r}] = _r{r};' for r in esc) + ' } while (0) /* fundir_cadeia.py */',
           '#define RELOAD do { ' + ' '.join(f'_r{r} = ctx->gpr[{r}];' for r in uso) + ' } while (0)',
           f'static SPU_PN void {fund}(spu_context* ctx) {{',
           '        u128 ' + ', '.join(f'_r{r}' for r in uso) + ';',
           '        RELOAD;'] + linhas + ['        SPILL;', '}', '#undef SPILL', '#undef RELOAD', '']
    h0 = f'static SPU_PN void {nome0}__b(spu_context* ctx) {{'
    i0 = src.rindex('#define SPILL', 0, src.index(h0))
    novo = src[:i0] + nl.join(txt) + nl + src[i0:]
    n_cham = novo.count(f'{nome0}__b(ctx);')
    novo = novo.replace(f'{nome0}__b(ctx);', f'{fund}(ctx);')
    n_cham += novo.count(f', {nome0}__b, {nome0})')   # SPU_TAILX (encadeia.py)
    novo = novo.replace(f', {nome0}__b, {nome0})', f', {fund}, {nome0})')
    assert h0 in novo
    k = novo.index('#include "spu_recomp.h"')
    k = novo.index(nl, k) + len(nl)
    novo = novo[:k] + f'static SPU_PN void {fund}(spu_context* ctx);' + nl + novo[k:]
    return novo, (f'{fund}: {len(pecas)} peças, {sum(len(p[3]) for p in pecas)} linhas, {n_goto} saltos -> goto, '
                  f'{len(uso)} locais, {n_cham} usos trocados')


tot = 0
for c in cadeias:
    src, msg = fundir(src, c)
    print(msg)
    tot += 1
f.write_bytes(src.encode('latin-1'))
print(f'{tot} cadeias em {f}')
