/* GOWA: gowajm_0072DF80_spu_func_0000DCC0__b loc_0000E30C com registradores locais (gerado por scripts_edge/laco_local.py; não editar).
 * Mesmas operações e ordem do código traduzido: idêntico bit a bit. Faz o laço inteiro e devolve 1 (o
 * gancho salta para loc_0000E30C_fim, depois do desvio); 0 = nada feito (desligado). */
#ifndef GOWA_LL_0000DCC0_0000E30C_H
#define GOWA_LL_0000DCC0_0000E30C_H
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
static int ll_0000DCC0_0000E30C(spu_context* ctx)
{
    if (!gowa_laco_local_on()) return 0;
    u128 g3 = ctx->gpr[3], g6 = ctx->gpr[6], g7 = ctx->gpr[7], g8 = ctx->gpr[8], g9 = ctx->gpr[9], g10 = ctx->gpr[10], g11 = ctx->gpr[11], g17 = ctx->gpr[17], g22 = ctx->gpr[22], g23 = ctx->gpr[23], g29 = ctx->gpr[29], g31 = ctx->gpr[31], g38 = ctx->gpr[38], g53 = ctx->gpr[53], g58 = ctx->gpr[58], g61 = ctx->gpr[61];
    do {
        g38 = spu_ai(g6, 16);
        g9 = spu_ai(g9, 16);
        g29 = spu_shufb(g11, g10, g3);
        g22 = spu_andi(g38, 15);
        g6 = spu_shlqbyi(g38, 0);
        g17 = spu_a(g7, g8);
        g23 = spu_shufb(g22, g22, g58);
        g31 = spu_clgt(g61, g9);
        spu_ls_write128(ctx, g7._u32[0] + 0x0, g29);
        g8 = spu_il(16);
        g10 = spu_ls_read128(ctx, g38._u32[0] + 0x10);
        g7 = spu_ori(g17, 0);
        g11 = spu_ls_read128(ctx, g38._u32[0] + 0x0);
        g3 = spu_a(g23, g53);
    } while (g31._u32[0] != 0);
    ctx->gpr[38] = g38;
    ctx->gpr[9] = g9;
    ctx->gpr[29] = g29;
    ctx->gpr[22] = g22;
    ctx->gpr[6] = g6;
    ctx->gpr[17] = g17;
    ctx->gpr[23] = g23;
    ctx->gpr[31] = g31;
    ctx->gpr[8] = g8;
    ctx->gpr[10] = g10;
    ctx->gpr[7] = g7;
    ctx->gpr[11] = g11;
    ctx->gpr[3] = g3;
    return 1;
}
#endif
