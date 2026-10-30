/* GOWA: gowajm_0072DF80_spu_func_000083E8__b loc_000083E8 com registradores locais (gerado por scripts_edge/laco_local.py; não editar).
 * Mesmas operações e ordem do código traduzido: idêntico bit a bit. Faz o laço inteiro e devolve 1 (o
 * gancho salta para loc_000083E8_fim, depois do desvio); 0 = nada feito (desligado). */
#ifndef GOWA_LL_000083E8_000083E8_H
#define GOWA_LL_000083E8_000083E8_H
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
static int ll_000083E8_000083E8(spu_context* ctx)
{
    if (!gowa_laco_local_on()) return 0;
    u128 g13 = ctx->gpr[13], g14 = ctx->gpr[14], g15 = ctx->gpr[15], g16 = ctx->gpr[16], g17 = ctx->gpr[17], g18 = ctx->gpr[18], g19 = ctx->gpr[19], g20 = ctx->gpr[20], g21 = ctx->gpr[21], g22 = ctx->gpr[22], g23 = ctx->gpr[23], g24 = ctx->gpr[24], g25 = ctx->gpr[25], g26 = ctx->gpr[26], g27 = ctx->gpr[27], g28 = ctx->gpr[28], g29 = ctx->gpr[29], g30 = ctx->gpr[30], g31 = ctx->gpr[31], g32 = ctx->gpr[32], g33 = ctx->gpr[33], g79 = ctx->gpr[79], g81 = ctx->gpr[81], g83 = ctx->gpr[83], g84 = ctx->gpr[84], g90 = ctx->gpr[90], g91 = ctx->gpr[91], g92 = ctx->gpr[92], g94 = ctx->gpr[94], g95 = ctx->gpr[95], g96 = ctx->gpr[96], g97 = ctx->gpr[97], g98 = ctx->gpr[98];
    do {
        g17 = spu_ai(g17, -8);
        g97 = spu_selb(g24, g33, g13);
        g33 = spu_ls_read128(ctx, g16._u32[0] + 0x0);
        g96 = spu_selb(g23, g30, g13);
        g24 = spu_ls_read128(ctx, g15._u32[0] + 0x0);
        g95 = spu_selb(g22, g29, g13);
        g30 = spu_ls_read128(ctx, g16._u32[0] + 0x10);
        g94 = spu_selb(g21, g28, g13);
        g23 = spu_ls_read128(ctx, g15._u32[0] + 0x10);
        g92 = spu_selb(g20, g27, g13);
        g29 = spu_ls_read128(ctx, g16._u32[0] + 0x20);
        g91 = spu_selb(g19, g26, g13);
        g22 = spu_ls_read128(ctx, g15._u32[0] + 0x20);
        g90 = spu_selb(g18, g25, g13);
        g28 = spu_ls_read128(ctx, g16._u32[0] + 0x30);
        g98 = spu_selb(g31, g32, g13);
        g21 = spu_ls_read128(ctx, g15._u32[0] + 0x30);
        g84 = spu_ai(g15, 128);
        g83 = spu_ai(g16, 128);
        g27 = spu_ls_read128(ctx, g16._u32[0] + 0x40);
        g81 = spu_ai(g14, 128);
        g20 = spu_ls_read128(ctx, g15._u32[0] + 0x40);
        g79 = spu_cgti(g17, 0);
        g26 = spu_ls_read128(ctx, g16._u32[0] + 0x50);
        g19 = spu_ls_read128(ctx, g15._u32[0] + 0x50);
        g25 = spu_ls_read128(ctx, g16._u32[0] + 0x60);
        g18 = spu_ls_read128(ctx, g15._u32[0] + 0x60);
        g32 = spu_ls_read128(ctx, g16._u32[0] + 0x70);
        g16 = spu_ori(g83, 0);
        g31 = spu_ls_read128(ctx, g15._u32[0] + 0x70);
        g15 = spu_ori(g84, 0);
        spu_ls_write128(ctx, g14._u32[0] + 0x70, g98);
        spu_ls_write128(ctx, g14._u32[0] + 0x0, g97);
        spu_ls_write128(ctx, g14._u32[0] + 0x10, g96);
        spu_ls_write128(ctx, g14._u32[0] + 0x20, g95);
        spu_ls_write128(ctx, g14._u32[0] + 0x30, g94);
        spu_ls_write128(ctx, g14._u32[0] + 0x40, g92);
        spu_ls_write128(ctx, g14._u32[0] + 0x50, g91);
        spu_ls_write128(ctx, g14._u32[0] + 0x60, g90);
        g14 = spu_ori(g81, 0);
    } while (g79._u32[0] != 0);
    ctx->gpr[17] = g17;
    ctx->gpr[97] = g97;
    ctx->gpr[33] = g33;
    ctx->gpr[96] = g96;
    ctx->gpr[24] = g24;
    ctx->gpr[95] = g95;
    ctx->gpr[30] = g30;
    ctx->gpr[94] = g94;
    ctx->gpr[23] = g23;
    ctx->gpr[92] = g92;
    ctx->gpr[29] = g29;
    ctx->gpr[91] = g91;
    ctx->gpr[22] = g22;
    ctx->gpr[90] = g90;
    ctx->gpr[28] = g28;
    ctx->gpr[98] = g98;
    ctx->gpr[21] = g21;
    ctx->gpr[84] = g84;
    ctx->gpr[83] = g83;
    ctx->gpr[27] = g27;
    ctx->gpr[81] = g81;
    ctx->gpr[20] = g20;
    ctx->gpr[79] = g79;
    ctx->gpr[26] = g26;
    ctx->gpr[19] = g19;
    ctx->gpr[25] = g25;
    ctx->gpr[18] = g18;
    ctx->gpr[32] = g32;
    ctx->gpr[16] = g16;
    ctx->gpr[31] = g31;
    ctx->gpr[15] = g15;
    ctx->gpr[14] = g14;
    return 1;
}
#endif
