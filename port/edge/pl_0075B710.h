/* GOWA (02/11, EDGE nativo, marco E4): o laço 0x30154 do plugin 0x75B710 (posições float3 -> float4).
 *
 * A entrada são pares de vértices de 36 bytes (18 por vértice; os 12 primeiros são a posição float3 big-endian) e a
 * saída são 32 bytes por par: as duas posições com w = 1,0 (r17 = {0, 0, 0, 1,0}, máscara r21 = os 4 últimos bytes).
 * O laço grava um par atrás: a volta 0 grava o par que a preparação (0x30018) já extraiu em r14/r12 e cada volta
 * lê o par seguinte (r15 aponta dois pares à frente; +36 por volta). Saídas em r8 + r9 e r10 + r9 (= r8 + 16), r9 de -32n até 0. Padrões conferidos
 * aqui (r20 = identidade, r18 = bytes 2..17, r19 = 0x03, r21, r17); outro formato ou entrada e saída sobrepostas
 * voltam 0 (fica o traduzido). No fim, como o laço: r3 = 0, r1 += 224; temporários diferentes
 * (GOWA_CONF_REGS_ABI=1). GOWA_EDGE_NATIVO=0 desliga. Gancho na entrada da função do laço (edge_nativo.py). */
#ifndef GOWA_PL_0075B710_H
#define GOWA_PL_0075B710_H
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

static inline int pl75b_igual(u128 a, uint32_t w0, uint32_t w1, uint32_t w2, uint32_t w3)
{
    return a._u32[0] == w0 && a._u32[1] == w1 && a._u32[2] == w2 && a._u32[3] == w3;
}

static int pl_0075B710_corpo(spu_context* ctx)
{
    if (!gowa_edge_nativo_on()) return 0;
    u128* g = ctx->gpr;
    if (!pl75b_igual(g[20], 0x00010203u, 0x04050607u, 0x08090A0Bu, 0x0C0D0E0Fu) ||
        !pl75b_igual(g[18], 0x02030405u, 0x06070809u, 0x0A0B0C0Du, 0x0E0F1011u) ||
        !pl75b_igual(g[19], 0x03030303u, 0x03030303u, 0x03030303u, 0x03030303u) ||
        !pl75b_igual(g[21], 0, 0, 0, 0xFFFFFFFFu) || !pl75b_igual(g[17], 0, 0, 0, 0x3F800000u))
        return 0;
    const uint32_t r9 = g[9]._u32[0], r8 = g[8]._u32[0], ent = g[15]._u32[0] & SPU_LS_MASK;
    if (r9 != g[11]._u32[0] || g[10]._u32[0] != r8 + 16u || !r9 || (r9 & 31u) || (0u - r9) > 0x20000u) return 0;
    /* a preparação já carregou dois pares: o 0 em r14/r12 e o 1 em r16/r6/r2; r15 aponta para o par 2, então o par k
     * está em r15 + 36(k - 2) (o 1 é lido aqui da LS, com o mesmo conteúdo que a preparação carregou) */
    const uint32_t n = (0u - r9) / 32u, out = (r8 + r9) & SPU_LS_MASK, ini = (ent - 36u) & SPU_LS_MASK;
    if ((out & 15u) || out + 32u * n > SPU_LS_SIZE || ent < 36u || ini + 36u * n + 64u > SPU_LS_SIZE) return 0;
    if (out < ini + 36u * n + 64u && ini < out + 32u * n) return 0;   /* o original lê uma volta à frente */
    uint8_t* ls = SPU_LS_BASE(ctx);
    /* volta 0: o par que a preparação extraiu (registradores) */
    spu_ls_write128(ctx, out, spu_selb(g[14], g[17], g[21]));
    spu_ls_write128(ctx, out + 16u, spu_selb(g[12], g[17], g[21]));
    /* voltas 1..n-1: o par k-1 a partir de r15, 12 bytes de cada vértice + w = 1,0 */
    static const uint8_t um[4] = { 0x3F, 0x80, 0x00, 0x00 };
    for (uint32_t k = 1; k < n; k++) {
        const uint8_t* p = ls + ini + 36u * (k - 1u);
        uint8_t* o = ls + out + 32u * k;
        memcpy(o, p, 12); memcpy(o + 12, um, 4);
        memcpy(o + 16, p + 18, 12); memcpy(o + 28, um, 4);
    }
    g[3] = spu_il(0);
    g[1] = spu_ai(g[1], 224);
    return 1;
}

static __attribute__((noinline)) int pl_0075B710_nativo(spu_context* ctx)
{
    static volatile long n[2];
    const int r = pl_0075B710_corpo(ctx);
    gowa_edge_conta("pl75B", n, r);
    return r;
}
#endif
