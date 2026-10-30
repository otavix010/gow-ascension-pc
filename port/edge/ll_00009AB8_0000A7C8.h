/* GOWA: gowajm_0072DF80_spu_func_00009AB8__b loc_0000A7C8 com registradores locais (gerado por scripts_edge/laco_local.py; não editar).
 * Mesmas operações e ordem do código traduzido: idêntico bit a bit. Faz o laço inteiro e devolve 1 (o
 * gancho salta para loc_0000A7C8_fim, depois do desvio); 0 = nada feito (desligado). */
#ifndef GOWA_LL_00009AB8_0000A7C8_H
#define GOWA_LL_00009AB8_0000A7C8_H
#ifndef GOWA_LACO_LOCAL_ON_DEF
#define GOWA_LACO_LOCAL_ON_DEF
#include <stdlib.h>
/* GOWA_LACO_LOCAL=0/1 desliga/liga todos (A/B). Padrão: só com AVX-512 (32 registradores vetoriais); no AVX2
 * (16) os ~60 registradores locais vão para a pilha e o laço fica mais lento que o traduzido (notebook, 30/10:
 * conferência 10,3 -> 12,5 ms). */
static int gowa_laco_local_on(void)
{
    static int on = -1;
#if defined(__AVX512F__)
    const int padrao = 1;
#else
    const int padrao = 0;
#endif
    if (on < 0) { const char* e = getenv("GOWA_LACO_LOCAL"); on = e && *e ? *e != '0' : padrao; }
    return on;
}
#endif
static int ll_00009AB8_0000A7C8(spu_context* ctx)
{
    if (!gowa_laco_local_on()) return 0;
    u128 g6 = ctx->gpr[6], g7 = ctx->gpr[7], g8 = ctx->gpr[8], g9 = ctx->gpr[9], g11 = ctx->gpr[11], g18 = ctx->gpr[18], g21 = ctx->gpr[21], g47 = ctx->gpr[47];
    do {
        g8 = spu_ai(g8, -16);
        g21 = spu_ls_read128(ctx, g7._u32[0] + 0x0);
        g7 = spu_ai(g7, 16);
        g18 = spu_clgti(g8, 15);
        g6 = spu_ls_read128(ctx, g7._u32[0] + 0x0);
        g47 = spu_shufb(g21, g6, g9);
        spu_ls_write128(ctx, g11._u32[0] + 0x0, g47);
        g11 = spu_ai(g11, 16);
    } while (g18._u32[0] != 0);
    ctx->gpr[8] = g8;
    ctx->gpr[21] = g21;
    ctx->gpr[7] = g7;
    ctx->gpr[18] = g18;
    ctx->gpr[6] = g6;
    ctx->gpr[47] = g47;
    ctx->gpr[11] = g11;
    return 1;
}
#endif
