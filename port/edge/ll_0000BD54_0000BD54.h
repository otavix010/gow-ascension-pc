/* GOWA: gowajm_0072DF80_spu_func_0000BD54__b loc_0000BD54 com registradores locais (gerado por scripts_edge/laco_local.py; não editar).
 * Mesmas operações e ordem do código traduzido: idêntico bit a bit. Faz o laço inteiro e devolve 1 (o
 * gancho salta para loc_0000BD54_fim, depois do desvio); 0 = nada feito (desligado). */
#ifndef GOWA_LL_0000BD54_0000BD54_H
#define GOWA_LL_0000BD54_0000BD54_H
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
static int ll_0000BD54_0000BD54(spu_context* ctx)
{
    if (!gowa_laco_local_on()) return 0;
    u128 g2 = ctx->gpr[2], g3 = ctx->gpr[3], g4 = ctx->gpr[4], g5 = ctx->gpr[5], g6 = ctx->gpr[6], g7 = ctx->gpr[7], g8 = ctx->gpr[8], g9 = ctx->gpr[9], g10 = ctx->gpr[10], g11 = ctx->gpr[11], g12 = ctx->gpr[12], g13 = ctx->gpr[13], g14 = ctx->gpr[14], g15 = ctx->gpr[15], g16 = ctx->gpr[16], g17 = ctx->gpr[17], g18 = ctx->gpr[18], g19 = ctx->gpr[19], g20 = ctx->gpr[20], g21 = ctx->gpr[21], g22 = ctx->gpr[22], g23 = ctx->gpr[23], g24 = ctx->gpr[24], g25 = ctx->gpr[25], g26 = ctx->gpr[26], g27 = ctx->gpr[27], g28 = ctx->gpr[28], g29 = ctx->gpr[29], g30 = ctx->gpr[30], g31 = ctx->gpr[31], g32 = ctx->gpr[32], g33 = ctx->gpr[33], g34 = ctx->gpr[34], g35 = ctx->gpr[35], g36 = ctx->gpr[36], g37 = ctx->gpr[37], g38 = ctx->gpr[38], g39 = ctx->gpr[39], g40 = ctx->gpr[40], g41 = ctx->gpr[41], g42 = ctx->gpr[42], g43 = ctx->gpr[43], g44 = ctx->gpr[44], g45 = ctx->gpr[45], g46 = ctx->gpr[46], g47 = ctx->gpr[47], g48 = ctx->gpr[48], g49 = ctx->gpr[49], g50 = ctx->gpr[50], g51 = ctx->gpr[51], g52 = ctx->gpr[52], g53 = ctx->gpr[53], g54 = ctx->gpr[54], g55 = ctx->gpr[55], g56 = ctx->gpr[56], g57 = ctx->gpr[57], g58 = ctx->gpr[58], g59 = ctx->gpr[59], g60 = ctx->gpr[60], g61 = ctx->gpr[61], g62 = ctx->gpr[62], g63 = ctx->gpr[63], g64 = ctx->gpr[64], g65 = ctx->gpr[65], g66 = ctx->gpr[66], g67 = ctx->gpr[67], g68 = ctx->gpr[68], g69 = ctx->gpr[69], g70 = ctx->gpr[70], g71 = ctx->gpr[71], g72 = ctx->gpr[72], g73 = ctx->gpr[73], g74 = ctx->gpr[74], g75 = ctx->gpr[75], g76 = ctx->gpr[76], g77 = ctx->gpr[77], g78 = ctx->gpr[78], g79 = ctx->gpr[79];
    do {
        g12 = spu_fma(g9, g9, g49);
        g15 = spu_fma(g7, g7, g10);
        g49 = spu_shufb(g16, g23, g51);
        g3 = spu_ori(g19, 0);
        g7 = spu_shufb(g16, g23, g68);
        g78 = spu_ai(g19, 128);
        g14 = spu_shufb(g16, g23, g66);
        g10 = spu_shufb(g16, g23, g56);
        g13 = spu_shufb(g18, g23, g51);
        g77 = spu_fm(g24, g49);
        g79 = spu_shufb(g18, g23, g68);
        g76 = spu_fm(g25, g7);
        g71 = spu_shufb(g39, g38, g31);
        g75 = spu_fm(g27, g14);
        g4 = spu_shufb(g37, g36, g22);
        g74 = spu_fm(g26, g10);
        g70 = spu_shufb(g35, g34, g31);
        g73 = spu_fm(g28, g13);
        g2 = spu_shufb(g33, g40, g22);
        g72 = spu_fm(g29, g79);
        g9 = spu_shufb(g18, g23, g66);
        g11 = spu_selb(g71, g4, g20);
        g6 = spu_shufb(g18, g23, g56);
        g19 = spu_ori(g78, 0);
        spu_ls_write128(ctx, g17._u32[0] + g3._u32[0], g77);
        g5 = spu_selb(g70, g2, g20);
        spu_ls_write128(ctx, g69._u32[0] + g3._u32[0], g76);
        g8 = spu_fm(g30, g9);
        spu_ls_write128(ctx, g67._u32[0] + g3._u32[0], g75);
        g29 = spu_fm(g32, g6);
        spu_ls_write128(ctx, g65._u32[0] + g3._u32[0], g74);
        g27 = spu_fm_r(g11, g11);
        g30 = spu_frsqest(g12);
        g25 = spu_fm_r(g5, g5);
        g28 = spu_frsqest(g15);
        g32 = spu_ori(g41, 0);
        spu_ls_write128(ctx, g64._u32[0] + g3._u32[0], g73);
        g41 = spu_ori(g40, 0);
        g26 = spu_shufb(g4, g71, g50);
        g16 = spu_fi(g12, g30);
        g24 = spu_shufb(g2, g70, g50);
        g18 = spu_fi(g15, g28);
        spu_ls_write128(ctx, g63._u32[0] + g3._u32[0], g72);
        g30 = spu_ori(g42, 0);
        spu_ls_write128(ctx, g62._u32[0] + g3._u32[0], g8);
        g49 = spu_fma(g26, g26, g27);
        spu_ls_write128(ctx, g61._u32[0] + g3._u32[0], g29);
        g10 = spu_fma(g24, g24, g25);
        g14 = spu_shufb(g39, g38, g21);
        g42 = spu_ori(g33, 0);
        g13 = spu_shufb(g37, g36, g21);
        g28 = spu_ori(g44, 0);
        g7 = spu_shufb(g33, g40, g21);
        g44 = spu_ori(g35, 0);
        g79 = spu_shufb(g35, g34, g21);
        g29 = spu_ori(g43, 0);
        g40 = spu_ls_read128(ctx, g52._u32[0] + g3._u32[0]);
        g43 = spu_ori(g34, 0);
        g33 = spu_ls_read128(ctx, g53._u32[0] + g3._u32[0]);
        g26 = spu_ori(g45, 0);
        g34 = spu_ls_read128(ctx, g54._u32[0] + g3._u32[0]);
        g45 = spu_ori(g36, 0);
        g35 = spu_ls_read128(ctx, g55._u32[0] + g3._u32[0]);
        g27 = spu_ori(g46, 0);
        g36 = spu_ls_read128(ctx, g57._u32[0] + g3._u32[0]);
        g46 = spu_ori(g37, 0);
        g37 = spu_ls_read128(ctx, g58._u32[0] + g3._u32[0]);
        g25 = spu_ori(g47, 0);
        g24 = spu_shlqbyi(g48, 0);
        g47 = spu_ori(g38, 0);
        g38 = spu_ls_read128(ctx, g59._u32[0] + g3._u32[0]);
        g48 = spu_ori(g39, 0);
        g39 = spu_ls_read128(ctx, g60._u32[0] + g3._u32[0]);
        g9 = spu_selb(g14, g13, g20);
        g7 = spu_selb(g79, g7, g20);
    } while (g78._u32[0] != 0);
    ctx->gpr[12] = g12;
    ctx->gpr[15] = g15;
    ctx->gpr[49] = g49;
    ctx->gpr[3] = g3;
    ctx->gpr[7] = g7;
    ctx->gpr[78] = g78;
    ctx->gpr[14] = g14;
    ctx->gpr[10] = g10;
    ctx->gpr[13] = g13;
    ctx->gpr[77] = g77;
    ctx->gpr[79] = g79;
    ctx->gpr[76] = g76;
    ctx->gpr[71] = g71;
    ctx->gpr[75] = g75;
    ctx->gpr[4] = g4;
    ctx->gpr[74] = g74;
    ctx->gpr[70] = g70;
    ctx->gpr[73] = g73;
    ctx->gpr[2] = g2;
    ctx->gpr[72] = g72;
    ctx->gpr[9] = g9;
    ctx->gpr[11] = g11;
    ctx->gpr[6] = g6;
    ctx->gpr[19] = g19;
    ctx->gpr[5] = g5;
    ctx->gpr[8] = g8;
    ctx->gpr[29] = g29;
    ctx->gpr[27] = g27;
    ctx->gpr[30] = g30;
    ctx->gpr[25] = g25;
    ctx->gpr[28] = g28;
    ctx->gpr[32] = g32;
    ctx->gpr[41] = g41;
    ctx->gpr[26] = g26;
    ctx->gpr[16] = g16;
    ctx->gpr[24] = g24;
    ctx->gpr[18] = g18;
    ctx->gpr[42] = g42;
    ctx->gpr[44] = g44;
    ctx->gpr[40] = g40;
    ctx->gpr[43] = g43;
    ctx->gpr[33] = g33;
    ctx->gpr[34] = g34;
    ctx->gpr[45] = g45;
    ctx->gpr[35] = g35;
    ctx->gpr[36] = g36;
    ctx->gpr[46] = g46;
    ctx->gpr[37] = g37;
    ctx->gpr[47] = g47;
    ctx->gpr[38] = g38;
    ctx->gpr[48] = g48;
    ctx->gpr[39] = g39;
    return 1;
}
#endif
