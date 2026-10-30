/* GOWA: gowajm_0072DF80_spu_func_0000CE60__b loc_0000CE60 com registradores locais (gerado por scripts_edge/laco_local.py; não editar).
 * Mesmas operações e ordem do código traduzido: idêntico bit a bit. Faz o laço inteiro e devolve 1 (o
 * gancho salta para loc_0000CE60_fim, depois do desvio); 0 = nada feito (desligado). */
#ifndef GOWA_LL_0000CE60_0000CE60_H
#define GOWA_LL_0000CE60_0000CE60_H
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
static int ll_0000CE60_0000CE60(spu_context* ctx)
{
    if (!gowa_laco_local_on()) return 0;
    u128 g6 = ctx->gpr[6], g15 = ctx->gpr[15], g17 = ctx->gpr[17], g23 = ctx->gpr[23], g55 = ctx->gpr[55], g56 = ctx->gpr[56], g57 = ctx->gpr[57], g58 = ctx->gpr[58], g59 = ctx->gpr[59], g60 = ctx->gpr[60], g61 = ctx->gpr[61], g62 = ctx->gpr[62], g63 = ctx->gpr[63], g64 = ctx->gpr[64], g65 = ctx->gpr[65], g66 = ctx->gpr[66], g67 = ctx->gpr[67], g68 = ctx->gpr[68], g69 = ctx->gpr[69], g70 = ctx->gpr[70], g71 = ctx->gpr[71], g72 = ctx->gpr[72];
    do {
        g56 = spu_ori(g6, 0);
        g15 = spu_ls_read128(ctx, g6._u32[0] + 0x0);
        g61 = spu_ls_read128(ctx, g6._u32[0] + 0x10);
        g72 = spu_ls_read128(ctx, g6._u32[0] + 0x20);
        g71 = spu_ls_read128(ctx, g6._u32[0] + 0x30);
        g70 = spu_ls_read128(ctx, g6._u32[0] + 0x40);
        g62 = spu_ls_read128(ctx, g6._u32[0] + 0x50);
        g63 = spu_ls_read128(ctx, g6._u32[0] + 0x60);
        g65 = spu_ls_read128(ctx, g6._u32[0] + 0x70);
        g64 = spu_ls_read128(ctx, g6._u32[0] + -0x10);
        g6 = spu_ai(g6, -128);
        g69 = spu_shufb(g61, g72, g17);
        g68 = spu_shufb(g72, g71, g17);
        g67 = spu_shufb(g71, g70, g17);
        g55 = spu_ceq(g6, g23);
        g66 = spu_shufb(g70, g62, g17);
        g60 = spu_shufb(g63, g65, g17);
        g59 = spu_shufb(g64, g15, g17);
        g58 = spu_shufb(g62, g63, g17);
        g57 = spu_shufb(g15, g61, g17);
        spu_ls_write128(ctx, g56._u32[0] + 0x20, g69);
        spu_ls_write128(ctx, g56._u32[0] + 0x30, g68);
        spu_ls_write128(ctx, g56._u32[0] + 0x40, g67);
        spu_ls_write128(ctx, g56._u32[0] + 0x50, g66);
        spu_ls_write128(ctx, g56._u32[0] + 0x70, g60);
        spu_ls_write128(ctx, g56._u32[0] + 0x0, g59);
        spu_ls_write128(ctx, g56._u32[0] + 0x60, g58);
        spu_ls_write128(ctx, g56._u32[0] + 0x10, g57);
    } while (g55._u32[0] == 0);
    ctx->gpr[56] = g56;
    ctx->gpr[15] = g15;
    ctx->gpr[61] = g61;
    ctx->gpr[72] = g72;
    ctx->gpr[71] = g71;
    ctx->gpr[70] = g70;
    ctx->gpr[62] = g62;
    ctx->gpr[63] = g63;
    ctx->gpr[65] = g65;
    ctx->gpr[64] = g64;
    ctx->gpr[6] = g6;
    ctx->gpr[69] = g69;
    ctx->gpr[68] = g68;
    ctx->gpr[67] = g67;
    ctx->gpr[55] = g55;
    ctx->gpr[66] = g66;
    ctx->gpr[60] = g60;
    ctx->gpr[59] = g59;
    ctx->gpr[58] = g58;
    ctx->gpr[57] = g57;
    return 1;
}
#endif
