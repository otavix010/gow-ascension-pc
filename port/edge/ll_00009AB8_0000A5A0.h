/* GOWA: gowajm_0072DF80_spu_func_00009AB8__b loc_0000A5A0 com registradores locais (gerado por scripts_edge/laco_local.py; não editar).
 * Mesmas operações e ordem do código traduzido: idêntico bit a bit. Faz o laço inteiro e devolve 1 (o
 * gancho salta para loc_0000A5A0_fim, depois do desvio); 0 = nada feito (desligado). */
#ifndef GOWA_LL_00009AB8_0000A5A0_H
#define GOWA_LL_00009AB8_0000A5A0_H
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
static int ll_00009AB8_0000A5A0(spu_context* ctx)
{
    if (!gowa_laco_local_on()) return 0;
    u128 g4 = ctx->gpr[4], g5 = ctx->gpr[5], g7 = ctx->gpr[7], g8 = ctx->gpr[8], g11 = ctx->gpr[11], g75 = ctx->gpr[75], g76 = ctx->gpr[76], g77 = ctx->gpr[77];
    do {
        g5 = spu_ai(g5, 16);
        g77 = spu_shlqbyi(g4, 0);
        g7 = spu_ai(g7, -16);
        g4 = spu_ls_read128(ctx, g5._u32[0] + 0x0);
        g75 = spu_clgti(g7, 15);
        g76 = spu_shufb(g77, g4, g8);
        spu_ls_write128(ctx, g11._u32[0] + 0x0, g76);
        g11 = spu_ai(g11, 16);
    } while (g75._u32[0] != 0);
    ctx->gpr[5] = g5;
    ctx->gpr[77] = g77;
    ctx->gpr[7] = g7;
    ctx->gpr[4] = g4;
    ctx->gpr[75] = g75;
    ctx->gpr[76] = g76;
    ctx->gpr[11] = g11;
    return 1;
}
#endif
