/* GOWA: gowajm_0072DF80_spu_func_000073E0__b loc_00007670 com registradores locais (gerado por scripts_edge/laco_local.py; não editar).
 * Mesmas operações e ordem do código traduzido: idêntico bit a bit. Faz o laço inteiro e devolve 1 (o
 * gancho salta para loc_00007670_fim, depois do desvio); 0 = nada feito (desligado). */
#ifndef GOWA_LL_000073E0_00007670_H
#define GOWA_LL_000073E0_00007670_H
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
static int ll_000073E0_00007670(spu_context* ctx)
{
    if (!gowa_laco_local_on()) return 0;
    u128 g2 = ctx->gpr[2], g9 = ctx->gpr[9], g10 = ctx->gpr[10], g11 = ctx->gpr[11], g12 = ctx->gpr[12], g13 = ctx->gpr[13], g14 = ctx->gpr[14], g15 = ctx->gpr[15], g16 = ctx->gpr[16], g17 = ctx->gpr[17], g18 = ctx->gpr[18], g19 = ctx->gpr[19], g52 = ctx->gpr[52], g53 = ctx->gpr[53], g54 = ctx->gpr[54], g55 = ctx->gpr[55], g57 = ctx->gpr[57], g58 = ctx->gpr[58], g59 = ctx->gpr[59], g61 = ctx->gpr[61], g62 = ctx->gpr[62], g63 = ctx->gpr[63], g65 = ctx->gpr[65], g66 = ctx->gpr[66], g67 = ctx->gpr[67], g81 = ctx->gpr[81];
    do {
        g67 = spu_ai(g10, 64);
        g66 = spu_ls_read128(ctx, g19._u32[0] + g9._u32[0]);
        g65 = spu_ai(g15, 64);
        g63 = spu_ls_read128(ctx, g18._u32[0] + g9._u32[0]);
        g61 = spu_ai(g14, 64);
        g59 = spu_ls_read128(ctx, g17._u32[0] + g9._u32[0]);
        g57 = spu_ai(g13, 64);
        g55 = spu_ls_read128(ctx, g16._u32[0] + g9._u32[0]);
        g53 = spu_ai(g12, 64);
        spu_ls_write128(ctx, g81._u32[0] + g11._u32[0], g15);
        g52 = spu_cgti(g67, -1);
        g15 = spu_shlqbyi(g65, 0);
        g62 = spu_a(g11, g66);
        g10 = spu_shlqbyi(g67, 0);
        g9 = spu_ori(g67, 0);
        g58 = spu_a(g62, g63);
        spu_ls_write128(ctx, g81._u32[0] + g62._u32[0], g14);
        g54 = spu_a(g58, g59);
        g14 = spu_ori(g61, 0);
        spu_ls_write128(ctx, g81._u32[0] + g58._u32[0], g13);
        g2 = spu_a(g54, g55);
        spu_ls_write128(ctx, g81._u32[0] + g54._u32[0], g12);
        g13 = spu_ori(g57, 0);
        g12 = spu_shlqbyi(g53, 0);
        g11 = spu_ori(g2, 0);
    } while (g52._u32[0] == 0);
    ctx->gpr[67] = g67;
    ctx->gpr[66] = g66;
    ctx->gpr[65] = g65;
    ctx->gpr[63] = g63;
    ctx->gpr[61] = g61;
    ctx->gpr[59] = g59;
    ctx->gpr[57] = g57;
    ctx->gpr[55] = g55;
    ctx->gpr[53] = g53;
    ctx->gpr[52] = g52;
    ctx->gpr[15] = g15;
    ctx->gpr[62] = g62;
    ctx->gpr[10] = g10;
    ctx->gpr[9] = g9;
    ctx->gpr[58] = g58;
    ctx->gpr[54] = g54;
    ctx->gpr[14] = g14;
    ctx->gpr[2] = g2;
    ctx->gpr[13] = g13;
    ctx->gpr[12] = g12;
    ctx->gpr[11] = g11;
    return 1;
}
#endif
