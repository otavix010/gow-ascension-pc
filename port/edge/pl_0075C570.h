/* GOWA (30/10, EDGE nativo): o laço 0x300D0 do plugin 0x75C570 (posições float4 -> float3 empacotado).
 *
 * A volta i grava em out + 24i os xyz dos dois vértices do par i (in + 32i e in + 32i + 16), com três gravações
 * alinhadas de 16 bytes que mesclam o começo do bloco (r17, lido da LS) e deixam lixo depois; o laço lê dois pares
 * à frente. O nativo grava os vértices das voltas 0..n-2 (out + 12k <- in + 16k, 12 bytes, a LS é big-endian dos
 * dois lados) e monta os registradores vivos da última volta, que o laço traduzido roda: o lixo do fim e a mescla
 * saem exatos. Conferido pelo oráculo (scripts_edge/pl75c_modelo.py: 12/12 fotos com a LS inteira; só o
 * temporário r39 muda). Só o formato visto nas fotos (os controles de shufb conferidos abaixo); fora disso devolve
 * 0 e o laço faz tudo. GOWA_EDGE_NATIVO=0 desliga. Gancho 'entrada_segue' (edge_nativo.py): devolvendo 1, o
 * traduzido recarrega os registradores e segue no laço. */
#ifndef GOWA_PL_0075C570_H
#define GOWA_PL_0075C570_H
#include <stdlib.h>
#include <string.h>
#include <immintrin.h>
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

static inline int pl75c_igual(u128 a, uint32_t w0, uint32_t w1, uint32_t w2, uint32_t w3)
{
    return a._u32[0] == w0 && a._u32[1] == w1 && a._u32[2] == w2 && a._u32[3] == w3;
}
static inline int pl75c_mesmo(u128 a, u128 b) { return memcmp(&a, &b, 16) == 0; }

static __attribute__((noinline)) int pl_0075C570_nativo(spu_context* ctx)
{
    if (!gowa_edge_nativo_on()) return 0;
    u128* g = ctx->gpr;
    const uint32_t tot = 0u - g[10]._u32[0];
    if (!tot || tot % 24u || tot > 0x20000u || g[14]._u32[0] != g[10]._u32[0]) return 0;
    if (!pl75c_igual(g[8], 0x10111213u, 0x14151617u, 0x18191A1Bu, 0x20212223u) ||
        !pl75c_igual(g[24], 0x24252627u, 0x28292A2Bu, 0x18191A1Bu, 0x1C1D1E1Fu) ||
        !pl75c_igual(g[25], 0x03030303u, 0x03030303u, 0x03030303u, 0x03030303u) ||
        !pl75c_igual(g[26], 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0) ||
        !pl75c_igual(g[18], 0x20212223u, 0x24252627u, 0x28292A2Bu, 0x2C2D2E2Fu))
        return 0;
    const uint32_t fim = g[23]._u32[0];
    if (g[21]._u32[0] != fim || g[20]._u32[0] != fim + 16u || g[19]._u32[0] != fim + 32u || g[22]._u32[0] != fim + 24u) return 0;
    const uint32_t n = tot / 24u, J = n - 1u;
    if (n < 2u || (g[12]._u32[0] & 15u)) return 0;
    const uint32_t out = fim - tot, ent = g[12]._u32[0] - 96u;
    if (out > SPU_LS_SIZE || out + tot + 48u > SPU_LS_SIZE || ent > SPU_LS_SIZE || ent + 32u * (n + 2u) > SPU_LS_SIZE) return 0;
    if (out < ent + 32u * (n + 2u) && ent < out + tot + 48u) return 0;
    uint8_t* ls = SPU_LS_BASE(ctx);
    /* os registradores da entrada têm de ser os do par 0 lido de ent (confere o ponteiro de entrada) */
    {
        const u128 A = spu_ls_read128(ctx, ent), B = spu_ls_read128(ctx, ent + 16u);
        if (g[9]._u32[0] != A._u32[0] || g[9]._u32[1] != A._u32[1] || g[9]._u32[2] != A._u32[2] || g[9]._u32[3] != B._u32[0] ||
            g[11]._u32[0] != B._u32[1] || g[11]._u32[1] != B._u32[2])
            return 0;
        if (!pl75c_mesmo(spu_ls_read128(ctx, ent + 32u), g[16]) || !pl75c_mesmo(spu_ls_read128(ctx, ent + 48u), g[13]) ||
            !pl75c_mesmo(spu_ls_read128(ctx, ent + 64u), g[2]) || !pl75c_mesmo(spu_ls_read128(ctx, ent + 80u), g[15]))
            return 0;
    }
    /* vértices 0..2J-1: 16 bytes gravados por vez (os 4 a mais são cobertos pelo vértice seguinte), o último com 12 */
    const uint32_t nv = 2u * J;
    for (uint32_t v = 0; v + 1u < nv; v++)
        _mm_storeu_si128((__m128i*)(ls + out + 12u * v), _mm_loadu_si128((const __m128i*)(ls + ent + 16u * v)));
    memcpy(ls + out + 12u * (nv - 1u), ls + ent + 16u * (nv - 1u), 12);
    /* os vivos da entrada da volta J */
    const uint32_t a = ent + 32u * J, b = a + 16u;
    {
        const u128 A = spu_ls_read128(ctx, a), B = spu_ls_read128(ctx, b);
        g[9]._u32[0] = A._u32[0]; g[9]._u32[1] = A._u32[1]; g[9]._u32[2] = A._u32[2]; g[9]._u32[3] = B._u32[0];
        g[11]._u32[0] = B._u32[1]; g[11]._u32[1] = B._u32[2];
    }
    g[16] = spu_ls_read128(ctx, a + 32u); g[13] = spu_ls_read128(ctx, a + 48u);
    g[2] = spu_ls_read128(ctx, a + 64u); g[15] = spu_ls_read128(ctx, a + 80u);
    for (int i = 0; i < 4; i++) { g[10]._u32[i] += 24u * J; g[14]._u32[i] += 24u * J; g[12]._u32[i] += 32u * J; }
    g[17] = spu_ls_read128(ctx, out + 24u * J);
    return 1;
}

/* 02/11: o plugin inteiro, da 0x30000 ou da 0x30010 (onde o job também chama direto; ali vale se r18 = r4 + 1,
 * r30 = a lista, r2 = 0 e r28 = a constante 0x30140, o que o 0x30000 deixa). O fluxo de entrada é o do atributo 1
 * (o primeiro byte 1 da lista em r3 + 0x160; ponteiro na vaga, em r3 + 0x10); n = r4, aos pares; saída em r5 (sem
 * alinhamento), 12 bytes por vértice (o xyz). O original grava em quadwords alinhados, preservando o que vem antes de
 * r5; depois do fim (E = r5 + 12m) fica um quadword com 8 zeros + os últimos 8 bytes, cortado em
 * ((E + 8) & ~15) + 16, e zeros no que sobrar (lido na bancada). Não escreve pilha nem contexto; r3 = 0. */
static int pl_0075C570_inteiro_corpo(spu_context* ctx, int na10)
{
    if (!gowa_edge_nativo_on()) return 0;
    u128* g = ctx->gpr;
    uint8_t* ls = SPU_LS_BASE(ctx);
    const uint32_t r3 = g[3]._u32[0] & SPU_LS_MASK, n = g[4]._u32[0], r5 = g[5]._u32[0] & SPU_LS_MASK;
    if ((r3 & 15u) || r3 + 0x170u > SPU_LS_SIZE || !n || n > 4096u) return 0;
    if (na10) {
        const u128 l = spu_ls_read128(ctx, r3 + 0x160u), c = spu_ls_read128(ctx, (uint32_t)(0x30140 + g_spu_hot_reloc));
        if (memcmp(&l, &g[30], 16) || memcmp(&c, &g[28], 16)) return 0;
        for (int i = 0; i < 4; i++) if (g[18]._u32[i] != g[4]._u32[i] + 1u || g[2]._u32[i]) return 0;
    }
    int k = 0;
    while (k < 16 && ls[r3 + 0x160u + (uint32_t)k] != 1u) k++;
    if (k == 16) return 0;
    const uint32_t a = r3 + 16u + 4u * (uint32_t)k;
    const uint32_t in = (((uint32_t)ls[a] << 24) | ((uint32_t)ls[a + 1] << 16) | ((uint32_t)ls[a + 2] << 8) | ls[a + 3]) & SPU_LS_MASK;
    const uint32_t m = (n + 1u) & ~1u, fim = r5 + 12u * m, fimq = ((fim + 8u) & ~15u) + 16u;
    if ((in & 15u) || in + 16u * m + 64u > SPU_LS_SIZE || fimq > SPU_LS_SIZE) return 0;
    if ((r5 < in + 16u * m + 64u && in < fimq) || (r5 < r3 + 0x170u && r3 < fimq)) return 0;
    for (uint32_t i = 0; i < m; i++) memcpy(ls + r5 + 12u * i, ls + in + 16u * i, 12);
    uint8_t cauda[24] = {0};
    memcpy(cauda + 8, ls + fim - 8u, 8);
    memcpy(ls + fim, cauda, fimq - fim);
    g[3] = spu_il(0);
    return 1;
}
static __attribute__((noinline)) int pl_0075C570_inteiro(spu_context* ctx)
{
    static volatile long c[2];
    const int r = pl_0075C570_inteiro_corpo(ctx, 0);
    gowa_edge_conta("pl75C570i", c, r);
    return r;
}
static __attribute__((noinline)) int pl_0075C570_n10(spu_context* ctx)
{
    static volatile long c[2];
    const int r = pl_0075C570_inteiro_corpo(ctx, 1);
    gowa_edge_conta("pl75C570i/10", c, r);
    return r;
}
#endif
