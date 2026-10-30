/* GOWA: gowajm_0072DF80_spu_func_0000D4DC__b loc_0000D4DC com registradores locais (gerado por scripts_edge/laco_local.py; não editar).
 * Mesmas operações e ordem do código traduzido: idêntico bit a bit. Faz o laço inteiro e devolve 1 (o
 * gancho salta para loc_0000D4DC_fim, depois do desvio); 0 = nada feito (desligado). */
#ifndef GOWA_LL_0000D4DC_0000D4DC_H
#define GOWA_LL_0000D4DC_0000D4DC_H
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
static int ll_0000D4DC_0000D4DC(spu_context* ctx)
{
    if (!gowa_laco_local_on()) return 0;
    u128 g2 = ctx->gpr[2], g3 = ctx->gpr[3], g4 = ctx->gpr[4], g5 = ctx->gpr[5], g6 = ctx->gpr[6], g7 = ctx->gpr[7], g8 = ctx->gpr[8], g9 = ctx->gpr[9], g10 = ctx->gpr[10], g11 = ctx->gpr[11], g12 = ctx->gpr[12], g13 = ctx->gpr[13], g14 = ctx->gpr[14], g15 = ctx->gpr[15], g16 = ctx->gpr[16], g17 = ctx->gpr[17], g18 = ctx->gpr[18], g19 = ctx->gpr[19], g20 = ctx->gpr[20], g21 = ctx->gpr[21], g22 = ctx->gpr[22], g23 = ctx->gpr[23], g24 = ctx->gpr[24], g25 = ctx->gpr[25], g26 = ctx->gpr[26], g27 = ctx->gpr[27], g28 = ctx->gpr[28], g29 = ctx->gpr[29], g30 = ctx->gpr[30], g31 = ctx->gpr[31], g32 = ctx->gpr[32], g33 = ctx->gpr[33], g34 = ctx->gpr[34], g35 = ctx->gpr[35], g36 = ctx->gpr[36], g37 = ctx->gpr[37], g38 = ctx->gpr[38], g39 = ctx->gpr[39], g40 = ctx->gpr[40], g41 = ctx->gpr[41], g42 = ctx->gpr[42], g43 = ctx->gpr[43], g44 = ctx->gpr[44], g45 = ctx->gpr[45], g46 = ctx->gpr[46], g47 = ctx->gpr[47], g48 = ctx->gpr[48], g49 = ctx->gpr[49], g50 = ctx->gpr[50], g51 = ctx->gpr[51], g52 = ctx->gpr[52], g53 = ctx->gpr[53], g54 = ctx->gpr[54], g55 = ctx->gpr[55], g56 = ctx->gpr[56], g57 = ctx->gpr[57], g58 = ctx->gpr[58], g59 = ctx->gpr[59], g60 = ctx->gpr[60], g61 = ctx->gpr[61], g62 = ctx->gpr[62], g63 = ctx->gpr[63], g64 = ctx->gpr[64], g65 = ctx->gpr[65], g66 = ctx->gpr[66], g67 = ctx->gpr[67], g68 = ctx->gpr[68], g69 = ctx->gpr[69], g70 = ctx->gpr[70], g71 = ctx->gpr[71], g72 = ctx->gpr[72], g73 = ctx->gpr[73], g74 = ctx->gpr[74], g75 = ctx->gpr[75], g76 = ctx->gpr[76], g77 = ctx->gpr[77], g78 = ctx->gpr[78], g79 = ctx->gpr[79], g80 = ctx->gpr[80];
    do {
        g10 = spu_fi(g24, g5);
        g9 = spu_fcgt(g64, g11);
        g20 = spu_ls_read128(ctx, g29._u32[0] + g23._u32[0]);
        g4 = spu_fcgt(g22, g60);
        g19 = spu_ls_read128(ctx, g33._u32[0] + g23._u32[0]);
        g14 = spu_fcgt(g6, g57);
        g8 = spu_ls_read128(ctx, g39._u32[0] + g23._u32[0]);
        g3 = spu_fcgt(g11, g63);
        g16 = spu_ls_read128(ctx, g32._u32[0] + g23._u32[0]);
        g2 = spu_fcgt(g62, g22);
        g17 = spu_shufb(g22, g6, g38);
        g7 = spu_fcgt(g61, g6);
        g18 = spu_shufb(g22, g6, g59);
        g21 = spu_a(g21, g31);
        g5 = spu_shufb(g4, g2, g27);
        g2 = spu_fnms_r(g10, g24, g71);
        g13 = spu_shufb(g14, g7, g27);
        g15 = spu_shufb(g8, g19, g38);
        g11 = spu_shufb(g16, g20, g78);
        g22 = spu_shufb(g3, g9, g27);
        g6 = spu_shufb(g5, g13, g38);
        g9 = spu_and(g24, g74);
        g8 = spu_shufb(g8, g19, g79);
        g4 = spu_selb(g15, g11, g50);
        g14 = spu_shufb(g11, g15, g80);
        g2 = spu_fma(g2, g10, g10);
        g11 = spu_shufb(g22, g12, g70);
        g15 = spu_fma_r(g4, g55, g56);
        g3 = spu_shufb(g22, g12, g75);
        g12 = spu_fma_r(g4, g48, g52);
        g22 = spu_shufb(g5, g13, g59);
        g13 = spu_fma_r(g4, g47, g51);
        g16 = spu_shufb(g16, g20, g79);
        g4 = spu_fma_r(g4, g46, g49);
        g10 = spu_shufb(g6, g11, g28);
        g19 = spu_shufb(g6, g11, g25);
        g7 = spu_shufb(g22, g3, g25);
        g15 = spu_fma(g14, g54, g15);
        g5 = spu_shufb(g22, g3, g28);
        g22 = spu_fm(g65, g2);
        g20 = spu_gbh(g10);
        g11 = spu_fm(g36, g2);
        g65 = spu_gbh(g19);
        g36 = spu_fm(g26, g2);
        g26 = spu_gbh(g7);
        g19 = spu_fma(g14, g45, g12);
        g5 = spu_gbh(g5);
        g13 = spu_fma(g14, g44, g13);
        g10 = spu_shufb(g17, g20, g30);
        g20 = spu_selb(g8, g16, g50);
        g17 = spu_shufb(g17, g65, g25);
        g14 = spu_fma(g14, g43, g4);
        g7 = spu_shufb(g18, g26, g25);
        g3 = spu_xor(g9, g22);
        g18 = spu_shufb(g18, g5, g30);
        g16 = spu_xor(g9, g36);
        g31 = spu_shufb(g31, g31, g69);
        g4 = spu_xor(g9, g11);
        spu_ls_write128(ctx, g66._u32[0] + g21._u32[0], g17);
        g17 = spu_fma(g20, g53, g15);
        spu_ls_write128(ctx, g37._u32[0] + g21._u32[0], g10);
        g22 = spu_fma(g3, g77, g76);
        spu_ls_write128(ctx, g35._u32[0] + g21._u32[0], g7);
        g3 = spu_ai(g23, 64);
        spu_ls_write128(ctx, g34._u32[0] + g21._u32[0], g18);
        g12 = spu_fcgt(g24, g58);
        g6 = spu_fma(g16, g73, g72);
        g11 = spu_fma(g4, g68, g67);
        g23 = spu_shlqbyi(g3, 0);
        g65 = spu_fma(g20, g42, g19);
        g5 = spu_frest(g17);
        g24 = spu_ori(g17, 0);
        g26 = spu_fma(g20, g41, g13);
        g36 = spu_fma(g20, g40, g14);
    } while (g3._u32[0] != 0);
    ctx->gpr[10] = g10;
    ctx->gpr[9] = g9;
    ctx->gpr[20] = g20;
    ctx->gpr[4] = g4;
    ctx->gpr[19] = g19;
    ctx->gpr[14] = g14;
    ctx->gpr[8] = g8;
    ctx->gpr[3] = g3;
    ctx->gpr[16] = g16;
    ctx->gpr[2] = g2;
    ctx->gpr[17] = g17;
    ctx->gpr[7] = g7;
    ctx->gpr[18] = g18;
    ctx->gpr[21] = g21;
    ctx->gpr[5] = g5;
    ctx->gpr[13] = g13;
    ctx->gpr[15] = g15;
    ctx->gpr[11] = g11;
    ctx->gpr[22] = g22;
    ctx->gpr[6] = g6;
    ctx->gpr[12] = g12;
    ctx->gpr[65] = g65;
    ctx->gpr[36] = g36;
    ctx->gpr[26] = g26;
    ctx->gpr[31] = g31;
    ctx->gpr[23] = g23;
    ctx->gpr[24] = g24;
    return 1;
}
#endif
