/* GOWA: gowajm_0072DF80_spu_func_00004F88__b loc_000050B0 com registradores locais (gerado por scripts_edge/laco_local.py; não editar).
 * Mesmas operações e ordem do código traduzido: idêntico bit a bit. Faz o laço inteiro e devolve 1 (o
 * gancho salta para loc_000050B0_fim, depois do desvio); 0 = nada feito (desligado). */
#ifndef GOWA_LL_00004F88_000050B0_H
#define GOWA_LL_00004F88_000050B0_H
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
static int ll_00004F88_000050B0(spu_context* ctx)
{
    if (!gowa_laco_local_on()) return 0;
    u128 g5 = ctx->gpr[5], g6 = ctx->gpr[6], g7 = ctx->gpr[7], g8 = ctx->gpr[8], g9 = ctx->gpr[9], g10 = ctx->gpr[10], g12 = ctx->gpr[12], g33 = ctx->gpr[33];
    do {
        g5 = spu_ai(g5, 16);
        g33 = spu_shlqbyi(g6, 0);
        g7 = spu_ai(g7, -16);
        g6 = spu_ls_read128(ctx, g5._u32[0] + 0x0);
        g10 = spu_clgti(g7, 15);
        g12 = spu_shufb(g33, g6, g9);
        spu_ls_write128(ctx, g8._u32[0] + 0x0, g12);
        g8 = spu_ai(g8, 16);
    } while (g10._u32[0] != 0);
    ctx->gpr[5] = g5;
    ctx->gpr[33] = g33;
    ctx->gpr[7] = g7;
    ctx->gpr[6] = g6;
    ctx->gpr[10] = g10;
    ctx->gpr[12] = g12;
    ctx->gpr[8] = g8;
    return 1;
}
#endif
