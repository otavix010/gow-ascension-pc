/* GOWA: gowajm_0072DF80_spu_func_000075D8__b loc_000075D8 com registradores locais (gerado por scripts_edge/laco_local.py; não editar).
 * Mesmas operações e ordem do código traduzido: idêntico bit a bit. Faz o laço inteiro e devolve 1 (o
 * gancho salta para loc_000075D8_fim, depois do desvio); 0 = nada feito (desligado). */
#ifndef GOWA_LL_000075D8_000075D8_H
#define GOWA_LL_000075D8_000075D8_H
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
static int ll_000075D8_000075D8(spu_context* ctx)
{
    if (!gowa_laco_local_on()) return 0;
    u128 g8 = ctx->gpr[8], g9 = ctx->gpr[9], g10 = ctx->gpr[10], g11 = ctx->gpr[11], g12 = ctx->gpr[12], g13 = ctx->gpr[13], g14 = ctx->gpr[14], g37 = ctx->gpr[37], g38 = ctx->gpr[38], g39 = ctx->gpr[39], g40 = ctx->gpr[40], g41 = ctx->gpr[41], g42 = ctx->gpr[42], g43 = ctx->gpr[43], g80 = ctx->gpr[80], g81 = ctx->gpr[81];
    do {
        g43 = spu_roti(g8, 16);
        g42 = spu_ls_read128(ctx, g80._u32[0] + g14._u32[0]);
        g41 = spu_roti(g9, 16);
        spu_ls_write128(ctx, g81._u32[0] + g8._u32[0], g10);
        g40 = spu_roti(g11, 16);
        spu_ls_write128(ctx, g81._u32[0] + g9._u32[0], g10);
        g39 = spu_roti(g12, 16);
        spu_ls_write128(ctx, g81._u32[0] + g11._u32[0], g10);
        g37 = spu_ai(g14, 16);
        spu_ls_write128(ctx, g81._u32[0] + g12._u32[0], g10);
        g13 = spu_ai(g13, -1);
        spu_ls_write128(ctx, g81._u32[0] + g43._u32[0], g10);
        g14 = spu_ori(g37, 0);
        g8 = spu_shlqbii(g42, 4);
        spu_ls_write128(ctx, g81._u32[0] + g41._u32[0], g10);
        spu_ls_write128(ctx, g81._u32[0] + g40._u32[0], g10);
        spu_ls_write128(ctx, g81._u32[0] + g39._u32[0], g10);
        g38 = spu_rotqbyi(g8, 12);
        g9 = spu_rotqbyi(g8, 4);
        g11 = spu_rotqbyi(g8, 8);
        g12 = spu_ori(g38, 0);
    } while (g13._u32[0] != 0);
    ctx->gpr[43] = g43;
    ctx->gpr[42] = g42;
    ctx->gpr[41] = g41;
    ctx->gpr[40] = g40;
    ctx->gpr[39] = g39;
    ctx->gpr[37] = g37;
    ctx->gpr[13] = g13;
    ctx->gpr[14] = g14;
    ctx->gpr[8] = g8;
    ctx->gpr[38] = g38;
    ctx->gpr[9] = g9;
    ctx->gpr[11] = g11;
    ctx->gpr[12] = g12;
    return 1;
}
#endif
