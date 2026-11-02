/* GOWA (01/11, EDGE nativo, marco E4): função 0xB1A8 do job 0x72DF80 inteira -- grava um comando do RSX no buffer de
 * comandos da LS: cabeçalho 0x00141EFC (método 0x1EFC, 5 palavras = carga de uma constante do programa de vértices),
 * o índice (r4) e as 4 palavras em r5; o cursor (palavra 2 do quadword em r3) avança 24 bytes. Buffer cheio
 * (cursor + 24 > fim, palavra 1) chama o descarregamento 0xAFF0 por DMA: esse caso (raro) e endereços fora do comum
 * voltam 0 e ficam com o código traduzido. Grava na pilha o que o prólogo do original grava (r80-r82, r0, r1): a
 * conferência compara a LS inteira. Temporários ficam diferentes (GOWA_CONF_REGS_ABI=1). GOWA_EDGE_NATIVO=0 desliga.
 * Gancho 'entrada' (edge_nativo.py). */
#ifndef GOWA_FN_0000B1A8_H
#define GOWA_FN_0000B1A8_H
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <intrin.h>
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


static int fn_0000B1A8_corpo(spu_context* ctx);
static __attribute__((noinline)) int fn_0000B1A8_nativo(spu_context* ctx)
{
    static volatile long n[2];
    const int r = fn_0000B1A8_corpo(ctx);
    gowa_edge_conta("B1A8", n, r);
    return r;
}

static int fn_0000B1A8_corpo(spu_context* ctx)
{
    if (!gowa_edge_nativo_on()) return 0;
    u128* g = ctx->gpr;
    const uint32_t r3 = g[3]._u32[0] & SPU_LS_MASK, src = g[5]._u32[0] & SPU_LS_MASK, sp = g[1]._u32[0] & SPU_LS_MASK;
    if ((r3 & 15u) || (src & 3u) || (sp & 15u) || sp < 0x60u || sp + 0x20u > SPU_LS_SIZE) return 0;
    const uint32_t fim = spu_ls_read32(ctx, r3 + 4u), cur = spu_ls_read32(ctx, r3 + 8u);
    if (cur + 24u > fim || (cur & 3u) || cur + 24u > SPU_LS_SIZE || src + 16u > SPU_LS_SIZE) return 0;   /* cheio: descarrega (traduzido) */
    if (src < cur + 24u && cur < src + 16u) return 0;                     /* a origem dentro do registro: a ordem importa */
    if (r3 < cur + 24u && cur < r3 + 16u) return 0;
    uint8_t* ls = SPU_LS_BASE(ctx);
    if (sp - 0x50u < cur + 24u && cur < sp + 0x20u) return 0;            /* registro sobre a pilha */
    /* prólogo do original: r80-r82 em sp-0x10/-0x20/-0x30, r0 em sp+0x10, r1 em sp-0x50 */
    spu_ls_write128(ctx, sp - 0x10u, g[80]);
    spu_ls_write128(ctx, sp - 0x20u, g[81]);
    spu_ls_write128(ctx, sp - 0x30u, g[82]);
    spu_ls_write128(ctx, sp + 0x10u, g[0]);
    spu_ls_write128(ctx, sp - 0x50u, g[1]);
    uint8_t rec[24];
    const uint32_t cab = __builtin_bswap32(0x00141EFCu), idx = __builtin_bswap32(g[4]._u32[0]);
    memcpy(rec, &cab, 4);
    memcpy(rec + 4, &idx, 4);
    memcpy(rec + 8, ls + src, 16);   /* as 4 palavras já em big-endian */
    memcpy(ls + cur, rec, 24);
    spu_ls_write32(ctx, r3 + 8u, cur + 24u);
    return 1;
}
#endif
