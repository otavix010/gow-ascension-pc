/* GOWA: gowajm_0072DF80_spu_func_0000B100__b loc_0000B100 com registradores locais (gerado por scripts_edge/laco_local.py; não editar).
 * Mesmas operações e ordem do código traduzido: idêntico bit a bit. Faz o laço inteiro e devolve 1 (o
 * gancho salta para loc_0000B100_fim, depois do desvio); 0 = nada feito (desligado). */
#ifndef GOWA_LL_0000B100_0000B100_H
#define GOWA_LL_0000B100_0000B100_H
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
static int ll_0000B100_0000B100(spu_context* ctx)
{
    if (!gowa_laco_local_on()) return 0;
    u128 g14 = ctx->gpr[14], g15 = ctx->gpr[15], g16 = ctx->gpr[16], g18 = ctx->gpr[18], g32 = ctx->gpr[32], g33 = ctx->gpr[33], g34 = ctx->gpr[34], g35 = ctx->gpr[35], g36 = ctx->gpr[36], g37 = ctx->gpr[37], g38 = ctx->gpr[38], g39 = ctx->gpr[39];
    do {
        g33 = spu_il(20);
        g37 = spu_cwd(g14, 4);
        g38 = spu_andi(g15, 1);
        g39 = spu_andi(g16, 16);
        g35 = spu_ls_read128(ctx, g14._u32[0] + g33._u32[0]);
        g18 = spu_ai(g18, 1);
        g36 = spu_or(g38, g39);
        g32 = spu_ceqi(g18, 10);
        g15 = spu_rotmi(g15, -1);
        g16 = spu_rotmi(g16, -1);
        g34 = spu_shufb(g36, g35, g37);
        spu_ls_write128(ctx, g14._u32[0] + g33._u32[0], g34);
        g14 = spu_ai(g14, 4);
    } while (g32._u32[0] == 0);
    ctx->gpr[33] = g33;
    ctx->gpr[37] = g37;
    ctx->gpr[38] = g38;
    ctx->gpr[39] = g39;
    ctx->gpr[35] = g35;
    ctx->gpr[18] = g18;
    ctx->gpr[36] = g36;
    ctx->gpr[32] = g32;
    ctx->gpr[15] = g15;
    ctx->gpr[16] = g16;
    ctx->gpr[34] = g34;
    ctx->gpr[14] = g14;
    return 1;
}
#endif
