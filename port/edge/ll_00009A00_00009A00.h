/* GOWA: gowajm_0072DF80_spu_func_00009A00__b loc_00009A00 com registradores locais (gerado por scripts_edge/laco_local.py; não editar).
 * Mesmas operações e ordem do código traduzido: idêntico bit a bit. Faz o laço inteiro e devolve 1 (o
 * gancho salta para loc_00009A00_fim, depois do desvio); 0 = nada feito (desligado). */
#ifndef GOWA_LL_00009A00_00009A00_H
#define GOWA_LL_00009A00_00009A00_H
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
static int ll_00009A00_00009A00(spu_context* ctx)
{
    if (!gowa_laco_local_on()) return 0;
    u128 g3 = ctx->gpr[3], g4 = ctx->gpr[4], g6 = ctx->gpr[6], g7 = ctx->gpr[7], g8 = ctx->gpr[8], g9 = ctx->gpr[9], g10 = ctx->gpr[10], g11 = ctx->gpr[11], g12 = ctx->gpr[12], g13 = ctx->gpr[13], g14 = ctx->gpr[14], g15 = ctx->gpr[15], g16 = ctx->gpr[16], g80 = ctx->gpr[80], g81 = ctx->gpr[81];
    do {
        g9 = spu_ai(g9, 1);
        g16 = spu_ls_read128(ctx, g8._u32[0] + 0x0);
        g11 = spu_ls_read128(ctx, g7._u32[0] + 0x0);
        g6 = spu_ceq(g81, g9);
        g12 = spu_cwd(g7, 0);
        g15 = spu_rotqby(g16, g8);
        g8 = spu_ai(g8, 8);
        g13 = spu_and(g15, g10);
        g14 = spu_ceqi(g13, 0);
        g3 = spu_selb(g80, g13, g14);
        g80 = spu_a(g80, g13);
        g4 = spu_shufb(g3, g11, g12);
        spu_ls_write128(ctx, g7._u32[0] + 0x0, g4);
        g7 = spu_ai(g7, 4);
    } while (g6._u32[0] == 0);
    ctx->gpr[9] = g9;
    ctx->gpr[16] = g16;
    ctx->gpr[11] = g11;
    ctx->gpr[6] = g6;
    ctx->gpr[12] = g12;
    ctx->gpr[15] = g15;
    ctx->gpr[8] = g8;
    ctx->gpr[13] = g13;
    ctx->gpr[14] = g14;
    ctx->gpr[3] = g3;
    ctx->gpr[80] = g80;
    ctx->gpr[4] = g4;
    ctx->gpr[7] = g7;
    return 1;
}
#endif
