"""Promove a locais os registradores de UMA função traduzida que tem chamadas, sincronizando
só o necessário em volta de cada chamada a uma função de conjuntos conhecidos (uma fundida
do fundir_cadeia.py ou uma promovida: os #define RELOAD/SPILL dela dizem o que lê e o que
escreve). O promover_regs.py deixa de fora toda função com chamada -- recarregar todos os
registradores depois de cada uma custava mais do que ganhava --, e por isso o laço 0x63B4
do 0x727200 (4 chamadas por volta) faz cada operação na memória (ctx->gpr).

Chamada reconhecida (forma do tradutor):
  ctx->gpr[0] = spu_link(R); { int32_t _si = ...; ctx->host_depth++; G(ctx); spu_drain_call(ctx, R); ctx->host_depth--; spu_img_restore(ctx, _si); }
vira
  _r0 = spu_link(R); { <grava os locais que G lê e esta função escreveu, e r0/r1>; ...; G(ctx); <relê os que G escreve>;
                       if (ctx->pc != R) { SPILL; spu_drain_call(ctx, R); RELOAD; } ...; }
(o dreno só tem trabalho se G não voltou direto para R; aí sincroniza tudo). Qualquer outra
chamada que mexa em registradores: SPILL/RELOAD completos em volta, como no promover_regs
(PROMOVER_LACOS). Saídas (return): SPILL dos escritos.

uso: python promover_chamadas.py port/spu/<mód> <EA da função, ex. 63B4>   (backup .sem_promchamadas)
     python promover_chamadas.py port/spu/<mód> --tirar"""
import re
import shutil
import sys
from pathlib import Path

d = Path(sys.argv[1])
f = d / 'spu_recomp.c'
bak = f.with_name(f.name + '.sem_promchamadas')
if '--tirar' in sys.argv:
    if bak.exists():
        shutil.copy2(bak, f)
        print('restaurado', f)
    sys.exit(0)
if not bak.exists():
    shutil.copy2(f, bak)
src = bak.read_bytes().decode('latin-1')
nl = '\r\n' if '\r\n' in src else '\n'
mod = d.name
ea = int(sys.argv[2], 16)
nome = f'{mod}_spu_func_{ea:08X}'
h = f'static SPU_PN void {nome}__b(spu_context* ctx) {{'
i = src.index(h)
assert src.rfind('/* promover_regs.py */', 0, i) < src.rfind('#undef RELOAD', 0, i), 'já promovida'
fim = src.index(nl + '}' + nl, i)
corpo = src[i + len(h) + len(nl):fim].split(nl)


def conjuntos(g):
    """(lidos, escritos) de uma função G pelos #define logo antes da definição dela."""
    hg = f'static SPU_PN void {g}(spu_context* ctx) {{'
    j = src.index(hg)
    a = src.rindex('#define SPILL', 0, j)
    r = src.index('#define RELOAD', a)
    sp = src[a:src.index(nl, a)]
    rl = src[r:src.index(nl, r)]
    assert 'promover_regs.py' in sp or 'fundir_cadeia.py' in sp, g
    usados = {int(x) for x in re.findall(r'_r(\d+) = ctx->gpr\[\1\];', rl)}
    escritos_g = {int(x) for x in re.findall(r'ctx->gpr\[(\d+)\] = _r\1;', sp)}
    # Lidos de verdade = vivos na entrada (lidos antes de escritos), quando o corpo é reto
    # (nenhum goto para trás): o RELOAD carrega todos os usados, e a maioria dos
    # temporários é escrita antes de ser lida -- gravá-los antes da chamada é desperdício.
    fim_g = src.index(nl + '}' + nl, j)
    corpo_g = src[src.index(nl, j) + len(nl):fim_g].split(nl)
    rot = {}
    reto = True
    for k, c in enumerate(corpo_g):
        mm = re.match(r'^\s*(\w+):;?', c)
        if mm:
            rot[mm.group(1)] = k
        for alvo in re.findall(r'goto (\w+);', c):
            if alvo in rot:
                reto = False
    if not reto:
        return usados, escritos_g
    vivos, escr = set(), set()
    for c in corpo_g:
        if c.strip().startswith('u128 '):
            continue   # a declaração dos locais
        c = c.replace('RELOAD;', '').replace('SPILL;', '')
        for st in c.split(';'):
            mm = re.match(r'^\s*(?:\{\s*)?_r(\d+)(\._u\d+\[\d+\])? = (?!=)(.*)$', st)
            if mm:
                lidos_st = {int(x) for x in re.findall(r'_r(\d+)', mm.group(3))}
                if mm.group(2):
                    lidos_st.add(int(mm.group(1)))
                vivos |= lidos_st - escr
                escr.add(int(mm.group(1)))
            else:
                vivos |= {int(x) for x in re.findall(r'_r(\d+)', st)} - escr
    return vivos & usados, escritos_g


texto = nl.join(corpo)
regs = sorted({int(x) for x in re.findall(r'ctx->gpr\[(\d+)\]', texto)} | {0, 1})
escritos = sorted({int(x) for x in re.findall(r'ctx->gpr\[(\d+)\](?:\._u\d+\[\d+\])? = (?!=)', texto)})
RE_CH = re.compile(r'^(\s*)ctx->gpr\[0\] = spu_link\((0x[0-9A-F]+)\); \{ int32_t _si = \(int32_t\)ctx->image_id; ctx->host_depth\+\+; '
                   r'(\w+)\(ctx\); spu_drain_call\(ctx, (0x[0-9A-F]+)\); ctx->host_depth--; spu_img_restore\(ctx, _si\); \}\s*$')
SAFE = {'spu_ls_read128', 'spu_ls_write128', 'spu_img_restore'}
novo = []
n_prec = n_total = 0
for c in corpo:
    m = RE_CH.match(c)
    if m and m.group(2) == m.group(4):
        ind, ret, g = m.group(1), m.group(2), m.group(3)
        lidos, esc_g = conjuntos(g)
        grava = sorted((lidos | {0, 1}) & (set(escritos) | {0}))
        rele = sorted(esc_g & set(regs))
        novo.append(f'{ind}_r0 = spu_link({ret}); {{ ' + ' '.join(f'ctx->gpr[{r}] = _r{r};' for r in grava) +
                    f' int32_t _si = (int32_t)ctx->image_id; ctx->host_depth++; {g}(ctx); ' +
                    ' '.join(f'_r{r} = ctx->gpr[{r}];' for r in rele) +
                    f' if (__builtin_expect(ctx->pc != {ret}u, 0)) {{ SPILL; spu_drain_call(ctx, {ret}); RELOAD; }}'
                    f' ctx->host_depth--; spu_img_restore(ctx, _si); }}')
        n_prec += 1
        continue
    outros = [mm.group(1) for mm in re.finditer(r'\b([A-Za-z_]\w*)\(ctx[,)]', c) if mm.group(1) not in SAFE]
    c2 = re.sub(r'ctx->gpr\[(\d+)\]', r'_r\1', c)
    if outros:
        # chamada genérica: sincronização completa em volta da linha inteira
        c2 = re.sub(r'^(\s*)', r'\1SPILL; ', c2, count=1) + ' RELOAD;'
        n_total += 1
    c2 = re.sub(r'\breturn;', '{ SPILL; return; }', c2)
    novo.append(c2)
# um "return" dentro de uma linha com SPILL/RELOAD em volta já grava antes; o RELOAD depois de um return é morto
cab = [f'#define SPILL do {{ ' + ' '.join(f'ctx->gpr[{r}] = _r{r};' for r in escritos) + ' } while (0) /* promover_chamadas.py */',
       f'#define RELOAD do {{ ' + ' '.join(f'_r{r} = ctx->gpr[{r}];' for r in regs) + ' } while (0)',
       h, '        u128 ' + ', '.join(f'_r{r}' for r in regs) + ';', '        RELOAD;']
saida = cab + novo + ['        SPILL;', '}', '#undef SPILL', '#undef RELOAD']
if '--check' in sys.argv:
    # build de conferência: a original fica como __orig, a promovida vira __p, e __b roda as
    # duas a partir do mesmo estado nas primeiras GOWA_PROMCHECK=<n> chamadas e compara
    # registradores, LS e pc (a função só lê/escreve LS e registradores e chama a fundida)
    orig = src[i:fim + len(nl) + 1].replace(f'{nome}__b(spu_context* ctx) {{', f'{nome}__orig(spu_context* ctx) {{', 1)
    prom = nl.join(saida).replace(f'{nome}__b(spu_context* ctx) {{', f'{nome}__p(spu_context* ctx) {{', 1)
    disp = nl.join([
        f'static SPU_PN void {nome}__b(spu_context* ctx) {{',
        '        static long n_chk = -1, n_bad = 0; static volatile long n_done = 0;',
        '        if (n_chk < 0) { const char* e = getenv("GOWA_PROMCHECK"); n_chk = e ? atol(e) : 0; }',
        '        if (n_done < n_chk) {',
        '            static __declspec(thread) uint8_t* ls_a; static __declspec(thread) uint8_t* ls_b;',
        '            if (!ls_a) { ls_a = (uint8_t*)malloc(SPU_LS_SIZE); ls_b = (uint8_t*)malloc(SPU_LS_SIZE); }',
        '            u128 g0[128], ga[128]; memcpy(g0, ctx->gpr, sizeof g0); memcpy(ls_a, ctx->ls, SPU_LS_SIZE);',
        '            const uint32_t pc0 = ctx->pc;',
        f'            {nome}__orig(ctx);',
        '            const uint32_t pca = ctx->pc; memcpy(ga, ctx->gpr, sizeof ga); memcpy(ls_b, ctx->ls, SPU_LS_SIZE);',
        '            memcpy(ctx->gpr, g0, sizeof g0); memcpy(ctx->ls, ls_a, SPU_LS_SIZE); ctx->pc = pc0;',
        f'            {nome}__p(ctx);',
        '            const long k = __atomic_add_fetch(&n_done, 1, __ATOMIC_RELAXED);',
        '            int dr = 0; for (int r = 0; r < 128; r++) if (memcmp(&ga[r], &ctx->gpr[r], 16)) { if (dr++ < 6) fprintf(stderr, "[promcheck] #%ld r%d orig %08X %08X %08X %08X prom %08X %08X %08X %08X\\n", k, r, ga[r]._u32[0], ga[r]._u32[1], ga[r]._u32[2], ga[r]._u32[3], ctx->gpr[r]._u32[0], ctx->gpr[r]._u32[1], ctx->gpr[r]._u32[2], ctx->gpr[r]._u32[3]); }',
        '            const int dl = memcmp(ls_b, ctx->ls, SPU_LS_SIZE) != 0;',
        '            if (dr || dl || pca != ctx->pc) { n_bad++; fprintf(stderr, "[promcheck] #%ld DIFERENTE: %d registradores, LS %s, pc %05X/%05X\\n", k, dr, dl ? "difere" : "igual", pca, ctx->pc); }',
        f'            if (k == n_chk) fprintf(stderr, "[promcheck] {nome}: %ld chamadas conferidas, %ld diferentes\\n", k, n_bad);',
        '            return;',
        '        }',
        f'        {nome}__p(ctx);',
        '}', ''])
    src2 = src[:i] + orig + nl + prom + nl + disp + src[fim + len(nl) + 1:]
    # protótipos
    k = src2.index('#include "spu_recomp.h"'); k = src2.index(nl, k) + len(nl)
    src2 = src2[:k] + f'static SPU_PN void {nome}__orig(spu_context* ctx); static SPU_PN void {nome}__p(spu_context* ctx);' + nl + src2[k:]
else:
    src2 = src[:i] + nl.join(saida) + src[fim + len(nl) + 1:]
f.write_bytes(src2.encode('latin-1'))
print(f'{nome}__b: {len(regs)} locais, {len(escritos)} escritos, {n_prec} chamadas com sincronização precisa, {n_total} com completa')
