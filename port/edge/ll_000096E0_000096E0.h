/* GOWA: gowajm_0072DF80_spu_func_000096E0__b loc_000096E0 com registradores locais (gerado por scripts_edge/laco_local.py; não editar).
 * Mesmas operações e ordem do código traduzido: idêntico bit a bit. Faz o laço inteiro e devolve 1 (o
 * gancho salta para loc_000096E0_fim, depois do desvio); 0 = nada feito (desligado). */
#ifndef GOWA_LL_000096E0_000096E0_H
#define GOWA_LL_000096E0_000096E0_H
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
static int ll_000096E0_000096E0(spu_context* ctx)
{
    if (!gowa_laco_local_on()) return 0;
    u128 g6 = ctx->gpr[6], g13 = ctx->gpr[13], g14 = ctx->gpr[14], g36 = ctx->gpr[36], g37 = ctx->gpr[37], g38 = ctx->gpr[38], g39 = ctx->gpr[39], g80 = ctx->gpr[80], g81 = ctx->gpr[81];
    do {
        g39 = spu_or(g81, g6);
        g38 = spu_ls_read128(ctx, g13._u32[0] + 0x0);
        g80 = spu_ai(g80, -256);
        g14 = spu_cwd(g13, 0);
        g81 = spu_ai(g81, 256);
        g36 = spu_clgti(g80, 256);
        g37 = spu_shufb(g39, g38, g14);
        spu_ls_write128(ctx, g13._u32[0] + 0x0, g37);
        g13 = spu_ai(g13, 4);
    } while (g36._u32[0] != 0);
    ctx->gpr[39] = g39;
    ctx->gpr[38] = g38;
    ctx->gpr[80] = g80;
    ctx->gpr[14] = g14;
    ctx->gpr[81] = g81;
    ctx->gpr[36] = g36;
    ctx->gpr[37] = g37;
    ctx->gpr[13] = g13;
    return 1;
}
#endif
