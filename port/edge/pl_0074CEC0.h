/* GOWA (02/11, EDGE nativo, marco E4): o plugin 0x74CEC0 do EDGE inteiro -- o empacotador do vértice de 50 bytes que
 * vai para o RSX (o 0x74D7E0 com a cor do atributo 20). Por vértice (contexto em r3, r4 = vértices, r5 = destino):
 *    0..11  atributo 1 (posição): float3 copiado
 *   12..15  atributo 2: 11:11:10 com sinal (as contas do pl_0074C580.h)
 *   16..19  atributo 20 (cor): 4 bytes, cada um = byte alto de cfltu(fma(x, 255, 0,5), 149) = clamp(floor(x * 255 +
 *           0,5), 0, 255) (o fma arredonda para zero, e o piso do valor truncado é o piso do exato; em double é exato)
 *   20..31  atributo 27: float3 copiado
 *   32..49  atributos 28, 29 e 30: meia precisão truncada
 * Refaz o prólogo (r80-r115 e r126 na pilha, o r1 antigo em -0x270, quadro de 624) e o bloco de 16 bytes depois do fim
 * (12 zeros + os últimos 4 bytes: zeros + os últimos (2 * registro) % 16). Sai com r3 = 0. Só o caso das gravações;
 * o resto volta 0. GOWA_EDGE_NATIVO=0 desliga. Ganchos 'entrada' na 0x30000 e na 0x30018. */
#ifndef GOWA_PL_0074CEC0_H
#define GOWA_PL_0074CEC0_H
#include <math.h>
#include "pl_0074C580.h"
#include "edge_comum.h"

/* um componente da cor como o plugin (float da SPU: denormal = 0, expoente 255 = número grande) */
static inline uint8_t pl74ce_byte(uint32_t w)
{
    const uint32_t e = (w >> 23) & 0xFFu;
    if (e == 0u) w = 0u;
    if (e == 0xFFu) return (w >> 31) ? 0u : 255u;
    float f; memcpy(&f, &w, 4);
    const double v = floor((double)f * 255.0 + 0.5);
    return v <= 0.0 ? 0u : v >= 255.0 ? 255u : (uint8_t)v;
}

static int pl_0074CEC0_corpo(spu_context* ctx)
{
#if PL_RAPIDO
    if (!gowa_edge_nativo_on()) return 0;
    uint8_t* ls = SPU_LS_BASE(ctx);
    const uint32_t r1 = ctx->gpr[1]._u32[0] & SPU_LS_MASK;
    const uint32_t r3 = ctx->gpr[3]._u32[0] & SPU_LS_MASK;
    const uint32_t n = ctx->gpr[4]._u32[0];
    const uint32_t r5 = ctx->gpr[5]._u32[0] & SPU_LS_MASK;
    if ((r5 & 15u) || (n & 7u) || !n || n > 4096 || r5 + 50u * n + 16u > SPU_LS_SIZE || (r3 & 15u) ||
        r3 + 0x170u > SPU_LS_SIZE || r1 < 0x300u)
        return 0;
    static const uint8_t ids[7] = {1, 2, 20, 27, 28, 29, 30};
    uint32_t p[7];
    for (int i = 0; i < 7; i++) {
        int k = 0;
        while (k < 16 && ls[r3 + 0x160 + k] != ids[i]) k++;
        if (k == 16) return 0;
        p[i] = pl_be32(&ls[r3 + 0x10 + 4 * k]) & SPU_LS_MASK & ~15u;
        if (p[i] + 16u * n > SPU_LS_SIZE) return 0;
        if (p[i] < r5 + 50u * n + 16u && r5 < p[i] + 16u * n) return 0;
    }
    for (int r = 80; r <= 115; r++) spu_ls_write128(ctx, (r1 - 0x10u * (uint32_t)(r - 79)) & ~15u, ctx->gpr[r]);
    spu_ls_write128(ctx, (r1 - 0x250u) & ~15u, ctx->gpr[126]);
    spu_ls_write128(ctx, (r1 - 0x270u) & ~15u, ctx->gpr[1]);
    uint8_t* o = &ls[r5];
    static const double M4[8] = {0.99951171875, 0.99951171875, 0.9990234375, 0, 0.99951171875, 0.99951171875, 0.9990234375, 0};
    static const double A4[8] = {-0.00048828125, -0.00048828125, -0.0009765625, 0, -0.00048828125, -0.00048828125, -0.0009765625, 0};
    const __m256i desl4 = _mm256_setr_epi32(21, 21, 22, 32, 21, 21, 22, 32);
    const __m256i mask4 = _mm256_setr_epi32(0x7FF, 0x7FF, 0x3FF, 0, 0x7FF, 0x7FF, 0x3FF, 0);
    const __m256i pos4 = _mm256_setr_epi32(0, 11, 22, 0, 0, 11, 22, 0);
    for (uint32_t v = 0; v < n; v += 2, o += 100) {
        const uint32_t d = 16u * v;
        __m256i f4 = pl_fma_cflts8(pl_entrada8(pl_carrega2(&ls[p[1] + d])), M4, A4, 2147483648.0);
        f4 = _mm256_sllv_epi32(_mm256_and_si256(_mm256_srlv_epi32(f4, desl4), mask4), pos4);
        f4 = _mm256_or_si256(f4, _mm256_shuffle_epi32(f4, 0xF5));
        f4 = _mm256_or_si256(f4, _mm256_shuffle_epi32(f4, 0xAA));
        const __m256i h28 = pl_meias(pl_half8(pl_carrega2(&ls[p[4] + d])), 0);
        const __m256i h29 = pl_meias(pl_half8(pl_carrega2(&ls[p[5] + d])), 0);
        const __m256i h30 = pl_meias(pl_half8(pl_carrega2(&ls[p[6] + d])), 0);
        for (int k = 0; k < 2; k++) {
            uint8_t* r = o + 50 * k;
            const uint32_t dk = d + 16u * (uint32_t)k;
            _mm_storeu_si128((__m128i*)r, _mm_loadu_si128((const __m128i*)&ls[p[0] + dk]));          /* posição */
            pl_put32(r + 12, (uint32_t)(k ? _mm256_extract_epi32(f4, 4) : _mm256_extract_epi32(f4, 0)));
            for (int c = 0; c < 4; c++) r[16 + c] = pl74ce_byte(pl_be32(&ls[p[2] + dk + 4u * (uint32_t)c]));
            _mm_storeu_si128((__m128i*)(r + 20), _mm_loadu_si128((const __m128i*)&ls[p[3] + dk]));   /* atributo 27 */
            _mm_storel_epi64((__m128i*)(r + 32), k ? _mm256_extracti128_si256(h28, 1) : _mm256_castsi256_si128(h28));
            _mm_storel_epi64((__m128i*)(r + 38), k ? _mm256_extracti128_si256(h29, 1) : _mm256_castsi256_si128(h29));
            _mm_storel_epi64((__m128i*)(r + 44), k ? _mm256_extracti128_si256(h30, 1) : _mm256_castsi256_si128(h30));
        }
    }
    /* o quadword depois do fim: zeros + os últimos (2 * 50) % 16 = 4 bytes (no 0x74C580 e no 0x74D7E0 são 12) */
    uint8_t cauda[16] = {0};
    memcpy(cauda + 12, o - 4, 4);
    memcpy(o, cauda, 16);
    ctx->gpr[3] = spu_il(0);
    return 1;
#else
    (void)ctx;
    return 0;
#endif
}

static __attribute__((noinline)) int pl_0074CEC0_nativo(spu_context* ctx)
{
    static volatile long c[2];
    const int r = pl_0074CEC0_corpo(ctx);
    gowa_edge_conta("pl74CEC0", c, r);
    return r;
}
#endif
