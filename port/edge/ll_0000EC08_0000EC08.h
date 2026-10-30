/* GOWA: gowajm_0072DF80_spu_func_0000EC08__b loc_0000EC08 com registradores locais (gerado por scripts_edge/laco_local.py; não editar).
 * Mesmas operações e ordem do código traduzido: idêntico bit a bit. Faz o laço inteiro e devolve 1 (o
 * gancho salta para loc_0000EC08_fim, depois do desvio); 0 = nada feito (desligado). */
#ifndef GOWA_LL_0000EC08_0000EC08_H
#define GOWA_LL_0000EC08_0000EC08_H
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
static int ll_0000EC08_0000EC08(spu_context* ctx)
{
    if (!gowa_laco_local_on()) return 0;
    u128 g2 = ctx->gpr[2], g3 = ctx->gpr[3], g4 = ctx->gpr[4], g5 = ctx->gpr[5], g6 = ctx->gpr[6], g7 = ctx->gpr[7], g8 = ctx->gpr[8], g9 = ctx->gpr[9], g10 = ctx->gpr[10], g11 = ctx->gpr[11], g12 = ctx->gpr[12], g13 = ctx->gpr[13], g14 = ctx->gpr[14], g15 = ctx->gpr[15], g16 = ctx->gpr[16], g17 = ctx->gpr[17], g18 = ctx->gpr[18], g19 = ctx->gpr[19], g20 = ctx->gpr[20], g21 = ctx->gpr[21], g22 = ctx->gpr[22], g23 = ctx->gpr[23], g24 = ctx->gpr[24], g25 = ctx->gpr[25], g26 = ctx->gpr[26], g27 = ctx->gpr[27], g28 = ctx->gpr[28], g30 = ctx->gpr[30], g31 = ctx->gpr[31], g33 = ctx->gpr[33], g34 = ctx->gpr[34], g35 = ctx->gpr[35], g36 = ctx->gpr[36], g37 = ctx->gpr[37], g38 = ctx->gpr[38], g40 = ctx->gpr[40], g41 = ctx->gpr[41], g42 = ctx->gpr[42], g43 = ctx->gpr[43], g44 = ctx->gpr[44], g45 = ctx->gpr[45], g46 = ctx->gpr[46], g47 = ctx->gpr[47], g48 = ctx->gpr[48], g49 = ctx->gpr[49], g50 = ctx->gpr[50], g51 = ctx->gpr[51], g52 = ctx->gpr[52], g53 = ctx->gpr[53], g54 = ctx->gpr[54], g55 = ctx->gpr[55], g56 = ctx->gpr[56], g57 = ctx->gpr[57], g58 = ctx->gpr[58], g59 = ctx->gpr[59], g60 = ctx->gpr[60], g61 = ctx->gpr[61], g69 = ctx->gpr[69], g70 = ctx->gpr[70], g77 = ctx->gpr[77], g78 = ctx->gpr[78], g80 = ctx->gpr[80], g81 = ctx->gpr[81], g82 = ctx->gpr[82], g83 = ctx->gpr[83], g84 = ctx->gpr[84], g86 = ctx->gpr[86], g91 = ctx->gpr[91], g93 = ctx->gpr[93], g94 = ctx->gpr[94], g95 = ctx->gpr[95], g97 = ctx->gpr[97], g102 = ctx->gpr[102];
    do {
        g80 = spu_ori(g4, 0);
        g78 = spu_ls_read128(ctx, g34._u32[0] + g3._u32[0]);
        g4 = spu_ori(g6, 0);
        g27 = spu_ls_read128(ctx, g35._u32[0] + g3._u32[0]);
        g58 = spu_ori(g11, 0);
        g83 = spu_ls_read128(ctx, g34._u32[0] + g80._u32[0]);
        g57 = spu_ori(g8, 0);
        g60 = spu_ls_read128(ctx, g35._u32[0] + g80._u32[0]);
        g48 = spu_ai(g48, -2);
        g28 = spu_ls_read128(ctx, g37._u32[0] + g80._u32[0]);
        g56 = spu_ls_read128(ctx, g37._u32[0] + g3._u32[0]);
        g22 = spu_ls_read128(ctx, g34._u32[0] + g4._u32[0]);
        g14 = spu_fm_r(g33, g78);
        g18 = spu_ls_read128(ctx, g34._u32[0] + g7._u32[0]);
        g16 = spu_fm_r(g33, g27);
        g26 = spu_ls_read128(ctx, g35._u32[0] + g4._u32[0]);
        g10 = spu_fm_r(g41, g83);
        g15 = spu_fm_r(g41, g60);
        g25 = spu_ls_read128(ctx, g35._u32[0] + g7._u32[0]);
        g2 = spu_fm_r(g41, g28);
        g21 = spu_ls_read128(ctx, g37._u32[0] + g4._u32[0]);
        g41 = spu_fm_r(g33, g56);
        g24 = spu_ls_read128(ctx, g37._u32[0] + g7._u32[0]);
        g13 = spu_fma_r(g40, g18, g14);
        g23 = spu_ls_read128(ctx, g34._u32[0] + g12._u32[0]);
        g31 = spu_ls_read128(ctx, g34._u32[0] + g9._u32[0]);
        g5 = spu_ls_read128(ctx, g35._u32[0] + g12._u32[0]);
        g17 = spu_fma_r(g42, g22, g10);
        g80 = spu_ls_read128(ctx, g35._u32[0] + g9._u32[0]);
        g19 = spu_fma_r(g42, g26, g15);
        g11 = spu_ls_read128(ctx, g37._u32[0] + g12._u32[0]);
        g20 = spu_fma_r(g40, g25, g16);
        g8 = spu_ls_read128(ctx, g37._u32[0] + g9._u32[0]);
        g83 = spu_fma_r(g42, g21, g2);
        g7 = spu_fma_r(g40, g24, g41);
        g4 = spu_ls_read128(ctx, g34._u32[0] + g58._u32[0]);
        g3 = spu_fma_r(g38, g31, g13);
        g12 = spu_ls_read128(ctx, g34._u32[0] + g57._u32[0]);
        g78 = spu_fma_r(g43, g23, g17);
        g61 = spu_ls_read128(ctx, g35._u32[0] + g58._u32[0]);
        g9 = spu_fma_r(g43, g5, g19);
        g59 = spu_ls_read128(ctx, g35._u32[0] + g57._u32[0]);
        g60 = spu_fma_r(g38, g80, g20);
        g58 = spu_ls_read128(ctx, g37._u32[0] + g58._u32[0]);
        g27 = spu_fma(g43, g11, g83);
        g57 = spu_ls_read128(ctx, g37._u32[0] + g57._u32[0]);
        g28 = spu_fma(g38, g8, g7);
        g6 = spu_ls_read128(ctx, g51._u32[0] + 0x0);
        g17 = spu_fma(g44, g12, g3);
        g18 = spu_fma(g45, g4, g78);
        g56 = spu_fma(g45, g61, g9);
        g25 = spu_fma(g44, g59, g60);
        g22 = spu_fma(g45, g58, g27);
        g24 = spu_fma(g44, g57, g28);
        g33 = spu_andhi(g6, 255);
        g30 = spu_shufb(g6, g81, g77);
        g15 = spu_selb(g84, g18, g84);
        g10 = spu_shufb(g6, g81, g102);
        g41 = spu_selb(g84, g56, g84);
        g26 = spu_shufb(g56, g18, g70);
        g38 = spu_selb(g84, g25, g84);
        g23 = spu_shufb(g25, g17, g70);
        g43 = spu_selb(g97, g22, g84);
        g42 = spu_selb(g97, g24, g84);
        g80 = spu_cuflt(g30, 155);
        g45 = spu_shufb(g26, g22, g69);
        g78 = spu_cuflt(g10, 155);
        g44 = spu_shufb(g23, g24, g69);
        g16 = spu_shli(g33, 4);
        g13 = spu_shli(g33, 5);
        g2 = spu_fma(g46, g43, g45);
        g46 = spu_ls_read128(ctx, g50._u32[0] + 0x0);
        g40 = spu_fma_r(g47, g42, g44);
        g47 = spu_ls_read128(ctx, g50._u32[0] + 0x10);
        g14 = spu_selb(g84, g17, g84);
        spu_ls_write128(ctx, g49._u32[0] + 0x0, g43);
        g6 = spu_a(g13, g16);
        spu_ls_write128(ctx, g49._u32[0] + 0x10, g41);
        g20 = spu_fm(g80, g91);
        spu_ls_write128(ctx, g49._u32[0] + 0x30, g42);
        g19 = spu_fm(g78, g91);
        g11 = spu_shlqbyi(g6, 4);
        g31 = spu_fma(g54, g41, g2);
        g7 = spu_shlqbyi(g6, 8);
        g5 = spu_fma(g55, g38, g40);
        g8 = spu_shlqbyi(g6, 12);
        g61 = spu_ai(g51, 16);
        spu_ls_write128(ctx, g49._u32[0] + 0x40, g38);
        g60 = spu_ai(g50, 32);
        spu_ls_write128(ctx, g49._u32[0] + 0x20, g15);
        g59 = spu_ai(g49, 96);
        spu_ls_write128(ctx, g49._u32[0] + 0x50, g14);
        g58 = spu_ai(g86, 32);
        g54 = spu_shufb(g46, g81, g36);
        g21 = spu_fma(g53, g15, g31);
        g55 = spu_shufb(g47, g81, g36);
        g83 = spu_fma(g52, g14, g5);
        g53 = spu_shufb(g46, g81, g82);
        g4 = spu_roti(g6, 16);
        g45 = spu_shufb(g20, g81, g93);
        g12 = spu_roti(g11, 16);
        g52 = spu_shufb(g47, g81, g82);
        g3 = spu_roti(g7, 16);
        g42 = spu_shufb(g20, g81, g95);
        g9 = spu_roti(g8, 16);
        g41 = spu_shufb(g20, g81, g81);
        g51 = spu_ori(g61, 0);
        g33 = spu_shufb(g19, g81, g81);
        g50 = spu_ori(g60, 0);
        g43 = spu_shufb(g20, g81, g94);
        g49 = spu_ori(g59, 0);
        g44 = spu_shufb(g19, g81, g93);
        g40 = spu_shufb(g19, g81, g95);
        g38 = spu_shufb(g19, g81, g94);
        spu_ls_write128(ctx, g86._u32[0] + 0x0, g21);
        spu_ls_write128(ctx, g86._u32[0] + 0x10, g83);
        g86 = spu_ori(g58, 0);
    } while (g48._u32[0] != 0);
    ctx->gpr[80] = g80;
    ctx->gpr[78] = g78;
    ctx->gpr[4] = g4;
    ctx->gpr[27] = g27;
    ctx->gpr[58] = g58;
    ctx->gpr[83] = g83;
    ctx->gpr[57] = g57;
    ctx->gpr[60] = g60;
    ctx->gpr[48] = g48;
    ctx->gpr[28] = g28;
    ctx->gpr[56] = g56;
    ctx->gpr[22] = g22;
    ctx->gpr[14] = g14;
    ctx->gpr[18] = g18;
    ctx->gpr[16] = g16;
    ctx->gpr[26] = g26;
    ctx->gpr[10] = g10;
    ctx->gpr[15] = g15;
    ctx->gpr[25] = g25;
    ctx->gpr[2] = g2;
    ctx->gpr[21] = g21;
    ctx->gpr[41] = g41;
    ctx->gpr[24] = g24;
    ctx->gpr[13] = g13;
    ctx->gpr[23] = g23;
    ctx->gpr[31] = g31;
    ctx->gpr[5] = g5;
    ctx->gpr[17] = g17;
    ctx->gpr[19] = g19;
    ctx->gpr[11] = g11;
    ctx->gpr[20] = g20;
    ctx->gpr[8] = g8;
    ctx->gpr[7] = g7;
    ctx->gpr[3] = g3;
    ctx->gpr[12] = g12;
    ctx->gpr[61] = g61;
    ctx->gpr[9] = g9;
    ctx->gpr[59] = g59;
    ctx->gpr[6] = g6;
    ctx->gpr[33] = g33;
    ctx->gpr[30] = g30;
    ctx->gpr[38] = g38;
    ctx->gpr[43] = g43;
    ctx->gpr[42] = g42;
    ctx->gpr[45] = g45;
    ctx->gpr[44] = g44;
    ctx->gpr[46] = g46;
    ctx->gpr[40] = g40;
    ctx->gpr[47] = g47;
    ctx->gpr[54] = g54;
    ctx->gpr[55] = g55;
    ctx->gpr[53] = g53;
    ctx->gpr[52] = g52;
    ctx->gpr[51] = g51;
    ctx->gpr[50] = g50;
    ctx->gpr[49] = g49;
    ctx->gpr[86] = g86;
    return 1;
}
#endif
