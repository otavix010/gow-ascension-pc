/* GOWA (02/11, EDGE nativo, marco E4): o plugin 0x75AA40 do EDGE inteiro -- o empacotador do vértice de 16 bytes (o
 * mais simples da família do 0x74C580). Contexto em r3 (lista de atributos em +0x160, ponteiros dos fluxos em +0x10),
 * r4 = vértices, r5 = destino. Por vértice, a partir dos fluxos float4 dos atributos:
 *    0..11  atributo 1 (posição): os 12 bytes copiados
 *   12..15  atributo 2: 11:11:10 com sinal (as contas do pl_0074C580.h: fma com 0x3F7FE000/0xBA000000 e
 *           0x3F7FC000/0xBA800000, cflts x 2^31, os bits altos de cada; o original junta com rotm 5/2/0, shufb e selb
 *           0x003FF800, que dá a mesma palavra)
 * O original faz os vértices aos pares (n ímpar grava também o registro do vértice n, que lê o fluxo depois do fim).
 * Não escreve pilha nem contexto; sai com r3 = 0. Só o caso comum (destino e fluxos alinhados em 16, os dois
 * atributos presentes); o resto volta 0. Não roda nas gravações: conferido na bancada sintética (o plugin traduzido x
 * este sobre as mesmas LS). GOWA_EDGE_NATIVO=0 desliga. Gancho 'entrada' na 0x30000. */
#ifndef GOWA_PL_0075AA40_H
#define GOWA_PL_0075AA40_H
#include "pl_0074C580.h"
#include "edge_comum.h"

static int pl_0075AA40_corpo(spu_context* ctx)
{
#if PL_RAPIDO
    if (!gowa_edge_nativo_on()) return 0;
    uint8_t* ls = SPU_LS_BASE(ctx);
    const uint32_t r3 = ctx->gpr[3]._u32[0] & SPU_LS_MASK;
    const uint32_t n = ctx->gpr[4]._u32[0];
    const uint32_t r5 = ctx->gpr[5]._u32[0] & SPU_LS_MASK;
    if (!n || n > 4096 || (r5 & 15u) || (r3 & 15u) || r3 + 0x170u > SPU_LS_SIZE) return 0;
    const uint32_t m = (n + 1u) & ~1u;   /* os vértices gravados (aos pares) */
    if (r5 + 16u * m > SPU_LS_SIZE) return 0;
    static const uint8_t ids[2] = {1, 2};
    uint32_t p[2];
    for (int i = 0; i < 2; i++) {
        int k = 0;
        while (k < 16 && ls[r3 + 0x160 + k] != ids[i]) k++;
        if (k == 16) return 0;
        p[i] = pl_be32(&ls[r3 + 0x10 + 4 * k]) & SPU_LS_MASK;
        if ((p[i] & 15u) || p[i] + 16u * m + 64u > SPU_LS_SIZE) return 0;
        if (p[i] < r5 + 16u * m && r5 < p[i] + 16u * m + 64u) return 0;   /* fluxo e destino separados */
    }
    static const double M4[8] = {0.99951171875, 0.99951171875, 0.9990234375, 0, 0.99951171875, 0.99951171875, 0.9990234375, 0};
    static const double A4[8] = {-0.00048828125, -0.00048828125, -0.0009765625, 0, -0.00048828125, -0.00048828125, -0.0009765625, 0};
    const __m256i desl4 = _mm256_setr_epi32(21, 21, 22, 32, 21, 21, 22, 32);
    const __m256i mask4 = _mm256_setr_epi32(0x7FF, 0x7FF, 0x3FF, 0, 0x7FF, 0x7FF, 0x3FF, 0);
    const __m256i pos4 = _mm256_setr_epi32(0, 11, 22, 0, 0, 11, 22, 0);
    for (uint32_t v = 0; v < m; v += 2) {
        const uint32_t d = 16u * v;
        __m256i f4 = pl_fma_cflts8(pl_entrada8(pl_carrega2(&ls[p[1] + d])), M4, A4, 2147483648.0);
        f4 = _mm256_sllv_epi32(_mm256_and_si256(_mm256_srlv_epi32(f4, desl4), mask4), pos4);
        f4 = _mm256_or_si256(f4, _mm256_shuffle_epi32(f4, 0xF5));
        f4 = _mm256_or_si256(f4, _mm256_shuffle_epi32(f4, 0xAA));
        for (int k = 0; k < 2; k++) {
            uint8_t* r = &ls[r5 + d + 16u * (uint32_t)k];
            memcpy(r, &ls[p[0] + d + 16u * (uint32_t)k], 12);
            pl_put32(r + 12, (uint32_t)(k ? _mm256_extract_epi32(f4, 4) : _mm256_extract_epi32(f4, 0)));
        }
    }
    ctx->gpr[3] = spu_il(0);
    return 1;
#else
    (void)ctx;
    return 0;
#endif
}

static __attribute__((noinline)) int pl_0075AA40_nativo(spu_context* ctx)
{
    static volatile long c[2];
    const int r = pl_0075AA40_corpo(ctx);
    gowa_edge_conta("pl75AA40", c, r);
    return r;
}
#endif
