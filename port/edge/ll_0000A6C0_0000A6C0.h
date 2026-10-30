/* GOWA: gowajm_0072DF80_spu_func_0000A6C0__b loc_0000A6C0 com registradores locais (gerado por scripts_edge/laco_local.py; não editar).
 * Mesmas operações e ordem do código traduzido: idêntico bit a bit. Faz o laço inteiro e devolve 1 (o
 * gancho salta para loc_0000A6C0_fim, depois do desvio); 0 = nada feito (desligado). */
#ifndef GOWA_LL_0000A6C0_0000A6C0_H
#define GOWA_LL_0000A6C0_0000A6C0_H
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
static int ll_0000A6C0_0000A6C0(spu_context* ctx)
{
    if (!gowa_laco_local_on()) return 0;
    u128 g5 = ctx->gpr[5], g6 = ctx->gpr[6], g7 = ctx->gpr[7], g10 = ctx->gpr[10], g11 = ctx->gpr[11], g38 = ctx->gpr[38], g39 = ctx->gpr[39], g40 = ctx->gpr[40];
    do {
        g5 = spu_ai(g5, -16);
        g10 = spu_ls_read128(ctx, g6._u32[0] + 0x0);
        g6 = spu_ai(g6, 16);
        g38 = spu_clgti(g5, 15);
        g40 = spu_ls_read128(ctx, g6._u32[0] + 0x0);
        g39 = spu_shufb(g10, g40, g7);
        spu_ls_write128(ctx, g11._u32[0] + 0x0, g39);
        g11 = spu_ai(g11, 16);
    } while (g38._u32[0] != 0);
    ctx->gpr[5] = g5;
    ctx->gpr[10] = g10;
    ctx->gpr[6] = g6;
    ctx->gpr[38] = g38;
    ctx->gpr[40] = g40;
    ctx->gpr[39] = g39;
    ctx->gpr[11] = g11;
    return 1;
}
#endif
