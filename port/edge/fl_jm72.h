/* GOWA (E4): funções folha puras do job com registradores locais (gerado por scripts_edge/funcao_local.py;
 * não editar). Mesmas operações, ordem e desvios do código traduzido: idêntico bit a bit. */
#ifndef GOWA_FL_GOWAJM_0072DF80_H
#define GOWA_FL_GOWAJM_0072DF80_H
#include <stdlib.h>
/* GOWA_FUNCAO_LOCAL=1 liga (padrão desligado até o A/B no AVX2) */
static int gowa_funcao_local_on(void)
{
    static int on = -1;
    if (on < 0) { const char* e = getenv("GOWA_FUNCAO_LOCAL"); on = e && *e == '1'; }
    return on;
}
static int fl_00004100(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g1 = ctx->gpr[1], g3 = ctx->gpr[3], g9 = ctx->gpr[9];
    g1 = spu_ai(g1, 48);
    g3 = spu_ls_read128(ctx, g9._u32[0] + 0x0);
    g3 = spu_rotqby(g3, g9);
    { ctx->gpr[1] = g1; ctx->gpr[3] = g3; return 1; }
}

static int fl_00004160(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g0 = ctx->gpr[0], g1 = ctx->gpr[1], g3 = ctx->gpr[3], g5 = ctx->gpr[5], g6 = ctx->gpr[6], g7 = ctx->gpr[7], g8 = ctx->gpr[8], g9 = ctx->gpr[9], g10 = ctx->gpr[10], g11 = ctx->gpr[11], g80 = ctx->gpr[80], g81 = ctx->gpr[81], g82 = ctx->gpr[82], g83 = ctx->gpr[83];
    g11 = spu_sf(g81, g82);
    g0 = spu_ls_read128(ctx, g1._u32[0] + 0x70);
    g10 = spu_sf(g83, g8);
    g1 = spu_ai(g1, 96);
    g9 = spu_clgt(g8, g81);
    g7 = spu_sf(g81, g8);
    g80 = spu_ls_read128(ctx, g1._u32[0] + -0x10);
    g5 = spu_a(g11, g10);
    g82 = spu_ls_read128(ctx, g1._u32[0] + -0x30);
    g6 = spu_ceqi(g9, 0);
    g81 = spu_ls_read128(ctx, g1._u32[0] + -0x20);
    g83 = spu_ls_read128(ctx, g1._u32[0] + -0x40);
    g3 = spu_selb(g7, g5, g6);
    { ctx->gpr[0] = g0; ctx->gpr[1] = g1; ctx->gpr[3] = g3; ctx->gpr[5] = g5; ctx->gpr[6] = g6; ctx->gpr[7] = g7; ctx->gpr[9] = g9; ctx->gpr[10] = g10; ctx->gpr[11] = g11; ctx->gpr[80] = g80; ctx->gpr[81] = g81; ctx->gpr[82] = g82; ctx->gpr[83] = g83; return 1; }
}

static int fl_00004378(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g0 = ctx->gpr[0], g1 = ctx->gpr[1], g80 = ctx->gpr[80], g81 = ctx->gpr[81], g82 = ctx->gpr[82], g83 = ctx->gpr[83], g84 = ctx->gpr[84], g85 = ctx->gpr[85], g86 = ctx->gpr[86], g87 = ctx->gpr[87], g88 = ctx->gpr[88], g89 = ctx->gpr[89], g90 = ctx->gpr[90], g126 = ctx->gpr[126];
    g0 = spu_ls_read128(ctx, g1._u32[0] + 0xF0);
    g1 = spu_ai(g1, 224);
    g80 = spu_ls_read128(ctx, g1._u32[0] + -0x10);
    g81 = spu_ls_read128(ctx, g1._u32[0] + -0x20);
    g82 = spu_ls_read128(ctx, g1._u32[0] + -0x30);
    g83 = spu_ls_read128(ctx, g1._u32[0] + -0x40);
    g84 = spu_ls_read128(ctx, g1._u32[0] + -0x50);
    g85 = spu_ls_read128(ctx, g1._u32[0] + -0x60);
    g86 = spu_ls_read128(ctx, g1._u32[0] + -0x70);
    g87 = spu_ls_read128(ctx, g1._u32[0] + -0x80);
    g88 = spu_ls_read128(ctx, g1._u32[0] + -0x90);
    g89 = spu_ls_read128(ctx, g1._u32[0] + -0xA0);
    g90 = spu_ls_read128(ctx, g1._u32[0] + -0xB0);
    g126 = spu_ls_read128(ctx, g1._u32[0] + -0xC0);
    { ctx->gpr[0] = g0; ctx->gpr[1] = g1; ctx->gpr[80] = g80; ctx->gpr[81] = g81; ctx->gpr[82] = g82; ctx->gpr[83] = g83; ctx->gpr[84] = g84; ctx->gpr[85] = g85; ctx->gpr[86] = g86; ctx->gpr[87] = g87; ctx->gpr[88] = g88; ctx->gpr[89] = g89; ctx->gpr[90] = g90; ctx->gpr[126] = g126; return 1; }
}

static int fl_000044C8(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    {  return 1; }
}

static int fl_00004654(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    {  return 1; }
}

static int fl_000055C0(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g0 = ctx->gpr[0], g1 = ctx->gpr[1], g80 = ctx->gpr[80], g81 = ctx->gpr[81], g82 = ctx->gpr[82], g83 = ctx->gpr[83], g84 = ctx->gpr[84], g85 = ctx->gpr[85], g86 = ctx->gpr[86], g87 = ctx->gpr[87], g88 = ctx->gpr[88], g89 = ctx->gpr[89], g90 = ctx->gpr[90], g91 = ctx->gpr[91], g92 = ctx->gpr[92], g93 = ctx->gpr[93], g94 = ctx->gpr[94], g95 = ctx->gpr[95], g96 = ctx->gpr[96], g97 = ctx->gpr[97], g98 = ctx->gpr[98], g99 = ctx->gpr[99], g100 = ctx->gpr[100], g126 = ctx->gpr[126];
    g86 = spu_il(688);
    g0 = spu_ls_read128(ctx, g1._u32[0] + 0x2C0);
    g1 = spu_a(g1, g86);
    g80 = spu_ls_read128(ctx, g1._u32[0] + -0x10);
    g81 = spu_ls_read128(ctx, g1._u32[0] + -0x20);
    g82 = spu_ls_read128(ctx, g1._u32[0] + -0x30);
    g83 = spu_ls_read128(ctx, g1._u32[0] + -0x40);
    g84 = spu_ls_read128(ctx, g1._u32[0] + -0x50);
    g85 = spu_ls_read128(ctx, g1._u32[0] + -0x60);
    g86 = spu_ls_read128(ctx, g1._u32[0] + -0x70);
    g87 = spu_ls_read128(ctx, g1._u32[0] + -0x80);
    g88 = spu_ls_read128(ctx, g1._u32[0] + -0x90);
    g89 = spu_ls_read128(ctx, g1._u32[0] + -0xA0);
    g90 = spu_ls_read128(ctx, g1._u32[0] + -0xB0);
    g91 = spu_ls_read128(ctx, g1._u32[0] + -0xC0);
    g92 = spu_ls_read128(ctx, g1._u32[0] + -0xD0);
    g93 = spu_ls_read128(ctx, g1._u32[0] + -0xE0);
    g94 = spu_ls_read128(ctx, g1._u32[0] + -0xF0);
    g95 = spu_ls_read128(ctx, g1._u32[0] + -0x100);
    g96 = spu_ls_read128(ctx, g1._u32[0] + -0x110);
    g97 = spu_ls_read128(ctx, g1._u32[0] + -0x120);
    g98 = spu_ls_read128(ctx, g1._u32[0] + -0x130);
    g99 = spu_ls_read128(ctx, g1._u32[0] + -0x140);
    g100 = spu_ls_read128(ctx, g1._u32[0] + -0x150);
    g126 = spu_ls_read128(ctx, g1._u32[0] + -0x160);
    { ctx->gpr[0] = g0; ctx->gpr[1] = g1; ctx->gpr[80] = g80; ctx->gpr[81] = g81; ctx->gpr[82] = g82; ctx->gpr[83] = g83; ctx->gpr[84] = g84; ctx->gpr[85] = g85; ctx->gpr[86] = g86; ctx->gpr[87] = g87; ctx->gpr[88] = g88; ctx->gpr[89] = g89; ctx->gpr[90] = g90; ctx->gpr[91] = g91; ctx->gpr[92] = g92; ctx->gpr[93] = g93; ctx->gpr[94] = g94; ctx->gpr[95] = g95; ctx->gpr[96] = g96; ctx->gpr[97] = g97; ctx->gpr[98] = g98; ctx->gpr[99] = g99; ctx->gpr[100] = g100; ctx->gpr[126] = g126; return 1; }
}

static int fl_00005C88(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g2 = ctx->gpr[2], g3 = ctx->gpr[3], g4 = ctx->gpr[4], g5 = ctx->gpr[5], g6 = ctx->gpr[6], g7 = ctx->gpr[7], g8 = ctx->gpr[8], g9 = ctx->gpr[9], g10 = ctx->gpr[10], g11 = ctx->gpr[11], g12 = ctx->gpr[12], g13 = ctx->gpr[13], g14 = ctx->gpr[14], g15 = ctx->gpr[15], g16 = ctx->gpr[16];
    g5 = spu_ls_read128(ctx, g3._u32[0] + 0x0);
    g2 = spu_rotqbyi(g5, 15);
    g7 = spu_andi(g2, 255);
    if (g7._u32[0] == 0) goto fl_00005CEC;
    g6 = spu_il(0);
    g11 = spu_ls_read128(ctx, g3._u32[0] + 0x0);
    g10 = spu_rotqbyi(g11, 8);
    g9 = spu_andi(g10, 255);
    g8 = spu_ceq(g9, g4);
    if (g8._u32[0] != 0) goto fl_00005CF0;
    g3 = spu_ai(g3, 11);
    goto fl_00005CD8;
fl_00005CC0: ;
    g15 = spu_ls_read128(ctx, g3._u32[0] + 0x0);
    g14 = spu_rotqby(g15, g5);
    g13 = spu_andi(g14, 255);
    g12 = spu_ceq(g13, g4);
    if (g12._u32[0] != 0) goto fl_00005CF0;
fl_00005CD8: ;
    g6 = spu_ai(g6, 1);
    g3 = spu_ai(g3, 8);
    g16 = spu_ceq(g6, g7);
    g5 = spu_ai(g3, 13);
    if (g16._u32[0] == 0) goto fl_00005CC0;
fl_00005CEC: ;
    g6 = spu_il(-1);
fl_00005CF0: ;
    g3 = spu_ori(g6, 0);
    { ctx->gpr[2] = g2; ctx->gpr[3] = g3; ctx->gpr[5] = g5; ctx->gpr[6] = g6; ctx->gpr[7] = g7; ctx->gpr[8] = g8; ctx->gpr[9] = g9; ctx->gpr[10] = g10; ctx->gpr[11] = g11; ctx->gpr[12] = g12; ctx->gpr[13] = g13; ctx->gpr[14] = g14; ctx->gpr[15] = g15; ctx->gpr[16] = g16; return 1; }
}

static int fl_00005CF0(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g3 = ctx->gpr[3], g6 = ctx->gpr[6];
    g3 = spu_ori(g6, 0);
    { ctx->gpr[3] = g3; return 1; }
}

static int fl_00005D00(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g2 = ctx->gpr[2], g3 = ctx->gpr[3], g4 = ctx->gpr[4], g5 = ctx->gpr[5], g6 = ctx->gpr[6], g7 = ctx->gpr[7], g8 = ctx->gpr[8], g9 = ctx->gpr[9], g10 = ctx->gpr[10], g11 = ctx->gpr[11], g12 = ctx->gpr[12], g13 = ctx->gpr[13], g14 = ctx->gpr[14], g79 = ctx->gpr[79];
    g8 = spu_ila(0xCB80);
    g9 = spu_shlqbyi(g3, 0);
    g5 = spu_il(0);
    g79 = spu_sf(g2, g4);
    g4 = spu_fsmbi(0x0);
    g8 = spu_a(g8, g79);
fl_00005D20: ;
    g6 = spu_a(g4, g9);
    g3 = spu_ls_read128(ctx, g4._u32[0] + g9._u32[0]);
    g12 = spu_ai(g6, 13);
    g7 = spu_cbx(g8, g5);
    g4 = spu_ai(g4, 1);
    g6 = spu_ceqi(g4, 16);
    g3 = spu_rotqby(g3, g12);
    g11 = spu_ceqbi(g3, -1);
    g10 = spu_xsbh(g11);
    if (g10._u16[0] != 0) goto fl_00005D60;
    g14 = spu_ls_read128(ctx, g8._u32[0] + g5._u32[0]);
    g13 = spu_shufb(g3, g14, g7);
    spu_ls_write128(ctx, g8._u32[0] + g5._u32[0], g13);
    g5 = spu_ai(g5, 1);
fl_00005D60: ;
    if (g6._u32[0] == 0) goto fl_00005D20;
    { ctx->gpr[3] = g3; ctx->gpr[4] = g4; ctx->gpr[5] = g5; ctx->gpr[6] = g6; ctx->gpr[7] = g7; ctx->gpr[8] = g8; ctx->gpr[9] = g9; ctx->gpr[10] = g10; ctx->gpr[11] = g11; ctx->gpr[12] = g12; ctx->gpr[13] = g13; ctx->gpr[14] = g14; ctx->gpr[79] = g79; return 1; }
}

static int fl_00005E78(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    {  return 1; }
}

static int fl_00005F58(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g0 = ctx->gpr[0], g1 = ctx->gpr[1], g3 = ctx->gpr[3], g4 = ctx->gpr[4], g80 = ctx->gpr[80], g81 = ctx->gpr[81], g82 = ctx->gpr[82], g83 = ctx->gpr[83], g84 = ctx->gpr[84], g85 = ctx->gpr[85], g126 = ctx->gpr[126];
    g3 = spu_ori(g4, 0);
    g0 = spu_ls_read128(ctx, g1._u32[0] + 0xA0);
    g1 = spu_ai(g1, 144);
    g80 = spu_ls_read128(ctx, g1._u32[0] + -0x10);
    g81 = spu_ls_read128(ctx, g1._u32[0] + -0x20);
    g82 = spu_ls_read128(ctx, g1._u32[0] + -0x30);
    g83 = spu_ls_read128(ctx, g1._u32[0] + -0x40);
    g84 = spu_ls_read128(ctx, g1._u32[0] + -0x50);
    g85 = spu_ls_read128(ctx, g1._u32[0] + -0x60);
    g126 = spu_ls_read128(ctx, g1._u32[0] + -0x70);
    { ctx->gpr[0] = g0; ctx->gpr[1] = g1; ctx->gpr[3] = g3; ctx->gpr[80] = g80; ctx->gpr[81] = g81; ctx->gpr[82] = g82; ctx->gpr[83] = g83; ctx->gpr[84] = g84; ctx->gpr[85] = g85; ctx->gpr[126] = g126; return 1; }
}

static int fl_00006078(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g0 = ctx->gpr[0], g1 = ctx->gpr[1], g3 = ctx->gpr[3], g80 = ctx->gpr[80], g81 = ctx->gpr[81], g126 = ctx->gpr[126];
    g3 = spu_ori(g81, 0);
    g0 = spu_ls_read128(ctx, g1._u32[0] + 0x60);
    g1 = spu_ai(g1, 80);
    g80 = spu_ls_read128(ctx, g1._u32[0] + -0x10);
    g81 = spu_ls_read128(ctx, g1._u32[0] + -0x20);
    g126 = spu_ls_read128(ctx, g1._u32[0] + -0x30);
    { ctx->gpr[0] = g0; ctx->gpr[1] = g1; ctx->gpr[3] = g3; ctx->gpr[80] = g80; ctx->gpr[81] = g81; ctx->gpr[126] = g126; return 1; }
}

static int fl_00006774(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g0 = ctx->gpr[0], g1 = ctx->gpr[1], g80 = ctx->gpr[80], g81 = ctx->gpr[81], g82 = ctx->gpr[82], g83 = ctx->gpr[83], g84 = ctx->gpr[84], g85 = ctx->gpr[85], g86 = ctx->gpr[86], g87 = ctx->gpr[87], g88 = ctx->gpr[88], g89 = ctx->gpr[89], g90 = ctx->gpr[90], g91 = ctx->gpr[91], g92 = ctx->gpr[92], g93 = ctx->gpr[93], g126 = ctx->gpr[126];
    g0 = spu_ls_read128(ctx, g1._u32[0] + 0x120);
    g1 = spu_ai(g1, 272);
    g80 = spu_ls_read128(ctx, g1._u32[0] + -0x10);
    g81 = spu_ls_read128(ctx, g1._u32[0] + -0x20);
    g82 = spu_ls_read128(ctx, g1._u32[0] + -0x30);
    g83 = spu_ls_read128(ctx, g1._u32[0] + -0x40);
    g84 = spu_ls_read128(ctx, g1._u32[0] + -0x50);
    g85 = spu_ls_read128(ctx, g1._u32[0] + -0x60);
    g86 = spu_ls_read128(ctx, g1._u32[0] + -0x70);
    g87 = spu_ls_read128(ctx, g1._u32[0] + -0x80);
    g88 = spu_ls_read128(ctx, g1._u32[0] + -0x90);
    g89 = spu_ls_read128(ctx, g1._u32[0] + -0xA0);
    g90 = spu_ls_read128(ctx, g1._u32[0] + -0xB0);
    g91 = spu_ls_read128(ctx, g1._u32[0] + -0xC0);
    g92 = spu_ls_read128(ctx, g1._u32[0] + -0xD0);
    g93 = spu_ls_read128(ctx, g1._u32[0] + -0xE0);
    g126 = spu_ls_read128(ctx, g1._u32[0] + -0xF0);
    { ctx->gpr[0] = g0; ctx->gpr[1] = g1; ctx->gpr[80] = g80; ctx->gpr[81] = g81; ctx->gpr[82] = g82; ctx->gpr[83] = g83; ctx->gpr[84] = g84; ctx->gpr[85] = g85; ctx->gpr[86] = g86; ctx->gpr[87] = g87; ctx->gpr[88] = g88; ctx->gpr[89] = g89; ctx->gpr[90] = g90; ctx->gpr[91] = g91; ctx->gpr[92] = g92; ctx->gpr[93] = g93; ctx->gpr[126] = g126; return 1; }
}

static int fl_00006A68(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g0 = ctx->gpr[0], g1 = ctx->gpr[1], g3 = ctx->gpr[3], g4 = ctx->gpr[4], g5 = ctx->gpr[5], g6 = ctx->gpr[6], g7 = ctx->gpr[7], g13 = ctx->gpr[13], g14 = ctx->gpr[14], g15 = ctx->gpr[15], g16 = ctx->gpr[16], g17 = ctx->gpr[17], g18 = ctx->gpr[18], g19 = ctx->gpr[19], g80 = ctx->gpr[80], g81 = ctx->gpr[81], g82 = ctx->gpr[82], g83 = ctx->gpr[83], g84 = ctx->gpr[84], g85 = ctx->gpr[85], g86 = ctx->gpr[86], g87 = ctx->gpr[87], g88 = ctx->gpr[88];
    g5 = spu_cbd(g85, 0);
    g4 = spu_shufb(g6, g19, g5);
    spu_ls_write128(ctx, g85._u32[0] + 0x10, g4);
    g16 = spu_ls_read128(ctx, g86._u32[0] + g18._u32[0]);
    g14 = spu_rotqby(g16, g17);
    g13 = spu_clgt(g14, g15);
    if (g13._u32[0] != 0) goto fl_00006A8C;
    g87 = spu_shufb(g6, g4, g5);
    spu_ls_write128(ctx, g85._u32[0] + 0x10, g87);
fl_00006A8C: ;
    g3 = spu_ori(g7, 0);
    g0 = spu_ls_read128_kcx(ctx, g1._u32[0] + 0xC0);
    g1 = spu_ai(g1, 176);
    g80 = spu_ls_read128(ctx, g1._u32[0] + -0x10);
    g81 = spu_ls_read128(ctx, g1._u32[0] + -0x20);
    g82 = spu_ls_read128(ctx, g1._u32[0] + -0x30);
    g83 = spu_ls_read128(ctx, g1._u32[0] + -0x40);
    g84 = spu_ls_read128(ctx, g1._u32[0] + -0x50);
    g85 = spu_ls_read128(ctx, g1._u32[0] + -0x60);
    g86 = spu_ls_read128(ctx, g1._u32[0] + -0x70);
    g87 = spu_ls_read128(ctx, g1._u32[0] + -0x80);
    g88 = spu_ls_read128(ctx, g1._u32[0] + -0x90);
    { ctx->gpr[0] = g0; ctx->gpr[1] = g1; ctx->gpr[3] = g3; ctx->gpr[4] = g4; ctx->gpr[5] = g5; ctx->gpr[13] = g13; ctx->gpr[14] = g14; ctx->gpr[16] = g16; ctx->gpr[80] = g80; ctx->gpr[81] = g81; ctx->gpr[82] = g82; ctx->gpr[83] = g83; ctx->gpr[84] = g84; ctx->gpr[85] = g85; ctx->gpr[86] = g86; ctx->gpr[87] = g87; ctx->gpr[88] = g88; return 1; }
}

static int fl_00006A8C(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g0 = ctx->gpr[0], g1 = ctx->gpr[1], g3 = ctx->gpr[3], g7 = ctx->gpr[7], g80 = ctx->gpr[80], g81 = ctx->gpr[81], g82 = ctx->gpr[82], g83 = ctx->gpr[83], g84 = ctx->gpr[84], g85 = ctx->gpr[85], g86 = ctx->gpr[86], g87 = ctx->gpr[87], g88 = ctx->gpr[88];
    g3 = spu_ori(g7, 0);
    g0 = spu_ls_read128_kcx(ctx, g1._u32[0] + 0xC0);
    g1 = spu_ai(g1, 176);
    g80 = spu_ls_read128(ctx, g1._u32[0] + -0x10);
    g81 = spu_ls_read128(ctx, g1._u32[0] + -0x20);
    g82 = spu_ls_read128(ctx, g1._u32[0] + -0x30);
    g83 = spu_ls_read128(ctx, g1._u32[0] + -0x40);
    g84 = spu_ls_read128(ctx, g1._u32[0] + -0x50);
    g85 = spu_ls_read128(ctx, g1._u32[0] + -0x60);
    g86 = spu_ls_read128(ctx, g1._u32[0] + -0x70);
    g87 = spu_ls_read128(ctx, g1._u32[0] + -0x80);
    g88 = spu_ls_read128(ctx, g1._u32[0] + -0x90);
    { ctx->gpr[0] = g0; ctx->gpr[1] = g1; ctx->gpr[3] = g3; ctx->gpr[80] = g80; ctx->gpr[81] = g81; ctx->gpr[82] = g82; ctx->gpr[83] = g83; ctx->gpr[84] = g84; ctx->gpr[85] = g85; ctx->gpr[86] = g86; ctx->gpr[87] = g87; ctx->gpr[88] = g88; return 1; }
}

static int fl_00006F98(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g1 = ctx->gpr[1], g80 = ctx->gpr[80], g81 = ctx->gpr[81], g82 = ctx->gpr[82], g83 = ctx->gpr[83], g84 = ctx->gpr[84], g85 = ctx->gpr[85], g126 = ctx->gpr[126];
    g1 = spu_ai(g1, 144);
    g80 = spu_ls_read128(ctx, g1._u32[0] + -0x10);
    g81 = spu_ls_read128(ctx, g1._u32[0] + -0x20);
    g82 = spu_ls_read128(ctx, g1._u32[0] + -0x30);
    g83 = spu_ls_read128(ctx, g1._u32[0] + -0x40);
    g84 = spu_ls_read128(ctx, g1._u32[0] + -0x50);
    g85 = spu_ls_read128(ctx, g1._u32[0] + -0x60);
    g126 = spu_ls_read128(ctx, g1._u32[0] + -0x70);
    { ctx->gpr[1] = g1; ctx->gpr[80] = g80; ctx->gpr[81] = g81; ctx->gpr[82] = g82; ctx->gpr[83] = g83; ctx->gpr[84] = g84; ctx->gpr[85] = g85; ctx->gpr[126] = g126; return 1; }
}

static int fl_000076F0(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g0 = ctx->gpr[0], g1 = ctx->gpr[1], g59 = ctx->gpr[59], g80 = ctx->gpr[80], g81 = ctx->gpr[81], g82 = ctx->gpr[82], g83 = ctx->gpr[83], g84 = ctx->gpr[84], g85 = ctx->gpr[85], g86 = ctx->gpr[86], g87 = ctx->gpr[87], g88 = ctx->gpr[88], g89 = ctx->gpr[89], g90 = ctx->gpr[90], g91 = ctx->gpr[91], g92 = ctx->gpr[92], g93 = ctx->gpr[93], g94 = ctx->gpr[94], g95 = ctx->gpr[95], g96 = ctx->gpr[96], g97 = ctx->gpr[97], g98 = ctx->gpr[98], g99 = ctx->gpr[99], g100 = ctx->gpr[100], g101 = ctx->gpr[101], g102 = ctx->gpr[102], g103 = ctx->gpr[103], g104 = ctx->gpr[104], g105 = ctx->gpr[105], g106 = ctx->gpr[106], g107 = ctx->gpr[107], g108 = ctx->gpr[108], g109 = ctx->gpr[109], g110 = ctx->gpr[110], g111 = ctx->gpr[111], g112 = ctx->gpr[112], g113 = ctx->gpr[113], g114 = ctx->gpr[114], g115 = ctx->gpr[115], g116 = ctx->gpr[116], g117 = ctx->gpr[117], g118 = ctx->gpr[118], g119 = ctx->gpr[119], g120 = ctx->gpr[120], g121 = ctx->gpr[121], g122 = ctx->gpr[122], g123 = ctx->gpr[123], g124 = ctx->gpr[124], g125 = ctx->gpr[125], g126 = ctx->gpr[126], g127 = ctx->gpr[127];
    g59 = spu_il(864);
    g0 = spu_ls_read128(ctx, g1._u32[0] + 0x370);
    g1 = spu_a(g1, g59);
    g80 = spu_ls_read128(ctx, g1._u32[0] + -0x10);
    g81 = spu_ls_read128(ctx, g1._u32[0] + -0x20);
    g82 = spu_ls_read128(ctx, g1._u32[0] + -0x30);
    g83 = spu_ls_read128(ctx, g1._u32[0] + -0x40);
    g84 = spu_ls_read128(ctx, g1._u32[0] + -0x50);
    g85 = spu_ls_read128(ctx, g1._u32[0] + -0x60);
    g86 = spu_ls_read128(ctx, g1._u32[0] + -0x70);
    g87 = spu_ls_read128(ctx, g1._u32[0] + -0x80);
    g88 = spu_ls_read128(ctx, g1._u32[0] + -0x90);
    g89 = spu_ls_read128(ctx, g1._u32[0] + -0xA0);
    g90 = spu_ls_read128(ctx, g1._u32[0] + -0xB0);
    g91 = spu_ls_read128(ctx, g1._u32[0] + -0xC0);
    g92 = spu_ls_read128(ctx, g1._u32[0] + -0xD0);
    g93 = spu_ls_read128(ctx, g1._u32[0] + -0xE0);
    g94 = spu_ls_read128(ctx, g1._u32[0] + -0xF0);
    g95 = spu_ls_read128(ctx, g1._u32[0] + -0x100);
    g96 = spu_ls_read128(ctx, g1._u32[0] + -0x110);
    g97 = spu_ls_read128(ctx, g1._u32[0] + -0x120);
    g98 = spu_ls_read128(ctx, g1._u32[0] + -0x130);
    g99 = spu_ls_read128(ctx, g1._u32[0] + -0x140);
    g100 = spu_ls_read128(ctx, g1._u32[0] + -0x150);
    g101 = spu_ls_read128(ctx, g1._u32[0] + -0x160);
    g102 = spu_ls_read128(ctx, g1._u32[0] + -0x170);
    g103 = spu_ls_read128(ctx, g1._u32[0] + -0x180);
    g104 = spu_ls_read128(ctx, g1._u32[0] + -0x190);
    g105 = spu_ls_read128(ctx, g1._u32[0] + -0x1A0);
    g106 = spu_ls_read128(ctx, g1._u32[0] + -0x1B0);
    g107 = spu_ls_read128(ctx, g1._u32[0] + -0x1C0);
    g108 = spu_ls_read128(ctx, g1._u32[0] + -0x1D0);
    g109 = spu_ls_read128(ctx, g1._u32[0] + -0x1E0);
    g110 = spu_ls_read128(ctx, g1._u32[0] + -0x1F0);
    g111 = spu_ls_read128(ctx, g1._u32[0] + -0x200);
    g112 = spu_ls_read128(ctx, g1._u32[0] + -0x210);
    g113 = spu_ls_read128(ctx, g1._u32[0] + -0x220);
    g114 = spu_ls_read128(ctx, g1._u32[0] + -0x230);
    g115 = spu_ls_read128(ctx, g1._u32[0] + -0x240);
    g116 = spu_ls_read128(ctx, g1._u32[0] + -0x250);
    g117 = spu_ls_read128(ctx, g1._u32[0] + -0x260);
    g118 = spu_ls_read128(ctx, g1._u32[0] + -0x270);
    g119 = spu_ls_read128(ctx, g1._u32[0] + -0x280);
    g120 = spu_ls_read128(ctx, g1._u32[0] + -0x290);
    g121 = spu_ls_read128(ctx, g1._u32[0] + -0x2A0);
    g122 = spu_ls_read128(ctx, g1._u32[0] + -0x2B0);
    g123 = spu_ls_read128(ctx, g1._u32[0] + -0x2C0);
    g124 = spu_ls_read128(ctx, g1._u32[0] + -0x2D0);
    g125 = spu_ls_read128(ctx, g1._u32[0] + -0x2E0);
    g126 = spu_ls_read128(ctx, g1._u32[0] + -0x2F0);
    g127 = spu_ls_read128(ctx, g1._u32[0] + -0x300);
    { ctx->gpr[0] = g0; ctx->gpr[1] = g1; ctx->gpr[59] = g59; ctx->gpr[80] = g80; ctx->gpr[81] = g81; ctx->gpr[82] = g82; ctx->gpr[83] = g83; ctx->gpr[84] = g84; ctx->gpr[85] = g85; ctx->gpr[86] = g86; ctx->gpr[87] = g87; ctx->gpr[88] = g88; ctx->gpr[89] = g89; ctx->gpr[90] = g90; ctx->gpr[91] = g91; ctx->gpr[92] = g92; ctx->gpr[93] = g93; ctx->gpr[94] = g94; ctx->gpr[95] = g95; ctx->gpr[96] = g96; ctx->gpr[97] = g97; ctx->gpr[98] = g98; ctx->gpr[99] = g99; ctx->gpr[100] = g100; ctx->gpr[101] = g101; ctx->gpr[102] = g102; ctx->gpr[103] = g103; ctx->gpr[104] = g104; ctx->gpr[105] = g105; ctx->gpr[106] = g106; ctx->gpr[107] = g107; ctx->gpr[108] = g108; ctx->gpr[109] = g109; ctx->gpr[110] = g110; ctx->gpr[111] = g111; ctx->gpr[112] = g112; ctx->gpr[113] = g113; ctx->gpr[114] = g114; ctx->gpr[115] = g115; ctx->gpr[116] = g116; ctx->gpr[117] = g117; ctx->gpr[118] = g118; ctx->gpr[119] = g119; ctx->gpr[120] = g120; ctx->gpr[121] = g121; ctx->gpr[122] = g122; ctx->gpr[123] = g123; ctx->gpr[124] = g124; ctx->gpr[125] = g125; ctx->gpr[126] = g126; ctx->gpr[127] = g127; return 1; }
}

static int fl_000086C8(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g0 = ctx->gpr[0], g1 = ctx->gpr[1], g80 = ctx->gpr[80], g81 = ctx->gpr[81], g82 = ctx->gpr[82], g83 = ctx->gpr[83], g84 = ctx->gpr[84], g85 = ctx->gpr[85], g86 = ctx->gpr[86], g87 = ctx->gpr[87], g126 = ctx->gpr[126];
    g0 = spu_ls_read128_kcx(ctx, g1._u32[0] + 0xC0);
    g1 = spu_ai(g1, 176);
    g80 = spu_ls_read128(ctx, g1._u32[0] + -0x10);
    g81 = spu_ls_read128(ctx, g1._u32[0] + -0x20);
    g82 = spu_ls_read128(ctx, g1._u32[0] + -0x30);
    g83 = spu_ls_read128(ctx, g1._u32[0] + -0x40);
    g84 = spu_ls_read128(ctx, g1._u32[0] + -0x50);
    g85 = spu_ls_read128(ctx, g1._u32[0] + -0x60);
    g86 = spu_ls_read128(ctx, g1._u32[0] + -0x70);
    g87 = spu_ls_read128(ctx, g1._u32[0] + -0x80);
    g126 = spu_ls_read128(ctx, g1._u32[0] + -0x90);
    { ctx->gpr[0] = g0; ctx->gpr[1] = g1; ctx->gpr[80] = g80; ctx->gpr[81] = g81; ctx->gpr[82] = g82; ctx->gpr[83] = g83; ctx->gpr[84] = g84; ctx->gpr[85] = g85; ctx->gpr[86] = g86; ctx->gpr[87] = g87; ctx->gpr[126] = g126; return 1; }
}

static int fl_00009190(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g0 = ctx->gpr[0], g1 = ctx->gpr[1], g3 = ctx->gpr[3], g80 = ctx->gpr[80], g81 = ctx->gpr[81], g82 = ctx->gpr[82], g83 = ctx->gpr[83], g84 = ctx->gpr[84], g85 = ctx->gpr[85], g86 = ctx->gpr[86], g87 = ctx->gpr[87], g88 = ctx->gpr[88], g89 = ctx->gpr[89], g90 = ctx->gpr[90], g91 = ctx->gpr[91], g92 = ctx->gpr[92], g93 = ctx->gpr[93], g94 = ctx->gpr[94], g95 = ctx->gpr[95], g96 = ctx->gpr[96], g97 = ctx->gpr[97], g98 = ctx->gpr[98], g126 = ctx->gpr[126];
    g3 = spu_il(0);
    g0 = spu_ls_read128(ctx, g1._u32[0] + 0x1E0);
    g1 = spu_ai(g1, 464);
    g80 = spu_ls_read128(ctx, g1._u32[0] + -0x10);
    g81 = spu_ls_read128(ctx, g1._u32[0] + -0x20);
    g82 = spu_ls_read128(ctx, g1._u32[0] + -0x30);
    g83 = spu_ls_read128(ctx, g1._u32[0] + -0x40);
    g84 = spu_ls_read128(ctx, g1._u32[0] + -0x50);
    g85 = spu_ls_read128(ctx, g1._u32[0] + -0x60);
    g86 = spu_ls_read128(ctx, g1._u32[0] + -0x70);
    g87 = spu_ls_read128(ctx, g1._u32[0] + -0x80);
    g88 = spu_ls_read128(ctx, g1._u32[0] + -0x90);
    g89 = spu_ls_read128(ctx, g1._u32[0] + -0xA0);
    g90 = spu_ls_read128(ctx, g1._u32[0] + -0xB0);
    g91 = spu_ls_read128(ctx, g1._u32[0] + -0xC0);
    g92 = spu_ls_read128(ctx, g1._u32[0] + -0xD0);
    g93 = spu_ls_read128(ctx, g1._u32[0] + -0xE0);
    g94 = spu_ls_read128(ctx, g1._u32[0] + -0xF0);
    g95 = spu_ls_read128(ctx, g1._u32[0] + -0x100);
    g96 = spu_ls_read128(ctx, g1._u32[0] + -0x110);
    g97 = spu_ls_read128(ctx, g1._u32[0] + -0x120);
    g98 = spu_ls_read128(ctx, g1._u32[0] + -0x130);
    g126 = spu_ls_read128(ctx, g1._u32[0] + -0x140);
    { ctx->gpr[0] = g0; ctx->gpr[1] = g1; ctx->gpr[3] = g3; ctx->gpr[80] = g80; ctx->gpr[81] = g81; ctx->gpr[82] = g82; ctx->gpr[83] = g83; ctx->gpr[84] = g84; ctx->gpr[85] = g85; ctx->gpr[86] = g86; ctx->gpr[87] = g87; ctx->gpr[88] = g88; ctx->gpr[89] = g89; ctx->gpr[90] = g90; ctx->gpr[91] = g91; ctx->gpr[92] = g92; ctx->gpr[93] = g93; ctx->gpr[94] = g94; ctx->gpr[95] = g95; ctx->gpr[96] = g96; ctx->gpr[97] = g97; ctx->gpr[98] = g98; ctx->gpr[126] = g126; return 1; }
}

static int fl_00009A40(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g0 = ctx->gpr[0], g1 = ctx->gpr[1], g3 = ctx->gpr[3], g80 = ctx->gpr[80], g81 = ctx->gpr[81], g82 = ctx->gpr[82];
    g3 = spu_ori(g80, 0);
    g0 = spu_ls_read128(ctx, g1._u32[0] + 0x60);
    g1 = spu_ai(g1, 80);
    g80 = spu_ls_read128(ctx, g1._u32[0] + -0x10);
    g81 = spu_ls_read128(ctx, g1._u32[0] + -0x20);
    g82 = spu_ls_read128(ctx, g1._u32[0] + -0x30);
    { ctx->gpr[0] = g0; ctx->gpr[1] = g1; ctx->gpr[3] = g3; ctx->gpr[80] = g80; ctx->gpr[81] = g81; ctx->gpr[82] = g82; return 1; }
}

static int fl_0000A360(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g1 = ctx->gpr[1], g3 = ctx->gpr[3], g27 = ctx->gpr[27];
    g3 = spu_ori(g27, 0);
    g27 = spu_il(2640);
    g1 = spu_a(g1, g27);
    { ctx->gpr[1] = g1; ctx->gpr[3] = g3; ctx->gpr[27] = g27; return 1; }
}

static int fl_0000AD4C(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g0 = ctx->gpr[0], g1 = ctx->gpr[1], g80 = ctx->gpr[80], g81 = ctx->gpr[81], g82 = ctx->gpr[82], g83 = ctx->gpr[83], g126 = ctx->gpr[126];
    g0 = spu_ls_read128(ctx, g1._u32[0] + 0x80);
    g1 = spu_ai(g1, 112);
    g80 = spu_ls_read128(ctx, g1._u32[0] + -0x10);
    g81 = spu_ls_read128(ctx, g1._u32[0] + -0x20);
    g82 = spu_ls_read128(ctx, g1._u32[0] + -0x30);
    g83 = spu_ls_read128(ctx, g1._u32[0] + -0x40);
    g126 = spu_ls_read128(ctx, g1._u32[0] + -0x50);
    { ctx->gpr[0] = g0; ctx->gpr[1] = g1; ctx->gpr[80] = g80; ctx->gpr[81] = g81; ctx->gpr[82] = g82; ctx->gpr[83] = g83; ctx->gpr[126] = g126; return 1; }
}

static int fl_0000AF14(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g3 = ctx->gpr[3], g4 = ctx->gpr[4], g5 = ctx->gpr[5], g6 = ctx->gpr[6], g7 = ctx->gpr[7], g11 = ctx->gpr[11];
    g11 = spu_and(g7, g4);
    g6 = spu_shli(g11, 2);
    g3 = spu_a(g6, g5);
    { ctx->gpr[3] = g3; ctx->gpr[6] = g6; ctx->gpr[11] = g11; return 1; }
}

static int fl_0000B018(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g0 = ctx->gpr[0], g1 = ctx->gpr[1], g3 = ctx->gpr[3], g5 = ctx->gpr[5];
    g3 = spu_ori(g5, 0);
    g0 = spu_ls_read128(ctx, g1._u32[0] + 0x30);
    g1 = spu_ai(g1, 32);
    { ctx->gpr[0] = g0; ctx->gpr[1] = g1; ctx->gpr[3] = g3; return 1; }
}

static int fl_0000B154(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g0 = ctx->gpr[0], g1 = ctx->gpr[1], g80 = ctx->gpr[80], g81 = ctx->gpr[81], g82 = ctx->gpr[82], g83 = ctx->gpr[83], g84 = ctx->gpr[84];
    g0 = spu_ls_read128(ctx, g1._u32[0] + 0x80);
    g1 = spu_ai(g1, 112);
    g80 = spu_ls_read128(ctx, g1._u32[0] + -0x10);
    g81 = spu_ls_read128(ctx, g1._u32[0] + -0x20);
    g82 = spu_ls_read128(ctx, g1._u32[0] + -0x30);
    g83 = spu_ls_read128(ctx, g1._u32[0] + -0x40);
    g84 = spu_ls_read128(ctx, g1._u32[0] + -0x50);
    { ctx->gpr[0] = g0; ctx->gpr[1] = g1; ctx->gpr[80] = g80; ctx->gpr[81] = g81; ctx->gpr[82] = g82; ctx->gpr[83] = g83; ctx->gpr[84] = g84; return 1; }
}

static int fl_0000B298(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g0 = ctx->gpr[0], g1 = ctx->gpr[1], g80 = ctx->gpr[80], g81 = ctx->gpr[81], g82 = ctx->gpr[82];
    g0 = spu_ls_read128(ctx, g1._u32[0] + 0x60);
    g1 = spu_ai(g1, 80);
    g80 = spu_ls_read128(ctx, g1._u32[0] + -0x10);
    g81 = spu_ls_read128(ctx, g1._u32[0] + -0x20);
    g82 = spu_ls_read128(ctx, g1._u32[0] + -0x30);
    { ctx->gpr[0] = g0; ctx->gpr[1] = g1; ctx->gpr[80] = g80; ctx->gpr[81] = g81; ctx->gpr[82] = g82; return 1; }
}

static int fl_0000B388(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g0 = ctx->gpr[0], g1 = ctx->gpr[1], g80 = ctx->gpr[80], g81 = ctx->gpr[81], g82 = ctx->gpr[82], g83 = ctx->gpr[83], g84 = ctx->gpr[84];
    g0 = spu_ls_read128(ctx, g1._u32[0] + 0x80);
    g1 = spu_ai(g1, 112);
    g80 = spu_ls_read128(ctx, g1._u32[0] + -0x10);
    g81 = spu_ls_read128(ctx, g1._u32[0] + -0x20);
    g82 = spu_ls_read128(ctx, g1._u32[0] + -0x30);
    g83 = spu_ls_read128(ctx, g1._u32[0] + -0x40);
    g84 = spu_ls_read128(ctx, g1._u32[0] + -0x50);
    { ctx->gpr[0] = g0; ctx->gpr[1] = g1; ctx->gpr[80] = g80; ctx->gpr[81] = g81; ctx->gpr[82] = g82; ctx->gpr[83] = g83; ctx->gpr[84] = g84; return 1; }
}

static int fl_0000B5A4(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g0 = ctx->gpr[0], g1 = ctx->gpr[1], g80 = ctx->gpr[80], g81 = ctx->gpr[81], g82 = ctx->gpr[82], g83 = ctx->gpr[83], g84 = ctx->gpr[84], g85 = ctx->gpr[85], g86 = ctx->gpr[86];
    g0 = spu_ls_read128(ctx, g1._u32[0] + 0xA0);
    g1 = spu_ai(g1, 144);
    g80 = spu_ls_read128(ctx, g1._u32[0] + -0x10);
    g81 = spu_ls_read128(ctx, g1._u32[0] + -0x20);
    g82 = spu_ls_read128(ctx, g1._u32[0] + -0x30);
    g83 = spu_ls_read128(ctx, g1._u32[0] + -0x40);
    g84 = spu_ls_read128(ctx, g1._u32[0] + -0x50);
    g85 = spu_ls_read128(ctx, g1._u32[0] + -0x60);
    g86 = spu_ls_read128(ctx, g1._u32[0] + -0x70);
    { ctx->gpr[0] = g0; ctx->gpr[1] = g1; ctx->gpr[80] = g80; ctx->gpr[81] = g81; ctx->gpr[82] = g82; ctx->gpr[83] = g83; ctx->gpr[84] = g84; ctx->gpr[85] = g85; ctx->gpr[86] = g86; return 1; }
}

static int fl_0000B5F0(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g3 = ctx->gpr[3];
    g3 = spu_ls_read128(ctx, g3._u32[0] + 0x50);
    g3 = spu_rotqbyi(g3, 8);
    { ctx->gpr[3] = g3; return 1; }
}

static int fl_0000B600(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g1 = ctx->gpr[1], g2 = ctx->gpr[2], g3 = ctx->gpr[3], g4 = ctx->gpr[4], g5 = ctx->gpr[5], g6 = ctx->gpr[6];
    g6 = spu_ls_read128(ctx, g3._u32[0] + 0x50);
    g5 = spu_cwd(g1, 8);
    g2 = spu_shufb(g4, g6, g5);
    spu_ls_write128(ctx, g3._u32[0] + 0x50, g2);
    { ctx->gpr[2] = g2; ctx->gpr[5] = g5; ctx->gpr[6] = g6; return 1; }
}

static int fl_0000B618(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g2 = ctx->gpr[2], g3 = ctx->gpr[3], g4 = ctx->gpr[4];
    g2 = spu_shli(g4, 2);
    g4 = spu_a(g2, g3);
    g3 = spu_ls_read128(ctx, g4._u32[0] + 0x10);
    g3 = spu_rotqby(g3, g4);
    { ctx->gpr[2] = g2; ctx->gpr[3] = g3; ctx->gpr[4] = g4; return 1; }
}

static int fl_0000B630(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    {  return 1; }
}

static int fl_0000B638(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g3 = ctx->gpr[3];
    g3 = spu_ls_read128(ctx, g3._u32[0] + 0x50);
    g3 = spu_rotqbyi(g3, 12);
    { ctx->gpr[3] = g3; return 1; }
}

static int fl_0000B648(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g3 = ctx->gpr[3];
    g3 = spu_ls_read128(ctx, g3._u32[0] + 0x60);
    { ctx->gpr[3] = g3; return 1; }
}

static int fl_0000B650(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g3 = ctx->gpr[3];
    g3 = spu_ls_read128(ctx, g3._u32[0] + 0x60);
    g3 = spu_rotqbyi(g3, 4);
    { ctx->gpr[3] = g3; return 1; }
}

static int fl_0000B660(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g3 = ctx->gpr[3];
    g3 = spu_ls_read128(ctx, g3._u32[0] + 0x60);
    g3 = spu_rotqbyi(g3, 8);
    { ctx->gpr[3] = g3; return 1; }
}

static int fl_0000B670(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g3 = ctx->gpr[3];
    g3 = spu_ls_read128(ctx, g3._u32[0] + 0x50);
    { ctx->gpr[3] = g3; return 1; }
}

static int fl_0000B6E4(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g3 = ctx->gpr[3], g6 = ctx->gpr[6], g7 = ctx->gpr[7], g8 = ctx->gpr[8], g20 = ctx->gpr[20], g21 = ctx->gpr[21], g22 = ctx->gpr[22];
    g22 = spu_ls_read128(ctx, g6._u32[0] + 0x0);
    g21 = spu_ls_read128(ctx, g7._u32[0] + 0x0);
    g8 = spu_rotqby(g22, g6);
    g20 = spu_rotqby(g21, g7);
    g3 = spu_sf(g20, g8);
    { ctx->gpr[3] = g3; ctx->gpr[8] = g8; ctx->gpr[20] = g20; ctx->gpr[21] = g21; ctx->gpr[22] = g22; return 1; }
}

static int fl_0000B9C0(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g1 = ctx->gpr[1], g3 = ctx->gpr[3], g14 = ctx->gpr[14];
    g3 = spu_ori(g14, 0);
    g1 = spu_ai(g1, 288);
    { ctx->gpr[1] = g1; ctx->gpr[3] = g3; return 1; }
}

static int fl_0000BE88(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g2 = ctx->gpr[2], g3 = ctx->gpr[3], g4 = ctx->gpr[4], g5 = ctx->gpr[5], g6 = ctx->gpr[6], g7 = ctx->gpr[7], g8 = ctx->gpr[8], g9 = ctx->gpr[9], g10 = ctx->gpr[10], g11 = ctx->gpr[11], g12 = ctx->gpr[12], g13 = ctx->gpr[13], g14 = ctx->gpr[14], g15 = ctx->gpr[15], g16 = ctx->gpr[16], g17 = ctx->gpr[17], g18 = ctx->gpr[18], g19 = ctx->gpr[19];
    g19 = spu_ilh(0x303);
    g18 = spu_ls_read128(ctx, g3._u32[0] + 0x0);
    g17 = spu_ls_read128(ctx, g3._u32[0] + 0x160);
    g16 = spu_shufb(g4, g19, g19);
    g14 = spu_rotqbyi(g18, 13);
    g13 = spu_ceqb(g16, g17);
    g8 = spu_gbb(g13);
    g15 = spu_cgtbi(g14, -1);
    g11 = spu_andi(g14, 15);
    g12 = spu_xsbh(g15);
    g9 = spu_ahi(g11, 1);
    g10 = spu_ceqhi(g12, 0);
    g6 = spu_clz(g8);
    g7 = spu_selb(g11, g9, g10);
    g4 = spu_ai(g6, -16);
    g5 = spu_andi(g7, 255);
    g2 = spu_cgt(g5, g4);
    g3 = spu_ceqi(g2, 0);
    g3 = spu_or(g4, g3);
    { ctx->gpr[2] = g2; ctx->gpr[3] = g3; ctx->gpr[4] = g4; ctx->gpr[5] = g5; ctx->gpr[6] = g6; ctx->gpr[7] = g7; ctx->gpr[8] = g8; ctx->gpr[9] = g9; ctx->gpr[10] = g10; ctx->gpr[11] = g11; ctx->gpr[12] = g12; ctx->gpr[13] = g13; ctx->gpr[14] = g14; ctx->gpr[15] = g15; ctx->gpr[16] = g16; ctx->gpr[17] = g17; ctx->gpr[18] = g18; ctx->gpr[19] = g19; return 1; }
}

static int fl_0000BF90(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g1 = ctx->gpr[1], g80 = ctx->gpr[80], g81 = ctx->gpr[81], g126 = ctx->gpr[126];
    g1 = spu_ai(g1, 80);
    g80 = spu_ls_read128(ctx, g1._u32[0] + -0x10);
    g81 = spu_ls_read128(ctx, g1._u32[0] + -0x20);
    g126 = spu_ls_read128(ctx, g1._u32[0] + -0x30);
    { ctx->gpr[1] = g1; ctx->gpr[80] = g80; ctx->gpr[81] = g81; ctx->gpr[126] = g126; return 1; }
}

static int fl_0000C334(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g2 = ctx->gpr[2], g38 = ctx->gpr[38];
    spu_ls_write128(ctx, g38._u32[0] + 0x180, g2);
    {  return 1; }
}

static int fl_0000C454(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g0 = ctx->gpr[0], g1 = ctx->gpr[1], g3 = ctx->gpr[3], g4 = ctx->gpr[4], g5 = ctx->gpr[5], g6 = ctx->gpr[6], g80 = ctx->gpr[80], g81 = ctx->gpr[81], g82 = ctx->gpr[82], g83 = ctx->gpr[83], g84 = ctx->gpr[84], g85 = ctx->gpr[85], g86 = ctx->gpr[86];
    g4 = spu_cbd(g1, 13);
    g5 = spu_shufb(g3, g6, g4);
    spu_ls_write128(ctx, g86._u32[0] + 0x10, g5);
    g0 = spu_ls_read128(ctx, g1._u32[0] + 0xA0);
    g1 = spu_ai(g1, 144);
    g80 = spu_ls_read128(ctx, g1._u32[0] + -0x10);
    g81 = spu_ls_read128(ctx, g1._u32[0] + -0x20);
    g82 = spu_ls_read128(ctx, g1._u32[0] + -0x30);
    g83 = spu_ls_read128(ctx, g1._u32[0] + -0x40);
    g84 = spu_ls_read128(ctx, g1._u32[0] + -0x50);
    g85 = spu_ls_read128(ctx, g1._u32[0] + -0x60);
    g86 = spu_ls_read128(ctx, g1._u32[0] + -0x70);
    { ctx->gpr[0] = g0; ctx->gpr[1] = g1; ctx->gpr[4] = g4; ctx->gpr[5] = g5; ctx->gpr[80] = g80; ctx->gpr[81] = g81; ctx->gpr[82] = g82; ctx->gpr[83] = g83; ctx->gpr[84] = g84; ctx->gpr[85] = g85; ctx->gpr[86] = g86; return 1; }
}

static int fl_0000C480(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g1 = ctx->gpr[1], g85 = ctx->gpr[85], g86 = ctx->gpr[86];
    g85 = spu_ls_read128(ctx, g1._u32[0] + -0x60);
    g86 = spu_ls_read128(ctx, g1._u32[0] + -0x70);
    { ctx->gpr[85] = g85; ctx->gpr[86] = g86; return 1; }
}

static int fl_0000C5E8(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g0 = ctx->gpr[0], g1 = ctx->gpr[1], g3 = ctx->gpr[3], g5 = ctx->gpr[5], g8 = ctx->gpr[8], g9 = ctx->gpr[9], g80 = ctx->gpr[80], g81 = ctx->gpr[81], g82 = ctx->gpr[82], g83 = ctx->gpr[83], g84 = ctx->gpr[84], g85 = ctx->gpr[85], g86 = ctx->gpr[86], g87 = ctx->gpr[87], g88 = ctx->gpr[88];
    g9 = spu_cbd(g1, 13);
    g5 = spu_shufb(g3, g8, g9);
    spu_ls_write128(ctx, g87._u32[0] + 0x10, g5);
    g0 = spu_ls_read128_kcx(ctx, g1._u32[0] + 0xC0);
    g1 = spu_ai(g1, 176);
    g80 = spu_ls_read128(ctx, g1._u32[0] + -0x10);
    g81 = spu_ls_read128(ctx, g1._u32[0] + -0x20);
    g82 = spu_ls_read128(ctx, g1._u32[0] + -0x30);
    g83 = spu_ls_read128(ctx, g1._u32[0] + -0x40);
    g84 = spu_ls_read128(ctx, g1._u32[0] + -0x50);
    g85 = spu_ls_read128(ctx, g1._u32[0] + -0x60);
    g86 = spu_ls_read128(ctx, g1._u32[0] + -0x70);
    g87 = spu_ls_read128(ctx, g1._u32[0] + -0x80);
    g88 = spu_ls_read128(ctx, g1._u32[0] + -0x90);
    { ctx->gpr[0] = g0; ctx->gpr[1] = g1; ctx->gpr[5] = g5; ctx->gpr[9] = g9; ctx->gpr[80] = g80; ctx->gpr[81] = g81; ctx->gpr[82] = g82; ctx->gpr[83] = g83; ctx->gpr[84] = g84; ctx->gpr[85] = g85; ctx->gpr[86] = g86; ctx->gpr[87] = g87; ctx->gpr[88] = g88; return 1; }
}

static int fl_0000C610(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g1 = ctx->gpr[1], g84 = ctx->gpr[84], g85 = ctx->gpr[85], g86 = ctx->gpr[86], g87 = ctx->gpr[87], g88 = ctx->gpr[88];
    g84 = spu_ls_read128(ctx, g1._u32[0] + -0x50);
    g85 = spu_ls_read128(ctx, g1._u32[0] + -0x60);
    g86 = spu_ls_read128(ctx, g1._u32[0] + -0x70);
    g87 = spu_ls_read128(ctx, g1._u32[0] + -0x80);
    g88 = spu_ls_read128(ctx, g1._u32[0] + -0x90);
    { ctx->gpr[84] = g84; ctx->gpr[85] = g85; ctx->gpr[86] = g86; ctx->gpr[87] = g87; ctx->gpr[88] = g88; return 1; }
}

static int fl_0000C9A0(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g1 = ctx->gpr[1], g126 = ctx->gpr[126];
    g126 = spu_ls_read128(ctx, g1._u32[0] + -0xC0);
    { ctx->gpr[126] = g126; return 1; }
}

static int fl_0000CC80(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g1 = ctx->gpr[1], g82 = ctx->gpr[82], g83 = ctx->gpr[83], g84 = ctx->gpr[84], g85 = ctx->gpr[85], g126 = ctx->gpr[126];
    g82 = spu_ls_read128(ctx, g1._u32[0] + -0x30);
    g83 = spu_ls_read128(ctx, g1._u32[0] + -0x40);
    g84 = spu_ls_read128(ctx, g1._u32[0] + -0x50);
    g85 = spu_ls_read128(ctx, g1._u32[0] + -0x60);
    g126 = spu_ls_read128(ctx, g1._u32[0] + -0x70);
    { ctx->gpr[82] = g82; ctx->gpr[83] = g83; ctx->gpr[84] = g84; ctx->gpr[85] = g85; ctx->gpr[126] = g126; return 1; }
}

static int fl_0000D110(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    {  return 1; }
}

static int fl_0000D150(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g3 = ctx->gpr[3], g4 = ctx->gpr[4], g6 = ctx->gpr[6];
    g3 = spu_selb(g4, g3, g6);
    { ctx->gpr[3] = g3; return 1; }
}

static int fl_0000D234(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g0 = ctx->gpr[0], g1 = ctx->gpr[1], g3 = ctx->gpr[3], g80 = ctx->gpr[80], g81 = ctx->gpr[81], g82 = ctx->gpr[82];
    g0 = spu_ls_read128(ctx, g1._u32[0] + 0x60);
    g1 = spu_ai(g1, 80);
    g3 = spu_shlqbyi(g82, 0);
    g80 = spu_ls_read128(ctx, g1._u32[0] + -0x10);
    g81 = spu_ls_read128(ctx, g1._u32[0] + -0x20);
    g82 = spu_ls_read128(ctx, g1._u32[0] + -0x30);
    { ctx->gpr[0] = g0; ctx->gpr[1] = g1; ctx->gpr[3] = g3; ctx->gpr[80] = g80; ctx->gpr[81] = g81; ctx->gpr[82] = g82; return 1; }
}

static int fl_0000DBA8(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g0 = ctx->gpr[0], g1 = ctx->gpr[1], g3 = ctx->gpr[3], g80 = ctx->gpr[80], g81 = ctx->gpr[81], g82 = ctx->gpr[82], g126 = ctx->gpr[126];
    g3 = spu_ori(g80, 0);
    g0 = spu_ls_read128(ctx, g1._u32[0] + 0xA0);
    g1 = spu_ai(g1, 144);
    g80 = spu_ls_read128(ctx, g1._u32[0] + -0x10);
    g81 = spu_ls_read128(ctx, g1._u32[0] + -0x20);
    g82 = spu_ls_read128(ctx, g1._u32[0] + -0x30);
    g126 = spu_ls_read128(ctx, g1._u32[0] + -0x40);
    { ctx->gpr[0] = g0; ctx->gpr[1] = g1; ctx->gpr[3] = g3; ctx->gpr[80] = g80; ctx->gpr[81] = g81; ctx->gpr[82] = g82; ctx->gpr[126] = g126; return 1; }
}

static int fl_0000E538(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g0 = ctx->gpr[0], g1 = ctx->gpr[1], g80 = ctx->gpr[80], g81 = ctx->gpr[81];
    g0 = spu_ls_read128(ctx, g1._u32[0] + 0x50);
    g1 = spu_ai(g1, 64);
    g80 = spu_ls_read128(ctx, g1._u32[0] + -0x10);
    g81 = spu_ls_read128(ctx, g1._u32[0] + -0x20);
    { ctx->gpr[0] = g0; ctx->gpr[1] = g1; ctx->gpr[80] = g80; ctx->gpr[81] = g81; return 1; }
}

static int fl_0000F4BC(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g0 = ctx->gpr[0], g1 = ctx->gpr[1], g80 = ctx->gpr[80], g81 = ctx->gpr[81], g82 = ctx->gpr[82], g83 = ctx->gpr[83], g84 = ctx->gpr[84], g85 = ctx->gpr[85], g86 = ctx->gpr[86], g87 = ctx->gpr[87], g88 = ctx->gpr[88], g89 = ctx->gpr[89], g90 = ctx->gpr[90], g91 = ctx->gpr[91], g92 = ctx->gpr[92], g93 = ctx->gpr[93], g94 = ctx->gpr[94], g95 = ctx->gpr[95], g96 = ctx->gpr[96], g97 = ctx->gpr[97], g98 = ctx->gpr[98], g99 = ctx->gpr[99], g100 = ctx->gpr[100], g101 = ctx->gpr[101], g102 = ctx->gpr[102], g126 = ctx->gpr[126];
    g0 = spu_ls_read128(ctx, g1._u32[0] + 0x2A0);
    g85 = spu_il(656);
    g1 = spu_a(g1, g85);
    g80 = spu_ls_read128(ctx, g1._u32[0] + -0x10);
    g81 = spu_ls_read128(ctx, g1._u32[0] + -0x20);
    g82 = spu_ls_read128(ctx, g1._u32[0] + -0x30);
    g83 = spu_ls_read128(ctx, g1._u32[0] + -0x40);
    g84 = spu_ls_read128(ctx, g1._u32[0] + -0x50);
    g85 = spu_ls_read128(ctx, g1._u32[0] + -0x60);
    g86 = spu_ls_read128(ctx, g1._u32[0] + -0x70);
    g87 = spu_ls_read128(ctx, g1._u32[0] + -0x80);
    g88 = spu_ls_read128(ctx, g1._u32[0] + -0x90);
    g89 = spu_ls_read128(ctx, g1._u32[0] + -0xA0);
    g90 = spu_ls_read128(ctx, g1._u32[0] + -0xB0);
    g91 = spu_ls_read128(ctx, g1._u32[0] + -0xC0);
    g92 = spu_ls_read128(ctx, g1._u32[0] + -0xD0);
    g93 = spu_ls_read128(ctx, g1._u32[0] + -0xE0);
    g94 = spu_ls_read128(ctx, g1._u32[0] + -0xF0);
    g95 = spu_ls_read128(ctx, g1._u32[0] + -0x100);
    g96 = spu_ls_read128(ctx, g1._u32[0] + -0x110);
    g97 = spu_ls_read128(ctx, g1._u32[0] + -0x120);
    g98 = spu_ls_read128(ctx, g1._u32[0] + -0x130);
    g99 = spu_ls_read128(ctx, g1._u32[0] + -0x140);
    g100 = spu_ls_read128(ctx, g1._u32[0] + -0x150);
    g101 = spu_ls_read128(ctx, g1._u32[0] + -0x160);
    g102 = spu_ls_read128(ctx, g1._u32[0] + -0x170);
    g126 = spu_ls_read128(ctx, g1._u32[0] + -0x180);
    { ctx->gpr[0] = g0; ctx->gpr[1] = g1; ctx->gpr[80] = g80; ctx->gpr[81] = g81; ctx->gpr[82] = g82; ctx->gpr[83] = g83; ctx->gpr[84] = g84; ctx->gpr[85] = g85; ctx->gpr[86] = g86; ctx->gpr[87] = g87; ctx->gpr[88] = g88; ctx->gpr[89] = g89; ctx->gpr[90] = g90; ctx->gpr[91] = g91; ctx->gpr[92] = g92; ctx->gpr[93] = g93; ctx->gpr[94] = g94; ctx->gpr[95] = g95; ctx->gpr[96] = g96; ctx->gpr[97] = g97; ctx->gpr[98] = g98; ctx->gpr[99] = g99; ctx->gpr[100] = g100; ctx->gpr[101] = g101; ctx->gpr[102] = g102; ctx->gpr[126] = g126; return 1; }
}

#endif
