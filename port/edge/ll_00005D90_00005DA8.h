/* GOWA: gowajm_0072DF80_spu_func_00005D90__b loc_00005DA8 com registradores locais (gerado por scripts_edge/laco_local.py; não editar).
 * Mesmas operações e ordem do código traduzido: idêntico bit a bit. Faz o laço inteiro e devolve 1 (o
 * gancho salta para loc_00005DA8_fim, depois do desvio); 0 = nada feito (desligado). */
#ifndef GOWA_LL_00005D90_00005DA8_H
#define GOWA_LL_00005D90_00005DA8_H
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
static int ll_00005D90_00005DA8(spu_context* ctx)
{
    if (!gowa_laco_local_on()) return 0;
    u128 g3 = ctx->gpr[3], g4 = ctx->gpr[4], g5 = ctx->gpr[5], g6 = ctx->gpr[6], g7 = ctx->gpr[7];
    do {
        g6 = spu_ai(g6, 8);
        spu_ls_write128(ctx, g3._u32[0] + 0x0, g5);
        spu_ls_write128(ctx, g3._u32[0] + 0x10, g5);
        g7 = spu_cgt(g4, g6);
        spu_ls_write128(ctx, g3._u32[0] + 0x20, g5);
        spu_ls_write128(ctx, g3._u32[0] + 0x30, g5);
        spu_ls_write128(ctx, g3._u32[0] + 0x40, g5);
        spu_ls_write128(ctx, g3._u32[0] + 0x50, g5);
        spu_ls_write128(ctx, g3._u32[0] + 0x60, g5);
        spu_ls_write128(ctx, g3._u32[0] + 0x70, g5);
        g3 = spu_ai(g3, 128);
    } while (g7._u32[0] != 0);
    ctx->gpr[6] = g6;
    ctx->gpr[7] = g7;
    ctx->gpr[3] = g3;
    return 1;
}
#endif
