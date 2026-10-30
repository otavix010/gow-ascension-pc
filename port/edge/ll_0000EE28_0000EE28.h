/* GOWA: gowajm_0072DF80_spu_func_0000EE28__b loc_0000EE28 com registradores locais (gerado por scripts_edge/laco_local.py; não editar).
 * Mesmas operações e ordem do código traduzido: idêntico bit a bit. Faz o laço inteiro e devolve 1 (o
 * gancho salta para loc_0000EE28_fim, depois do desvio); 0 = nada feito (desligado). */
#ifndef GOWA_LL_0000EE28_0000EE28_H
#define GOWA_LL_0000EE28_0000EE28_H
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
static int ll_0000EE28_0000EE28(spu_context* ctx)
{
    if (!gowa_laco_local_on()) return 0;
    u128 g2 = ctx->gpr[2], g3 = ctx->gpr[3], g4 = ctx->gpr[4], g5 = ctx->gpr[5], g6 = ctx->gpr[6], g7 = ctx->gpr[7], g8 = ctx->gpr[8], g9 = ctx->gpr[9], g10 = ctx->gpr[10], g11 = ctx->gpr[11], g12 = ctx->gpr[12], g13 = ctx->gpr[13], g14 = ctx->gpr[14], g15 = ctx->gpr[15], g16 = ctx->gpr[16], g17 = ctx->gpr[17], g18 = ctx->gpr[18], g19 = ctx->gpr[19], g20 = ctx->gpr[20], g21 = ctx->gpr[21], g22 = ctx->gpr[22], g23 = ctx->gpr[23], g24 = ctx->gpr[24], g25 = ctx->gpr[25], g26 = ctx->gpr[26], g27 = ctx->gpr[27], g28 = ctx->gpr[28], g29 = ctx->gpr[29], g30 = ctx->gpr[30], g31 = ctx->gpr[31], g32 = ctx->gpr[32], g33 = ctx->gpr[33], g34 = ctx->gpr[34], g35 = ctx->gpr[35], g36 = ctx->gpr[36], g37 = ctx->gpr[37], g38 = ctx->gpr[38], g40 = ctx->gpr[40], g41 = ctx->gpr[41], g42 = ctx->gpr[42], g43 = ctx->gpr[43], g44 = ctx->gpr[44], g45 = ctx->gpr[45], g46 = ctx->gpr[46], g47 = ctx->gpr[47], g48 = ctx->gpr[48], g49 = ctx->gpr[49], g50 = ctx->gpr[50], g51 = ctx->gpr[51], g52 = ctx->gpr[52], g53 = ctx->gpr[53], g54 = ctx->gpr[54], g55 = ctx->gpr[55], g56 = ctx->gpr[56], g57 = ctx->gpr[57], g58 = ctx->gpr[58], g59 = ctx->gpr[59], g60 = ctx->gpr[60], g61 = ctx->gpr[61], g78 = ctx->gpr[78], g79 = ctx->gpr[79], g80 = ctx->gpr[80], g81 = ctx->gpr[81], g82 = ctx->gpr[82], g83 = ctx->gpr[83], g87 = ctx->gpr[87];
    do {
        g22 = spu_ai(g87, 64);
        g25 = spu_shufb(g48, g48, g36);
        g52 = spu_ori(g87, 0);
        g23 = spu_shufb(g45, g45, g36);
        g31 = spu_ai(g50, 192);
        g11 = spu_shufb(g42, g42, g36);
        g3 = spu_ori(g50, 0);
        g33 = spu_shufb(g38, g38, g36);
        g50 = spu_ori(g31, 0);
        g61 = spu_shufb(g47, g47, g82);
        g51 = spu_ai(g51, -4);
        g78 = spu_shufb(g41, g41, g82);
        g58 = spu_shufb(g44, g44, g82);
        g56 = spu_shufb(g37, g37, g82);
        g21 = spu_fm_r(g61, g25);
        g10 = spu_shufb(g49, g49, g82);
        g19 = spu_fm_r(g78, g11);
        g17 = spu_shufb(g46, g46, g82);
        g20 = spu_fm_r(g58, g23);
        g29 = spu_shufb(g43, g43, g82);
        g18 = spu_fm_r(g56, g33);
        g13 = spu_shufb(g40, g40, g82);
        g15 = spu_shufb(g49, g49, g36);
        g8 = spu_shufb(g43, g43, g36);
        g79 = spu_shufb(g40, g40, g36);
        g14 = spu_shufb(g47, g47, g36);
        g53 = spu_fms_r(g15, g10, g21);
        g30 = spu_shufb(g46, g46, g36);
        g80 = spu_fms(g8, g29, g19);
        g83 = spu_shufb(g44, g44, g36);
        g16 = spu_fms(g79, g13, g18);
        g9 = spu_shufb(g41, g41, g36);
        g26 = spu_fm_r(g14, g10);
        g7 = spu_shufb(g37, g37, g36);
        g34 = spu_fms_r(g30, g17, g20);
        g54 = spu_ls_read128(ctx, g87._u32[0] + 0x0);
        g24 = spu_fm_r(g83, g17);
        g35 = spu_ls_read128(ctx, g87._u32[0] + 0x10);
        g12 = spu_fm_r(g9, g29);
        g27 = spu_ls_read128(ctx, g87._u32[0] + 0x20);
        g32 = spu_fm_r(g7, g13);
        g28 = spu_ls_read128(ctx, g87._u32[0] + 0x30);
        g87 = spu_ori(g22, 0);
        g5 = spu_shufb(g45, g45, g82);
        g55 = spu_shufb(g38, g38, g82);
        g6 = spu_shufb(g48, g48, g82);
        g22 = spu_fm(g53, g54);
        g4 = spu_shufb(g42, g42, g82);
        g20 = spu_fm(g34, g35);
        g31 = spu_shufb(g54, g81, g36);
        g17 = spu_fms(g5, g23, g24);
        g10 = spu_shufb(g35, g81, g36);
        g21 = spu_fms(g6, g25, g26);
        g29 = spu_shufb(g27, g81, g36);
        g18 = spu_fms(g4, g11, g12);
        g26 = spu_shufb(g28, g81, g36);
        g57 = spu_fm(g5, g30);
        g49 = spu_ls_read128(ctx, g3._u32[0] + 0x0);
        g59 = spu_fm(g6, g15);
        g47 = spu_ls_read128(ctx, g3._u32[0] + 0x20);
        g15 = spu_fms(g55, g33, g32);
        g46 = spu_ls_read128(ctx, g3._u32[0] + 0x30);
        g60 = spu_fm(g4, g8);
        g44 = spu_ls_read128(ctx, g3._u32[0] + 0x50);
        g2 = spu_fm(g55, g79);
        g43 = spu_ls_read128(ctx, g3._u32[0] + 0x60);
        g19 = spu_fm(g80, g27);
        g41 = spu_ls_read128(ctx, g3._u32[0] + 0x80);
        g25 = spu_fm(g16, g28);
        g40 = spu_ls_read128(ctx, g3._u32[0] + 0x90);
        g23 = spu_fms(g14, g61, g59);
        g37 = spu_ls_read128(ctx, g3._u32[0] + 0xB0);
        g11 = spu_fms(g83, g58, g57);
        g33 = spu_fms(g9, g78, g60);
        g48 = spu_ls_read128(ctx, g3._u32[0] + 0x10);
        g55 = spu_fms(g7, g56, g2);
        g45 = spu_ls_read128(ctx, g3._u32[0] + 0x40);
        g24 = spu_fma(g21, g31, g22);
        g42 = spu_ls_read128(ctx, g3._u32[0] + 0x70);
        g5 = spu_fma(g17, g10, g20);
        g38 = spu_ls_read128(ctx, g3._u32[0] + 0xA0);
        g32 = spu_fma(g18, g29, g19);
        g12 = spu_shufb(g35, g81, g82);
        g79 = spu_fma(g15, g26, g25);
        g13 = spu_shufb(g54, g81, g82);
        g8 = spu_shufb(g27, g81, g82);
        g3 = spu_shufb(g28, g81, g82);
        g54 = spu_fma(g23, g13, g24);
        g53 = spu_fma(g11, g12, g5);
        g35 = spu_fma(g33, g8, g32);
        g34 = spu_fma(g55, g3, g79);
        spu_ls_write128(ctx, g52._u32[0] + 0x0, g54);
        spu_ls_write128(ctx, g52._u32[0] + 0x10, g53);
        spu_ls_write128(ctx, g52._u32[0] + 0x20, g35);
        spu_ls_write128(ctx, g52._u32[0] + 0x30, g34);
    } while (g51._u32[0] != 0);
    ctx->gpr[22] = g22;
    ctx->gpr[25] = g25;
    ctx->gpr[52] = g52;
    ctx->gpr[23] = g23;
    ctx->gpr[31] = g31;
    ctx->gpr[11] = g11;
    ctx->gpr[3] = g3;
    ctx->gpr[33] = g33;
    ctx->gpr[50] = g50;
    ctx->gpr[61] = g61;
    ctx->gpr[51] = g51;
    ctx->gpr[78] = g78;
    ctx->gpr[58] = g58;
    ctx->gpr[56] = g56;
    ctx->gpr[21] = g21;
    ctx->gpr[10] = g10;
    ctx->gpr[19] = g19;
    ctx->gpr[17] = g17;
    ctx->gpr[20] = g20;
    ctx->gpr[29] = g29;
    ctx->gpr[18] = g18;
    ctx->gpr[13] = g13;
    ctx->gpr[15] = g15;
    ctx->gpr[8] = g8;
    ctx->gpr[79] = g79;
    ctx->gpr[14] = g14;
    ctx->gpr[53] = g53;
    ctx->gpr[30] = g30;
    ctx->gpr[80] = g80;
    ctx->gpr[83] = g83;
    ctx->gpr[16] = g16;
    ctx->gpr[9] = g9;
    ctx->gpr[26] = g26;
    ctx->gpr[7] = g7;
    ctx->gpr[34] = g34;
    ctx->gpr[54] = g54;
    ctx->gpr[24] = g24;
    ctx->gpr[35] = g35;
    ctx->gpr[12] = g12;
    ctx->gpr[27] = g27;
    ctx->gpr[32] = g32;
    ctx->gpr[28] = g28;
    ctx->gpr[87] = g87;
    ctx->gpr[5] = g5;
    ctx->gpr[55] = g55;
    ctx->gpr[6] = g6;
    ctx->gpr[4] = g4;
    ctx->gpr[57] = g57;
    ctx->gpr[49] = g49;
    ctx->gpr[59] = g59;
    ctx->gpr[47] = g47;
    ctx->gpr[46] = g46;
    ctx->gpr[60] = g60;
    ctx->gpr[44] = g44;
    ctx->gpr[2] = g2;
    ctx->gpr[43] = g43;
    ctx->gpr[41] = g41;
    ctx->gpr[40] = g40;
    ctx->gpr[37] = g37;
    ctx->gpr[48] = g48;
    ctx->gpr[45] = g45;
    ctx->gpr[42] = g42;
    ctx->gpr[38] = g38;
    return 1;
}
#endif
