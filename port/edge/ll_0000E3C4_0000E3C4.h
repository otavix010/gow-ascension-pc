/* GOWA: gowajm_0072DF80_spu_func_0000E3C4__b loc_0000E3C4 com registradores locais (gerado por scripts_edge/laco_local.py; não editar).
 * Mesmas operações e ordem do código traduzido: idêntico bit a bit. Faz o laço inteiro e devolve 1 (o
 * gancho salta para loc_0000E3C4_fim, depois do desvio); 0 = nada feito (desligado). */
#ifndef GOWA_LL_0000E3C4_0000E3C4_H
#define GOWA_LL_0000E3C4_0000E3C4_H
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
static int ll_0000E3C4_0000E3C4(spu_context* ctx)
{
    if (!gowa_laco_local_on()) return 0;
    u128 g2 = ctx->gpr[2], g3 = ctx->gpr[3], g4 = ctx->gpr[4], g5 = ctx->gpr[5], g7 = ctx->gpr[7], g8 = ctx->gpr[8], g9 = ctx->gpr[9], g10 = ctx->gpr[10], g11 = ctx->gpr[11], g12 = ctx->gpr[12], g13 = ctx->gpr[13], g17 = ctx->gpr[17], g18 = ctx->gpr[18], g19 = ctx->gpr[19], g20 = ctx->gpr[20], g21 = ctx->gpr[21], g22 = ctx->gpr[22], g23 = ctx->gpr[23], g24 = ctx->gpr[24], g25 = ctx->gpr[25], g26 = ctx->gpr[26], g27 = ctx->gpr[27], g28 = ctx->gpr[28], g29 = ctx->gpr[29], g30 = ctx->gpr[30], g31 = ctx->gpr[31], g32 = ctx->gpr[32], g33 = ctx->gpr[33], g34 = ctx->gpr[34], g35 = ctx->gpr[35], g36 = ctx->gpr[36], g37 = ctx->gpr[37], g39 = ctx->gpr[39], g40 = ctx->gpr[40], g42 = ctx->gpr[42], g43 = ctx->gpr[43], g44 = ctx->gpr[44], g45 = ctx->gpr[45], g46 = ctx->gpr[46], g47 = ctx->gpr[47], g49 = ctx->gpr[49], g50 = ctx->gpr[50], g51 = ctx->gpr[51], g52 = ctx->gpr[52], g53 = ctx->gpr[53], g58 = ctx->gpr[58], g59 = ctx->gpr[59], g62 = ctx->gpr[62], g65 = ctx->gpr[65], g66 = ctx->gpr[66], g67 = ctx->gpr[67], g68 = ctx->gpr[68], g69 = ctx->gpr[69], g70 = ctx->gpr[70], g71 = ctx->gpr[71], g72 = ctx->gpr[72], g73 = ctx->gpr[73], g74 = ctx->gpr[74], g75 = ctx->gpr[75], g76 = ctx->gpr[76], g77 = ctx->gpr[77], g78 = ctx->gpr[78], g79 = ctx->gpr[79];
    do {
        g66 = spu_ls_read128(ctx, g29._u32[0] + g27._u32[0]);
        g47 = spu_ori(g17, 0);
        g8 = spu_shufb(g24, g30, g10);
        g17 = spu_a(g17, g5);
        g49 = spu_shufb(g31, g25, g10);
        g2 = spu_mpyi(g12, 6);
        g65 = spu_shufb(g25, g24, g10);
        g4 = spu_ai(g27, 2);
        g73 = spu_ls_read128(ctx, g26._u32[0] + g3._u32[0]);
        g21 = spu_ai(g21, -1);
        g76 = spu_ls_read128(ctx, g23._u32[0] + g3._u32[0]);
        g39 = spu_shufb(g22, g49, g28);
        g30 = spu_shufb(g49, g65, g19);
        g25 = spu_shufb(g65, g8, g19);
        g74 = spu_shufb(g8, g8, g19);
        g78 = spu_a(g2, g18);
        g62 = spu_rotqby(g66, g27);
        g27 = spu_ori(g4, 0);
        g43 = spu_ls_read128(ctx, g26._u32[0] + g7._u32[0]);
        g77 = spu_a(g78, g20);
        g42 = spu_ls_read128(ctx, g23._u32[0] + g7._u32[0]);
        g5 = spu_shufb(g30, g25, g73);
        g67 = spu_shufb(g39, g30, g76);
        g13 = spu_a(g62, g62);
        g40 = spu_shufb(g25, g74, g73);
        g71 = spu_and(g62, g35);
        g68 = spu_ls_read128(ctx, g23._u32[0] + g11._u32[0]);
        g75 = spu_and(g62, g13);
        g30 = spu_ls_read128(ctx, g77._u32[0] + 0x30);
        g28 = spu_rotmi(g71, -8);
        g50 = spu_shufb(g67, g5, g42);
        g72 = spu_and(g75, g37);
        g9 = spu_shufb(g5, g40, g43);
        g70 = spu_andi(g77, 15);
        g11 = spu_shufb(g39, g67, g34);
        g69 = spu_cntb(g72);
        g31 = spu_ls_read128(ctx, g77._u32[0] + 0x0);
        g59 = spu_andi(g62, 15);
        g45 = spu_shufb(g67, g50, g33);
        g46 = spu_and(g62, g36);
        g22 = spu_shufb(g50, g9, g68);
        g20 = spu_ori(g77, 0);
        g25 = spu_ls_read128(ctx, g77._u32[0] + 0x10);
        g51 = spu_rotmi(g69, -8);
        g24 = spu_ls_read128(ctx, g77._u32[0] + 0x20);
        g52 = spu_andi(g69, 255);
        spu_ls_write128(ctx, g47._u32[0] + 0x0, g11);
        g3 = spu_rotmi(g46, -4);
        g19 = spu_shufb(g50, g22, g32);
        g11 = spu_shli(g59, 4);
        spu_ls_write128(ctx, g47._u32[0] + 0x10, g45);
        g12 = spu_a(g51, g52);
        g79 = spu_shufb(g70, g70, g58);
        g5 = spu_il(48);
        spu_ls_write128(ctx, g47._u32[0] + 0x20, g19);
        g44 = spu_sfi(g12, 8);
        g19 = spu_ls_read128(ctx, g26._u32[0] + g28._u32[0]);
        g7 = spu_andi(g62, 240);
        g28 = spu_ls_read128(ctx, g23._u32[0] + g28._u32[0]);
        g18 = spu_a(g44, g44);
        g10 = spu_a(g79, g53);
    } while (g21._u32[0] != 0);
    ctx->gpr[66] = g66;
    ctx->gpr[47] = g47;
    ctx->gpr[8] = g8;
    ctx->gpr[17] = g17;
    ctx->gpr[49] = g49;
    ctx->gpr[2] = g2;
    ctx->gpr[65] = g65;
    ctx->gpr[4] = g4;
    ctx->gpr[73] = g73;
    ctx->gpr[21] = g21;
    ctx->gpr[76] = g76;
    ctx->gpr[39] = g39;
    ctx->gpr[30] = g30;
    ctx->gpr[25] = g25;
    ctx->gpr[74] = g74;
    ctx->gpr[78] = g78;
    ctx->gpr[62] = g62;
    ctx->gpr[27] = g27;
    ctx->gpr[43] = g43;
    ctx->gpr[77] = g77;
    ctx->gpr[42] = g42;
    ctx->gpr[5] = g5;
    ctx->gpr[67] = g67;
    ctx->gpr[13] = g13;
    ctx->gpr[40] = g40;
    ctx->gpr[71] = g71;
    ctx->gpr[68] = g68;
    ctx->gpr[75] = g75;
    ctx->gpr[28] = g28;
    ctx->gpr[50] = g50;
    ctx->gpr[72] = g72;
    ctx->gpr[9] = g9;
    ctx->gpr[70] = g70;
    ctx->gpr[11] = g11;
    ctx->gpr[69] = g69;
    ctx->gpr[31] = g31;
    ctx->gpr[59] = g59;
    ctx->gpr[45] = g45;
    ctx->gpr[46] = g46;
    ctx->gpr[22] = g22;
    ctx->gpr[20] = g20;
    ctx->gpr[51] = g51;
    ctx->gpr[24] = g24;
    ctx->gpr[52] = g52;
    ctx->gpr[3] = g3;
    ctx->gpr[19] = g19;
    ctx->gpr[12] = g12;
    ctx->gpr[79] = g79;
    ctx->gpr[44] = g44;
    ctx->gpr[7] = g7;
    ctx->gpr[18] = g18;
    ctx->gpr[10] = g10;
    return 1;
}
#endif
