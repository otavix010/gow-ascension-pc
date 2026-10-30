/* GOWA: gowajm_0072DF80_spu_func_00009AB8__b loc_00009B90 com registradores locais (gerado por scripts_edge/laco_local.py; não editar).
 * Mesmas operações e ordem do código traduzido: idêntico bit a bit. Faz o laço inteiro e devolve 1 (o
 * gancho salta para loc_00009B90_fim, depois do desvio); 0 = nada feito (desligado). */
#ifndef GOWA_LL_00009AB8_00009B90_H
#define GOWA_LL_00009AB8_00009B90_H
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
static int ll_00009AB8_00009B90(spu_context* ctx)
{
    if (!gowa_laco_local_on()) return 0;
    u128 g6 = ctx->gpr[6], g7 = ctx->gpr[7], g8 = ctx->gpr[8], g9 = ctx->gpr[9], g18 = ctx->gpr[18], g52 = ctx->gpr[52], g53 = ctx->gpr[53], g54 = ctx->gpr[54], g55 = ctx->gpr[55], g56 = ctx->gpr[56], g57 = ctx->gpr[57], g58 = ctx->gpr[58], g59 = ctx->gpr[59], g60 = ctx->gpr[60], g61 = ctx->gpr[61], g62 = ctx->gpr[62];
    do {
        g62 = spu_a(g7, g8);
        g60 = spu_ls_read128(ctx, g7._u32[0] + g8._u32[0]);
        g7 = spu_ai(g7, 1);
        g54 = spu_ls_read128(ctx, g6._u32[0] + 0x0);
        g61 = spu_ai(g62, 13);
        g52 = spu_ceq(g18, g7);
        g59 = spu_rotqby(g60, g61);
        g58 = spu_cgtbi(g59, -1);
        g57 = spu_xsbh(g58);
        g56 = spu_ceqhi(g57, 0);
        g55 = spu_fsm(g56);
        g53 = spu_selb(g54, g9, g55);
        spu_ls_write128(ctx, g6._u32[0] + 0x0, g53);
        g6 = spu_ai(g6, 16);
    } while (g52._u32[0] == 0);
    ctx->gpr[62] = g62;
    ctx->gpr[60] = g60;
    ctx->gpr[7] = g7;
    ctx->gpr[54] = g54;
    ctx->gpr[61] = g61;
    ctx->gpr[52] = g52;
    ctx->gpr[59] = g59;
    ctx->gpr[58] = g58;
    ctx->gpr[57] = g57;
    ctx->gpr[56] = g56;
    ctx->gpr[55] = g55;
    ctx->gpr[53] = g53;
    ctx->gpr[6] = g6;
    return 1;
}
#endif
