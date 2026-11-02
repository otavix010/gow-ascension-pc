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

/* o laço dos copiadores: os registradores mudam de plugin para plugin (a mesma conta). passo = bytes por vértice;
 * b = o controle do embaralhamento do vértice B: os bytes desde passo (passo < 16) ou passo - 16 da janela seguinte. */
typedef struct { uint8_t passo, off, off2, out, out16, in, w, id, b, sel, tres; } plcopia_regs;

static int plcopia_laco(spu_context* ctx, const plcopia_regs* q)
{
    if (!gowa_edge_nativo_on()) return 0;
    u128* g = ctx->gpr;
    const uint32_t passo = q->passo, off = g[q->off]._u32[0];
    if (!off || (off & 31u) || (0u - off) > 0x20000u || g[q->off2]._u32[0] != off) return 0;
    const uint32_t bi = (passo < 16u ? passo : passo - 16u) * 0x01010101u;
    if (!plaf20_igual(g[q->id], 0x00010203u, 0x04050607u, 0x08090A0Bu, 0x0C0D0E0Fu) ||
        !plaf20_igual(g[q->b], bi + 0x00010203u, bi + 0x04050607u, bi + 0x08090A0Bu, bi + 0x0C0D0E0Fu) ||
        !plaf20_igual(g[q->sel], 0, 0, 0, 0xFFFFFFFFu) || !plaf20_igual(g[q->tres], 0x03030303u, 0x03030303u, 0x03030303u, 0x03030303u))
        return 0;
    const uint32_t n = (0u - off) / 32u, ent = (g[q->in]._u32[0] - 4u * passo) & SPU_LS_MASK, out = (g[q->out]._u32[0] + off) & SPU_LS_MASK;
    const uint32_t tam = 2u * passo * n + 4u * passo + 64u;   /* o original lê duas voltas à frente */
    if (g[q->out16]._u32[0] != g[q->out]._u32[0] + 16u) return 0;
    if ((out & 15u) || ent + tam > SPU_LS_SIZE || out + 32u * n > SPU_LS_SIZE) return 0;
    if (out < ent + tam && ent < out + 32u * n) return 0;
    uint8_t* ls = SPU_LS_BASE(ctx);
    uint32_t w;
    memcpy(&w, &g[q->w]._u32[3], 4);
    w = __builtin_bswap32(w);
    for (uint32_t i = 0; i < 2u * n; i++) {
        uint8_t* d = ls + out + 16u * i;
        memcpy(d, ls + ent + passo * i, 12);
        memcpy(d + 12, &w, 4);
    }
    g[3] = spu_il(0);
    g[1] = spu_ai(g[1], 224);
    return 1;
}

static __attribute__((noinline)) int pl_0075AF20_nativo(spu_context* ctx)
{
    static const plcopia_regs q = { 15, 9, 7, 8, 11, 14, 13, 19, 17, 20, 18 };
    static volatile long c[2];
    const int r = plcopia_laco(ctx, &q);
    gowa_edge_conta("pl75AF20", c, r);
    return r;
}

/* 02/11: o 0x75B470 (vértice de 22 bytes; laço 0x30154). Não roda nas gravações: conferido na bancada sintética
 * (o laço traduzido e este sobre as mesmas LS e registradores, preparação traduzida). */
static __attribute__((noinline)) int pl_0075B470_nativo(spu_context* ctx)
{
    static const plcopia_regs q = { 22, 11, 9, 8, 10, 15, 17, 20, 18, 21, 19 };
    static volatile long c[2];
    const int r = plcopia_laco(ctx, &q);
    gowa_edge_conta("pl75B470", c, r);
    return r;
}
#endif
