/* GOWA: gowajm_0072DF80_spu_func_00004EB8__b loc_00004EB8 com registradores locais (gerado por scripts_edge/laco_local.py; não editar).
 * Mesmas operações e ordem do código traduzido: idêntico bit a bit. Faz o laço inteiro e devolve 1 (o
 * gancho salta para loc_00004EB8_fim, depois do desvio); 0 = nada feito (desligado). */
#ifndef GOWA_LL_00004EB8_00004EB8_H
#define GOWA_LL_00004EB8_00004EB8_H
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
static int ll_00004EB8_00004EB8(spu_context* ctx)
{
    if (!gowa_laco_local_on()) return 0;
    u128 g7 = ctx->gpr[7], g8 = ctx->gpr[8], g9 = ctx->gpr[9], g10 = ctx->gpr[10], g48 = ctx->gpr[48], g49 = ctx->gpr[49], g50 = ctx->gpr[50], g51 = ctx->gpr[51];
    do {
        g9 = spu_ai(g9, -16);
        g50 = spu_ls_read128(ctx, g8._u32[0] + 0x0);
        g8 = spu_ai(g8, 16);
        g48 = spu_clgti(g9, 15);
        g51 = spu_ls_read128(ctx, g8._u32[0] + 0x0);
        g49 = spu_shufb(g50, g51, g10);
        spu_ls_write128(ctx, g7._u32[0] + 0x0, g49);
        g7 = spu_ai(g7, 16);
    } while (g48._u32[0] != 0);
    ctx->gpr[9] = g9;
    ctx->gpr[50] = g50;
    ctx->gpr[8] = g8;
    ctx->gpr[48] = g48;
    ctx->gpr[51] = g51;
    ctx->gpr[49] = g49;
    ctx->gpr[7] = g7;
    return 1;
}
#endif
