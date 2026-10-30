/* GOWA: gowajm_0072DF80_spu_func_00005720__b loc_00005720 com registradores locais (gerado por scripts_edge/laco_local.py; não editar).
 * Mesmas operações e ordem do código traduzido: idêntico bit a bit. Faz o laço inteiro e devolve 1 (o
 * gancho salta para loc_00005720_fim, depois do desvio); 0 = nada feito (desligado). */
#ifndef GOWA_LL_00005720_00005720_H
#define GOWA_LL_00005720_00005720_H
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
static int ll_00005720_00005720(spu_context* ctx)
{
    if (!gowa_laco_local_on()) return 0;
    u128 g12 = ctx->gpr[12], g13 = ctx->gpr[13], g14 = ctx->gpr[14], g15 = ctx->gpr[15], g33 = ctx->gpr[33], g34 = ctx->gpr[34], g35 = ctx->gpr[35], g36 = ctx->gpr[36], g37 = ctx->gpr[37], g38 = ctx->gpr[38], g40 = ctx->gpr[40], g41 = ctx->gpr[41], g42 = ctx->gpr[42], g43 = ctx->gpr[43], g44 = ctx->gpr[44], g45 = ctx->gpr[45], g46 = ctx->gpr[46], g47 = ctx->gpr[47], g81 = ctx->gpr[81];
    do {
        g14 = spu_ai(g14, 8);
        g47 = spu_ls_read128(ctx, g12._u32[0] + 0x70);
        g38 = spu_ai(g12, 128);
        g46 = spu_ls_read128(ctx, g12._u32[0] + 0x0);
        g37 = spu_ai(g13, 128);
        g45 = spu_ls_read128(ctx, g12._u32[0] + 0x10);
        g44 = spu_ls_read128(ctx, g12._u32[0] + 0x20);
        g43 = spu_ls_read128(ctx, g12._u32[0] + 0x30);
        g42 = spu_ls_read128(ctx, g12._u32[0] + 0x40);
        g41 = spu_ls_read128(ctx, g12._u32[0] + 0x50);
        g40 = spu_ls_read128(ctx, g12._u32[0] + 0x60);
        g12 = spu_ori(g38, 0);
        spu_ls_write128(ctx, g13._u32[0] + 0x70, g47);
        spu_ls_write128(ctx, g13._u32[0] + 0x0, g46);
        spu_ls_write128(ctx, g13._u32[0] + 0x10, g45);
        spu_ls_write128(ctx, g13._u32[0] + 0x20, g44);
        spu_ls_write128(ctx, g13._u32[0] + 0x30, g43);
        spu_ls_write128(ctx, g13._u32[0] + 0x40, g42);
        spu_ls_write128(ctx, g13._u32[0] + 0x50, g41);
        spu_ls_write128(ctx, g13._u32[0] + 0x60, g40);
        g13 = spu_ori(g37, 0);
        g36 = spu_ls_read128(ctx, g81._u32[0] + 0x0);
        g35 = spu_rotqbyi(g36, 6);
        g34 = spu_and(g15, g35);
        g33 = spu_cgt(g34, g14);
    } while (g33._u32[0] != 0);
    ctx->gpr[14] = g14;
    ctx->gpr[47] = g47;
    ctx->gpr[38] = g38;
    ctx->gpr[46] = g46;
    ctx->gpr[37] = g37;
    ctx->gpr[45] = g45;
    ctx->gpr[44] = g44;
    ctx->gpr[43] = g43;
    ctx->gpr[42] = g42;
    ctx->gpr[41] = g41;
    ctx->gpr[40] = g40;
    ctx->gpr[12] = g12;
    ctx->gpr[13] = g13;
    ctx->gpr[36] = g36;
    ctx->gpr[35] = g35;
    ctx->gpr[34] = g34;
    ctx->gpr[33] = g33;
    return 1;
}
#endif
