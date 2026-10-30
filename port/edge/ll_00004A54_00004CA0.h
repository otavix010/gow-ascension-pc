/* GOWA: gowajm_0072DF80_spu_func_00004A54__b loc_00004CA0 com registradores locais (gerado por scripts_edge/laco_local.py; não editar).
 * Mesmas operações e ordem do código traduzido: idêntico bit a bit. Faz o laço inteiro e devolve 1 (o
 * gancho salta para loc_00004CA0_fim, depois do desvio); 0 = nada feito (desligado). */
#ifndef GOWA_LL_00004A54_00004CA0_H
#define GOWA_LL_00004A54_00004CA0_H
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
static int ll_00004A54_00004CA0(spu_context* ctx)
{
    if (!gowa_laco_local_on()) return 0;
    u128 g10 = ctx->gpr[10], g11 = ctx->gpr[11], g12 = ctx->gpr[12], g13 = ctx->gpr[13], g14 = ctx->gpr[14], g15 = ctx->gpr[15], g16 = ctx->gpr[16], g51 = ctx->gpr[51], g52 = ctx->gpr[52], g53 = ctx->gpr[53], g54 = ctx->gpr[54], g55 = ctx->gpr[55], g56 = ctx->gpr[56], g57 = ctx->gpr[57], g58 = ctx->gpr[58], g59 = ctx->gpr[59], g60 = ctx->gpr[60], g61 = ctx->gpr[61], g62 = ctx->gpr[62], g63 = ctx->gpr[63], g64 = ctx->gpr[64], g65 = ctx->gpr[65], g66 = ctx->gpr[66];
    do {
        g66 = spu_ai(g10, 144);
        g13 = spu_ai(g13, 1);
        g65 = spu_a(g15, g66);
        g63 = spu_ls_read128(ctx, g15._u32[0] + g66._u32[0]);
        g52 = spu_a(g10, g16);
        g51 = spu_cgt(g14, g13);
        g62 = spu_ls_read128(ctx, g65._u32[0] + 0x30);
        g61 = spu_ls_read128(ctx, g65._u32[0] + 0x10);
        g64 = spu_ls_read128(ctx, g65._u32[0] + 0x20);
        g60 = spu_shufb(g61, g62, g12);
        g59 = spu_shufb(g63, g64, g12);
        g57 = spu_shufb(g63, g64, g11);
        g58 = spu_shufb(g61, g62, g11);
        g56 = spu_shufb(g59, g60, g12);
        g55 = spu_shufb(g59, g60, g11);
        g54 = spu_shufb(g57, g58, g11);
        g53 = spu_shufb(g57, g58, g12);
        spu_ls_write128(ctx, g10._u32[0] + g16._u32[0], g56);
        g10 = spu_ai(g10, 64);
        spu_ls_write128(ctx, g52._u32[0] + 0x10, g55);
        spu_ls_write128(ctx, g52._u32[0] + 0x30, g54);
        spu_ls_write128(ctx, g52._u32[0] + 0x20, g53);
    } while (g51._u32[0] != 0);
    ctx->gpr[66] = g66;
    ctx->gpr[13] = g13;
    ctx->gpr[65] = g65;
    ctx->gpr[63] = g63;
    ctx->gpr[52] = g52;
    ctx->gpr[51] = g51;
    ctx->gpr[62] = g62;
    ctx->gpr[61] = g61;
    ctx->gpr[64] = g64;
    ctx->gpr[60] = g60;
    ctx->gpr[59] = g59;
    ctx->gpr[57] = g57;
    ctx->gpr[58] = g58;
    ctx->gpr[56] = g56;
    ctx->gpr[55] = g55;
    ctx->gpr[54] = g54;
    ctx->gpr[53] = g53;
    ctx->gpr[10] = g10;
    return 1;
}
#endif
