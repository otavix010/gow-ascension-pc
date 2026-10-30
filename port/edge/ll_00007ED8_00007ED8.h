/* GOWA: gowajm_0072DF80_spu_func_00007ED8__b loc_00007ED8 com registradores locais (gerado por scripts_edge/laco_local.py; não editar).
 * Mesmas operações e ordem do código traduzido: idêntico bit a bit. Faz o laço inteiro e devolve 1 (o
 * gancho salta para loc_00007ED8_fim, depois do desvio); 0 = nada feito (desligado). */
#ifndef GOWA_LL_00007ED8_00007ED8_H
#define GOWA_LL_00007ED8_00007ED8_H
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
static int ll_00007ED8_00007ED8(spu_context* ctx)
{
    if (!gowa_laco_local_on()) return 0;
    u128 g2 = ctx->gpr[2], g3 = ctx->gpr[3], g4 = ctx->gpr[4], g5 = ctx->gpr[5], g6 = ctx->gpr[6], g7 = ctx->gpr[7], g8 = ctx->gpr[8], g9 = ctx->gpr[9], g10 = ctx->gpr[10], g11 = ctx->gpr[11], g12 = ctx->gpr[12], g13 = ctx->gpr[13], g14 = ctx->gpr[14], g15 = ctx->gpr[15], g16 = ctx->gpr[16], g17 = ctx->gpr[17], g18 = ctx->gpr[18], g19 = ctx->gpr[19], g20 = ctx->gpr[20], g21 = ctx->gpr[21], g22 = ctx->gpr[22], g23 = ctx->gpr[23], g24 = ctx->gpr[24], g25 = ctx->gpr[25], g26 = ctx->gpr[26], g27 = ctx->gpr[27], g28 = ctx->gpr[28], g29 = ctx->gpr[29], g30 = ctx->gpr[30], g31 = ctx->gpr[31], g32 = ctx->gpr[32], g33 = ctx->gpr[33], g34 = ctx->gpr[34], g35 = ctx->gpr[35], g36 = ctx->gpr[36], g37 = ctx->gpr[37], g38 = ctx->gpr[38], g39 = ctx->gpr[39], g40 = ctx->gpr[40], g41 = ctx->gpr[41], g42 = ctx->gpr[42], g43 = ctx->gpr[43], g44 = ctx->gpr[44], g45 = ctx->gpr[45], g46 = ctx->gpr[46], g50 = ctx->gpr[50], g52 = ctx->gpr[52], g53 = ctx->gpr[53], g54 = ctx->gpr[54], g55 = ctx->gpr[55], g56 = ctx->gpr[56], g58 = ctx->gpr[58], g61 = ctx->gpr[61], g62 = ctx->gpr[62], g63 = ctx->gpr[63], g64 = ctx->gpr[64], g65 = ctx->gpr[65], g66 = ctx->gpr[66], g67 = ctx->gpr[67], g68 = ctx->gpr[68], g69 = ctx->gpr[69], g70 = ctx->gpr[70], g71 = ctx->gpr[71], g83 = ctx->gpr[83];
    do {
        g10 = spu_fcgt(g12, g33);
        g13 = spu_ls_read128(ctx, g30._u32[0] + 0x0);
        g4 = spu_cgti(g12, 0);
        g62 = spu_ls_read128(ctx, g30._u32[0] + 0x30);
        g11 = spu_selb(g12, g33, g10);
        g27 = spu_ls_read128(ctx, g30._u32[0] + 0x10);
        g23 = spu_fcgt(g5, g33);
        g28 = spu_ls_read128(ctx, g30._u32[0] + 0x40);
        g3 = spu_and(g11, g4);
        g63 = spu_ls_read128(ctx, g30._u32[0] + 0x50);
        g24 = spu_selb(g5, g33, g23);
        g66 = spu_ls_read128(ctx, g30._u32[0] + 0x20);
        g58 = spu_fm(g54, g3);
        g54 = spu_ls_read128(ctx, g31._u32[0] + 0x20);
        g61 = spu_fm(g56, g3);
        g56 = spu_ls_read128(ctx, g31._u32[0] + 0x0);
        g7 = spu_fm(g55, g3);
        g12 = spu_cgti(g5, 0);
        g55 = spu_ls_read128(ctx, g31._u32[0] + 0x10);
        g29 = spu_fm_r(g71, g13);
        g2 = spu_and(g24, g12);
        g10 = spu_xor(g58, g44);
        g5 = spu_fm(g53, g2);
        g53 = spu_ls_read128(ctx, g31._u32[0] + 0x30);
        g26 = spu_fm(g52, g2);
        g52 = spu_ls_read128(ctx, g31._u32[0] + 0x40);
        g8 = spu_fm(g50, g2);
        g50 = spu_ls_read128(ctx, g31._u32[0] + 0x50);
        g9 = spu_xor(g7, g44);
        g14 = spu_cgti(g61, 0);
        g65 = spu_fm_r(g70, g62);
        g4 = spu_and(g61, g14);
        g21 = spu_cgti(g9, 0);
        g22 = spu_cgti(g10, 0);
        g12 = spu_xor(g26, g44);
        g6 = spu_xor(g61, g44);
        g11 = spu_xor(g5, g44);
        g13 = spu_xor(g8, g44);
        g9 = spu_and(g9, g21);
        g21 = spu_and(g10, g22);
        g22 = spu_fa(g46, g4);
        g15 = spu_cgti(g7, 0);
        g16 = spu_cgti(g58, 0);
        g17 = spu_cgti(g5, 0);
        g18 = spu_cgti(g26, 0);
        g24 = spu_cgti(g12, 0);
        g19 = spu_cgti(g8, 0);
        g46 = spu_shlqbyi(g22, 0);
        g20 = spu_cgti(g6, 0);
        g23 = spu_cgti(g11, 0);
        g25 = spu_cgti(g13, 0);
        g29 = spu_fma(g69, g27, g29);
        g65 = spu_fma(g68, g28, g65);
        g62 = spu_and(g7, g15);
        g61 = spu_and(g58, g16);
        g14 = spu_and(g26, g18);
        g58 = spu_and(g5, g17);
        g16 = spu_and(g12, g24);
        g26 = spu_and(g8, g19);
        g6 = spu_and(g6, g20);
        g18 = spu_and(g13, g25);
        g20 = spu_and(g11, g23);
        g3 = spu_fa(g42, g62);
        g7 = spu_fa(g43, g61);
        g4 = spu_fa(g35, g58);
        g2 = spu_fa(g45, g14);
        g25 = spu_fa(g34, g26);
        g24 = spu_fa(g39, g6);
        g23 = spu_fa(g38, g9);
        g42 = spu_shlqbyi(g3, 0);
        g19 = spu_fa(g36, g21);
        g43 = spu_shlqbyi(g7, 0);
        g15 = spu_fa(g41, g20);
        g35 = spu_shlqbyi(g4, 0);
        g17 = spu_fa(g40, g16);
        g45 = spu_shlqbyi(g2, 0);
        g22 = spu_fa(g37, g18);
        g34 = spu_shlqbyi(g25, 0);
        g32 = spu_ai(g32, 1);
        g39 = spu_shlqbyi(g24, 0);
        g12 = spu_fma(g67, g66, g29);
        g38 = spu_shlqbyi(g23, 0);
        g5 = spu_fma(g64, g63, g65);
        g36 = spu_shlqbyi(g19, 0);
        g62 = spu_ai(g31, 96);
        g41 = spu_shlqbyi(g15, 0);
        g61 = spu_ai(g30, 96);
        g40 = spu_shlqbyi(g17, 0);
        g58 = spu_ceq(g83, g32);
        g37 = spu_shlqbyi(g22, 0);
        g31 = spu_ori(g62, 0);
        g30 = spu_shlqbyi(g61, 0);
    } while (g58._u32[0] == 0);
    ctx->gpr[10] = g10;
    ctx->gpr[13] = g13;
    ctx->gpr[4] = g4;
    ctx->gpr[62] = g62;
    ctx->gpr[11] = g11;
    ctx->gpr[27] = g27;
    ctx->gpr[23] = g23;
    ctx->gpr[28] = g28;
    ctx->gpr[3] = g3;
    ctx->gpr[63] = g63;
    ctx->gpr[24] = g24;
    ctx->gpr[66] = g66;
    ctx->gpr[58] = g58;
    ctx->gpr[54] = g54;
    ctx->gpr[61] = g61;
    ctx->gpr[56] = g56;
    ctx->gpr[7] = g7;
    ctx->gpr[12] = g12;
    ctx->gpr[55] = g55;
    ctx->gpr[29] = g29;
    ctx->gpr[2] = g2;
    ctx->gpr[5] = g5;
    ctx->gpr[53] = g53;
    ctx->gpr[26] = g26;
    ctx->gpr[52] = g52;
    ctx->gpr[8] = g8;
    ctx->gpr[50] = g50;
    ctx->gpr[9] = g9;
    ctx->gpr[14] = g14;
    ctx->gpr[65] = g65;
    ctx->gpr[21] = g21;
    ctx->gpr[22] = g22;
    ctx->gpr[6] = g6;
    ctx->gpr[15] = g15;
    ctx->gpr[16] = g16;
    ctx->gpr[17] = g17;
    ctx->gpr[18] = g18;
    ctx->gpr[19] = g19;
    ctx->gpr[46] = g46;
    ctx->gpr[20] = g20;
    ctx->gpr[25] = g25;
    ctx->gpr[42] = g42;
    ctx->gpr[43] = g43;
    ctx->gpr[35] = g35;
    ctx->gpr[45] = g45;
    ctx->gpr[34] = g34;
    ctx->gpr[32] = g32;
    ctx->gpr[39] = g39;
    ctx->gpr[38] = g38;
    ctx->gpr[36] = g36;
    ctx->gpr[41] = g41;
    ctx->gpr[40] = g40;
    ctx->gpr[37] = g37;
    ctx->gpr[31] = g31;
    ctx->gpr[30] = g30;
    return 1;
}
#endif
