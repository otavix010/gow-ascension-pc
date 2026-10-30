/* GOWA: gowajm_0072DF80_spu_func_0000E804__b loc_0000E804 com registradores locais (gerado por scripts_edge/laco_local.py; não editar).
 * Mesmas operações e ordem do código traduzido: idêntico bit a bit. Faz o laço inteiro e devolve 1 (o
 * gancho salta para loc_0000E804_fim, depois do desvio); 0 = nada feito (desligado). */
#ifndef GOWA_LL_0000E804_0000E804_H
#define GOWA_LL_0000E804_0000E804_H
#ifndef GOWA_LACO_LOCAL_ON_DEF
#define GOWA_LACO_LOCAL_ON_DEF
#include <stdlib.h>
static int gowa_laco_local_on(void)   /* GOWA_LACO_LOCAL=0 desliga todos (A/B) */
{
    static int on = -1;
    if (on < 0) { const char* e = getenv("GOWA_LACO_LOCAL"); on = !(e && *e == '0'); }
    return on;
}
#endif
static int ll_0000E804_0000E804(spu_context* ctx)
{
    if (!gowa_laco_local_on()) return 0;
    u128 g9 = ctx->gpr[9], g10 = ctx->gpr[10], g11 = ctx->gpr[11], g12 = ctx->gpr[12], g13 = ctx->gpr[13], g14 = ctx->gpr[14], g15 = ctx->gpr[15], g16 = ctx->gpr[16], g17 = ctx->gpr[17], g18 = ctx->gpr[18], g19 = ctx->gpr[19], g20 = ctx->gpr[20], g21 = ctx->gpr[21], g22 = ctx->gpr[22], g23 = ctx->gpr[23], g24 = ctx->gpr[24], g25 = ctx->gpr[25], g26 = ctx->gpr[26], g27 = ctx->gpr[27], g28 = ctx->gpr[28], g75 = ctx->gpr[75], g76 = ctx->gpr[76], g77 = ctx->gpr[77], g78 = ctx->gpr[78], g79 = ctx->gpr[79], g80 = ctx->gpr[80], g86 = ctx->gpr[86], g87 = ctx->gpr[87], g89 = ctx->gpr[89];
    do {
        g79 = spu_ls_read128(ctx, g19._u32[0] + g11._u32[0]);
        g89 = spu_selb(g14, g17, g22);
        g76 = spu_ls_read128(ctx, g25._u32[0] + g11._u32[0]);
        g87 = spu_selb(g16, g13, g22);
        g75 = spu_ls_read128(ctx, g21._u32[0] + g11._u32[0]);
        g86 = spu_selb(g12, g18, g22);
        g77 = spu_ls_read128(ctx, g20._u32[0] + g11._u32[0]);
        g80 = spu_ai(g11, 64);
        spu_ls_write128(ctx, g24._u32[0] + g9._u32[0], g89);
        g78 = spu_ai(g9, 48);
        spu_ls_write128(ctx, g23._u32[0] + g9._u32[0], g87);
        g11 = spu_ori(g80, 0);
        spu_ls_write128(ctx, g10._u32[0] + g9._u32[0], g86);
        g9 = spu_ori(g78, 0);
        g17 = spu_shufb(g76, g79, g27);
        g12 = spu_selb(g76, g75, g15);
        g13 = spu_shufb(g75, g79, g26);
        g16 = spu_selb(g77, g76, g15);
        g18 = spu_shufb(g77, g79, g28);
        g14 = spu_selb(g75, g77, g15);
    } while (g80._u32[0] != 0);
    ctx->gpr[79] = g79;
    ctx->gpr[89] = g89;
    ctx->gpr[76] = g76;
    ctx->gpr[87] = g87;
    ctx->gpr[75] = g75;
    ctx->gpr[86] = g86;
    ctx->gpr[77] = g77;
    ctx->gpr[80] = g80;
    ctx->gpr[78] = g78;
    ctx->gpr[11] = g11;
    ctx->gpr[9] = g9;
    ctx->gpr[17] = g17;
    ctx->gpr[12] = g12;
    ctx->gpr[13] = g13;
    ctx->gpr[16] = g16;
    ctx->gpr[18] = g18;
    ctx->gpr[14] = g14;
    return 1;
}
#endif
