/* GOWA (02/11, EDGE nativo, marco E4): o plugin 0x74BA80 do EDGE inteiro -- o empacotador do vértice de 58 bytes que
 * vai para o RSX (o 0x74C580 com a cor do 0x74CEC0). Por vértice (contexto em r3, r4 = vértices, r5 = destino):
 *    0..11  atributo 1 (posição): float3 copiado
 *   12..19  atributo 3: 4 x int16 normalizado (as contas do pl_0074C580.h)
 *   20..23  atributo 4: 11:11:10 com sinal (idem)
 *   24..27  atributo 20 (cor): 4 bytes, clamp(floor(x * 255 + 0,5), 0, 255) (pl74ce_byte do pl_0074CEC0.h)
 *   28..39  atributo 27: float3 copiado
 *   40..57  atributos 28, 29 e 30: meia precisão truncada
 * Refaz o prólogo (r80-r127 na pilha em -0x10 * (r - 79), o r1 antigo em -0x360, os 4 controles de embaralhamento
 * que a preparação guarda no quadro) e o quadword depois do fim (zeros + os últimos (2 * 58) % 16 = 4 bytes). Sai
 * com r3 = 0. Só o caso comum (destino alinhado em 16, vértices múltiplos de 8, os 8 atributos presentes); o resto volta 0. Não roda nas gravações: conferido na bancada sintética (o plugin
 * traduzido x este sobre as mesmas LS, as duas builds). GOWA_EDGE_NATIVO=0 desliga. Gancho 'entrada' na 0x30000. */
#ifndef GOWA_PL_0074BA80_H
#define GOWA_PL_0074BA80_H
#include <math.h>
#include "pl_0074C580.h"
#include "pl_0074CEC0.h"
#include "edge_comum.h"

static int pl_0074BA80_corpo(spu_context* ctx)
{
#if PL_RAPIDO
    if (!gowa_edge_nativo_on()) return 0;
    uint8_t* ls = SPU_LS_BASE(ctx);
    const uint32_t r1 = ctx->gpr[1]._u32[0] & SPU_LS_MASK;
    const uint32_t r3 = ctx->gpr[3]._u32[0] & SPU_LS_MASK;
    const uint32_t n = ctx->gpr[4]._u32[0];
    const uint32_t r5 = ctx->gpr[5]._u32[0] & SPU_LS_MASK;
    if ((r5 & 15u) || (n & 7u) || !n || n > 4096 || r5 + 58u * n + 16u > SPU_LS_SIZE || (r3 & 15u) ||
        r3 + 0x170u > SPU_LS_SIZE || r1 < 0x400u)
        return 0;
    static const uint8_t ids[8] = {1, 3, 4, 20, 27, 28, 29, 30};
    uint32_t p[8];
    for (int i = 0; i < 8; i++) {
        int k = 0;
        while (k < 16 && ls[r3 + 0x160 + k] != ids[i]) k++;
        if (k == 16) return 0;
        p[i] = pl_be32(&ls[r3 + 0x10 + 4 * k]) & SPU_LS_MASK & ~15u;
        if (p[i] + 16u * n > SPU_LS_SIZE) return 0;
        if (p[i] < r5 + 58u * n + 16u && r5 < p[i] + 16u * n) return 0;
    }
    for (int r = 80; r <= 127; r++) spu_ls_write128(ctx, (r1 - 0x10u * (uint32_t)(r - 79)) & ~15u, ctx->gpr[r]);
    spu_ls_write128(ctx, (r1 - 0x360u) & ~15u, ctx->gpr[1]);
    /* os controles de embaralhamento que a preparação guarda no quadro (r1 - 0x360 + 0x20..0x50): com o destino
     * alinhado são sempre estes (lidos na bancada) */
    static const uint8_t ctl[64] = {
        0x2A, 0x2B, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
        0x08, 0x09, 0x0C, 0x0D, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F,
        0x10, 0x11, 0x12, 0x13, 0x14, 0x15, 0x16, 0x17, 0x18, 0x19, 0x20, 0x21, 0x22, 0x23, 0x24, 0x25,
        0x26, 0x27, 0x28, 0x29, 0x2A, 0x2B, 0x16, 0x17, 0x18, 0x19, 0x1A, 0x1B, 0x1C, 0x1D, 0x1E, 0x1F};
    memcpy(&ls[(r1 - 0x360u + 0x20u) & SPU_LS_MASK & ~15u], ctl, 64);
    uint8_t* o = &ls[r5];
    static const double M3[8] = {32767.5, 32767.5, 32767.5, 32767.5, 32767.5, 32767.5, 32767.5, 32767.5};
    static const double A3[8] = {-0.5, -0.5, -0.5, -0.5, -0.5, -0.5, -0.5, -0.5};
    static const double M4[8] = {0.99951171875, 0.99951171875, 0.9990234375, 0, 0.99951171875, 0.99951171875, 0.9990234375, 0};
    static const double A4[8] = {-0.00048828125, -0.00048828125, -0.0009765625, 0, -0.00048828125, -0.00048828125, -0.0009765625, 0};
    const __m256i desl4 = _mm256_setr_epi32(21, 21, 22, 32, 21, 21, 22, 32);
    const __m256i mask4 = _mm256_setr_epi32(0x7FF, 0x7FF, 0x3FF, 0, 0x7FF, 0x7FF, 0x3FF, 0);
    const __m256i pos4 = _mm256_setr_epi32(0, 11, 22, 0, 0, 11, 22, 0);
    for (uint32_t v = 0; v < n; v += 2, o += 116) {
        const uint32_t d = 16u * v;
        const __m256i t3 = pl_meias(pl_fma_cflts8(pl_entrada8(pl_carrega2(&ls[p[1] + d])), M3, A3, 65536.0), 1);
        __m256i f4 = pl_fma_cflts8(pl_entrada8(pl_carrega2(&ls[p[2] + d])), M4, A4, 2147483648.0);
        f4 = _mm256_sllv_epi32(_mm256_and_si256(_mm256_srlv_epi32(f4, desl4), mask4), pos4);
        f4 = _mm256_or_si256(f4, _mm256_shuffle_epi32(f4, 0xF5));
        f4 = _mm256_or_si256(f4, _mm256_shuffle_epi32(f4, 0xAA));
        const __m256i h28 = pl_meias(pl_half8(pl_carrega2(&ls[p[5] + d])), 0);
        const __m256i h29 = pl_meias(pl_half8(pl_carrega2(&ls[p[6] + d])), 0);
        const __m256i h30 = pl_meias(pl_half8(pl_carrega2(&ls[p[7] + d])), 0);
        for (int k = 0; k < 2; k++) {
            uint8_t* r = o + 58 * k;
            const uint32_t dk = d + 16u * (uint32_t)k;
            _mm_storeu_si128((__m128i*)r, _mm_loadu_si128((const __m128i*)&ls[p[0] + dk]));          /* posição */
            _mm_storel_epi64((__m128i*)(r + 12), k ? _mm256_extracti128_si256(t3, 1) : _mm256_castsi256_si128(t3));
            pl_put32(r + 20, (uint32_t)(k ? _mm256_extract_epi32(f4, 4) : _mm256_extract_epi32(f4, 0)));
            for (int c = 0; c < 4; c++) r[24 + c] = pl74ce_byte(pl_be32(&ls[p[3] + dk + 4u * (uint32_t)c]));
            _mm_storeu_si128((__m128i*)(r + 28), _mm_loadu_si128((const __m128i*)&ls[p[4] + dk]));   /* atributo 27 */
            _mm_storel_epi64((__m128i*)(r + 40), k ? _mm256_extracti128_si256(h28, 1) : _mm256_castsi256_si128(h28));
            _mm_storel_epi64((__m128i*)(r + 46), k ? _mm256_extracti128_si256(h29, 1) : _mm256_castsi256_si128(h29));
            _mm_storel_epi64((__m128i*)(r + 52), k ? _mm256_extracti128_si256(h30, 1) : _mm256_castsi256_si128(h30));
        }
    }
    /* o quadword depois do fim: zeros + os últimos (2 * 58) % 16 = 4 bytes */
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

static __attribute__((noinline)) int pl_0074BA80_nativo(spu_context* ctx)
{
    static volatile long c[2];
    const int r = pl_0074BA80_corpo(ctx);
    gowa_edge_conta("pl74BA80", c, r);
    return r;
}
#endif
