/* GOWA: gowajm_0072DF80_spu_func_0000F24C__b loc_0000F24C com registradores locais (gerado por scripts_edge/laco_local.py; não editar).
 * Mesmas operações e ordem do código traduzido: idêntico bit a bit. Faz o laço inteiro e devolve 1 (o
 * gancho salta para loc_0000F24C_fim, depois do desvio); 0 = nada feito (desligado). */
#ifndef GOWA_LL_0000F24C_0000F24C_H
#define GOWA_LL_0000F24C_0000F24C_H
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
static int ll_0000F24C_0000F24C(spu_context* ctx)
{
    if (!gowa_laco_local_on()) return 0;
    u128 g4 = ctx->gpr[4], g5 = ctx->gpr[5], g6 = ctx->gpr[6], g8 = ctx->gpr[8], g9 = ctx->gpr[9], g10 = ctx->gpr[10], g11 = ctx->gpr[11], g12 = ctx->gpr[12], g13 = ctx->gpr[13], g14 = ctx->gpr[14], g15 = ctx->gpr[15], g16 = ctx->gpr[16], g17 = ctx->gpr[17], g18 = ctx->gpr[18], g19 = ctx->gpr[19], g20 = ctx->gpr[20], g21 = ctx->gpr[21], g22 = ctx->gpr[22], g23 = ctx->gpr[23], g24 = ctx->gpr[24], g25 = ctx->gpr[25], g26 = ctx->gpr[26], g27 = ctx->gpr[27], g28 = ctx->gpr[28], g29 = ctx->gpr[29], g30 = ctx->gpr[30], g31 = ctx->gpr[31], g32 = ctx->gpr[32], g33 = ctx->gpr[33], g34 = ctx->gpr[34], g35 = ctx->gpr[35], g36 = ctx->gpr[36], g37 = ctx->gpr[37], g38 = ctx->gpr[38], g40 = ctx->gpr[40], g41 = ctx->gpr[41], g42 = ctx->gpr[42], g43 = ctx->gpr[43], g44 = ctx->gpr[44], g45 = ctx->gpr[45], g46 = ctx->gpr[46], g47 = ctx->gpr[47], g48 = ctx->gpr[48], g49 = ctx->gpr[49], g50 = ctx->gpr[50], g51 = ctx->gpr[51], g52 = ctx->gpr[52], g53 = ctx->gpr[53], g54 = ctx->gpr[54], g55 = ctx->gpr[55], g56 = ctx->gpr[56], g57 = ctx->gpr[57], g58 = ctx->gpr[58], g59 = ctx->gpr[59], g60 = ctx->gpr[60], g61 = ctx->gpr[61], g62 = ctx->gpr[62], g65 = ctx->gpr[65], g68 = ctx->gpr[68], g79 = ctx->gpr[79], g80 = ctx->gpr[80], g81 = ctx->gpr[81], g82 = ctx->gpr[82], g83 = ctx->gpr[83];
    do {
        g8 = spu_ls_read128(ctx, g65._u32[0] + 0x0);
        g19 = spu_fm_r(g37, g52);
        g5 = spu_ls_read128(ctx, g65._u32[0] + 0x30);
        g20 = spu_fm_r(g35, g49);
        g9 = spu_ls_read128(ctx, g65._u32[0] + 0x10);
        g27 = spu_fm_r(g34, g46);
        g10 = spu_ls_read128(ctx, g65._u32[0] + 0x20);
        g18 = spu_fm_r(g38, g43);
        g13 = spu_ls_read128(ctx, g68._u32[0] + 0x0);
        g61 = spu_ai(g61, -4);
        g6 = spu_ls_read128(ctx, g68._u32[0] + 0x10);
        g26 = spu_fm_r(g8, g52);
        g83 = spu_ls_read128(ctx, g68._u32[0] + 0x20);
        g29 = spu_fm_r(g5, g43);
        g79 = spu_ls_read128(ctx, g68._u32[0] + 0x30);
        g80 = spu_fm_r(g9, g49);
        g22 = spu_shufb(g8, g81, g36);
        g28 = spu_fm_r(g10, g46);
        g15 = spu_shufb(g5, g81, g36);
        g30 = spu_fm_r(g13, g52);
        g11 = spu_shufb(g9, g81, g36);
        g31 = spu_fm_r(g6, g49);
        g21 = spu_shufb(g10, g81, g36);
        g32 = spu_fm(g83, g46);
        g12 = spu_shufb(g6, g81, g36);
        g33 = spu_fm(g79, g43);
        g14 = spu_shufb(g13, g81, g36);
        g25 = spu_fma_r(g60, g51, g19);
        g16 = spu_shufb(g83, g81, g36);
        g24 = spu_fma_r(g59, g48, g20);
        g17 = spu_shufb(g79, g81, g36);
        g23 = spu_fma_r(g55, g45, g27);
        g60 = spu_shufb(g8, g81, g82);
        g4 = spu_fma(g54, g42, g18);
        g59 = spu_shufb(g9, g81, g82);
        g19 = spu_fma(g22, g51, g26);
        g54 = spu_shufb(g5, g81, g82);
        g20 = spu_fma(g11, g48, g80);
        g55 = spu_shufb(g10, g81, g82);
        g27 = spu_fma(g21, g45, g28);
        g11 = spu_shufb(g13, g81, g82);
        g22 = spu_fma(g15, g42, g29);
        g13 = spu_shufb(g83, g81, g82);
        g15 = spu_fma(g12, g48, g31);
        g12 = spu_shufb(g6, g81, g82);
        g80 = spu_fma(g14, g51, g30);
        g6 = spu_shufb(g79, g81, g82);
        g28 = spu_fma(g16, g45, g32);
        g37 = spu_ls_read128(ctx, g62._u32[0] + 0x40);
        g17 = spu_fma(g17, g42, g33);
        g35 = spu_ls_read128(ctx, g62._u32[0] + 0x50);
        g25 = spu_fma(g56, g50, g25);
        g34 = spu_ls_read128(ctx, g62._u32[0] + 0x60);
        g24 = spu_fma(g53, g47, g24);
        g38 = spu_ls_read128(ctx, g62._u32[0] + 0x70);
        g23 = spu_fma(g57, g44, g23);
        g52 = spu_ls_read128(ctx, g40._u32[0] + 0x0);
        g18 = spu_fma(g58, g41, g4);
        g49 = spu_ls_read128(ctx, g40._u32[0] + 0x30);
        g31 = spu_fma(g60, g50, g19);
        g46 = spu_ls_read128(ctx, g40._u32[0] + 0x60);
        g29 = spu_fma(g59, g47, g20);
        g43 = spu_ls_read128(ctx, g40._u32[0] + 0x90);
        g26 = spu_fma(g55, g44, g27);
        g83 = spu_fma(g54, g41, g22);
        g51 = spu_ls_read128(ctx, g40._u32[0] + 0x10);
        g53 = spu_fma(g11, g50, g80);
        g48 = spu_ls_read128(ctx, g40._u32[0] + 0x40);
        g57 = spu_fma(g12, g47, g15);
        g45 = spu_ls_read128(ctx, g40._u32[0] + 0x70);
        g56 = spu_fma(g13, g44, g28);
        g42 = spu_ls_read128(ctx, g40._u32[0] + 0xA0);
        g30 = spu_fma(g6, g41, g17);
        g50 = spu_ls_read128(ctx, g40._u32[0] + 0x20);
        g58 = spu_ai(g40, 192);
        g47 = spu_ls_read128(ctx, g40._u32[0] + 0x50);
        g5 = spu_ai(g62, 64);
        g44 = spu_ls_read128(ctx, g40._u32[0] + 0x80);
        g8 = spu_ai(g65, 64);
        g41 = spu_ls_read128(ctx, g40._u32[0] + 0xB0);
        g21 = spu_ai(g68, 64);
        g40 = spu_ori(g58, 0);
        spu_ls_write128(ctx, g62._u32[0] + 0x30, g18);
        spu_ls_write128(ctx, g62._u32[0] + 0x0, g25);
        spu_ls_write128(ctx, g62._u32[0] + 0x10, g24);
        spu_ls_write128(ctx, g62._u32[0] + 0x20, g23);
        spu_ls_write128(ctx, g65._u32[0] + 0x30, g83);
        g62 = spu_ori(g5, 0);
        spu_ls_write128(ctx, g65._u32[0] + 0x0, g31);
        spu_ls_write128(ctx, g65._u32[0] + 0x10, g29);
        spu_ls_write128(ctx, g65._u32[0] + 0x20, g26);
        g65 = spu_ori(g8, 0);
        spu_ls_write128(ctx, g68._u32[0] + 0x0, g53);
        spu_ls_write128(ctx, g68._u32[0] + 0x10, g57);
        spu_ls_write128(ctx, g68._u32[0] + 0x20, g56);
        g60 = spu_shufb(g37, g81, g36);
        g59 = spu_shufb(g35, g81, g36);
        g55 = spu_shufb(g34, g81, g36);
        g54 = spu_shufb(g38, g81, g36);
        g56 = spu_shufb(g37, g81, g82);
        g57 = spu_shufb(g34, g81, g82);
        g53 = spu_shufb(g35, g81, g82);
        g58 = spu_shufb(g38, g81, g82);
        spu_ls_write128(ctx, g68._u32[0] + 0x30, g30);
        g68 = spu_ori(g21, 0);
    } while (g61._u32[0] != 0);
    ctx->gpr[8] = g8;
    ctx->gpr[19] = g19;
    ctx->gpr[5] = g5;
    ctx->gpr[20] = g20;
    ctx->gpr[9] = g9;
    ctx->gpr[27] = g27;
    ctx->gpr[10] = g10;
    ctx->gpr[18] = g18;
    ctx->gpr[13] = g13;
    ctx->gpr[61] = g61;
    ctx->gpr[6] = g6;
    ctx->gpr[26] = g26;
    ctx->gpr[83] = g83;
    ctx->gpr[29] = g29;
    ctx->gpr[79] = g79;
    ctx->gpr[80] = g80;
    ctx->gpr[22] = g22;
    ctx->gpr[28] = g28;
    ctx->gpr[15] = g15;
    ctx->gpr[30] = g30;
    ctx->gpr[11] = g11;
    ctx->gpr[31] = g31;
    ctx->gpr[21] = g21;
    ctx->gpr[32] = g32;
    ctx->gpr[12] = g12;
    ctx->gpr[33] = g33;
    ctx->gpr[14] = g14;
    ctx->gpr[25] = g25;
    ctx->gpr[16] = g16;
    ctx->gpr[24] = g24;
    ctx->gpr[17] = g17;
    ctx->gpr[23] = g23;
    ctx->gpr[60] = g60;
    ctx->gpr[4] = g4;
    ctx->gpr[59] = g59;
    ctx->gpr[54] = g54;
    ctx->gpr[55] = g55;
    ctx->gpr[37] = g37;
    ctx->gpr[35] = g35;
    ctx->gpr[34] = g34;
    ctx->gpr[38] = g38;
    ctx->gpr[52] = g52;
    ctx->gpr[49] = g49;
    ctx->gpr[46] = g46;
    ctx->gpr[43] = g43;
    ctx->gpr[51] = g51;
    ctx->gpr[53] = g53;
    ctx->gpr[48] = g48;
    ctx->gpr[57] = g57;
    ctx->gpr[45] = g45;
    ctx->gpr[56] = g56;
    ctx->gpr[42] = g42;
    ctx->gpr[50] = g50;
    ctx->gpr[58] = g58;
    ctx->gpr[47] = g47;
    ctx->gpr[44] = g44;
    ctx->gpr[41] = g41;
    ctx->gpr[40] = g40;
    ctx->gpr[62] = g62;
    ctx->gpr[65] = g65;
    ctx->gpr[68] = g68;
    return 1;
}
#endif
