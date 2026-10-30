/* GOWA: gowajm_0072DF80_spu_func_0000E134__b loc_0000E134 com registradores locais (gerado por scripts_edge/laco_local.py; não editar).
 * Mesmas operações e ordem do código traduzido: idêntico bit a bit. Faz o laço inteiro e devolve 1 (o
 * gancho salta para loc_0000E134_fim, depois do desvio); 0 = nada feito (desligado). */
#ifndef GOWA_LL_0000E134_0000E134_H
#define GOWA_LL_0000E134_0000E134_H
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
static int ll_0000E134_0000E134(spu_context* ctx)
{
    if (!gowa_laco_local_on()) return 0;
    u128 g3 = ctx->gpr[3], g9 = ctx->gpr[9], g10 = ctx->gpr[10], g11 = ctx->gpr[11], g12 = ctx->gpr[12], g13 = ctx->gpr[13], g14 = ctx->gpr[14], g15 = ctx->gpr[15], g16 = ctx->gpr[16], g17 = ctx->gpr[17], g18 = ctx->gpr[18], g19 = ctx->gpr[19], g20 = ctx->gpr[20], g21 = ctx->gpr[21], g25 = ctx->gpr[25], g30 = ctx->gpr[30], g32 = ctx->gpr[32], g37 = ctx->gpr[37], g38 = ctx->gpr[38], g41 = ctx->gpr[41], g44 = ctx->gpr[44], g56 = ctx->gpr[56], g57 = ctx->gpr[57];
    do {
        g32 = spu_ah(g19, g10);
        g56 = spu_ori(g13, 0);
        g37 = spu_ls_read128(ctx, g14._u32[0] + g3._u32[0]);
        g25 = spu_ah(g18, g32);
        g30 = spu_ls_read128(ctx, g17._u32[0] + g3._u32[0]);
        g57 = spu_a(g13, g9);
        g44 = spu_ls_read128(ctx, g16._u32[0] + g3._u32[0]);
        g38 = spu_ah(g20, g25);
        g41 = spu_ls_read128(ctx, g15._u32[0] + g3._u32[0]);
        g12 = spu_ai(g12, -1);
        spu_ls_write128(ctx, g17._u32[0] + g13._u32[0], g32);
        g10 = spu_ah(g21, g38);
        spu_ls_write128(ctx, g16._u32[0] + g13._u32[0], g25);
        g3 = spu_ai(g3, 64);
        spu_ls_write128(ctx, g15._u32[0] + g13._u32[0], g38);
        g19 = spu_sfh(g11, g30);
        spu_ls_write128(ctx, g14._u32[0] + g56._u32[0], g10);
        g18 = spu_sfh(g11, g44);
        g20 = spu_sfh(g11, g41);
        g13 = spu_shlqbyi(g57, 0);
        g21 = spu_sfh(g11, g37);
        g9 = spu_il(64);
    } while (g12._u32[0] != 0);
    ctx->gpr[32] = g32;
    ctx->gpr[56] = g56;
    ctx->gpr[37] = g37;
    ctx->gpr[25] = g25;
    ctx->gpr[30] = g30;
    ctx->gpr[57] = g57;
    ctx->gpr[44] = g44;
    ctx->gpr[38] = g38;
    ctx->gpr[41] = g41;
    ctx->gpr[12] = g12;
    ctx->gpr[10] = g10;
    ctx->gpr[3] = g3;
    ctx->gpr[19] = g19;
    ctx->gpr[18] = g18;
    ctx->gpr[20] = g20;
    ctx->gpr[13] = g13;
    ctx->gpr[21] = g21;
    ctx->gpr[9] = g9;
    return 1;
}
#endif
