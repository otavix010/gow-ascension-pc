/* GOWA: gowajm_0072DF80_spu_func_0000F620__b loc_0000F620 com registradores locais (gerado por scripts_edge/laco_local.py; não editar).
 * Mesmas operações e ordem do código traduzido: idêntico bit a bit. Faz o laço inteiro e devolve 1 (o
 * gancho salta para loc_0000F620_fim, depois do desvio); 0 = nada feito (desligado). */
#ifndef GOWA_LL_0000F620_0000F620_H
#define GOWA_LL_0000F620_0000F620_H
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
static int ll_0000F620_0000F620(spu_context* ctx)
{
    if (!gowa_laco_local_on()) return 0;
    u128 g2 = ctx->gpr[2], g3 = ctx->gpr[3], g4 = ctx->gpr[4], g5 = ctx->gpr[5], g6 = ctx->gpr[6], g7 = ctx->gpr[7], g8 = ctx->gpr[8], g9 = ctx->gpr[9], g10 = ctx->gpr[10], g11 = ctx->gpr[11], g12 = ctx->gpr[12], g13 = ctx->gpr[13], g14 = ctx->gpr[14], g15 = ctx->gpr[15], g16 = ctx->gpr[16], g17 = ctx->gpr[17], g18 = ctx->gpr[18], g19 = ctx->gpr[19], g20 = ctx->gpr[20], g21 = ctx->gpr[21], g22 = ctx->gpr[22], g23 = ctx->gpr[23], g24 = ctx->gpr[24], g25 = ctx->gpr[25], g26 = ctx->gpr[26], g27 = ctx->gpr[27], g28 = ctx->gpr[28], g29 = ctx->gpr[29], g30 = ctx->gpr[30], g31 = ctx->gpr[31], g32 = ctx->gpr[32], g33 = ctx->gpr[33], g34 = ctx->gpr[34], g35 = ctx->gpr[35], g36 = ctx->gpr[36], g37 = ctx->gpr[37], g38 = ctx->gpr[38], g39 = ctx->gpr[39], g40 = ctx->gpr[40], g41 = ctx->gpr[41], g42 = ctx->gpr[42], g43 = ctx->gpr[43], g44 = ctx->gpr[44], g45 = ctx->gpr[45], g46 = ctx->gpr[46], g47 = ctx->gpr[47], g48 = ctx->gpr[48], g49 = ctx->gpr[49], g50 = ctx->gpr[50], g51 = ctx->gpr[51], g52 = ctx->gpr[52], g53 = ctx->gpr[53], g54 = ctx->gpr[54], g55 = ctx->gpr[55], g56 = ctx->gpr[56], g57 = ctx->gpr[57], g58 = ctx->gpr[58], g59 = ctx->gpr[59], g60 = ctx->gpr[60], g61 = ctx->gpr[61], g62 = ctx->gpr[62], g63 = ctx->gpr[63], g64 = ctx->gpr[64], g65 = ctx->gpr[65], g66 = ctx->gpr[66], g67 = ctx->gpr[67], g68 = ctx->gpr[68], g69 = ctx->gpr[69], g70 = ctx->gpr[70], g72 = ctx->gpr[72], g73 = ctx->gpr[73], g74 = ctx->gpr[74], g75 = ctx->gpr[75], g76 = ctx->gpr[76], g77 = ctx->gpr[77], g78 = ctx->gpr[78], g79 = ctx->gpr[79];
    do {
        g7 = spu_fma(g53, g34, g9);
        g29 = spu_ls_read128(ctx, g38._u32[0] + 0x10);
        g2 = spu_fma(g52, g34, g43);
        g4 = spu_ls_read128(ctx, g38._u32[0] + 0x0);
        g5 = spu_fma(g51, g34, g41);
        g19 = spu_ls_read128(ctx, g38._u32[0] + 0x70);
        g76 = spu_fma(g57, g34, g50);
        g6 = spu_ls_read128(ctx, g38._u32[0] + 0x20);
        g73 = spu_fma(g56, g34, g49);
        g13 = spu_ls_read128(ctx, g38._u32[0] + 0x30);
        g72 = spu_fma(g55, g34, g48);
        g14 = spu_ls_read128(ctx, g38._u32[0] + 0x40);
        g30 = spu_fm_r(g29, g35);
        g15 = spu_ls_read128(ctx, g38._u32[0] + 0x50);
        g31 = spu_fm_r(g4, g35);
        g16 = spu_ls_read128(ctx, g38._u32[0] + 0x60);
        g27 = spu_fm(g19, g35);
        g59 = spu_rotqbyi(g29, 4);
        g10 = spu_fm(g6, g35);
        g60 = spu_rotqbyi(g4, 4);
        g11 = spu_fm(g13, g35);
        spu_ls_write128(ctx, g58._u32[0] + g39._u32[0], g20);
        g12 = spu_fm(g14, g35);
        spu_ls_write128(ctx, g54._u32[0] + g39._u32[0], g21);
        g28 = spu_fm_r(g15, g35);
        g20 = spu_rotqbyi(g29, 8);
        g17 = spu_fma(g60, g36, g31);
        g21 = spu_rotqbyi(g4, 8);
        g18 = spu_fma(g59, g36, g30);
        g22 = spu_rotqbyi(g6, 4);
        g8 = spu_fm(g16, g35);
        g23 = spu_rotqbyi(g13, 4);
        g37 = spu_ai(g37, -1);
        g24 = spu_rotqbyi(g14, 4);
        g25 = spu_rotqbyi(g15, 4);
        g26 = spu_rotqbyi(g16, 4);
        g31 = spu_fma(g21, g33, g17);
        g3 = spu_rotqbyi(g19, 4);
        g30 = spu_fma(g20, g33, g18);
        g59 = spu_rotqbyi(g29, 12);
        g66 = spu_fma(g25, g36, g28);
        g60 = spu_rotqbyi(g4, 12);
        g75 = spu_fma(g22, g36, g10);
        g74 = spu_rotqbyi(g6, 8);
        g70 = spu_fma(g23, g36, g11);
        g69 = spu_rotqbyi(g13, 8);
        g68 = spu_fma(g24, g36, g12);
        g67 = spu_rotqbyi(g14, 8);
        g64 = spu_fma(g26, g36, g8);
        g65 = spu_rotqbyi(g15, 8);
        g62 = spu_fma(g3, g36, g27);
        g63 = spu_rotqbyi(g16, 8);
        g20 = spu_fma(g60, g34, g31);
        g61 = spu_rotqbyi(g19, 8);
        g9 = spu_fma(g74, g33, g75);
        spu_ls_write128(ctx, g47._u32[0] + g39._u32[0], g7);
        g43 = spu_fma(g69, g33, g70);
        spu_ls_write128(ctx, g46._u32[0] + g39._u32[0], g2);
        g41 = spu_fma(g67, g33, g68);
        g79 = spu_rotqbyi(g15, 12);
        g50 = spu_fma(g65, g33, g66);
        g78 = spu_rotqbyi(g16, 12);
        g49 = spu_fma(g63, g33, g64);
        g77 = spu_rotqbyi(g19, 12);
        g48 = spu_fma(g61, g33, g62);
        spu_ls_write128(ctx, g45._u32[0] + g39._u32[0], g5);
        g21 = spu_fma(g59, g34, g30);
        spu_ls_write128(ctx, g44._u32[0] + g39._u32[0], g76);
        g28 = spu_a(g39, g32);
        spu_ls_write128(ctx, g42._u32[0] + g39._u32[0], g73);
        g29 = spu_ai(g38, 128);
        g53 = spu_rotqbyi(g6, 12);
        g57 = spu_ori(g79, 0);
        g52 = spu_rotqbyi(g13, 12);
        g56 = spu_ori(g78, 0);
        g51 = spu_rotqbyi(g14, 12);
        g55 = spu_ori(g77, 0);
        spu_ls_write128(ctx, g40._u32[0] + g39._u32[0], g72);
        g38 = spu_ori(g29, 0);
        g39 = spu_shlqbyi(g28, 0);
        g32 = spu_il(128);
    } while (g37._u32[0] != 0);
    ctx->gpr[7] = g7;
    ctx->gpr[29] = g29;
    ctx->gpr[2] = g2;
    ctx->gpr[4] = g4;
    ctx->gpr[5] = g5;
    ctx->gpr[19] = g19;
    ctx->gpr[76] = g76;
    ctx->gpr[6] = g6;
    ctx->gpr[73] = g73;
    ctx->gpr[13] = g13;
    ctx->gpr[72] = g72;
    ctx->gpr[14] = g14;
    ctx->gpr[30] = g30;
    ctx->gpr[15] = g15;
    ctx->gpr[31] = g31;
    ctx->gpr[16] = g16;
    ctx->gpr[27] = g27;
    ctx->gpr[59] = g59;
    ctx->gpr[10] = g10;
    ctx->gpr[60] = g60;
    ctx->gpr[11] = g11;
    ctx->gpr[12] = g12;
    ctx->gpr[28] = g28;
    ctx->gpr[20] = g20;
    ctx->gpr[17] = g17;
    ctx->gpr[21] = g21;
    ctx->gpr[18] = g18;
    ctx->gpr[22] = g22;
    ctx->gpr[8] = g8;
    ctx->gpr[23] = g23;
    ctx->gpr[37] = g37;
    ctx->gpr[24] = g24;
    ctx->gpr[25] = g25;
    ctx->gpr[26] = g26;
    ctx->gpr[3] = g3;
    ctx->gpr[66] = g66;
    ctx->gpr[75] = g75;
    ctx->gpr[74] = g74;
    ctx->gpr[70] = g70;
    ctx->gpr[69] = g69;
    ctx->gpr[68] = g68;
    ctx->gpr[67] = g67;
    ctx->gpr[64] = g64;
    ctx->gpr[65] = g65;
    ctx->gpr[62] = g62;
    ctx->gpr[63] = g63;
    ctx->gpr[61] = g61;
    ctx->gpr[9] = g9;
    ctx->gpr[43] = g43;
    ctx->gpr[41] = g41;
    ctx->gpr[79] = g79;
    ctx->gpr[50] = g50;
    ctx->gpr[78] = g78;
    ctx->gpr[49] = g49;
    ctx->gpr[77] = g77;
    ctx->gpr[48] = g48;
    ctx->gpr[53] = g53;
    ctx->gpr[57] = g57;
    ctx->gpr[52] = g52;
    ctx->gpr[56] = g56;
    ctx->gpr[51] = g51;
    ctx->gpr[55] = g55;
    ctx->gpr[38] = g38;
    ctx->gpr[39] = g39;
    ctx->gpr[32] = g32;
    return 1;
}
#endif
