/* GOWA (01/11, EDGE nativo, marco E4): função 0xB2D0 do job 0x72DF80 inteira -- grava um comando do RSX de 12 bytes no
 * buffer de comandos da LS: cabeçalho 0x000808C0 (método 0x08C0, 2 palavras), (r6 << 16) | r4 e (r7 << 16) | r5; o
 * cursor (palavra 2 do quadword em r3) avança 12. Buffer cheio (cursor + 12 > fim) chama o descarregamento 0xAFF0
 * por DMA: volta 0 e fica com o código traduzido, como os endereços fora do comum. Grava na pilha o que o prólogo do
 * original grava (r80-r84, r0, r1). Temporários ficam diferentes (GOWA_CONF_REGS_ABI=1). GOWA_EDGE_NATIVO=0 desliga.
 * Gancho 'entrada' (edge_nativo.py). */
#ifndef GOWA_FN_0000B2D0_H
#define GOWA_FN_0000B2D0_H
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

static int fn_0000B2D0_corpo(spu_context* ctx)
{
    if (!gowa_edge_nativo_on()) return 0;
    u128* g = ctx->gpr;
    const uint32_t r3 = g[3]._u32[0] & SPU_LS_MASK, sp = g[1]._u32[0] & SPU_LS_MASK;
    if ((r3 & 15u) || (sp & 15u) || sp < 0x80u || sp + 0x20u > SPU_LS_SIZE) return 0;
    const uint32_t fim = spu_ls_read32(ctx, r3 + 4u), cur = spu_ls_read32(ctx, r3 + 8u);
    if (cur + 12u > fim || (cur & 3u) || cur + 12u > SPU_LS_SIZE) return 0;   /* cheio: descarrega (traduzido) */
    if (r3 < cur + 12u && cur < r3 + 16u) return 0;
    if (sp - 0x70u < cur + 12u && cur < sp + 0x20u) return 0;               /* registro sobre a pilha */
    static const int regs[] = { 83, 80, 81, 82, 84, 0, 1 };
    static const int desl[] = { -0x40, -0x10, -0x20, -0x30, -0x50, 0x10, -0x70 };
    gowa_edge_prologo(ctx, sp, regs, desl, 7);
    spu_ls_write32(ctx, cur, 0x000808C0u);
    spu_ls_write32(ctx, cur + 4u, (g[6]._u32[0] << 16) | g[4]._u32[0]);
    spu_ls_write32(ctx, cur + 8u, (g[7]._u32[0] << 16) | g[5]._u32[0]);
    spu_ls_write32(ctx, r3 + 8u, cur + 12u);
    return 1;
}

static __attribute__((noinline)) int fn_0000B2D0_nativo(spu_context* ctx)
{
    static volatile long n[2];
    const int r = fn_0000B2D0_corpo(ctx);
    gowa_edge_conta("B2D0", n, r);
    return r;
}
#endif
