/* GOWA (02/11, EDGE nativo, marco E4): o plugin 0x755F80 do EDGE inteiro -- o empacotador do vértice de 36 bytes (da
 * família do 0x74C580). Contexto em r3 (lista de atributos em +0x160, ponteiros dos fluxos em +0x10), r4 = vértices,
 * r5 = destino. Por vértice, a partir dos fluxos float4:
 *    0..11  atributo 1 (posição): os 12 bytes copiados
 *   12..19  atributo 3: 4 x int16 normalizado (as contas do pl_0074C580.h)
 *   20..23  atributo 4: 11:11:10 com sinal (idem)
 *   24..35  atributo 31: os 12 bytes copiados
 * Aos pares (m = n arredondado para par), em quadwords alinhados; depois do fim (E = r5 + 36m) um quadword com 8
 * zeros + os últimos 8 bytes e zeros até o próximo múltiplo de 16 (lido na bancada, como no 0x758B20). Não escreve
 * pilha nem contexto; sai com r3 = 0. Só o caso comum (destino e fluxos alinhados em 16, os quatro atributos
 * presentes); o resto volta 0. Não roda nas gravações: conferido na bancada sintética (as duas builds).
 * GOWA_EDGE_NATIVO=0 desliga. Ganchos 'entrada' na 0x30000 e na 0x30030 (o job chama direto em +0x30: ali o nativo
 * só vale se os registradores que o 0x30000 prepara estão como ele os deixaria). */
#ifndef GOWA_PL_00755F80_H
#define GOWA_PL_00755F80_H
#include "pl_0074C580.h"
#include "edge_comum.h"

static int pl_00755F80_corpo(spu_context* ctx)
{
#if PL_RAPIDO
    if (!gowa_edge_nativo_on()) return 0;
    uint8_t* ls = SPU_LS_BASE(ctx);
    const uint32_t r3 = ctx->gpr[3]._u32[0] & SPU_LS_MASK;
    const uint32_t n = ctx->gpr[4]._u32[0];
    const uint32_t r5 = ctx->gpr[5]._u32[0] & SPU_LS_MASK;
    if (!n || n > 4096 || (r5 & 15u) || (r3 & 15u) || r3 + 0x170u > SPU_LS_SIZE) return 0;
    const uint32_t m = (n + 1u) & ~1u, fim = r5 + 36u * m;
    if (fim + 32u > SPU_LS_SIZE) return 0;
    static const uint8_t ids[4] = {1, 3, 4, 31};
    uint32_t p[4];
    for (int i = 0; i < 4; i++) {
        int k = 0;
        while (k < 16 && ls[r3 + 0x160 + k] != ids[i]) k++;
        if (k == 16) return 0;
        p[i] = pl_be32(&ls[r3 + 0x10 + 4 * k]) & SPU_LS_MASK;
        if ((p[i] & 15u) || p[i] + 16u * m + 64u > SPU_LS_SIZE) return 0;
        if (p[i] < fim + 32u && (r5 & ~15u) < p[i] + 16u * m + 64u) return 0;   /* fluxo e destino separados */
    }
    static const double M3[8] = {32767.5, 32767.5, 32767.5, 32767.5, 32767.5, 32767.5, 32767.5, 32767.5};
    static const double A3[8] = {-0.5, -0.5, -0.5, -0.5, -0.5, -0.5, -0.5, -0.5};
    static const double M4[8] = {0.99951171875, 0.99951171875, 0.9990234375, 0, 0.99951171875, 0.99951171875, 0.9990234375, 0};
    static const double A4[8] = {-0.00048828125, -0.00048828125, -0.0009765625, 0, -0.00048828125, -0.00048828125, -0.0009765625, 0};
    const __m256i desl4 = _mm256_setr_epi32(21, 21, 22, 32, 21, 21, 22, 32);
    const __m256i mask4 = _mm256_setr_epi32(0x7FF, 0x7FF, 0x3FF, 0, 0x7FF, 0x7FF, 0x3FF, 0);
    const __m256i pos4 = _mm256_setr_epi32(0, 11, 22, 0, 0, 11, 22, 0);
    for (uint32_t v = 0; v < m; v += 2) {
        const uint32_t d = 16u * v;
        const __m256i t3 = pl_meias(pl_fma_cflts8(pl_entrada8(pl_carrega2(&ls[p[1] + d])), M3, A3, 65536.0), 1);
        __m256i f4 = pl_fma_cflts8(pl_entrada8(pl_carrega2(&ls[p[2] + d])), M4, A4, 2147483648.0);
        f4 = _mm256_sllv_epi32(_mm256_and_si256(_mm256_srlv_epi32(f4, desl4), mask4), pos4);
        f4 = _mm256_or_si256(f4, _mm256_shuffle_epi32(f4, 0xF5));
        f4 = _mm256_or_si256(f4, _mm256_shuffle_epi32(f4, 0xAA));
        for (int k = 0; k < 2; k++) {
            uint8_t* r = &ls[r5 + 36u * (v + (uint32_t)k)];
            const uint32_t dk = d + 16u * (uint32_t)k;
            memcpy(r, &ls[p[0] + dk], 12);
            _mm_storel_epi64((__m128i*)(r + 12), k ? _mm256_extracti128_si256(t3, 1) : _mm256_castsi256_si128(t3));
            pl_put32(r + 20, (uint32_t)(k ? _mm256_extract_epi32(f4, 4) : _mm256_extract_epi32(f4, 0)));
            memcpy(r + 24, &ls[p[3] + dk], 12);
        }
    }
    /* depois do fim: 8 zeros + os últimos 8 bytes, e zeros até o próximo múltiplo de 16 */
    uint8_t cauda[16] = {0};
    memcpy(cauda + 8, &ls[fim - 8u], 8);
    memcpy(&ls[fim], cauda, 16);
    memset(&ls[fim + 16u], 0, ((fim + 31u) & ~15u) - (fim + 16u));
    ctx->gpr[3] = spu_il(0);
    return 1;
#else
    (void)ctx;
    return 0;
#endif
}

static __attribute__((noinline)) int pl_00755F80_nativo(spu_context* ctx)
{
    static volatile long c[2];
    const int r = pl_00755F80_corpo(ctx);
    gowa_edge_conta("pl755F80", c, r);
    return r;
}
/* a entrada +0x30 (o job chama direto nela): vale se os registradores que o 0x30000 prepara estão como ele os deixaria */
static inline int pl755f_q(spu_context* ctx, int r, uint32_t a)
{
    const u128 q = spu_ls_read128(ctx, a);
    return !memcmp(&q, &ctx->gpr[r], 16);
}
static __attribute__((noinline)) int pl_00755F80_n30(spu_context* ctx)
{
    static volatile long c[2];
    const u128* g = ctx->gpr;
    int ok = pl755f_q(ctx, 67, g[3]._u32[0] + 0x160u) && pl755f_q(ctx, 62, (uint32_t)(0x303F0 + g_spu_hot_reloc)) &&
             pl755f_q(ctx, 61, (uint32_t)(0x30400 + g_spu_hot_reloc)) && pl755f_q(ctx, 72, (uint32_t)(0x30410 + g_spu_hot_reloc)) &&
             pl755f_q(ctx, 69, (uint32_t)(0x30420 + g_spu_hot_reloc)) && pl755f_q(ctx, 44, (uint32_t)(0x30430 + g_spu_hot_reloc));
    for (int i = 0; i < 4 && ok; i++)
        ok = g[31]._u32[i] == g[3]._u32[i] + 16u && g[52]._u32[i] == g[4]._u32[i] + 1u && g[51]._u32[i] == 0x46FFFF00u &&
             g[60]._u32[i] == 0xBF000000u && g[71]._u32[i] == 0u;
    const int r = ok ? pl_00755F80_corpo(ctx) : 0;
    gowa_edge_conta("pl755F80/30", c, r);
    return r;
}
#endif
