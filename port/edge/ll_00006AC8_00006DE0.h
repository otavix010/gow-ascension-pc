/* GOWA: gowajm_0072DF80_spu_func_00006AC8__b loc_00006DE0 com registradores locais (gerado por scripts_edge/laco_local.py; não editar).
 * Mesmas operações e ordem do código traduzido: idêntico bit a bit. Faz o laço inteiro e devolve 1 (o
 * gancho salta para loc_00006DE0_fim, depois do desvio); 0 = nada feito (desligado). */
#ifndef GOWA_LL_00006AC8_00006DE0_H
#define GOWA_LL_00006AC8_00006DE0_H
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
static int ll_00006AC8_00006DE0(spu_context* ctx)
{
    if (!gowa_laco_local_on()) return 0;
    u128 g2 = ctx->gpr[2], g3 = ctx->gpr[3], g4 = ctx->gpr[4], g5 = ctx->gpr[5], g6 = ctx->gpr[6], g7 = ctx->gpr[7], g8 = ctx->gpr[8], g9 = ctx->gpr[9], g10 = ctx->gpr[10], g11 = ctx->gpr[11], g12 = ctx->gpr[12], g13 = ctx->gpr[13], g14 = ctx->gpr[14], g15 = ctx->gpr[15], g16 = ctx->gpr[16], g17 = ctx->gpr[17], g18 = ctx->gpr[18], g19 = ctx->gpr[19], g20 = ctx->gpr[20], g21 = ctx->gpr[21], g22 = ctx->gpr[22], g23 = ctx->gpr[23], g24 = ctx->gpr[24], g25 = ctx->gpr[25], g26 = ctx->gpr[26], g27 = ctx->gpr[27], g28 = ctx->gpr[28], g29 = ctx->gpr[29], g30 = ctx->gpr[30], g31 = ctx->gpr[31], g32 = ctx->gpr[32], g33 = ctx->gpr[33], g35 = ctx->gpr[35], g36 = ctx->gpr[36], g37 = ctx->gpr[37], g38 = ctx->gpr[38], g39 = ctx->gpr[39], g40 = ctx->gpr[40], g41 = ctx->gpr[41], g42 = ctx->gpr[42], g43 = ctx->gpr[43], g44 = ctx->gpr[44], g45 = ctx->gpr[45], g46 = ctx->gpr[46], g47 = ctx->gpr[47], g50 = ctx->gpr[50], g51 = ctx->gpr[51], g52 = ctx->gpr[52], g53 = ctx->gpr[53], g64 = ctx->gpr[64], g65 = ctx->gpr[65], g66 = ctx->gpr[66];
    do {
        g13 = spu_fm(g4, g15);
        g6 = spu_frsqest(g21);
        g16 = spu_fm(g3, g15);
        g18 = spu_frsqest(g22);
        g33 = spu_ai(g33, 1);
        g10 = spu_ls_read128(ctx, g51._u32[0] + g19._u32[0]);
        g17 = spu_ls_read128(ctx, g50._u32[0] + g19._u32[0]);
        g5 = spu_fi(g21, g6);
        g6 = spu_fi(g22, g18);
        g4 = spu_fcgt(g13, g29);
        g8 = spu_fcgt(g16, g29);
        g7 = spu_selb(g13, g29, g4);
        g14 = spu_selb(g16, g29, g8);
        g15 = spu_shufb(g10, g10, g47);
        g2 = spu_fs_r(g29, g7);
        g9 = spu_shufb(g10, g10, g41);
        g3 = spu_fs_r(g29, g14);
        g18 = spu_shufb(g10, g10, g39);
        g11 = spu_fm_r(g5, g21);
        g10 = spu_shufb(g10, g10, g35);
        g8 = spu_fm_r(g6, g22);
        g4 = spu_fs_r(g15, g43);
        g7 = spu_fs_r(g15, g44);
        g12 = spu_fnms_r(g66, g2, g65);
        g16 = spu_fm_r(g2, g2);
        g13 = spu_fnms_r(g66, g3, g65);
        g14 = spu_fm_r(g3, g3);
        g11 = spu_fnms_r(g11, g5, g29);
        g15 = spu_fs_r(g9, g40);
        g4 = spu_fm_r(g4, g4);
        g2 = spu_fm_r(g16, g12);
        g12 = spu_fm_r(g5, g52);
        g16 = spu_fm_r(g14, g13);
        g14 = spu_fnms_r(g8, g6, g29);
        g13 = spu_fm_r(g6, g52);
        g7 = spu_fm_r(g7, g7);
        g2 = spu_fm_r(g2, g2);
        g11 = spu_fma_r(g11, g12, g5);
        g3 = spu_fm_r(g16, g16);
        g16 = spu_fs_r(g9, g42);
        g8 = spu_fma_r(g14, g13, g6);
        g15 = spu_fma_r(g15, g15, g4);
        g5 = spu_fm(g2, g23);
        g14 = spu_fs(g18, g38);
        g3 = spu_fm(g3, g23);
        g6 = spu_fma_r(g16, g16, g7);
        g23 = spu_fm(g53, g17);
        g16 = spu_fs(g18, g37);
        g8 = spu_fm(g8, g22);
        g4 = spu_shufb(g5, g5, g47);
        g11 = spu_fm(g11, g21);
        g12 = spu_shufb(g5, g5, g35);
        g19 = spu_ai(g19, 16);
        g22 = spu_shufb(g5, g5, g41);
        g21 = spu_fma(g14, g14, g6);
        g18 = spu_shufb(g5, g5, g39);
        g7 = spu_fma(g4, g20, g26);
        g13 = spu_shufb(g3, g3, g35);
        g12 = spu_fma(g12, g20, g31);
        g26 = spu_shufb(g3, g3, g39);
        g6 = spu_fma(g22, g20, g32);
        g31 = spu_shufb(g3, g3, g47);
        g2 = spu_fma(g18, g20, g25);
        g9 = spu_shufb(g3, g3, g41);
        g13 = spu_fma(g13, g20, g27);
        g25 = spu_shufb(g23, g23, g35);
        g27 = spu_fma(g26, g20, g30);
        g3 = spu_shlqbyi(g8, 0);
        g5 = spu_fma(g31, g20, g24);
        g26 = spu_shlqbyi(g7, 0);
        g4 = spu_fma(g9, g20, g28);
        g31 = spu_shlqbyi(g12, 0);
        g22 = spu_fma(g16, g16, g15);
        g20 = spu_shlqbyi(g36, 0);
        g18 = spu_ceq(g64, g33);
        g15 = spu_shlqbyi(g46, 0);
        g36 = spu_ori(g23, 0);
        g46 = spu_shlqbyi(g10, 0);
        g23 = spu_ori(g45, 0);
        g30 = spu_shlqbyi(g27, 0);
        g45 = spu_ori(g25, 0);
        g27 = spu_shlqbyi(g13, 0);
        g28 = spu_ori(g4, 0);
        g25 = spu_shlqbyi(g2, 0);
        g4 = spu_ori(g11, 0);
        g32 = spu_shlqbyi(g6, 0);
        g24 = spu_ori(g5, 0);
    } while (g18._u32[0] == 0);
    ctx->gpr[13] = g13;
    ctx->gpr[6] = g6;
    ctx->gpr[16] = g16;
    ctx->gpr[18] = g18;
    ctx->gpr[33] = g33;
    ctx->gpr[10] = g10;
    ctx->gpr[17] = g17;
    ctx->gpr[5] = g5;
    ctx->gpr[4] = g4;
    ctx->gpr[8] = g8;
    ctx->gpr[7] = g7;
    ctx->gpr[14] = g14;
    ctx->gpr[15] = g15;
    ctx->gpr[2] = g2;
    ctx->gpr[9] = g9;
    ctx->gpr[3] = g3;
    ctx->gpr[11] = g11;
    ctx->gpr[12] = g12;
    ctx->gpr[23] = g23;
    ctx->gpr[19] = g19;
    ctx->gpr[22] = g22;
    ctx->gpr[21] = g21;
    ctx->gpr[26] = g26;
    ctx->gpr[31] = g31;
    ctx->gpr[25] = g25;
    ctx->gpr[27] = g27;
    ctx->gpr[20] = g20;
    ctx->gpr[36] = g36;
    ctx->gpr[46] = g46;
    ctx->gpr[30] = g30;
    ctx->gpr[45] = g45;
    ctx->gpr[28] = g28;
    ctx->gpr[32] = g32;
    ctx->gpr[24] = g24;
    return 1;
}
#endif
