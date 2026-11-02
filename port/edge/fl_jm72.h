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

static int fl_00005DA8(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g3 = ctx->gpr[3], g4 = ctx->gpr[4], g5 = ctx->gpr[5], g6 = ctx->gpr[6], g7 = ctx->gpr[7];
fl_00005DA8: ;
    g6 = spu_ai(g6, 8);
    spu_ls_write128(ctx, g3._u32[0] + 0x0, g5);
    spu_ls_write128(ctx, g3._u32[0] + 0x10, g5);
    g7 = spu_cgt(g4, g6);
    spu_ls_write128(ctx, g3._u32[0] + 0x20, g5);
    spu_ls_write128(ctx, g3._u32[0] + 0x30, g5);
    spu_ls_write128(ctx, g3._u32[0] + 0x40, g5);
    spu_ls_write128(ctx, g3._u32[0] + 0x50, g5);
    spu_ls_write128(ctx, g3._u32[0] + 0x60, g5);
    spu_ls_write128(ctx, g3._u32[0] + 0x70, g5);
    g3 = spu_ai(g3, 128);
    if (g7._u32[0] != 0) goto fl_00005DA8;
    { ctx->gpr[3] = g3; ctx->gpr[6] = g6; ctx->gpr[7] = g7; return 1; }
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

static int fl_000071C8(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g2 = ctx->gpr[2], g3 = ctx->gpr[3], g4 = ctx->gpr[4], g5 = ctx->gpr[5], g6 = ctx->gpr[6], g7 = ctx->gpr[7], g8 = ctx->gpr[8], g9 = ctx->gpr[9], g10 = ctx->gpr[10], g11 = ctx->gpr[11], g12 = ctx->gpr[12], g13 = ctx->gpr[13], g14 = ctx->gpr[14], g15 = ctx->gpr[15], g16 = ctx->gpr[16], g17 = ctx->gpr[17], g18 = ctx->gpr[18], g19 = ctx->gpr[19], g20 = ctx->gpr[20], g21 = ctx->gpr[21], g22 = ctx->gpr[22], g23 = ctx->gpr[23], g24 = ctx->gpr[24], g25 = ctx->gpr[25], g26 = ctx->gpr[26], g27 = ctx->gpr[27], g28 = ctx->gpr[28], g29 = ctx->gpr[29], g30 = ctx->gpr[30], g31 = ctx->gpr[31], g32 = ctx->gpr[32], g33 = ctx->gpr[33], g34 = ctx->gpr[34], g35 = ctx->gpr[35], g36 = ctx->gpr[36], g37 = ctx->gpr[37], g38 = ctx->gpr[38], g39 = ctx->gpr[39], g40 = ctx->gpr[40], g41 = ctx->gpr[41], g42 = ctx->gpr[42], g43 = ctx->gpr[43], g44 = ctx->gpr[44], g45 = ctx->gpr[45], g46 = ctx->gpr[46], g47 = ctx->gpr[47], g48 = ctx->gpr[48], g49 = ctx->gpr[49], g50 = ctx->gpr[50], g51 = ctx->gpr[51], g52 = ctx->gpr[52], g53 = ctx->gpr[53], g54 = ctx->gpr[54], g55 = ctx->gpr[55], g56 = ctx->gpr[56], g57 = ctx->gpr[57], g58 = ctx->gpr[58], g59 = ctx->gpr[59], g60 = ctx->gpr[60], g61 = ctx->gpr[61], g62 = ctx->gpr[62], g63 = ctx->gpr[63], g64 = ctx->gpr[64], g65 = ctx->gpr[65], g66 = ctx->gpr[66], g67 = ctx->gpr[67], g68 = ctx->gpr[68], g69 = ctx->gpr[69], g70 = ctx->gpr[70], g71 = ctx->gpr[71], g72 = ctx->gpr[72], g73 = ctx->gpr[73], g74 = ctx->gpr[74], g75 = ctx->gpr[75], g76 = ctx->gpr[76], g77 = ctx->gpr[77], g78 = ctx->gpr[78], g79 = ctx->gpr[79];
fl_000071C8: ;
    g56 = spu_ai(g56, 4);
    g10 = spu_ls_read128(ctx, 0x10B90);
    g11 = spu_ls_read128(ctx, g38._u32[0] + 0x0);
    g12 = spu_ls_read128(ctx, g38._u32[0] + 0x10);
    g22 = spu_ls_read128(ctx, g38._u32[0] + 0x20);
    g29 = spu_ls_read128(ctx, g38._u32[0] + 0x30);
    g38 = spu_ai(g38, 64);
    g33 = spu_ls_read128(ctx, g37._u32[0] + 0x0);
    g34 = spu_ls_read128(ctx, g37._u32[0] + 0x10);
    g31 = spu_fcgt(g10, g11);
    g35 = spu_ls_read128(ctx, g37._u32[0] + 0x20);
    g79 = spu_fcgt(g10, g12);
    g36 = spu_ls_read128(ctx, g37._u32[0] + 0x30);
    g78 = spu_fcgt(g10, g22);
    g77 = spu_fcgt(g10, g29);
    g72 = spu_andc(g22, g54);
    g71 = spu_andc(g29, g54);
    g76 = spu_andc(g11, g54);
    g74 = spu_andc(g12, g54);
    g27 = spu_selb(g48, g47, g31);
    g3 = spu_selb(g48, g47, g79);
    g4 = spu_selb(g48, g47, g78);
    g26 = spu_selb(g48, g47, g77);
    g69 = spu_fm(g27, g76);
    g67 = spu_fm(g3, g74);
    g65 = spu_fm(g4, g72);
    g5 = spu_fm(g26, g71);
    g23 = spu_selb(g46, g45, g31);
    g30 = spu_selb(g46, g45, g79);
    g24 = spu_selb(g46, g45, g78);
    g70 = spu_shufb(g69, g69, g41);
    g25 = spu_selb(g46, g45, g77);
    g68 = spu_shufb(g67, g67, g41);
    g6 = spu_fm(g23, g76);
    g66 = spu_shufb(g65, g65, g41);
    g7 = spu_fm(g30, g74);
    g64 = spu_shufb(g5, g5, g41);
    g8 = spu_fm(g24, g72);
    g60 = spu_shufb(g69, g69, g40);
    g9 = spu_fm(g25, g71);
    g32 = spu_shufb(g67, g67, g40);
    g75 = spu_selb(g44, g43, g31);
    g28 = spu_shufb(g65, g65, g40);
    g73 = spu_selb(g44, g43, g79);
    g29 = spu_shufb(g5, g5, g40);
    g18 = spu_selb(g44, g43, g78);
    g63 = spu_shufb(g6, g6, g41);
    g14 = spu_selb(g44, g43, g77);
    g62 = spu_shufb(g7, g7, g41);
    g11 = spu_fm(g75, g76);
    g61 = spu_shufb(g8, g8, g41);
    g12 = spu_fm(g73, g74);
    g21 = spu_shufb(g9, g9, g41);
    g10 = spu_fm(g18, g72);
    g30 = spu_shufb(g6, g6, g40);
    g78 = spu_fm(g14, g71);
    g31 = spu_shufb(g7, g7, g40);
    g59 = spu_fa_r(g69, g70);
    g77 = spu_shufb(g8, g8, g40);
    g58 = spu_fa_r(g67, g68);
    g74 = spu_shufb(g9, g9, g40);
    g16 = spu_fa_r(g65, g66);
    g15 = spu_shufb(g11, g11, g41);
    g22 = spu_fa_r(g5, g64);
    g17 = spu_shufb(g12, g12, g41);
    g24 = spu_fa_r(g6, g63);
    g13 = spu_shufb(g10, g10, g41);
    g79 = spu_fa(g7, g62);
    g23 = spu_shufb(g78, g78, g41);
    g76 = spu_fa(g8, g61);
    g18 = spu_shufb(g11, g11, g40);
    g73 = spu_fa(g9, g21);
    g69 = spu_shufb(g12, g12, g40);
    g2 = spu_fa(g59, g60);
    g66 = spu_shufb(g10, g10, g40);
    g20 = spu_fa(g58, g32);
    g62 = spu_shufb(g78, g78, g40);
    g19 = spu_fa(g16, g28);
    g27 = spu_fa(g22, g29);
    g71 = spu_fa(g11, g15);
    g68 = spu_fa(g12, g17);
    g65 = spu_fa(g10, g13);
    g26 = spu_shufb(g2, g2, g39);
    g61 = spu_fa(g78, g23);
    g3 = spu_shufb(g20, g20, g39);
    g75 = spu_fa(g24, g30);
    g4 = spu_shufb(g19, g19, g39);
    g72 = spu_fa(g79, g31);
    g25 = spu_shufb(g27, g27, g39);
    g70 = spu_fa(g76, g77);
    g63 = spu_shufb(g26, g53, g51);
    g67 = spu_fa(g73, g74);
    g59 = spu_shufb(g3, g53, g51);
    g16 = spu_fa(g71, g18);
    g58 = spu_shufb(g4, g53, g51);
    g30 = spu_fa(g68, g69);
    g5 = spu_shufb(g25, g53, g51);
    g21 = spu_fa(g65, g66);
    g64 = spu_shufb(g75, g75, g39);
    g29 = spu_fa(g61, g62);
    g60 = spu_shufb(g72, g72, g39);
    g7 = spu_cgt(g57, g56);
    g32 = spu_shufb(g70, g70, g39);
    g31 = spu_shufb(g67, g67, g39);
    g2 = spu_shufb(g64, g63, g50);
    g20 = spu_shufb(g60, g59, g50);
    g17 = spu_shufb(g32, g58, g50);
    g8 = spu_shufb(g31, g5, g50);
    g22 = spu_shufb(g16, g16, g39);
    g28 = spu_shufb(g30, g30, g39);
    g19 = spu_shufb(g21, g21, g39);
    g15 = spu_shufb(g29, g29, g39);
    g14 = spu_shufb(g22, g2, g49);
    g6 = spu_shufb(g28, g20, g49);
    g27 = spu_shufb(g19, g17, g49);
    g9 = spu_shufb(g15, g8, g49);
    g26 = spu_shufb(g52, g14, g55);
    g13 = spu_shufb(g52, g6, g55);
    g12 = spu_shufb(g52, g27, g55);
    g11 = spu_shufb(g52, g9, g55);
    g23 = spu_fma(g42, g26, g33);
    g3 = spu_fma(g42, g13, g34);
    g4 = spu_fma(g42, g12, g35);
    g24 = spu_fma(g42, g11, g36);
    spu_ls_write128(ctx, g37._u32[0] + 0x0, g23);
    spu_ls_write128(ctx, g37._u32[0] + 0x10, g3);
    spu_ls_write128(ctx, g37._u32[0] + 0x20, g4);
    spu_ls_write128(ctx, g37._u32[0] + 0x30, g24);
    g37 = spu_ai(g37, 64);
    if (g7._u32[0] != 0) goto fl_000071C8;
    { ctx->gpr[2] = g2; ctx->gpr[3] = g3; ctx->gpr[4] = g4; ctx->gpr[5] = g5; ctx->gpr[6] = g6; ctx->gpr[7] = g7; ctx->gpr[8] = g8; ctx->gpr[9] = g9; ctx->gpr[10] = g10; ctx->gpr[11] = g11; ctx->gpr[12] = g12; ctx->gpr[13] = g13; ctx->gpr[14] = g14; ctx->gpr[15] = g15; ctx->gpr[16] = g16; ctx->gpr[17] = g17; ctx->gpr[18] = g18; ctx->gpr[19] = g19; ctx->gpr[20] = g20; ctx->gpr[21] = g21; ctx->gpr[22] = g22; ctx->gpr[23] = g23; ctx->gpr[24] = g24; ctx->gpr[25] = g25; ctx->gpr[26] = g26; ctx->gpr[27] = g27; ctx->gpr[28] = g28; ctx->gpr[29] = g29; ctx->gpr[30] = g30; ctx->gpr[31] = g31; ctx->gpr[32] = g32; ctx->gpr[33] = g33; ctx->gpr[34] = g34; ctx->gpr[35] = g35; ctx->gpr[36] = g36; ctx->gpr[37] = g37; ctx->gpr[38] = g38; ctx->gpr[56] = g56; ctx->gpr[58] = g58; ctx->gpr[59] = g59; ctx->gpr[60] = g60; ctx->gpr[61] = g61; ctx->gpr[62] = g62; ctx->gpr[63] = g63; ctx->gpr[64] = g64; ctx->gpr[65] = g65; ctx->gpr[66] = g66; ctx->gpr[67] = g67; ctx->gpr[68] = g68; ctx->gpr[69] = g69; ctx->gpr[70] = g70; ctx->gpr[71] = g71; ctx->gpr[72] = g72; ctx->gpr[73] = g73; ctx->gpr[74] = g74; ctx->gpr[75] = g75; ctx->gpr[76] = g76; ctx->gpr[77] = g77; ctx->gpr[78] = g78; ctx->gpr[79] = g79; return 1; }
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

static int fl_00009AB8(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g1 = ctx->gpr[1], g2 = ctx->gpr[2], g3 = ctx->gpr[3], g4 = ctx->gpr[4], g5 = ctx->gpr[5], g6 = ctx->gpr[6], g7 = ctx->gpr[7], g8 = ctx->gpr[8], g9 = ctx->gpr[9], g10 = ctx->gpr[10], g11 = ctx->gpr[11], g12 = ctx->gpr[12], g13 = ctx->gpr[13], g14 = ctx->gpr[14], g15 = ctx->gpr[15], g16 = ctx->gpr[16], g17 = ctx->gpr[17], g18 = ctx->gpr[18], g19 = ctx->gpr[19], g20 = ctx->gpr[20], g21 = ctx->gpr[21], g22 = ctx->gpr[22], g23 = ctx->gpr[23], g24 = ctx->gpr[24], g25 = ctx->gpr[25], g26 = ctx->gpr[26], g27 = ctx->gpr[27], g28 = ctx->gpr[28], g29 = ctx->gpr[29], g30 = ctx->gpr[30], g31 = ctx->gpr[31], g32 = ctx->gpr[32], g33 = ctx->gpr[33], g34 = ctx->gpr[34], g35 = ctx->gpr[35], g36 = ctx->gpr[36], g37 = ctx->gpr[37], g38 = ctx->gpr[38], g39 = ctx->gpr[39], g40 = ctx->gpr[40], g41 = ctx->gpr[41], g42 = ctx->gpr[42], g43 = ctx->gpr[43], g44 = ctx->gpr[44], g45 = ctx->gpr[45], g46 = ctx->gpr[46], g47 = ctx->gpr[47], g48 = ctx->gpr[48], g49 = ctx->gpr[49], g50 = ctx->gpr[50], g51 = ctx->gpr[51], g52 = ctx->gpr[52], g53 = ctx->gpr[53], g54 = ctx->gpr[54], g55 = ctx->gpr[55], g56 = ctx->gpr[56], g57 = ctx->gpr[57], g58 = ctx->gpr[58], g59 = ctx->gpr[59], g60 = ctx->gpr[60], g61 = ctx->gpr[61], g62 = ctx->gpr[62], g63 = ctx->gpr[63], g64 = ctx->gpr[64], g65 = ctx->gpr[65], g66 = ctx->gpr[66], g67 = ctx->gpr[67], g68 = ctx->gpr[68], g69 = ctx->gpr[69], g70 = ctx->gpr[70], g71 = ctx->gpr[71], g72 = ctx->gpr[72], g73 = ctx->gpr[73], g74 = ctx->gpr[74], g75 = ctx->gpr[75], g76 = ctx->gpr[76], g77 = ctx->gpr[77], g78 = ctx->gpr[78], g79 = ctx->gpr[79];
    g33 = spu_shufb(g42, g27, g38);
    g27 = spu_ori(g3, 0);
    g24 = spu_shufb(g20, g48, g38);
    g17 = spu_cwd(g1, 12);
    g21 = spu_shufb(g20, g47, g38);
    g79 = spu_sf(g50, g79);
    g19 = spu_shufb(g42, g33, g14);
    g33 = spu_ori(g16, 0);
    g46 = spu_shufb(g20, g24, g14);
    g24 = spu_ori(g4, 0);
    g11 = spu_rotqbyi(g16, 12);
    g45 = spu_shufb(g20, g21, g14);
    g21 = spu_ori(g34, 0);
    g43 = spu_shufb(g42, g19, g15);
    g19 = spu_ori(g6, 0);
    g41 = spu_shufb(g20, g46, g15);
    g10 = spu_rotqbyi(g16, 4);
    g40 = spu_shufb(g44, g45, g15);
    g39 = spu_shufb(g42, g43, g17);
    g36 = spu_shufb(g20, g41, g17);
    g4 = spu_rotqbyi(g16, 8);
    g35 = spu_shufb(g20, g40, g38);
    g32 = spu_shufb(g20, g39, g38);
    g8 = spu_shufb(g37, g36, g38);
    g28 = spu_rotqbyi(g34, 12);
    g30 = spu_shufb(g20, g35, g14);
    g25 = spu_shufb(g20, g32, g14);
    g13 = spu_shufb(g31, g8, g14);
    g8 = spu_a(g3, g11);
    g23 = spu_rotqbyi(g34, 4);
    g31 = spu_a(g3, g4);
    g26 = spu_rotqbyi(g34, 8);
    g6 = spu_shufb(g20, g25, g15);
    g5 = spu_shufb(g12, g13, g15);
    g29 = spu_a(g34, g23);
    spu_ls_write128(ctx, g1._u32[0] + 0x40, g30);
    g30 = spu_a(g3, g10);
    spu_ls_write128(ctx, g1._u32[0] + 0x0, g49);
    g3 = spu_il(2544);
    g9 = spu_shufb(g20, g6, g17);
    g18 = spu_a(g26, g28);
    g2 = spu_shufb(g3, g5, g17);
    g18 = spu_a(g29, g18);
    spu_ls_write128(ctx, g1._u32[0] + 0x30, g9);
    spu_ls_write128(ctx, g1._u32[0] + 0x20, g2);
    if (g7._u16[0] != 0) goto fl_00009BD0;
    g51 = spu_cgti(g18, 0);
    if (g51._u32[0] == 0) goto fl_00009BD0;
    g7 = spu_il(0);
    g9 = spu_il(0);
    g6 = spu_shlqbyi(g30, 0);
fl_00009B90: ;
    g62 = spu_a(g7, g8);
    g60 = spu_ls_read128(ctx, g7._u32[0] + g8._u32[0]);
    g7 = spu_ai(g7, 1);
    g54 = spu_ls_read128(ctx, g6._u32[0] + 0x0);
    g61 = spu_ai(g62, 13);
    g52 = spu_ceq(g18, g7);
    g59 = spu_rotqby(g60, g61);
    g58 = spu_cgtbi(g59, -1);
    g57 = spu_xsbh(g58);
    g56 = spu_ceqhi(g57, 0);
    g55 = spu_fsm(g56);
    g53 = spu_selb(g54, g9, g55);
    spu_ls_write128(ctx, g6._u32[0] + 0x0, g53);
    g6 = spu_ai(g6, 16);
    if (g52._u32[0] == 0) goto fl_00009B90;
fl_00009BD0: ;
    g63 = spu_cgt(g18, g19);
    if (g63._u32[0] == 0) goto fl_0000A360;
    g72 = spu_cgti(g19, 0);
    g68 = spu_ls_read128(ctx, g22._u32[0] + 0x0);
    g71 = spu_cgti(g21, 0);
    g5 = spu_sfi(g72, 0);
    g70 = spu_sfi(g71, 0);
    g69 = spu_ai(g22, 13);
    g67 = spu_and(g70, g5);
    g13 = spu_ai(g22, 1);
    g66 = spu_rotqby(g68, g69);
    g65 = spu_ceqbi(g67, 0);
    g29 = spu_a(g33, g27);
    g64 = spu_xsbh(g65);
    g32 = spu_andi(g66, 255);
    if (g64._u16[0] != 0) goto fl_0000A600;
    g76 = spu_mpyh(g18, g24);
    g7 = spu_ls_read128(ctx, g1._u32[0] + 0x20);
    g77 = spu_mpyh(g24, g18);
    g20 = spu_ls_read128(ctx, g1._u32[0] + 0x40);
    g75 = spu_mpyu(g18, g24);
    g9 = spu_ls_read128(ctx, g1._u32[0] + 0x30);
    g22 = spu_il(0);
    g17 = spu_fsmbi(0x0);
    g16 = spu_il(0);
    g25 = spu_ori(g7, 0);
    g15 = spu_rotqbyi(g7, 8);
    g14 = spu_rotqbyi(g7, 4);
    g74 = spu_a(g76, g77);
    g7 = spu_fsmbi(0x0);
    g73 = spu_a(g74, g75);
    g10 = spu_a(g73, g32);
fl_00009C60: ;
    g5 = spu_a(g17, g10);
    if (g13._u32[0] == 0) goto fl_00009CA8;
    g40 = spu_ai(g5, 7);
    g41 = spu_cgti(g5, -1);
    g4 = spu_andi(g5, 7);
    g39 = spu_selb(g40, g5, g41);
    g8 = spu_sfi(g4, 0);
    g38 = spu_rotmai(g39, -3);
    g37 = spu_a(g13, g38);
    g35 = spu_ls_read128(ctx, g13._u32[0] + g38._u32[0]);
    g36 = spu_ai(g37, 13);
    g34 = spu_rotqby(g35, g36);
    g12 = spu_andi(g34, 255);
    g6 = spu_rotma(g12, g8);
    g5 = spu_andi(g6, 1);
    g3 = spu_ceqbi(g5, 0);
    g2 = spu_xsbh(g3);
    if (g2._u16[0] != 0) goto fl_00009CF8;
fl_00009CA8: ;
    g45 = spu_shli(g9, 4);
    g5 = spu_ls_read128(ctx, g7._u32[0] + g30._u32[0]);
    g46 = spu_ai(g1, 32);
    g8 = spu_ls_read128(ctx, g7._u32[0] + g31._u32[0]);
    g22 = spu_ai(g22, 1);
    g6 = spu_ls_read128(ctx, g7._u32[0] + g29._u32[0]);
    g4 = spu_a(g45, g25);
    g3 = spu_a(g45, g46);
    g44 = spu_fceq(g5, g16);
    g43 = spu_gb(g44);
    g42 = spu_ceqi(g43, 15);
    if (g42._u32[0] != 0) goto fl_00009CF8;
    g47 = spu_ai(g4, 32);
    g9 = spu_ai(g9, 1);
    g20 = spu_ai(g20, 1);
    spu_ls_write128(ctx, g47._u32[0] + g1._u32[0], g6);
    spu_ls_write128(ctx, g3._u32[0] + g14._u32[0], g5);
    spu_ls_write128(ctx, g3._u32[0] + g15._u32[0], g8);
fl_00009CF8: ;
    g17 = spu_ai(g17, 1);
    g52 = spu_cgt(g19, g22);
    g51 = spu_cgt(g21, g17);
    g5 = spu_sfi(g52, 0);
    g50 = spu_sfi(g51, 0);
    g7 = spu_ai(g7, 16);
    g78 = spu_and(g50, g5);
    g49 = spu_ceqbi(g78, 0);
    g48 = spu_xsbh(g49);
    if (g48._u16[0] == 0) goto fl_00009C60;
    g53 = spu_ls_read128(ctx, g1._u32[0] + 0x40);
    g31 = spu_ls_read128(ctx, g1._u32[0] + 0x30);
    g21 = spu_cwd(g1, 0);
    g10 = spu_shufb(g20, g53, g21);
    g16 = spu_shufb(g9, g31, g21);
    spu_ls_write128(ctx, g1._u32[0] + 0x40, g10);
    spu_ls_write128(ctx, g1._u32[0] + 0x30, g16);
fl_00009D3C: ;
    g58 = spu_cgti(g23, 0);
    g12 = spu_cgt(g19, g22);
    g57 = spu_sfi(g58, 0);
    g56 = spu_and(g57, g5);
    g55 = spu_ceqbi(g56, 0);
    g54 = spu_xsbh(g55);
    if (g54._u16[0] != 0) goto fl_00009E94;
    g64 = spu_mpyh(g18, g24);
    g67 = spu_ls_read128(ctx, g1._u32[0] + 0x30);
    g65 = spu_mpyh(g24, g18);
    g66 = spu_ls_read128(ctx, g1._u32[0] + 0x20);
    g62 = spu_mpyu(g18, g24);
    g16 = spu_ls_read128(ctx, g1._u32[0] + 0x40);
    g63 = spu_shli(g17, 4);
    g10 = spu_fsmbi(0x0);
    g31 = spu_il(0);
    g15 = spu_ori(g67, 0);
    g14 = spu_rotqbyi(g67, 4);
    g9 = spu_a(g63, g30);
    g21 = spu_rotqbyi(g66, 4);
    g61 = spu_a(g64, g65);
    g6 = spu_a(g63, g29);
    g60 = spu_a(g61, g62);
    g59 = spu_a(g60, g32);
    g8 = spu_a(g59, g17);
fl_00009DA8: ;
    g5 = spu_a(g8, g10);
    if (g13._u32[0] == 0) goto fl_00009DF0;
    g12 = spu_ai(g5, 7);
    g4 = spu_cgti(g5, -1);
    g74 = spu_andi(g5, 7);
    g3 = spu_selb(g12, g5, g4);
    g73 = spu_sfi(g74, 0);
    g2 = spu_rotmai(g3, -3);
    g7 = spu_a(g13, g2);
    g76 = spu_ls_read128(ctx, g13._u32[0] + g2._u32[0]);
    g77 = spu_ai(g7, 13);
    g75 = spu_rotqby(g76, g77);
    g72 = spu_andi(g75, 255);
    g71 = spu_rotma(g72, g73);
    g70 = spu_andi(g71, 1);
    g69 = spu_ceqbi(g70, 0);
    g68 = spu_xsbh(g69);
    if (g68._u16[0] != 0) goto fl_00009E40;
fl_00009DF0: ;
    g39 = spu_a(g15, g14);
    g5 = spu_ls_read128(ctx, g9._u32[0] + 0x0);
    g36 = spu_ai(g1, 32);
    g7 = spu_ls_read128(ctx, g6._u32[0] + 0x0);
    g35 = spu_shli(g39, 4);
    g22 = spu_ai(g22, 1);
    g38 = spu_fceq(g5, g31);
    g4 = spu_a(g35, g25);
    g3 = spu_a(g35, g36);
    g37 = spu_gb(g38);
    g34 = spu_ceqi(g37, 15);
    if (g34._u32[0] != 0) goto fl_00009E40;
    g40 = spu_ai(g4, 32);
    g20 = spu_ai(g20, 1);
    g14 = spu_ai(g14, 1);
    spu_ls_write128(ctx, g40._u32[0] + g1._u32[0], g7);
    g16 = spu_ori(g20, 0);
    spu_ls_write128(ctx, g3._u32[0] + g21._u32[0], g5);
fl_00009E40: ;
    g10 = spu_ai(g10, 1);
    g12 = spu_cgt(g19, g22);
    g45 = spu_cgt(g23, g10);
    g44 = spu_sfi(g12, 0);
    g43 = spu_sfi(g45, 0);
    g6 = spu_ai(g6, 16);
    g42 = spu_and(g43, g44);
    g9 = spu_ai(g9, 16);
    g41 = spu_ceqbi(g42, 0);
    g5 = spu_xsbh(g41);
    if (g5._u16[0] == 0) goto fl_00009DA8;
    g17 = spu_a(g10, g17);
    g8 = spu_ls_read128(ctx, g1._u32[0] + 0x40);
    g9 = spu_ls_read128(ctx, g1._u32[0] + 0x30);
    g46 = spu_cwd(g1, 0);
    g6 = spu_cwd(g1, 4);
    g15 = spu_shufb(g16, g8, g46);
    g23 = spu_shufb(g14, g9, g6);
    spu_ls_write128(ctx, g1._u32[0] + 0x40, g15);
    spu_ls_write128(ctx, g1._u32[0] + 0x30, g23);
fl_00009E94: ;
    g50 = spu_cgti(g26, 0);
    g6 = spu_sfi(g12, 0);
    g78 = spu_sfi(g50, 0);
    g49 = spu_and(g78, g6);
    g48 = spu_ceqbi(g49, 0);
    g47 = spu_xsbh(g48);
    if (g47._u16[0] != 0) goto fl_00009FF8;
    g56 = spu_mpyh(g18, g24);
    g58 = spu_ls_read128(ctx, g1._u32[0] + 0x30);
    g57 = spu_mpyh(g24, g18);
    g31 = spu_ls_read128(ctx, g1._u32[0] + 0x20);
    g54 = spu_mpyu(g18, g24);
    g21 = spu_ls_read128(ctx, g1._u32[0] + 0x40);
    g55 = spu_shli(g17, 4);
    g10 = spu_fsmbi(0x0);
    g16 = spu_ori(g58, 0);
    g14 = spu_rotqbyi(g58, 8);
    g9 = spu_a(g55, g30);
    g15 = spu_rotqbyi(g58, 4);
    g53 = spu_a(g56, g57);
    g23 = spu_rotqbyi(g31, 4);
    g6 = spu_a(g55, g29);
    g31 = spu_fsmbi(0x0);
    g52 = spu_a(g53, g54);
    g51 = spu_a(g52, g32);
    g8 = spu_a(g17, g51);
fl_00009F00: ;
    g5 = spu_a(g8, g10);
    if (g13._u32[0] == 0) goto fl_00009F48;
    g72 = spu_ai(g5, 7);
    g73 = spu_cgti(g5, -1);
    g65 = spu_andi(g5, 7);
    g71 = spu_selb(g72, g5, g73);
    g64 = spu_sfi(g65, 0);
    g70 = spu_rotmai(g71, -3);
    g69 = spu_a(g13, g70);
    g67 = spu_ls_read128(ctx, g13._u32[0] + g70._u32[0]);
    g68 = spu_ai(g69, 13);
    g66 = spu_rotqby(g67, g68);
    g63 = spu_andi(g66, 255);
    g62 = spu_rotma(g63, g64);
    g61 = spu_andi(g62, 1);
    g60 = spu_ceqbi(g61, 0);
    g59 = spu_xsbh(g60);
    if (g59._u16[0] != 0) goto fl_00009FA8;
fl_00009F48: ;
    g12 = spu_a(g16, g14);
    g5 = spu_ls_read128(ctx, g9._u32[0] + 0x0);
    g22 = spu_ai(g22, 1);
    g7 = spu_ls_read128(ctx, g6._u32[0] + 0x0);
    g3 = spu_a(g12, g15);
    g76 = spu_ai(g1, 32);
    g75 = spu_shli(g3, 4);
    g12 = spu_cgt(g19, g22);
    g2 = spu_fceq(g5, g31);
    g4 = spu_a(g75, g25);
    g77 = spu_gb(g2);
    g3 = spu_a(g75, g76);
    g74 = spu_ceqi(g77, 15);
    if (g74._u32[0] != 0) goto fl_00009FA8;
    g4 = spu_ai(g4, 32);
    g20 = spu_ai(g20, 1);
    g14 = spu_ai(g14, 1);
    spu_ls_write128(ctx, g4._u32[0] + g1._u32[0], g7);
    g21 = spu_ori(g20, 0);
    spu_ls_write128(ctx, g3._u32[0] + g23._u32[0], g5);
fl_00009FA8: ;
    g10 = spu_ai(g10, 1);
    g4 = spu_sfi(g12, 0);
    g37 = spu_cgt(g26, g10);
    g6 = spu_ai(g6, 16);
    g36 = spu_sfi(g37, 0);
    g9 = spu_ai(g9, 16);
    g35 = spu_and(g36, g4);
    g34 = spu_ceqbi(g35, 0);
    g7 = spu_xsbh(g34);
    if (g7._u16[0] == 0) goto fl_00009F00;
    g17 = spu_a(g17, g10);
    g39 = spu_ls_read128(ctx, g1._u32[0] + 0x40);
    g6 = spu_ori(g4, 0);
    g16 = spu_ls_read128(ctx, g1._u32[0] + 0x30);
    g40 = spu_cwd(g1, 0);
    g38 = spu_cwd(g1, 8);
    g10 = spu_shufb(g21, g39, g40);
    g26 = spu_shufb(g14, g16, g38);
    spu_ls_write128(ctx, g1._u32[0] + 0x40, g10);
    spu_ls_write128(ctx, g1._u32[0] + 0x30, g26);
fl_00009FF8: ;
    g44 = spu_cgti(g28, 0);
    g43 = spu_sfi(g44, 0);
    g42 = spu_and(g43, g6);
    g41 = spu_ceqbi(g42, 0);
    g5 = spu_xsbh(g41);
    if (g5._u16[0] != 0) goto fl_0000A158;
    g9 = spu_mpyh(g24, g18);
    g6 = spu_ls_read128(ctx, g1._u32[0] + 0x30);
    g46 = spu_mpyh(g18, g24);
    g48 = spu_ls_read128(ctx, g1._u32[0] + 0x20);
    g45 = spu_mpyu(g18, g24);
    g23 = spu_ls_read128(ctx, g1._u32[0] + 0x40);
    g8 = spu_shli(g17, 4);
    g15 = spu_fsmbi(0x0);
    g78 = spu_rotqbyi(g6, 8);
    g14 = spu_a(g30, g8);
    g18 = spu_rotqbyi(g6, 4);
    g24 = spu_a(g46, g9);
    g21 = spu_rotqbyi(g48, 4);
    g7 = spu_a(g29, g8);
    g16 = spu_rotqbyi(g6, 12);
    g30 = spu_a(g24, g45);
    g9 = spu_fsmbi(0x0);
    g47 = spu_a(g6, g78);
    g29 = spu_a(g32, g30);
    g18 = spu_a(g47, g18);
    g5 = spu_a(g17, g29);
fl_0000A068: ;
    g3 = spu_ai(g5, 7);
    if (g13._u32[0] == 0) goto fl_0000A0B0;
    g60 = spu_cgti(g5, -1);
    g54 = spu_andi(g5, 7);
    g59 = spu_selb(g3, g5, g60);
    g53 = spu_sfi(g54, 0);
    g31 = spu_rotmai(g59, -3);
    g58 = spu_a(g13, g31);
    g56 = spu_ls_read128(ctx, g13._u32[0] + g31._u32[0]);
    g57 = spu_ai(g58, 13);
    g55 = spu_rotqby(g56, g57);
    g52 = spu_andi(g55, 255);
    g51 = spu_rotma(g52, g53);
    g50 = spu_andi(g51, 1);
    g32 = spu_ceqbi(g50, 0);
    g17 = spu_xsbh(g32);
    if (g17._u16[0] != 0) goto fl_0000A108;
fl_0000A0B0: ;
    g66 = spu_a(g18, g16);
    g6 = spu_ls_read128(ctx, g14._u32[0] + 0x0);
    g22 = spu_ai(g22, 1);
    g8 = spu_ls_read128(ctx, g7._u32[0] + 0x0);
    g62 = spu_shli(g66, 4);
    g63 = spu_ai(g1, 32);
    g12 = spu_cgt(g19, g22);
    g65 = spu_fceq(g6, g9);
    g4 = spu_a(g62, g25);
    g3 = spu_a(g62, g63);
    g64 = spu_gb(g65);
    g61 = spu_ceqi(g64, 15);
    if (g61._u32[0] != 0) goto fl_0000A108;
    g67 = spu_ai(g4, 32);
    g20 = spu_ai(g20, 1);
    g16 = spu_ai(g16, 1);
    spu_ls_write128(ctx, g67._u32[0] + g1._u32[0], g8);
    g23 = spu_ori(g20, 0);
    spu_ls_write128(ctx, g3._u32[0] + g21._u32[0], g6);
fl_0000A108: ;
    g15 = spu_ai(g15, 1);
    g72 = spu_sfi(g12, 0);
    g73 = spu_cgt(g28, g15);
    g5 = spu_ai(g5, 1);
    g71 = spu_sfi(g73, 0);
    g7 = spu_ai(g7, 16);
    g70 = spu_and(g71, g72);
    g14 = spu_ai(g14, 16);
    g69 = spu_ceqbi(g70, 0);
    g68 = spu_xsbh(g69);
    if (g68._u16[0] == 0) goto fl_0000A068;
    g15 = spu_ls_read128(ctx, g1._u32[0] + 0x40);
    g19 = spu_ls_read128(ctx, g1._u32[0] + 0x30);
    g14 = spu_cwd(g1, 0);
    g22 = spu_cwd(g1, 12);
    g28 = spu_shufb(g23, g15, g14);
    g13 = spu_shufb(g16, g19, g22);
    spu_ls_write128(ctx, g1._u32[0] + 0x40, g28);
    spu_ls_write128(ctx, g1._u32[0] + 0x30, g13);
fl_0000A158: ;
    g12 = spu_shli(g20, 4);
    g3 = spu_ls_read128(ctx, g1._u32[0] + 0x40);
    g77 = spu_cwd(g1, 4);
    g2 = spu_rotqbyi(g3, 4);
    g76 = spu_ori(g2, 1);
    g74 = spu_shufb(g76, g3, g77);
    spu_ls_write128(ctx, g1._u32[0] + 0x40, g74);
    if (g12._u32[0] == 0) goto fl_0000A2BC;
    g14 = spu_a(g27, g33);
    g33 = spu_ai(g1, 32);
    g15 = spu_andi(g14, 15);
    g13 = spu_a(g25, g33);
    g17 = spu_shlqbyi(g33, 0);
    g5 = spu_andi(g13, 15);
    g11 = spu_or(g5, g15);
    if (g11._u32[0] != 0) goto fl_0000A3E0;
    g25 = spu_clgti(g12, 15);
    if (g25._u32[0] == 0) goto fl_0000A388;
    g4 = spu_il(0);
fl_0000A1A8: ;
    g12 = spu_ai(g12, -16);
    g7 = spu_ls_read128(ctx, g4._u32[0] + g13._u32[0]);
    g20 = spu_clgti(g12, 15);
    spu_ls_write128(ctx, g4._u32[0] + g14._u32[0], g7);
    g4 = spu_ai(g4, 16);
    if (g20._u32[0] != 0) goto fl_0000A1A8;
    g13 = spu_a(g4, g13);
    g14 = spu_a(g4, g14);
    if (g12._u32[0] != 0) goto fl_0000A388;
fl_0000A1D0: ;
    g20 = spu_ls_read128(ctx, g1._u32[0] + 0x40);
    g67 = spu_ls_read128(ctx, g27._u32[0] + 0x0);
    g66 = spu_ls_read128(ctx, g1._u32[0] + 0x20);
    g12 = spu_shli(g20, 4);
    g5 = spu_rotqbyi(g67, 4);
    g3 = spu_rotqbyi(g66, 4);
    if (g12._u32[0] == 0) goto fl_0000A370;
    g14 = spu_a(g27, g5);
    g13 = spu_a(g17, g3);
    g15 = spu_andi(g14, 15);
    g5 = spu_andi(g13, 15);
    g68 = spu_or(g5, g15);
    if (g68._u32[0] != 0) goto fl_0000A728;
    g69 = spu_clgti(g12, 15);
    if (g69._u32[0] == 0) goto fl_0000A824;
    g4 = spu_il(0);
fl_0000A218: ;
    g12 = spu_ai(g12, -16);
    g71 = spu_ls_read128(ctx, g4._u32[0] + g13._u32[0]);
    g70 = spu_clgti(g12, 15);
    spu_ls_write128(ctx, g4._u32[0] + g14._u32[0], g71);
    g4 = spu_ai(g4, 16);
    if (g70._u32[0] != 0) goto fl_0000A218;
    g13 = spu_a(g4, g13);
    g14 = spu_a(g14, g4);
    if (g12._u32[0] != 0) goto fl_0000A824;
fl_0000A240: ;
    g20 = spu_ls_read128(ctx, g1._u32[0] + 0x40);
    g51 = spu_ls_read128(ctx, g27._u32[0] + 0x0);
    g9 = spu_ls_read128(ctx, g1._u32[0] + 0x20);
    g12 = spu_shli(g20, 4);
    g6 = spu_rotqbyi(g51, 8);
    g3 = spu_rotqbyi(g9, 8);
    if (g12._u32[0] == 0) goto fl_0000A370;
    g14 = spu_a(g27, g6);
    g13 = spu_a(g17, g3);
    g15 = spu_andi(g14, 15);
    g5 = spu_andi(g13, 15);
    g52 = spu_or(g5, g15);
    if (g52._u32[0] != 0) goto fl_0000A618;
    g17 = spu_clgti(g12, 15);
    if (g17._u32[0] == 0) goto fl_0000A850;
    g4 = spu_il(0);
fl_0000A288: ;
    g12 = spu_ai(g12, -16);
    g54 = spu_ls_read128(ctx, g4._u32[0] + g13._u32[0]);
    g53 = spu_clgti(g12, 15);
    spu_ls_write128(ctx, g4._u32[0] + g14._u32[0], g54);
    g4 = spu_ai(g4, 16);
    if (g53._u32[0] != 0) goto fl_0000A288;
    g13 = spu_a(g4, g13);
    g14 = spu_a(g4, g14);
    if (g12._u32[0] != 0) goto fl_0000A850;
fl_0000A2B0: ;
    g45 = spu_ls_read128(ctx, g27._u32[0] + 0x0);
    g20 = spu_ls_read128(ctx, g1._u32[0] + 0x40);
    g11 = spu_rotqbyi(g45, 12);
fl_0000A2BC: ;
    g46 = spu_ls_read128(ctx, g1._u32[0] + 0x20);
    g3 = spu_rotqbyi(g46, 12);
    if (g20._u32[0] == 0) goto fl_0000A31C;
fl_0000A2C8: ;
    g18 = spu_ai(g1, 32);
    g13 = spu_a(g11, g27);
    g12 = spu_a(g3, g18);
    g14 = spu_andi(g13, 15);
    g5 = spu_andi(g12, 15);
    g16 = spu_or(g5, g14);
    if (g16._u32[0] != 0) goto fl_0000A4F0;
    g79 = spu_clgti(g20, 15);
    g4 = spu_shlqbyi(g20, 0);
    if (g79._u32[0] == 0) goto fl_0000A3B4;
fl_0000A2F8: ;
    g4 = spu_ai(g4, -16);
    g21 = spu_ls_read128(ctx, g12._u32[0] + 0x0);
    g12 = spu_ai(g12, 16);
    g47 = spu_clgti(g4, 15);
    spu_ls_write128(ctx, g13._u32[0] + 0x0, g21);
    g13 = spu_ai(g13, 16);
    if (g47._u32[0] != 0) goto fl_0000A2F8;
    if (g4._u32[0] != 0) goto fl_0000A3B4;
fl_0000A31C: ;
    g12 = spu_ls_read128(ctx, g1._u32[0] + 0x30);
    g29 = spu_ls_read128(ctx, g27._u32[0] + 0x10);
    g24 = spu_cwd(g1, 4);
    g42 = spu_cwd(g1, 0);
    g10 = spu_cwd(g1, 8);
    g41 = spu_ls_read128(ctx, g27._u32[0] + 0x20);
    g35 = spu_rotqbyi(g12, 4);
    g39 = spu_rotqbyi(g12, 8);
    g38 = spu_cwd(g1, 12);
    g37 = spu_rotqbyi(g12, 12);
    g43 = spu_shufb(g35, g29, g24);
    g7 = spu_shufb(g20, g41, g42);
    g40 = spu_shufb(g12, g43, g42);
    spu_ls_write128(ctx, g27._u32[0] + 0x20, g7);
    g26 = spu_shufb(g39, g40, g10);
    g15 = spu_shufb(g37, g26, g38);
    spu_ls_write128(ctx, g27._u32[0] + 0x10, g15);
fl_0000A360: ;
    g3 = spu_ori(g27, 0);
    g27 = spu_il(2640);
    g1 = spu_a(g1, g27);
    { ctx->gpr[1] = g1; ctx->gpr[2] = g2; ctx->gpr[3] = g3; ctx->gpr[4] = g4; ctx->gpr[5] = g5; ctx->gpr[6] = g6; ctx->gpr[7] = g7; ctx->gpr[8] = g8; ctx->gpr[9] = g9; ctx->gpr[10] = g10; ctx->gpr[11] = g11; ctx->gpr[12] = g12; ctx->gpr[13] = g13; ctx->gpr[14] = g14; ctx->gpr[15] = g15; ctx->gpr[16] = g16; ctx->gpr[17] = g17; ctx->gpr[18] = g18; ctx->gpr[19] = g19; ctx->gpr[20] = g20; ctx->gpr[21] = g21; ctx->gpr[22] = g22; ctx->gpr[23] = g23; ctx->gpr[24] = g24; ctx->gpr[25] = g25; ctx->gpr[26] = g26; ctx->gpr[27] = g27; ctx->gpr[28] = g28; ctx->gpr[29] = g29; ctx->gpr[30] = g30; ctx->gpr[31] = g31; ctx->gpr[32] = g32; ctx->gpr[33] = g33; ctx->gpr[34] = g34; ctx->gpr[35] = g35; ctx->gpr[36] = g36; ctx->gpr[37] = g37; ctx->gpr[38] = g38; ctx->gpr[39] = g39; ctx->gpr[40] = g40; ctx->gpr[41] = g41; ctx->gpr[42] = g42; ctx->gpr[43] = g43; ctx->gpr[44] = g44; ctx->gpr[45] = g45; ctx->gpr[46] = g46; ctx->gpr[47] = g47; ctx->gpr[48] = g48; ctx->gpr[49] = g49; ctx->gpr[50] = g50; ctx->gpr[51] = g51; ctx->gpr[52] = g52; ctx->gpr[53] = g53; ctx->gpr[54] = g54; ctx->gpr[55] = g55; ctx->gpr[56] = g56; ctx->gpr[57] = g57; ctx->gpr[58] = g58; ctx->gpr[59] = g59; ctx->gpr[60] = g60; ctx->gpr[61] = g61; ctx->gpr[62] = g62; ctx->gpr[63] = g63; ctx->gpr[64] = g64; ctx->gpr[65] = g65; ctx->gpr[66] = g66; ctx->gpr[67] = g67; ctx->gpr[68] = g68; ctx->gpr[69] = g69; ctx->gpr[70] = g70; ctx->gpr[71] = g71; ctx->gpr[72] = g72; ctx->gpr[73] = g73; ctx->gpr[74] = g74; ctx->gpr[75] = g75; ctx->gpr[76] = g76; ctx->gpr[77] = g77; ctx->gpr[78] = g78; ctx->gpr[79] = g79; return 1; }
fl_0000A370: ;
    g14 = spu_ls_read128(ctx, g27._u32[0] + 0x0);
    g46 = spu_ls_read128(ctx, g1._u32[0] + 0x20);
    g11 = spu_rotqbyi(g14, 12);
    g3 = spu_rotqbyi(g46, 12);
    if (g20._u32[0] == 0) goto fl_0000A31C;
    goto fl_0000A2C8;
fl_0000A388: ;
    g37 = spu_il(-1);
    g34 = spu_ls_read128(ctx, g13._u32[0] + 0x0);
    g4 = spu_sfi(g12, 0);
    g36 = spu_ls_read128(ctx, g14._u32[0] + 0x0);
    g35 = spu_rotqmby(g37, g4);
    g12 = spu_selb(g34, g36, g35);
    spu_ls_write128(ctx, g14._u32[0] + 0x0, g12);
    goto fl_0000A1D0;
fl_0000A3B4: ;
    g30 = spu_il(-1);
    g48 = spu_ls_read128(ctx, g12._u32[0] + 0x0);
    g23 = spu_sfi(g4, 0);
    g78 = spu_ls_read128(ctx, g13._u32[0] + 0x0);
    g49 = spu_rotqmby(g30, g23);
    g8 = spu_selb(g48, g78, g49);
    spu_ls_write128(ctx, g13._u32[0] + 0x0, g8);
    goto fl_0000A31C;
fl_0000A3E0: ;
    g5 = spu_sf(g15, g5);
    g18 = spu_ls_read128(ctx, g13._u32[0] + 0x10);
    g32 = spu_ai(g25, 32);
    g45 = spu_fsmbi(0xFFFF);
    g49 = spu_shli(g5, 4);
    g40 = spu_ls_read128(ctx, g14._u32[0] + 0x0);
    g78 = spu_ila(0xC2C0);
    g9 = spu_ls_read128(ctx, g32._u32[0] + g1._u32[0]);
    g6 = spu_a(g12, g15);
    g48 = spu_a(g78, g79);
    g47 = spu_cgti(g5, 0);
    g21 = spu_sfi(g6, 0);
    g8 = spu_ls_read128(ctx, g49._u32[0] + g48._u32[0]);
    g26 = spu_clgti(g6, 15);
    g16 = spu_ceqi(g47, 0);
    g30 = spu_rotqmby(g9, g5);
    g5 = spu_sfi(g15, 16);
    g44 = spu_rotqmby(g45, g21);
    g46 = spu_ceqi(g26, 0);
    g29 = spu_fsm(g16);
    g41 = spu_shlqby(g45, g5);
    g24 = spu_shufb(g9, g18, g8);
    g42 = spu_fsm(g46);
    g43 = spu_or(g41, g44);
    g38 = spu_selb(g24, g30, g29);
    g39 = spu_selb(g41, g43, g42);
    g10 = spu_selb(g38, g40, g39);
    spu_ls_write128(ctx, g14._u32[0] + 0x0, g10);
    if (g26._u32[0] == 0) goto fl_0000A1D0;
    g7 = spu_a(g13, g5);
    g53 = spu_ila(0xC2C0);
    g54 = spu_andi(g7, 15);
    g51 = spu_a(g53, g79);
    g52 = spu_shli(g54, 4);
    g8 = spu_sf(g5, g12);
    g11 = spu_a(g14, g5);
    g50 = spu_clgti(g8, 15);
    g9 = spu_ls_read128(ctx, g52._u32[0] + g51._u32[0]);
    if (g50._u32[0] == 0) goto fl_0000A4B8;
fl_0000A490: ;
    g8 = spu_ai(g8, -16);
    g57 = spu_ls_read128(ctx, g7._u32[0] + 0x0);
    g7 = spu_ai(g7, 16);
    g55 = spu_clgti(g8, 15);
    g58 = spu_ls_read128(ctx, g7._u32[0] + 0x0);
    g56 = spu_shufb(g57, g58, g9);
    spu_ls_write128(ctx, g11._u32[0] + 0x0, g56);
    g11 = spu_ai(g11, 16);
    if (g55._u32[0] != 0) goto fl_0000A490;
fl_0000A4B8: ;
    if (g8._u32[0] == 0) goto fl_0000A1D0;
    g65 = spu_sfi(g8, 0);
    g64 = spu_il(-1);
    g63 = spu_ls_read128(ctx, g7._u32[0] + 0x10);
    g62 = spu_ls_read128(ctx, g7._u32[0] + 0x0);
    g61 = spu_ls_read128(ctx, g11._u32[0] + 0x0);
    g60 = spu_rotqmby(g64, g65);
    g59 = spu_shufb(g62, g63, g9);
    g31 = spu_selb(g59, g61, g60);
    spu_ls_write128(ctx, g11._u32[0] + 0x0, g31);
    goto fl_0000A1D0;
fl_0000A4F0: ;
    g66 = spu_sf(g14, g5);
    g62 = spu_ls_read128(ctx, g12._u32[0] + 0x10);
    g69 = spu_ila(0xC2C0);
    g31 = spu_fsmbi(0xFFFF);
    g68 = spu_shli(g66, 4);
    g52 = spu_ls_read128(ctx, g13._u32[0] + 0x0);
    g72 = spu_ai(g3, 32);
    g67 = spu_a(g69, g79);
    g6 = spu_a(g20, g14);
    g61 = spu_ls_read128(ctx, g72._u32[0] + g1._u32[0]);
    g64 = spu_cgti(g66, 0);
    g63 = spu_ls_read128(ctx, g68._u32[0] + g67._u32[0]);
    g65 = spu_sfi(g6, 0);
    g32 = spu_clgti(g6, 15);
    g14 = spu_sfi(g14, 16);
    g55 = spu_rotqmby(g31, g65);
    g60 = spu_ceqi(g64, 0);
    g59 = spu_ceqi(g32, 0);
    g17 = spu_shlqby(g31, g14);
    g57 = spu_rotqmby(g61, g66);
    g58 = spu_shufb(g61, g62, g63);
    g56 = spu_fsm(g60);
    g54 = spu_or(g17, g55);
    g53 = spu_fsm(g59);
    g9 = spu_selb(g58, g57, g56);
    g51 = spu_selb(g17, g54, g53);
    g50 = spu_selb(g9, g52, g51);
    spu_ls_write128(ctx, g13._u32[0] + 0x0, g50);
    if (g32._u32[0] == 0) goto fl_0000A31C;
    g5 = spu_a(g12, g14);
    g22 = spu_ila(0xC2C0);
    g74 = spu_andi(g5, 15);
    g28 = spu_a(g22, g79);
    g19 = spu_shli(g74, 4);
    g7 = spu_sf(g14, g20);
    g11 = spu_a(g13, g14);
    g73 = spu_clgti(g7, 15);
    g8 = spu_ls_read128(ctx, g19._u32[0] + g28._u32[0]);
    if (g73._u32[0] == 0) goto fl_0000A5C8;
    g4 = spu_ls_read128(ctx, g12._u32[0] + g14._u32[0]);
fl_0000A5A0: ;
    g5 = spu_ai(g5, 16);
    g77 = spu_shlqbyi(g4, 0);
    g7 = spu_ai(g7, -16);
    g4 = spu_ls_read128(ctx, g5._u32[0] + 0x0);
    g75 = spu_clgti(g7, 15);
    g76 = spu_shufb(g77, g4, g8);
    spu_ls_write128(ctx, g11._u32[0] + 0x0, g76);
    g11 = spu_ai(g11, 16);
    if (g75._u32[0] != 0) goto fl_0000A5A0;
fl_0000A5C8: ;
    if (g7._u32[0] == 0) goto fl_0000A31C;
    g13 = spu_sfi(g7, 0);
    g2 = spu_il(-1);
    g36 = spu_ls_read128(ctx, g5._u32[0] + 0x10);
    g70 = spu_ls_read128(ctx, g5._u32[0] + 0x0);
    g5 = spu_ls_read128(ctx, g11._u32[0] + 0x0);
    g34 = spu_rotqmby(g2, g13);
    g25 = spu_shufb(g70, g36, g8);
    g33 = spu_selb(g25, g5, g34);
    spu_ls_write128(ctx, g11._u32[0] + 0x0, g33);
    goto fl_0000A31C;
fl_0000A600: ;
    g22 = spu_il(0);
    g25 = spu_ls_read128(ctx, g1._u32[0] + 0x20);
    g17 = spu_il(0);
    g20 = spu_ls_read128(ctx, g1._u32[0] + 0x40);
    goto fl_00009D3C;
fl_0000A618: ;
    g77 = spu_sf(g15, g5);
    g19 = spu_ls_read128(ctx, g17._u32[0] + g3._u32[0]);
    g2 = spu_ila(0xC2C0);
    g22 = spu_ls_read128(ctx, g13._u32[0] + 0x10);
    g34 = spu_shli(g77, 4);
    g72 = spu_fsmbi(0xFFFF);
    g25 = spu_a(g2, g79);
    g65 = spu_ls_read128(ctx, g14._u32[0] + 0x0);
    g33 = spu_a(g12, g15);
    g75 = spu_cgti(g77, 0);
    g76 = spu_sfi(g33, 0);
    g74 = spu_ls_read128(ctx, g34._u32[0] + g25._u32[0]);
    g61 = spu_clgti(g33, 15);
    g70 = spu_rotqmby(g19, g77);
    g15 = spu_sfi(g15, 16);
    g68 = spu_rotqmby(g72, g76);
    g28 = spu_ceqi(g75, 0);
    g73 = spu_ceqi(g61, 0);
    g66 = spu_shlqby(g72, g15);
    g69 = spu_fsm(g28);
    g71 = spu_shufb(g19, g22, g74);
    g67 = spu_fsm(g73);
    g5 = spu_or(g66, g68);
    g63 = spu_selb(g71, g70, g69);
    g64 = spu_selb(g66, g5, g67);
    g62 = spu_selb(g63, g65, g64);
    spu_ls_write128(ctx, g14._u32[0] + 0x0, g62);
    if (g61._u32[0] == 0) goto fl_0000A2B0;
    g6 = spu_a(g13, g15);
    g7 = spu_ila(0xC2C0);
    g26 = spu_andi(g6, 15);
    g5 = spu_sf(g15, g12);
    g37 = spu_shli(g26, 4);
    g11 = spu_a(g14, g15);
    g15 = spu_a(g7, g79);
    g13 = spu_clgti(g5, 15);
    g7 = spu_ls_read128(ctx, g37._u32[0] + g15._u32[0]);
    if (g13._u32[0] == 0) goto fl_0000A6E8;
fl_0000A6C0: ;
    g5 = spu_ai(g5, -16);
    g10 = spu_ls_read128(ctx, g6._u32[0] + 0x0);
    g6 = spu_ai(g6, 16);
    g38 = spu_clgti(g5, 15);
    g40 = spu_ls_read128(ctx, g6._u32[0] + 0x0);
    g39 = spu_shufb(g10, g40, g7);
    spu_ls_write128(ctx, g11._u32[0] + 0x0, g39);
    g11 = spu_ai(g11, 16);
    if (g38._u32[0] != 0) goto fl_0000A6C0;
fl_0000A6E8: ;
    if (g5._u32[0] == 0) goto fl_0000A2B0;
    g35 = spu_sfi(g5, 0);
    g12 = spu_ls_read128(ctx, g6._u32[0] + 0x10);
    g29 = spu_il(-1);
    g24 = spu_ls_read128(ctx, g6._u32[0] + 0x0);
    g44 = spu_ls_read128(ctx, g11._u32[0] + 0x0);
    g43 = spu_rotqmby(g29, g35);
    g20 = spu_ls_read128(ctx, g1._u32[0] + 0x40);
    g42 = spu_shufb(g24, g12, g7);
    g41 = spu_selb(g42, g44, g43);
    spu_ls_write128(ctx, g11._u32[0] + 0x0, g41);
    g4 = spu_ls_read128(ctx, g27._u32[0] + 0x0);
    g11 = spu_rotqbyi(g4, 12);
    goto fl_0000A2BC;
fl_0000A728: ;
    g41 = spu_sf(g15, g5);
    g39 = spu_ls_read128(ctx, g17._u32[0] + g3._u32[0]);
    g29 = spu_ila(0xC2C0);
    g10 = spu_ls_read128(ctx, g13._u32[0] + 0x10);
    g44 = spu_shli(g41, 4);
    g37 = spu_fsmbi(0xFFFF);
    g43 = spu_a(g29, g79);
    g77 = spu_ls_read128(ctx, g14._u32[0] + 0x0);
    g42 = spu_a(g12, g15);
    g5 = spu_sfi(g15, 16);
    g4 = spu_sfi(g42, 0);
    g38 = spu_ls_read128(ctx, g44._u32[0] + g43._u32[0]);
    g74 = spu_clgti(g42, 15);
    g11 = spu_shlqby(g37, g5);
    g40 = spu_cgti(g41, 0);
    g34 = spu_rotqmby(g37, g4);
    g7 = spu_ceqi(g74, 0);
    g35 = spu_rotqmby(g39, g41);
    g26 = spu_ceqi(g40, 0);
    g33 = spu_fsm(g7);
    g25 = spu_or(g11, g34);
    g36 = spu_shufb(g39, g10, g38);
    g2 = spu_fsm(g26);
    g76 = spu_selb(g11, g25, g33);
    g75 = spu_selb(g36, g35, g2);
    g15 = spu_selb(g75, g77, g76);
    spu_ls_write128(ctx, g14._u32[0] + 0x0, g15);
    if (g74._u32[0] == 0) goto fl_0000A240;
    g7 = spu_a(g13, g5);
    g46 = spu_ila(0xC2C0);
    g16 = spu_andi(g7, 15);
    g11 = spu_a(g14, g5);
    g45 = spu_shli(g16, 4);
    g14 = spu_a(g46, g79);
    g8 = spu_sf(g5, g12);
    g12 = spu_clgti(g8, 15);
    g9 = spu_ls_read128(ctx, g45._u32[0] + g14._u32[0]);
    if (g12._u32[0] == 0) goto fl_0000A7F0;
fl_0000A7C8: ;
    g8 = spu_ai(g8, -16);
    g21 = spu_ls_read128(ctx, g7._u32[0] + 0x0);
    g7 = spu_ai(g7, 16);
    g18 = spu_clgti(g8, 15);
    g6 = spu_ls_read128(ctx, g7._u32[0] + 0x0);
    g47 = spu_shufb(g21, g6, g9);
    spu_ls_write128(ctx, g11._u32[0] + 0x0, g47);
    g11 = spu_ai(g11, 16);
    if (g18._u32[0] != 0) goto fl_0000A7C8;
fl_0000A7F0: ;
    if (g8._u32[0] == 0) goto fl_0000A240;
    g50 = spu_sfi(g8, 0);
    g32 = spu_il(-1);
    g23 = spu_ls_read128(ctx, g7._u32[0] + 0x10);
    g30 = spu_ls_read128(ctx, g7._u32[0] + 0x0);
    g78 = spu_ls_read128(ctx, g11._u32[0] + 0x0);
    g49 = spu_rotqmby(g32, g50);
    g48 = spu_shufb(g30, g23, g9);
    g8 = spu_selb(g48, g78, g49);
    spu_ls_write128(ctx, g11._u32[0] + 0x0, g8);
    goto fl_0000A240;
fl_0000A824: ;
    g19 = spu_il(-1);
    g73 = spu_ls_read128(ctx, g13._u32[0] + 0x0);
    g22 = spu_sfi(g12, 0);
    g28 = spu_ls_read128(ctx, g14._u32[0] + 0x0);
    g13 = spu_rotqmby(g19, g22);
    g72 = spu_selb(g73, g28, g13);
    spu_ls_write128(ctx, g14._u32[0] + 0x0, g72);
    goto fl_0000A240;
fl_0000A850: ;
    g60 = spu_sfi(g12, 0);
    g59 = spu_il(-1);
    g57 = spu_ls_read128(ctx, g13._u32[0] + 0x0);
    g31 = spu_ls_read128(ctx, g14._u32[0] + 0x0);
    g58 = spu_rotqmby(g59, g60);
    g20 = spu_ls_read128(ctx, g1._u32[0] + 0x40);
    g56 = spu_selb(g57, g31, g58);
    spu_ls_write128(ctx, g14._u32[0] + 0x0, g56);
    g55 = spu_ls_read128(ctx, g27._u32[0] + 0x0);
    g11 = spu_rotqbyi(g55, 12);
    goto fl_0000A2BC;
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

static int fl_0000BBC8(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g2 = ctx->gpr[2], g3 = ctx->gpr[3], g4 = ctx->gpr[4], g5 = ctx->gpr[5], g6 = ctx->gpr[6], g7 = ctx->gpr[7], g8 = ctx->gpr[8], g9 = ctx->gpr[9], g10 = ctx->gpr[10], g11 = ctx->gpr[11], g12 = ctx->gpr[12], g13 = ctx->gpr[13], g14 = ctx->gpr[14], g15 = ctx->gpr[15], g16 = ctx->gpr[16], g17 = ctx->gpr[17], g18 = ctx->gpr[18], g19 = ctx->gpr[19], g20 = ctx->gpr[20], g21 = ctx->gpr[21], g22 = ctx->gpr[22], g23 = ctx->gpr[23], g24 = ctx->gpr[24], g25 = ctx->gpr[25], g26 = ctx->gpr[26], g27 = ctx->gpr[27], g28 = ctx->gpr[28], g29 = ctx->gpr[29], g30 = ctx->gpr[30], g31 = ctx->gpr[31], g32 = ctx->gpr[32], g33 = ctx->gpr[33], g34 = ctx->gpr[34], g35 = ctx->gpr[35], g36 = ctx->gpr[36], g37 = ctx->gpr[37], g38 = ctx->gpr[38], g39 = ctx->gpr[39], g40 = ctx->gpr[40], g41 = ctx->gpr[41], g42 = ctx->gpr[42], g43 = ctx->gpr[43], g44 = ctx->gpr[44], g45 = ctx->gpr[45], g46 = ctx->gpr[46], g47 = ctx->gpr[47], g48 = ctx->gpr[48], g49 = ctx->gpr[49], g50 = ctx->gpr[50], g51 = ctx->gpr[51], g52 = ctx->gpr[52], g53 = ctx->gpr[53], g54 = ctx->gpr[54], g55 = ctx->gpr[55], g56 = ctx->gpr[56], g57 = ctx->gpr[57], g58 = ctx->gpr[58], g59 = ctx->gpr[59], g60 = ctx->gpr[60], g61 = ctx->gpr[61], g62 = ctx->gpr[62], g63 = ctx->gpr[63], g64 = ctx->gpr[64], g65 = ctx->gpr[65], g66 = ctx->gpr[66], g67 = ctx->gpr[67], g68 = ctx->gpr[68], g69 = ctx->gpr[69], g70 = ctx->gpr[70], g71 = ctx->gpr[71], g72 = ctx->gpr[72], g73 = ctx->gpr[73], g74 = ctx->gpr[74], g75 = ctx->gpr[75], g76 = ctx->gpr[76], g77 = ctx->gpr[77], g78 = ctx->gpr[78], g79 = ctx->gpr[79];
    g66 = spu_ai(g4, 7);
    g31 = spu_ls_const128(ctx, 0xFE70, 0x00010203u, 0x10111213u, 0x04050607u, 0x14151617u);
    g24 = spu_ls_read128(ctx, g3._u32[0] + 0x0);
    g23 = spu_andi(g66, -8);
    g25 = spu_ls_read128(ctx, g3._u32[0] + 0x10);
    g27 = spu_ls_read128(ctx, g3._u32[0] + 0x20);
    g4 = spu_shli(g23, 4);
    g26 = spu_ls_read128(ctx, g3._u32[0] + 0x30);
    g23 = spu_ilhu(0x3F80);
    g28 = spu_ls_read128(ctx, g3._u32[0] + 0x40);
    g29 = spu_ls_read128(ctx, g3._u32[0] + 0x50);
    g30 = spu_ls_read128(ctx, g3._u32[0] + 0x60);
    g17 = spu_a(g3, g4);
    g32 = spu_ls_read128(ctx, g3._u32[0] + 0x70);
    g19 = spu_sfi(g4, 0);
    g22 = spu_rotqbyi(g31, 8);
    g20 = spu_fsmbi(0xFF);
    g76 = spu_shufb(g24, g25, g31);
    g74 = spu_shufb(g28, g29, g31);
    g77 = spu_shufb(g27, g26, g22);
    g75 = spu_shufb(g30, g32, g22);
    g50 = spu_ls_const128(ctx, 0xFF60, 0x18191A1Bu, 0x1C1D1E1Fu, 0x00010203u, 0x04050607u);
    g56 = spu_selb(g74, g75, g20);
    g44 = spu_ls_read128_kcx(ctx, g3._u32[0] + 0xC0);
    g73 = spu_selb(g76, g77, g20);
    g43 = spu_ls_read128(ctx, g3._u32[0] + 0xD0);
    g63 = spu_fm_r(g56, g56);
    g42 = spu_ls_read128(ctx, g3._u32[0] + 0xE0);
    g69 = spu_fm_r(g73, g73);
    g41 = spu_ls_read128(ctx, g3._u32[0] + 0xF0);
    g21 = spu_ls_const128(ctx, 0xFF30, 0x08090A0Bu, 0x18191A1Bu, 0x08090A0Bu, 0x18191A1Bu);
    g67 = spu_shufb(g77, g76, g50);
    g62 = spu_shufb(g75, g74, g50);
    g48 = spu_ls_read128(ctx, g3._u32[0] + 0x80);
    g47 = spu_ls_read128(ctx, g3._u32[0] + 0x90);
    g46 = spu_ls_read128(ctx, g3._u32[0] + 0xA0);
    g11 = spu_fma_r(g67, g67, g69);
    g45 = spu_ls_read128(ctx, g3._u32[0] + 0xB0);
    g12 = spu_fma_r(g62, g62, g63);
    g65 = spu_shufb(g44, g43, g31);
    g69 = spu_ai(g17, 16);
    g64 = spu_shufb(g42, g41, g22);
    g67 = spu_ai(g17, 32);
    g60 = spu_shufb(g24, g25, g21);
    g63 = spu_ai(g17, 80);
    g61 = spu_shufb(g27, g26, g21);
    g62 = spu_ai(g17, 96);
    g58 = spu_shufb(g28, g29, g21);
    g57 = spu_selb(g65, g64, g20);
    g59 = spu_shufb(g30, g32, g21);
    g71 = spu_shufb(g48, g47, g31);
    g54 = spu_selb(g60, g61, g20);
    g10 = spu_fm_r(g57, g57);
    g70 = spu_shufb(g46, g45, g22);
    g53 = spu_selb(g58, g59, g20);
    g52 = spu_shufb(g64, g65, g50);
    g61 = spu_ai(g17, 112);
    g40 = spu_ls_read128(ctx, g3._u32[0] + 0x170);
    g2 = spu_fma(g53, g53, g12);
    g39 = spu_ls_read128(ctx, g3._u32[0] + 0x100);
    g72 = spu_selb(g71, g70, g20);
    g38 = spu_ls_read128(ctx, g3._u32[0] + 0x110);
    g65 = spu_ai(g17, 48);
    g37 = spu_ls_read128(ctx, g3._u32[0] + 0x120);
    g9 = spu_fm_r(g72, g72);
    g36 = spu_ls_read128(ctx, g3._u32[0] + 0x130);
    g8 = spu_fma_r(g52, g52, g10);
    g35 = spu_ls_read128(ctx, g3._u32[0] + 0x140);
    g64 = spu_ai(g17, 64);
    g34 = spu_ls_read128(ctx, g3._u32[0] + 0x150);
    g60 = spu_ai(g17, 384);
    g33 = spu_ls_read128(ctx, g3._u32[0] + 0x160);
    g3 = spu_fma(g54, g54, g11);
    g59 = spu_ai(g17, 400);
    g51 = spu_ls_const128(ctx, 0xFE60, 0x00010203u, 0x00010203u, 0x00010203u, 0x10111213u);
    g58 = spu_ai(g17, 416);
    g55 = spu_shufb(g70, g71, g50);
    g10 = spu_ori(g8, 0);
    g7 = spu_shufb(g46, g45, g21);
    g57 = spu_ai(g17, 432);
    g15 = spu_shufb(g48, g47, g21);
    g54 = spu_ai(g17, 464);
    g14 = spu_shufb(g44, g43, g21);
    g49 = spu_fma_r(g55, g55, g9);
    g13 = spu_shufb(g42, g41, g21);
    g68 = spu_orbi(g51, 4);
    g6 = spu_frsqest(g3);
    g9 = spu_selb(g15, g7, g20);
    g5 = spu_frsqest(g2);
    g66 = spu_orbi(g51, 8);
    g56 = spu_orbi(g51, 12);
    g16 = spu_fi(g3, g6);
    g18 = spu_fi(g2, g5);
    g7 = spu_selb(g14, g13, g20);
    g55 = spu_ai(g17, 448);
    g53 = spu_ai(g17, 480);
    g52 = spu_ai(g17, 496);
fl_0000BD54: ;
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
    if (g78._u32[0] != 0) goto fl_0000BD54;
    { ctx->gpr[2] = g2; ctx->gpr[3] = g3; ctx->gpr[4] = g4; ctx->gpr[5] = g5; ctx->gpr[6] = g6; ctx->gpr[7] = g7; ctx->gpr[8] = g8; ctx->gpr[9] = g9; ctx->gpr[10] = g10; ctx->gpr[11] = g11; ctx->gpr[12] = g12; ctx->gpr[13] = g13; ctx->gpr[14] = g14; ctx->gpr[15] = g15; ctx->gpr[16] = g16; ctx->gpr[17] = g17; ctx->gpr[18] = g18; ctx->gpr[19] = g19; ctx->gpr[20] = g20; ctx->gpr[21] = g21; ctx->gpr[22] = g22; ctx->gpr[23] = g23; ctx->gpr[24] = g24; ctx->gpr[25] = g25; ctx->gpr[26] = g26; ctx->gpr[27] = g27; ctx->gpr[28] = g28; ctx->gpr[29] = g29; ctx->gpr[30] = g30; ctx->gpr[31] = g31; ctx->gpr[32] = g32; ctx->gpr[33] = g33; ctx->gpr[34] = g34; ctx->gpr[35] = g35; ctx->gpr[36] = g36; ctx->gpr[37] = g37; ctx->gpr[38] = g38; ctx->gpr[39] = g39; ctx->gpr[40] = g40; ctx->gpr[41] = g41; ctx->gpr[42] = g42; ctx->gpr[43] = g43; ctx->gpr[44] = g44; ctx->gpr[45] = g45; ctx->gpr[46] = g46; ctx->gpr[47] = g47; ctx->gpr[48] = g48; ctx->gpr[49] = g49; ctx->gpr[50] = g50; ctx->gpr[51] = g51; ctx->gpr[52] = g52; ctx->gpr[53] = g53; ctx->gpr[54] = g54; ctx->gpr[55] = g55; ctx->gpr[56] = g56; ctx->gpr[57] = g57; ctx->gpr[58] = g58; ctx->gpr[59] = g59; ctx->gpr[60] = g60; ctx->gpr[61] = g61; ctx->gpr[62] = g62; ctx->gpr[63] = g63; ctx->gpr[64] = g64; ctx->gpr[65] = g65; ctx->gpr[66] = g66; ctx->gpr[67] = g67; ctx->gpr[68] = g68; ctx->gpr[69] = g69; ctx->gpr[70] = g70; ctx->gpr[71] = g71; ctx->gpr[72] = g72; ctx->gpr[73] = g73; ctx->gpr[74] = g74; ctx->gpr[75] = g75; ctx->gpr[76] = g76; ctx->gpr[77] = g77; ctx->gpr[78] = g78; ctx->gpr[79] = g79; return 1; }
}

static int fl_0000BD54(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g2 = ctx->gpr[2], g3 = ctx->gpr[3], g4 = ctx->gpr[4], g5 = ctx->gpr[5], g6 = ctx->gpr[6], g7 = ctx->gpr[7], g8 = ctx->gpr[8], g9 = ctx->gpr[9], g10 = ctx->gpr[10], g11 = ctx->gpr[11], g12 = ctx->gpr[12], g13 = ctx->gpr[13], g14 = ctx->gpr[14], g15 = ctx->gpr[15], g16 = ctx->gpr[16], g17 = ctx->gpr[17], g18 = ctx->gpr[18], g19 = ctx->gpr[19], g20 = ctx->gpr[20], g21 = ctx->gpr[21], g22 = ctx->gpr[22], g23 = ctx->gpr[23], g24 = ctx->gpr[24], g25 = ctx->gpr[25], g26 = ctx->gpr[26], g27 = ctx->gpr[27], g28 = ctx->gpr[28], g29 = ctx->gpr[29], g30 = ctx->gpr[30], g31 = ctx->gpr[31], g32 = ctx->gpr[32], g33 = ctx->gpr[33], g34 = ctx->gpr[34], g35 = ctx->gpr[35], g36 = ctx->gpr[36], g37 = ctx->gpr[37], g38 = ctx->gpr[38], g39 = ctx->gpr[39], g40 = ctx->gpr[40], g41 = ctx->gpr[41], g42 = ctx->gpr[42], g43 = ctx->gpr[43], g44 = ctx->gpr[44], g45 = ctx->gpr[45], g46 = ctx->gpr[46], g47 = ctx->gpr[47], g48 = ctx->gpr[48], g49 = ctx->gpr[49], g50 = ctx->gpr[50], g51 = ctx->gpr[51], g52 = ctx->gpr[52], g53 = ctx->gpr[53], g54 = ctx->gpr[54], g55 = ctx->gpr[55], g56 = ctx->gpr[56], g57 = ctx->gpr[57], g58 = ctx->gpr[58], g59 = ctx->gpr[59], g60 = ctx->gpr[60], g61 = ctx->gpr[61], g62 = ctx->gpr[62], g63 = ctx->gpr[63], g64 = ctx->gpr[64], g65 = ctx->gpr[65], g66 = ctx->gpr[66], g67 = ctx->gpr[67], g68 = ctx->gpr[68], g69 = ctx->gpr[69], g70 = ctx->gpr[70], g71 = ctx->gpr[71], g72 = ctx->gpr[72], g73 = ctx->gpr[73], g74 = ctx->gpr[74], g75 = ctx->gpr[75], g76 = ctx->gpr[76], g77 = ctx->gpr[77], g78 = ctx->gpr[78], g79 = ctx->gpr[79];
fl_0000BD54: ;
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
    if (g78._u32[0] != 0) goto fl_0000BD54;
    { ctx->gpr[2] = g2; ctx->gpr[3] = g3; ctx->gpr[4] = g4; ctx->gpr[5] = g5; ctx->gpr[6] = g6; ctx->gpr[7] = g7; ctx->gpr[8] = g8; ctx->gpr[9] = g9; ctx->gpr[10] = g10; ctx->gpr[11] = g11; ctx->gpr[12] = g12; ctx->gpr[13] = g13; ctx->gpr[14] = g14; ctx->gpr[15] = g15; ctx->gpr[16] = g16; ctx->gpr[18] = g18; ctx->gpr[19] = g19; ctx->gpr[24] = g24; ctx->gpr[25] = g25; ctx->gpr[26] = g26; ctx->gpr[27] = g27; ctx->gpr[28] = g28; ctx->gpr[29] = g29; ctx->gpr[30] = g30; ctx->gpr[32] = g32; ctx->gpr[33] = g33; ctx->gpr[34] = g34; ctx->gpr[35] = g35; ctx->gpr[36] = g36; ctx->gpr[37] = g37; ctx->gpr[38] = g38; ctx->gpr[39] = g39; ctx->gpr[40] = g40; ctx->gpr[41] = g41; ctx->gpr[42] = g42; ctx->gpr[43] = g43; ctx->gpr[44] = g44; ctx->gpr[45] = g45; ctx->gpr[46] = g46; ctx->gpr[47] = g47; ctx->gpr[48] = g48; ctx->gpr[49] = g49; ctx->gpr[70] = g70; ctx->gpr[71] = g71; ctx->gpr[72] = g72; ctx->gpr[73] = g73; ctx->gpr[74] = g74; ctx->gpr[75] = g75; ctx->gpr[76] = g76; ctx->gpr[77] = g77; ctx->gpr[78] = g78; ctx->gpr[79] = g79; return 1; }
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

static int fl_0000BFB8(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g1 = ctx->gpr[1], g2 = ctx->gpr[2], g3 = ctx->gpr[3], g4 = ctx->gpr[4], g5 = ctx->gpr[5], g6 = ctx->gpr[6], g7 = ctx->gpr[7], g8 = ctx->gpr[8], g9 = ctx->gpr[9], g10 = ctx->gpr[10], g11 = ctx->gpr[11], g12 = ctx->gpr[12], g13 = ctx->gpr[13], g14 = ctx->gpr[14], g15 = ctx->gpr[15], g16 = ctx->gpr[16], g17 = ctx->gpr[17], g18 = ctx->gpr[18], g19 = ctx->gpr[19], g20 = ctx->gpr[20], g21 = ctx->gpr[21], g22 = ctx->gpr[22], g23 = ctx->gpr[23], g24 = ctx->gpr[24], g25 = ctx->gpr[25], g26 = ctx->gpr[26], g27 = ctx->gpr[27], g28 = ctx->gpr[28], g29 = ctx->gpr[29], g30 = ctx->gpr[30], g31 = ctx->gpr[31], g32 = ctx->gpr[32], g33 = ctx->gpr[33], g34 = ctx->gpr[34], g35 = ctx->gpr[35], g36 = ctx->gpr[36], g37 = ctx->gpr[37], g38 = ctx->gpr[38], g39 = ctx->gpr[39], g40 = ctx->gpr[40], g41 = ctx->gpr[41], g42 = ctx->gpr[42], g43 = ctx->gpr[43], g44 = ctx->gpr[44], g45 = ctx->gpr[45], g46 = ctx->gpr[46], g47 = ctx->gpr[47], g48 = ctx->gpr[48], g49 = ctx->gpr[49], g50 = ctx->gpr[50], g51 = ctx->gpr[51], g52 = ctx->gpr[52], g53 = ctx->gpr[53], g54 = ctx->gpr[54], g55 = ctx->gpr[55], g56 = ctx->gpr[56], g57 = ctx->gpr[57], g58 = ctx->gpr[58], g59 = ctx->gpr[59], g60 = ctx->gpr[60], g61 = ctx->gpr[61], g62 = ctx->gpr[62], g63 = ctx->gpr[63], g64 = ctx->gpr[64], g65 = ctx->gpr[65], g66 = ctx->gpr[66], g67 = ctx->gpr[67], g68 = ctx->gpr[68], g69 = ctx->gpr[69], g70 = ctx->gpr[70], g71 = ctx->gpr[71], g72 = ctx->gpr[72], g73 = ctx->gpr[73], g74 = ctx->gpr[74], g75 = ctx->gpr[75], g76 = ctx->gpr[76], g77 = ctx->gpr[77], g78 = ctx->gpr[78], g79 = ctx->gpr[79];
    g52 = spu_a(g7, g8);
    g40 = spu_ls_read128(ctx, g3._u32[0] + 0x50);
    g48 = spu_il(0);
    g30 = spu_cwd(g1, 8);
    g43 = spu_ori(g11, 0);
    g21 = spu_cwd(g1, 0);
    g44 = spu_ori(g13, 0);
    g25 = spu_cwd(g1, 4);
    g63 = spu_a(g5, g6);
    g78 = spu_ls_read128(ctx, g3._u32[0] + 0x0);
    g76 = spu_ila(0xFFFF);
    g27 = spu_cwd(g1, 12);
    g42 = spu_ori(g5, 0);
    g39 = spu_shufb(g52, g40, g30);
    g40 = spu_ori(g10, 0);
    g73 = spu_chd(g1, 10);
    g45 = spu_il(0);
    g67 = spu_chd(g1, 8);
    g38 = spu_ori(g3, 0);
    g61 = spu_cbd(g1, 7);
    g41 = spu_shufb(g48, g39, g21);
    g34 = spu_cbd(g1, 6);
    g39 = spu_ori(g12, 0);
    g35 = spu_cbd(g1, 5);
    g36 = spu_cbd(g1, 4);
    g17 = spu_shufb(g48, g41, g25);
    g41 = spu_ori(g3, 0);
    g23 = spu_ls_read128(ctx, g3._u32[0] + 0x170);
    g69 = spu_ls_read128(ctx, g3._u32[0] + 0x190);
    g37 = spu_cbd(g1, 3);
    spu_ls_write128(ctx, g3._u32[0] + 0x50, g17);
    g58 = spu_ls_read128(ctx, g4._u32[0] + 0x0);
    g62 = spu_ls_read128(ctx, g3._u32[0] + 0x60);
    g31 = spu_chd(g1, 0);
    g64 = spu_shufb(g5, g23, g21);
    g29 = spu_cbd(g1, 2);
    g79 = spu_rotqbyi(g58, 12);
    g70 = spu_rotqbyi(g58, 8);
    g65 = spu_rotqbyi(g58, 6);
    g59 = spu_rotqbyi(g58, 4);
    g72 = spu_shufb(g79, g78, g27);
    g56 = spu_rotqbyi(g58, 3);
    g53 = spu_rotqbyi(g58, 2);
    g74 = spu_and(g65, g76);
    g50 = spu_rotqbyi(g58, 1);
    g66 = spu_shufb(g70, g72, g73);
    g47 = spu_rotqbyi(g58, 15);
    g71 = spu_ai(g74, 7);
    g55 = spu_ls_read128(ctx, g3._u32[0] + 0x150);
    g24 = spu_shufb(g70, g69, g31);
    g60 = spu_shufb(g65, g66, g67);
    g68 = spu_andi(g71, -8);
    g6 = spu_shufb(g63, g64, g25);
    g46 = spu_rotqbyi(g58, 14);
    g54 = spu_shufb(g48, g62, g21);
    g31 = spu_shli(g68, 4);
    g57 = spu_shufb(g59, g60, g61);
    g20 = spu_cbd(g1, 1);
    g51 = spu_ls_read128(ctx, g3._u32[0] + 0x180);
    g33 = spu_rotqbyi(g24, 15);
    g10 = spu_shufb(g56, g57, g34);
    g23 = spu_rotqbyi(g58, 13);
    g7 = spu_shufb(g7, g6, g30);
    g32 = spu_andi(g33, 127);
    g28 = spu_shufb(g9, g55, g21);
    g11 = spu_shufb(g53, g10, g35);
    g16 = spu_shufb(g48, g54, g25);
    g5 = spu_cbd(g1, 0);
    spu_ls_write128(ctx, g3._u32[0] + 0x160, g45);
    g3 = spu_andi(g23, 15);
    g13 = spu_shufb(g50, g11, g36);
    g45 = spu_il(0);
    g8 = spu_shufb(g52, g7, g27);
    g6 = spu_ai(g3, 1);
    g26 = spu_shufb(g14, g51, g21);
    g2 = spu_shufb(g32, g24, g29);
    g22 = spu_shufb(g58, g13, g37);
    g9 = spu_shufb(g9, g28, g25);
    g19 = spu_shufb(g48, g16, g30);
    g15 = spu_shufb(g48, g17, g27);
    g18 = spu_shufb(g47, g22, g29);
    spu_ls_write128(ctx, g38._u32[0] + 0x170, g8);
    spu_ls_write128(ctx, g38._u32[0] + 0x180, g26);
    spu_ls_write128(ctx, g38._u32[0] + 0x190, g2);
    g12 = spu_shufb(g46, g18, g20);
    spu_ls_write128(ctx, g38._u32[0] + 0x150, g9);
    spu_ls_write128(ctx, g38._u32[0] + 0x60, g19);
    spu_ls_write128(ctx, g38._u32[0] + 0x50, g15);
    g4 = spu_shufb(g23, g12, g5);
    spu_ls_write128(ctx, g38._u32[0] + 0x0, g4);
    goto fl_0000C140;
fl_0000C128: ;
    g18 = spu_ls_read128(ctx, g41._u32[0] + g4._u32[0]);
    g20 = spu_cwd(g41, 12);
    g14 = spu_shufb(g42, g18, g20);
    g42 = spu_a(g42, g31);
    spu_ls_write128(ctx, g41._u32[0] + g4._u32[0], g14);
fl_0000C140: ;
    g15 = spu_ceq(g45, g6);
    g4 = spu_il(12);
    g45 = spu_ai(g45, 1);
    g41 = spu_ai(g41, 4);
    if (g15._u32[0] == 0) goto fl_0000C128;
    if (g40._u32[0] == 0) goto fl_0000C19C;
    g17 = spu_ls_read128(ctx, g40._u32[0] + 0x0);
    spu_ls_write128(ctx, g38._u32[0] + 0xD0, g17);
    g48 = spu_ls_read128(ctx, g40._u32[0] + 0x10);
    spu_ls_write128(ctx, g38._u32[0] + 0xE0, g48);
    g29 = spu_ls_read128(ctx, g40._u32[0] + 0x20);
    spu_ls_write128(ctx, g38._u32[0] + 0xF0, g29);
    g22 = spu_ls_read128(ctx, g40._u32[0] + 0x30);
    spu_ls_write128(ctx, g38._u32[0] + 0x100, g22);
    g47 = spu_ls_read128(ctx, g40._u32[0] + 0x40);
    spu_ls_write128(ctx, g38._u32[0] + 0x110, g47);
    g9 = spu_ls_read128(ctx, g40._u32[0] + 0x50);
    spu_ls_write128(ctx, g38._u32[0] + 0x120, g9);
    g19 = spu_ls_read128(ctx, g40._u32[0] + 0x60);
    spu_ls_write128(ctx, g38._u32[0] + 0x130, g19);
    g42 = spu_ls_read128(ctx, g40._u32[0] + 0x70);
    spu_ls_write128(ctx, g38._u32[0] + 0x140, g42);
fl_0000C19C: ;
    if (g43._u32[0] == 0) goto fl_0000C1B8;
    g30 = spu_ls_read128(ctx, g43._u32[0] + 0x0);
    spu_ls_write128(ctx, g38._u32[0] + 0xA0, g30);
    g16 = spu_ls_read128(ctx, g43._u32[0] + 0x10);
    spu_ls_write128(ctx, g38._u32[0] + 0xB0, g16);
    g27 = spu_ls_read128(ctx, g43._u32[0] + 0x20);
    spu_ls_write128(ctx, g38._u32[0] + 0xC0, g27);
fl_0000C1B8: ;
    if (g39._u32[0] != 0) goto fl_0000C21C;
    g7 = spu_ls_read128(ctx, g38._u32[0] + 0x70);
    g52 = spu_ls_read128(ctx, g38._u32[0] + 0x80);
    g21 = spu_cwd(g1, 0);
    g26 = spu_cwd(g1, 8);
    g8 = spu_cwd(g1, 4);
    g32 = spu_shufb(g39, g7, g21);
    g36 = spu_shufb(g39, g52, g21);
    g24 = spu_ls_read128(ctx, g38._u32[0] + 0x60);
    g51 = spu_ls_read128(ctx, g38._u32[0] + 0x90);
    g2 = spu_shufb(g39, g32, g26);
    g37 = spu_shufb(g39, g36, g8);
    g49 = spu_cwd(g1, 12);
    g13 = spu_shufb(g39, g2, g8);
    g25 = spu_shufb(g39, g37, g26);
    g50 = spu_shufb(g39, g24, g49);
    g11 = spu_shufb(g39, g51, g21);
    g28 = spu_shufb(g39, g13, g49);
    g43 = spu_shufb(g39, g25, g49);
    spu_ls_write128(ctx, g38._u32[0] + 0x60, g50);
    spu_ls_write128(ctx, g38._u32[0] + 0x90, g11);
    spu_ls_write128(ctx, g38._u32[0] + 0x70, g28);
    spu_ls_write128(ctx, g38._u32[0] + 0x80, g43);
    goto fl_0000C2EC;
fl_0000C21C: ;
    g14 = spu_ls_read128(ctx, g39._u32[0] + 0x0);
    g45 = spu_ai(g39, 4);
    g41 = spu_ls_read128(ctx, g38._u32[0] + 0x60);
    g79 = spu_il(0);
    g34 = spu_cwd(g1, 12);
    g78 = spu_ai(g39, 8);
    g5 = spu_ls_read128(ctx, g38._u32[0] + 0x70);
    g31 = spu_ai(g39, 20);
    g35 = spu_cwd(g1, 0);
    g67 = spu_ai(g39, 24);
    g60 = spu_cwd(g1, 8);
    g62 = spu_ai(g39, 28);
    g40 = spu_rotqby(g14, g39);
    g33 = spu_ai(g39, 36);
    g65 = spu_cwd(g1, 4);
    g68 = spu_ls_read128(ctx, g38._u32[0] + 0x80);
    g53 = spu_ls_read128(ctx, g38._u32[0] + 0x90);
    g23 = spu_shufb(g40, g41, g34);
    spu_ls_write128(ctx, g38._u32[0] + 0x60, g23);
    g12 = spu_ls_read128(ctx, g45._u32[0] + 0x0);
    g46 = spu_rotqby(g12, g45);
    g4 = spu_shufb(g46, g5, g35);
    g76 = spu_shufb(g79, g4, g60);
    spu_ls_write128(ctx, g38._u32[0] + 0x70, g76);
    g77 = spu_ls_read128(ctx, g78._u32[0] + 0x0);
    g75 = spu_rotqby(g77, g78);
    g73 = spu_shufb(g75, g76, g65);
    spu_ls_write128(ctx, g38._u32[0] + 0x70, g73);
    g74 = spu_ls_read128(ctx, g39._u32[0] + 0x10);
    g72 = spu_rotqby(g74, g39);
    g71 = spu_shufb(g72, g73, g34);
    spu_ls_write128(ctx, g38._u32[0] + 0x70, g71);
    g70 = spu_ls_read128(ctx, g31._u32[0] + 0x0);
    g69 = spu_rotqby(g70, g31);
    g64 = spu_shufb(g69, g68, g35);
    spu_ls_write128(ctx, g38._u32[0] + 0x80, g64);
    g66 = spu_ls_read128(ctx, g67._u32[0] + 0x0);
    g63 = spu_rotqby(g66, g67);
    g59 = spu_shufb(g63, g64, g65);
    spu_ls_write128(ctx, g38._u32[0] + 0x80, g59);
    g61 = spu_ls_read128(ctx, g62._u32[0] + 0x0);
    g58 = spu_rotqby(g61, g62);
    g57 = spu_shufb(g58, g59, g60);
    spu_ls_write128(ctx, g38._u32[0] + 0x80, g57);
    g6 = spu_ls_read128(ctx, g39._u32[0] + 0x20);
    g56 = spu_rotqby(g6, g39);
    g55 = spu_shufb(g56, g57, g34);
    spu_ls_write128(ctx, g38._u32[0] + 0x80, g55);
    g54 = spu_ls_read128(ctx, g33._u32[0] + 0x0);
    g10 = spu_rotqby(g54, g33);
    g3 = spu_shufb(g10, g53, g35);
    spu_ls_write128(ctx, g38._u32[0] + 0x90, g3);
fl_0000C2EC: ;
    if (g44._u32[0] != 0) goto fl_0000C308;
    g20 = spu_ls_read128(ctx, g38._u32[0] + 0x180);
    g15 = spu_cwd(g1, 4);
    g18 = spu_cwd(g1, 8);
    g39 = spu_shufb(g44, g20, g15);
    g2 = spu_shufb(g44, g39, g18);
    goto fl_0000C334;
fl_0000C308: ;
    g47 = spu_ai(g44, 4);
    g17 = spu_ls_read128(ctx, g44._u32[0] + 0x0);
    g22 = spu_ls_read128(ctx, g38._u32[0] + 0x180);
    g48 = spu_cwd(g1, 4);
    g19 = spu_cwd(g1, 8);
    g29 = spu_rotqby(g17, g44);
    g42 = spu_shufb(g29, g22, g48);
    spu_ls_write128(ctx, g38._u32[0] + 0x180, g42);
    g9 = spu_ls_read128(ctx, g47._u32[0] + 0x0);
    g44 = spu_rotqby(g9, g47);
    g2 = spu_shufb(g44, g42, g19);
fl_0000C334: ;
    spu_ls_write128(ctx, g38._u32[0] + 0x180, g2);
    { ctx->gpr[2] = g2; ctx->gpr[3] = g3; ctx->gpr[4] = g4; ctx->gpr[5] = g5; ctx->gpr[6] = g6; ctx->gpr[7] = g7; ctx->gpr[8] = g8; ctx->gpr[9] = g9; ctx->gpr[10] = g10; ctx->gpr[11] = g11; ctx->gpr[12] = g12; ctx->gpr[13] = g13; ctx->gpr[14] = g14; ctx->gpr[15] = g15; ctx->gpr[16] = g16; ctx->gpr[17] = g17; ctx->gpr[18] = g18; ctx->gpr[19] = g19; ctx->gpr[20] = g20; ctx->gpr[21] = g21; ctx->gpr[22] = g22; ctx->gpr[23] = g23; ctx->gpr[24] = g24; ctx->gpr[25] = g25; ctx->gpr[26] = g26; ctx->gpr[27] = g27; ctx->gpr[28] = g28; ctx->gpr[29] = g29; ctx->gpr[30] = g30; ctx->gpr[31] = g31; ctx->gpr[32] = g32; ctx->gpr[33] = g33; ctx->gpr[34] = g34; ctx->gpr[35] = g35; ctx->gpr[36] = g36; ctx->gpr[37] = g37; ctx->gpr[38] = g38; ctx->gpr[39] = g39; ctx->gpr[40] = g40; ctx->gpr[41] = g41; ctx->gpr[42] = g42; ctx->gpr[43] = g43; ctx->gpr[44] = g44; ctx->gpr[45] = g45; ctx->gpr[46] = g46; ctx->gpr[47] = g47; ctx->gpr[48] = g48; ctx->gpr[49] = g49; ctx->gpr[50] = g50; ctx->gpr[51] = g51; ctx->gpr[52] = g52; ctx->gpr[53] = g53; ctx->gpr[54] = g54; ctx->gpr[55] = g55; ctx->gpr[56] = g56; ctx->gpr[57] = g57; ctx->gpr[58] = g58; ctx->gpr[59] = g59; ctx->gpr[60] = g60; ctx->gpr[61] = g61; ctx->gpr[62] = g62; ctx->gpr[63] = g63; ctx->gpr[64] = g64; ctx->gpr[65] = g65; ctx->gpr[66] = g66; ctx->gpr[67] = g67; ctx->gpr[68] = g68; ctx->gpr[69] = g69; ctx->gpr[70] = g70; ctx->gpr[71] = g71; ctx->gpr[72] = g72; ctx->gpr[73] = g73; ctx->gpr[74] = g74; ctx->gpr[75] = g75; ctx->gpr[76] = g76; ctx->gpr[77] = g77; ctx->gpr[78] = g78; ctx->gpr[79] = g79; return 1; }
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

static int fl_0000D4DC(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g1 = ctx->gpr[1], g2 = ctx->gpr[2], g3 = ctx->gpr[3], g4 = ctx->gpr[4], g5 = ctx->gpr[5], g6 = ctx->gpr[6], g7 = ctx->gpr[7], g8 = ctx->gpr[8], g9 = ctx->gpr[9], g10 = ctx->gpr[10], g11 = ctx->gpr[11], g12 = ctx->gpr[12], g13 = ctx->gpr[13], g14 = ctx->gpr[14], g15 = ctx->gpr[15], g16 = ctx->gpr[16], g17 = ctx->gpr[17], g18 = ctx->gpr[18], g19 = ctx->gpr[19], g20 = ctx->gpr[20], g21 = ctx->gpr[21], g22 = ctx->gpr[22], g23 = ctx->gpr[23], g24 = ctx->gpr[24], g25 = ctx->gpr[25], g26 = ctx->gpr[26], g27 = ctx->gpr[27], g28 = ctx->gpr[28], g29 = ctx->gpr[29], g30 = ctx->gpr[30], g31 = ctx->gpr[31], g32 = ctx->gpr[32], g33 = ctx->gpr[33], g34 = ctx->gpr[34], g35 = ctx->gpr[35], g36 = ctx->gpr[36], g37 = ctx->gpr[37], g38 = ctx->gpr[38], g39 = ctx->gpr[39], g40 = ctx->gpr[40], g41 = ctx->gpr[41], g42 = ctx->gpr[42], g43 = ctx->gpr[43], g44 = ctx->gpr[44], g45 = ctx->gpr[45], g46 = ctx->gpr[46], g47 = ctx->gpr[47], g48 = ctx->gpr[48], g49 = ctx->gpr[49], g50 = ctx->gpr[50], g51 = ctx->gpr[51], g52 = ctx->gpr[52], g53 = ctx->gpr[53], g54 = ctx->gpr[54], g55 = ctx->gpr[55], g56 = ctx->gpr[56], g57 = ctx->gpr[57], g58 = ctx->gpr[58], g59 = ctx->gpr[59], g60 = ctx->gpr[60], g61 = ctx->gpr[61], g62 = ctx->gpr[62], g63 = ctx->gpr[63], g64 = ctx->gpr[64], g65 = ctx->gpr[65], g66 = ctx->gpr[66], g67 = ctx->gpr[67], g68 = ctx->gpr[68], g69 = ctx->gpr[69], g70 = ctx->gpr[70], g71 = ctx->gpr[71], g72 = ctx->gpr[72], g73 = ctx->gpr[73], g74 = ctx->gpr[74], g75 = ctx->gpr[75], g76 = ctx->gpr[76], g77 = ctx->gpr[77], g78 = ctx->gpr[78], g79 = ctx->gpr[79], g80 = ctx->gpr[80], g126 = ctx->gpr[126];
fl_0000D4DC: ;
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
    if (g3._u32[0] != 0) goto fl_0000D4DC;
    g1 = spu_ai(g1, 64);
    g80 = spu_ls_read128(ctx, g1._u32[0] + -0x10);
    g126 = spu_ls_read128(ctx, g1._u32[0] + -0x20);
    { ctx->gpr[1] = g1; ctx->gpr[2] = g2; ctx->gpr[3] = g3; ctx->gpr[4] = g4; ctx->gpr[5] = g5; ctx->gpr[6] = g6; ctx->gpr[7] = g7; ctx->gpr[8] = g8; ctx->gpr[9] = g9; ctx->gpr[10] = g10; ctx->gpr[11] = g11; ctx->gpr[12] = g12; ctx->gpr[13] = g13; ctx->gpr[14] = g14; ctx->gpr[15] = g15; ctx->gpr[16] = g16; ctx->gpr[17] = g17; ctx->gpr[18] = g18; ctx->gpr[19] = g19; ctx->gpr[20] = g20; ctx->gpr[21] = g21; ctx->gpr[22] = g22; ctx->gpr[23] = g23; ctx->gpr[24] = g24; ctx->gpr[26] = g26; ctx->gpr[31] = g31; ctx->gpr[36] = g36; ctx->gpr[65] = g65; ctx->gpr[80] = g80; ctx->gpr[126] = g126; return 1; }
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

static int fl_0000F620(spu_context* ctx)
{
    if (!gowa_funcao_local_on()) return 0;
    u128 g2 = ctx->gpr[2], g3 = ctx->gpr[3], g4 = ctx->gpr[4], g5 = ctx->gpr[5], g6 = ctx->gpr[6], g7 = ctx->gpr[7], g8 = ctx->gpr[8], g9 = ctx->gpr[9], g10 = ctx->gpr[10], g11 = ctx->gpr[11], g12 = ctx->gpr[12], g13 = ctx->gpr[13], g14 = ctx->gpr[14], g15 = ctx->gpr[15], g16 = ctx->gpr[16], g17 = ctx->gpr[17], g18 = ctx->gpr[18], g19 = ctx->gpr[19], g20 = ctx->gpr[20], g21 = ctx->gpr[21], g22 = ctx->gpr[22], g23 = ctx->gpr[23], g24 = ctx->gpr[24], g25 = ctx->gpr[25], g26 = ctx->gpr[26], g27 = ctx->gpr[27], g28 = ctx->gpr[28], g29 = ctx->gpr[29], g30 = ctx->gpr[30], g31 = ctx->gpr[31], g32 = ctx->gpr[32], g33 = ctx->gpr[33], g34 = ctx->gpr[34], g35 = ctx->gpr[35], g36 = ctx->gpr[36], g37 = ctx->gpr[37], g38 = ctx->gpr[38], g39 = ctx->gpr[39], g40 = ctx->gpr[40], g41 = ctx->gpr[41], g42 = ctx->gpr[42], g43 = ctx->gpr[43], g44 = ctx->gpr[44], g45 = ctx->gpr[45], g46 = ctx->gpr[46], g47 = ctx->gpr[47], g48 = ctx->gpr[48], g49 = ctx->gpr[49], g50 = ctx->gpr[50], g51 = ctx->gpr[51], g52 = ctx->gpr[52], g53 = ctx->gpr[53], g54 = ctx->gpr[54], g55 = ctx->gpr[55], g56 = ctx->gpr[56], g57 = ctx->gpr[57], g58 = ctx->gpr[58], g59 = ctx->gpr[59], g60 = ctx->gpr[60], g61 = ctx->gpr[61], g62 = ctx->gpr[62], g63 = ctx->gpr[63], g64 = ctx->gpr[64], g65 = ctx->gpr[65], g66 = ctx->gpr[66], g67 = ctx->gpr[67], g68 = ctx->gpr[68], g69 = ctx->gpr[69], g70 = ctx->gpr[70], g72 = ctx->gpr[72], g73 = ctx->gpr[73], g74 = ctx->gpr[74], g75 = ctx->gpr[75], g76 = ctx->gpr[76], g77 = ctx->gpr[77], g78 = ctx->gpr[78], g79 = ctx->gpr[79];
fl_0000F620: ;
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
    if (g37._u32[0] != 0) goto fl_0000F620;
    { ctx->gpr[2] = g2; ctx->gpr[3] = g3; ctx->gpr[4] = g4; ctx->gpr[5] = g5; ctx->gpr[6] = g6; ctx->gpr[7] = g7; ctx->gpr[8] = g8; ctx->gpr[9] = g9; ctx->gpr[10] = g10; ctx->gpr[11] = g11; ctx->gpr[12] = g12; ctx->gpr[13] = g13; ctx->gpr[14] = g14; ctx->gpr[15] = g15; ctx->gpr[16] = g16; ctx->gpr[17] = g17; ctx->gpr[18] = g18; ctx->gpr[19] = g19; ctx->gpr[20] = g20; ctx->gpr[21] = g21; ctx->gpr[22] = g22; ctx->gpr[23] = g23; ctx->gpr[24] = g24; ctx->gpr[25] = g25; ctx->gpr[26] = g26; ctx->gpr[27] = g27; ctx->gpr[28] = g28; ctx->gpr[29] = g29; ctx->gpr[30] = g30; ctx->gpr[31] = g31; ctx->gpr[32] = g32; ctx->gpr[37] = g37; ctx->gpr[38] = g38; ctx->gpr[39] = g39; ctx->gpr[41] = g41; ctx->gpr[43] = g43; ctx->gpr[48] = g48; ctx->gpr[49] = g49; ctx->gpr[50] = g50; ctx->gpr[51] = g51; ctx->gpr[52] = g52; ctx->gpr[53] = g53; ctx->gpr[55] = g55; ctx->gpr[56] = g56; ctx->gpr[57] = g57; ctx->gpr[59] = g59; ctx->gpr[60] = g60; ctx->gpr[61] = g61; ctx->gpr[62] = g62; ctx->gpr[63] = g63; ctx->gpr[64] = g64; ctx->gpr[65] = g65; ctx->gpr[66] = g66; ctx->gpr[67] = g67; ctx->gpr[68] = g68; ctx->gpr[69] = g69; ctx->gpr[70] = g70; ctx->gpr[72] = g72; ctx->gpr[73] = g73; ctx->gpr[74] = g74; ctx->gpr[75] = g75; ctx->gpr[76] = g76; ctx->gpr[77] = g77; ctx->gpr[78] = g78; ctx->gpr[79] = g79; return 1; }
}

#endif
