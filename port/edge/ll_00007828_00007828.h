/* GOWA: gowajm_0072DF80_spu_func_00007828__b loc_00007828 com registradores locais (gerado por scripts_edge/laco_local.py; não editar).
 * Mesmas operações e ordem do código traduzido: idêntico bit a bit. Faz o laço inteiro e devolve 1 (o
 * gancho salta para loc_00007828_fim, depois do desvio); 0 = nada feito (desligado). */
#ifndef GOWA_LL_00007828_00007828_H
#define GOWA_LL_00007828_00007828_H
#ifndef GOWA_LACO_LOCAL_ON_DEF
#define GOWA_LACO_LOCAL_ON_DEF
#include <stdlib.h>
static int gowa_laco_local_on(void)   /* GOWA_LACO_LOCAL=0 desliga todos (A/B) */
{
    static int on = -1;
    if (on < 0) { const char* e = getenv("GOWA_LACO_LOCAL"); on = !(e && *e == '0'); }
    return on;
}
#endif
static int ll_00007828_00007828(spu_context* ctx)
{
    if (!gowa_laco_local_on()) return 0;
    u128 g6 = ctx->gpr[6], g7 = ctx->gpr[7], g8 = ctx->gpr[8], g9 = ctx->gpr[9], g10 = ctx->gpr[10], g11 = ctx->gpr[11], g12 = ctx->gpr[12], g13 = ctx->gpr[13], g14 = ctx->gpr[14], g15 = ctx->gpr[15], g16 = ctx->gpr[16], g17 = ctx->gpr[17], g18 = ctx->gpr[18], g19 = ctx->gpr[19], g20 = ctx->gpr[20], g21 = ctx->gpr[21], g22 = ctx->gpr[22], g23 = ctx->gpr[23], g24 = ctx->gpr[24], g105 = ctx->gpr[105], g106 = ctx->gpr[106], g108 = ctx->gpr[108], g109 = ctx->gpr[109], g110 = ctx->gpr[110], g111 = ctx->gpr[111], g112 = ctx->gpr[112], g113 = ctx->gpr[113], g114 = ctx->gpr[114];
    do {
        g114 = spu_selb(g17, g12, g8);
        g17 = spu_ls_read128(ctx, g20._u32[0] + g6._u32[0]);
        g113 = spu_selb(g16, g11, g8);
        g16 = spu_ls_read128(ctx, g19._u32[0] + g6._u32[0]);
        g112 = spu_selb(g15, g10, g8);
        g15 = spu_ls_read128(ctx, g14._u32[0] + g6._u32[0]);
        g111 = spu_selb(g18, g9, g8);
        g18 = spu_ls_read128(ctx, g13._u32[0] + g6._u32[0]);
        g110 = spu_ai(g7, 64);
        spu_ls_write128(ctx, g23._u32[0] + g6._u32[0], g114);
        g109 = spu_ai(g12, 64);
        spu_ls_write128(ctx, g24._u32[0] + g6._u32[0], g113);
        g108 = spu_ai(g11, 64);
        spu_ls_write128(ctx, g22._u32[0] + g6._u32[0], g112);
        g106 = spu_ai(g10, 64);
        spu_ls_write128(ctx, g21._u32[0] + g6._u32[0], g111);
        g105 = spu_ai(g9, 64);
        g12 = spu_ori(g109, 0);
        g11 = spu_shlqbyi(g108, 0);
        g6 = spu_ori(g110, 0);
        g10 = spu_shlqbyi(g106, 0);
        g9 = spu_ori(g105, 0);
        g7 = spu_shlqbyi(g110, 0);
    } while (g110._u32[0] != 0);
    ctx->gpr[114] = g114;
    ctx->gpr[17] = g17;
    ctx->gpr[113] = g113;
    ctx->gpr[16] = g16;
    ctx->gpr[112] = g112;
    ctx->gpr[15] = g15;
    ctx->gpr[111] = g111;
    ctx->gpr[18] = g18;
    ctx->gpr[110] = g110;
    ctx->gpr[109] = g109;
    ctx->gpr[108] = g108;
    ctx->gpr[106] = g106;
    ctx->gpr[105] = g105;
    ctx->gpr[12] = g12;
    ctx->gpr[11] = g11;
    ctx->gpr[6] = g6;
    ctx->gpr[10] = g10;
    ctx->gpr[9] = g9;
    ctx->gpr[7] = g7;
    return 1;
}
#endif
