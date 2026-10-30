/* GOWA: gowajm_0072DF80_spu_func_0000E210__b loc_0000E210 com registradores locais (gerado por scripts_edge/laco_local.py; não editar).
 * Mesmas operações e ordem do código traduzido: idêntico bit a bit. Faz o laço inteiro e devolve 1 (o
 * gancho salta para loc_0000E210_fim, depois do desvio); 0 = nada feito (desligado). */
#ifndef GOWA_LL_0000E210_0000E210_H
#define GOWA_LL_0000E210_0000E210_H
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
static int ll_0000E210_0000E210(spu_context* ctx)
{
    if (!gowa_laco_local_on()) return 0;
    u128 g2 = ctx->gpr[2], g3 = ctx->gpr[3], g4 = ctx->gpr[4], g5 = ctx->gpr[5], g6 = ctx->gpr[6], g7 = ctx->gpr[7], g8 = ctx->gpr[8], g9 = ctx->gpr[9], g10 = ctx->gpr[10], g11 = ctx->gpr[11], g12 = ctx->gpr[12], g13 = ctx->gpr[13], g14 = ctx->gpr[14], g15 = ctx->gpr[15], g16 = ctx->gpr[16], g17 = ctx->gpr[17], g18 = ctx->gpr[18], g19 = ctx->gpr[19], g20 = ctx->gpr[20], g21 = ctx->gpr[21], g22 = ctx->gpr[22], g23 = ctx->gpr[23], g24 = ctx->gpr[24], g25 = ctx->gpr[25], g26 = ctx->gpr[26], g27 = ctx->gpr[27], g29 = ctx->gpr[29], g30 = ctx->gpr[30], g31 = ctx->gpr[31], g32 = ctx->gpr[32], g33 = ctx->gpr[33], g34 = ctx->gpr[34], g36 = ctx->gpr[36], g39 = ctx->gpr[39], g40 = ctx->gpr[40], g42 = ctx->gpr[42], g43 = ctx->gpr[43], g49 = ctx->gpr[49], g51 = ctx->gpr[51], g52 = ctx->gpr[52], g53 = ctx->gpr[53], g54 = ctx->gpr[54], g56 = ctx->gpr[56], g57 = ctx->gpr[57], g58 = ctx->gpr[58], g64 = ctx->gpr[64], g65 = ctx->gpr[65], g66 = ctx->gpr[66], g67 = ctx->gpr[67], g68 = ctx->gpr[68], g69 = ctx->gpr[69], g70 = ctx->gpr[70], g72 = ctx->gpr[72], g73 = ctx->gpr[73], g74 = ctx->gpr[74], g75 = ctx->gpr[75], g76 = ctx->gpr[76], g77 = ctx->gpr[77], g78 = ctx->gpr[78];
    do {
        g7 = spu_a(g13, g13);
        g3 = spu_ls_read128(ctx, g25._u32[0] + g23._u32[0]);
        g57 = spu_andi(g8, 15);
        g66 = spu_ls_read128(ctx, g8._u32[0] + 0x10);
        g73 = spu_a(g7, g8);
        g12 = spu_ls_read128(ctx, g8._u32[0] + 0x0);
        g56 = spu_selb(g11, g18, g24);
        g16 = spu_shufb(g57, g57, g58);
        g14 = spu_andi(g73, 15);
        g78 = spu_ls_read128(ctx, g73._u32[0] + 0x10);
        g15 = spu_a(g56, g56);
        g77 = spu_ls_read128(ctx, g73._u32[0] + 0x0);
        g52 = spu_a(g73, g20);
        g49 = spu_rotqby(g3, g23);
        g64 = spu_a(g53, g16);
        g4 = spu_shufb(g14, g14, g58);
        g10 = spu_a(g17, g26);
        g6 = spu_shufb(g5, g5, g22);
        g68 = spu_ai(g23, 2);
        g65 = spu_shufb(g15, g15, g27);
        g36 = spu_cntb(g49);
        g54 = spu_shufb(g49, g49, g22);
        g13 = spu_a(g53, g4);
        g2 = spu_shufb(g15, g15, g29);
        g69 = spu_a(g21, g6);
        g74 = spu_shufb(g12, g66, g64);
        g43 = spu_ahi(g65, 1);
        g40 = spu_shufb(g77, g78, g13);
        g76 = spu_and(g54, g31);
        g72 = spu_fsmb(g49);
        g75 = spu_ahi(g2, 1);
        g70 = spu_shufb(g9, g9, g22);
        g18 = spu_cntb(g76);
        g13 = spu_shufb(g36, g36, g33);
        g51 = spu_andi(g36, 15);
        g42 = spu_shufb(g74, g21, g75);
        g19 = spu_ai(g19, -1);
        g39 = spu_shufb(g40, g69, g43);
        g23 = spu_ori(g68, 0);
        g24 = spu_shufb(g72, g72, g34);
        g67 = spu_sf(g18, g30);
        g8 = spu_shlqbyi(g52, 0);
        g21 = spu_a(g69, g70);
        spu_ls_write128(ctx, g17._u32[0] + 0x0, g42);
        g11 = spu_ah(g67, g32);
        spu_ls_write128(ctx, g17._u32[0] + 0x10, g39);
        g20 = spu_a(g51, g51);
        g17 = spu_shlqbyi(g10, 0);
        g5 = spu_sfi(g13, 8);
        g9 = spu_sfi(g51, 8);
        g26 = spu_il(32);
    } while (g19._u32[0] != 0);
    ctx->gpr[7] = g7;
    ctx->gpr[3] = g3;
    ctx->gpr[57] = g57;
    ctx->gpr[66] = g66;
    ctx->gpr[73] = g73;
    ctx->gpr[12] = g12;
    ctx->gpr[56] = g56;
    ctx->gpr[16] = g16;
    ctx->gpr[14] = g14;
    ctx->gpr[78] = g78;
    ctx->gpr[15] = g15;
    ctx->gpr[77] = g77;
    ctx->gpr[52] = g52;
    ctx->gpr[49] = g49;
    ctx->gpr[64] = g64;
    ctx->gpr[4] = g4;
    ctx->gpr[10] = g10;
    ctx->gpr[6] = g6;
    ctx->gpr[68] = g68;
    ctx->gpr[65] = g65;
    ctx->gpr[36] = g36;
    ctx->gpr[54] = g54;
    ctx->gpr[13] = g13;
    ctx->gpr[2] = g2;
    ctx->gpr[69] = g69;
    ctx->gpr[74] = g74;
    ctx->gpr[43] = g43;
    ctx->gpr[40] = g40;
    ctx->gpr[76] = g76;
    ctx->gpr[72] = g72;
    ctx->gpr[75] = g75;
    ctx->gpr[70] = g70;
    ctx->gpr[18] = g18;
    ctx->gpr[51] = g51;
    ctx->gpr[42] = g42;
    ctx->gpr[19] = g19;
    ctx->gpr[39] = g39;
    ctx->gpr[23] = g23;
    ctx->gpr[24] = g24;
    ctx->gpr[67] = g67;
    ctx->gpr[8] = g8;
    ctx->gpr[21] = g21;
    ctx->gpr[11] = g11;
    ctx->gpr[20] = g20;
    ctx->gpr[17] = g17;
    ctx->gpr[5] = g5;
    ctx->gpr[9] = g9;
    ctx->gpr[26] = g26;
    return 1;
}
#endif
