/* GOWA (01/11, EDGE nativo, marco E4): função 0xB3D0 do job 0x72DF80 inteira -- grava no buffer de comandos da LS um
 * registro de 60 bytes com 3 comandos do RSX: 0x00080A00 (2 palavras: (r6 << 16) | r4, (r7 << 16) | r5), 0x00080394
 * (2 palavras: r8, r9) e 0x00200A20 (8 palavras de viewport em float: x0 = r6/2 + r4, y0 = r7/2 + r5, (r8 + r9)/2, 0,
 * r6/2, -r7/2, (r9 - r8)/2, 0). As contas usam as mesmas operações de float da SPU (spu_cuflt/fa/fm/fs/xor), então
 * os bits saem iguais. Buffer cheio (cursor + 60 > fim) chama o descarregamento 0xAFF0 por DMA: volta 0 e fica com
 * o traduzido. Grava na pilha o que o prólogo do original grava (r80-r86, r0, r1). GOWA_EDGE_NATIVO=0 desliga.
 * Gancho 'entrada' (edge_nativo.py). */
#ifndef GOWA_FN_0000B3D0_H
#define GOWA_FN_0000B3D0_H
#include <stdlib.h>
#include <string.h>
#include "edge_comum.h"

#ifndef GOWA_EDGE_NATIVO_ON_DEF
#define GOWA_EDGE_NATIVO_ON_DEF
static int gowa_edge_nativo_on(void)
{
    static int on = -1;
    if (on < 0) { const char* e = getenv("GOWA_EDGE_NATIVO"); on = !(e && *e == '0'); }   /* 30/10 (E7): ligado por padrão; =0 desliga */
    return on;
}
#endif

static int fn_0000B3D0_corpo(spu_context* ctx)
{
    if (!gowa_edge_nativo_on()) return 0;
    u128* g = ctx->gpr;
    const uint32_t r3 = g[3]._u32[0] & SPU_LS_MASK, sp = g[1]._u32[0] & SPU_LS_MASK;
    if ((r3 & 15u) || (sp & 15u) || sp < 0xA0u || sp + 0x20u > SPU_LS_SIZE) return 0;
    const uint32_t fim = spu_ls_read32(ctx, r3 + 4u), cur = spu_ls_read32(ctx, r3 + 8u);
    if (cur + 60u > fim || (cur & 3u) || cur + 60u > SPU_LS_SIZE) return 0;   /* cheio: descarrega (traduzido) */
    if (r3 < cur + 60u && cur < r3 + 16u) return 0;
    if (sp - 0x90u < cur + 60u && cur < sp + 0x20u) return 0;               /* registro sobre a pilha */
    static const int regs[] = { 85, 86, 80, 81, 82, 83, 84, 0, 1 };
    static const int desl[] = { -0x60, -0x70, -0x10, -0x20, -0x30, -0x40, -0x50, 0x10, -0x90 };
    gowa_edge_prologo(ctx, sp, regs, desl, 9);
    const u128 r4 = g[4], r5 = g[5], r6 = g[6], r7 = g[7], r8 = g[8], r9 = g[9];
    const u128 meio = spu_ilhu(0x3F00), sinal = spu_ilhu(0x8000);
    const u128 w6 = spu_cuflt(r6, 154), w4 = spu_cuflt(r4, 155), h7 = spu_cuflt(r7, 154), h5 = spu_cuflt(r5, 155);
    const u128 x0 = spu_fa(w6, w4), y0 = spu_fa(h7, h5);
    const u128 z0 = spu_fm(spu_fa(r8, r9), meio), zs = spu_fm(spu_fs(r9, r8), meio), ys = spu_xor(h7, sinal);
    const uint32_t w[15] = {
        0x00080A00u, (r6._u32[0] << 16) | r4._u32[0], (r7._u32[0] << 16) | r5._u32[0],
        0x00080394u, r8._u32[0], r9._u32[0],
        0x00200A20u, x0._u32[0], y0._u32[0], z0._u32[0], 0u, w6._u32[0], ys._u32[0], zs._u32[0], 0u };
    for (int k = 0; k < 15; k++) spu_ls_write32(ctx, cur + 4u * (uint32_t)k, w[k]);
    spu_ls_write32(ctx, r3 + 8u, cur + 60u);
    return 1;
}

static __attribute__((noinline)) int fn_0000B3D0_nativo(spu_context* ctx)
{
    static volatile long n[2];
    const int r = fn_0000B3D0_corpo(ctx);
    gowa_edge_conta("B3D0", n, r);
    return r;
}
#endif
