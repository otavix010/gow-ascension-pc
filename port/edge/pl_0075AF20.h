/* GOWA (02/11, EDGE nativo, marco E4): o laço 0x30160 do plugin 0x75AF20 do EDGE, da entrada do laço ao fim (a
 * preparação 0x30000 continua a traduzida).
 *
 * Vértice de 15 bytes no fluxo: os 12 primeiros (a posição float3) vão para um float4 com w = a palavra 3 de r13 (a
 * constante do plugin em 0x302A0; selb com r20 = fsmbi 0xF); o resto do vértice este plugin não usa. 2 vértices por
 * volta, 32 bytes, a partir de r8 + r7; N = -r9/32 voltas. Na entrada do laço a preparação já embaralhou o par 0
 * (r16, r12) e o par 1 (r10, r6); a entrada é r14 - 60. Aqui todos os pares direto da LS (os mesmos bytes). No fim,
 * como o laço: r3 = 0, r1 += 224. Temporários ficam diferentes (GOWA_CONF_REGS_ABI=1). GOWA_EDGE_NATIVO=0 desliga.
 * Gancho 'entrada' na função do laço. */
#ifndef GOWA_PL_0075AF20_H
#define GOWA_PL_0075AF20_H
#include <stdlib.h>
#include <string.h>
#include "edge_comum.h"

#ifndef GOWA_EDGE_NATIVO_ON_DEF
#define GOWA_EDGE_NATIVO_ON_DEF
static int gowa_edge_nativo_on(void)
{
    static int on = -1;
    if (on < 0) { const char* e = getenv("GOWA_EDGE_NATIVO"); on = !(e && *e == '0'); }
    return on;
}
#endif

static inline int plaf20_igual(u128 a, uint32_t w0, uint32_t w1, uint32_t w2, uint32_t w3)
{
    return a._u32[0] == w0 && a._u32[1] == w1 && a._u32[2] == w2 && a._u32[3] == w3;
}

static int pl_0075AF20_corpo(spu_context* ctx)
{
    if (!gowa_edge_nativo_on()) return 0;
    u128* g = ctx->gpr;
    const uint32_t off = g[9]._u32[0];
    if (!off || (off & 31u) || (0u - off) > 0x20000u || g[7]._u32[0] != off) return 0;
    /* os controles do embaralhamento: identidade em r19, o vértice B (bytes 15..30) em r17, w em r20 */
    if (!plaf20_igual(g[19], 0x00010203u, 0x04050607u, 0x08090A0Bu, 0x0C0D0E0Fu) ||
        !plaf20_igual(g[17], 0x0F101112u, 0x13141516u, 0x1718191Au, 0x1B1C1D1Eu) ||
        !plaf20_igual(g[20], 0, 0, 0, 0xFFFFFFFFu) || !plaf20_igual(g[18], 0x03030303u, 0x03030303u, 0x03030303u, 0x03030303u))
        return 0;
    const uint32_t n = (0u - off) / 32u, ent = (g[14]._u32[0] - 60u) & SPU_LS_MASK, out = (g[8]._u32[0] + off) & SPU_LS_MASK;
    const uint32_t tam = 30u * n + 60u + 48u;   /* o original lê duas voltas à frente */
    if (g[11]._u32[0] != g[8]._u32[0] + 16u) return 0;
    if ((out & 15u) || ent + tam > SPU_LS_SIZE || out + 32u * n > SPU_LS_SIZE) return 0;
    if (out < ent + tam && ent < out + 32u * n) return 0;
    uint8_t* ls = SPU_LS_BASE(ctx);
    uint32_t w;
    memcpy(&w, &g[13]._u32[3], 4);
    w = __builtin_bswap32(w);
    for (uint32_t i = 0; i < 2u * n; i++) {
        uint8_t* d = ls + out + 16u * i;
        memcpy(d, ls + ent + 15u * i, 12);
        memcpy(d + 12, &w, 4);
    }
    g[3] = spu_il(0);
    g[1] = spu_ai(g[1], 224);
    return 1;
}

static __attribute__((noinline)) int pl_0075AF20_nativo(spu_context* ctx)
{
    static volatile long c[2];
    const int r = pl_0075AF20_corpo(ctx);
    gowa_edge_conta("pl75AF20", c, r);
    return r;
}
#endif
