/* GOWA (30/10, EDGE nativo, marco E3): o laço 0x30310 do plugin 0x755540 (posições comprimidas -> float4).
 *
 * Cada vértice ocupa 12 bytes no fluxo (a partir de r27 - 48; o laço lê 2 voltas à frente) e a posição são os 3
 * primeiros u16 (big-endian). Por componente c: v = (u16 & r33) + r31 (inteiro de 32 bits com sinal) e a saída é
 *     fa(csflt(v >> (32 - s), 155), cuflt(v << s, 123)) = v * 2^(s - 32),   s = r34, r17 = -(32 - s)
 * (as duas partes são exatas com 8 <= s <= 31 e |v| < 2^24), w = 1,0 (r35). 2 vértices por volta, 32 bytes, a partir
 * de r36 + r18; N = -r19/32 voltas. Conferido pelo oráculo (scripts_edge/pl755_modelo.py: 1928/1928 vértices das
 * 12 fotos). Só o formato visto nas fotos (as constantes de deslocamento e os controles de shufb conferidos abaixo);
 * o resto volta 0 e fica o vk. No fim, como o laço: r3 = 0, r1 += 224. Temporários ficam diferentes
 * (GOWA_CONF_REGS_ABI=1). GOWA_EDGE_NATIVO=0 desliga. Gancho na entrada da função do laço (edge_nativo.py). */
#ifndef GOWA_PL_00755540_H
#define GOWA_PL_00755540_H
#include <stdlib.h>
#include <string.h>
#include <immintrin.h>

#ifndef GOWA_EDGE_NATIVO_ON_DEF
#define GOWA_EDGE_NATIVO_ON_DEF
static int gowa_edge_nativo_on(void)
{
    static int on = -1;
    if (on < 0) { const char* e = getenv("GOWA_EDGE_NATIVO"); on = !(e && *e == '0'); }   /* 30/10 (E7): ligado por padrão; =0 desliga */
    return on;
}
#endif

static inline int pl755_igual(u128 a, uint32_t w0, uint32_t w1, uint32_t w2, uint32_t w3)
{
    return a._u32[0] == w0 && a._u32[1] == w1 && a._u32[2] == w2 && a._u32[3] == w3;
}

static __attribute__((noinline)) int pl_00755540_nativo(spu_context* ctx)
{
    if (!gowa_edge_nativo_on()) return 0;
    u128* g = ctx->gpr;
    const uint32_t off = g[19]._u32[0];
    if (!off || (off & 31u) || (0u - off) > 0x20000u || g[18]._u32[0] != off) return 0;
    /* o formato das fotos: 12 bytes por vértice, 3 x u16 no começo, máscara 0xFFFF, w = 1,0 */
    if (!pl755_igual(g[23], 0xFFFFFFD0u, 0xFFFFFFB7u, 0, 0) || !pl755_igual(g[24], 0xFFFFFFF0u, 0xFFFFFFE0u, 0xFFFFFFD0u, 0xFFFFFFB7u) ||
        !pl755_igual(g[26], 0xFFFFFFE0u, 0xFFFFFFD0u, 0xFFFFFFB7u, 0) || !pl755_igual(g[28], 0xFFFFFFB7u, 0, 0, 0) ||
        !pl755_igual(g[33], 0xFFFFu, 0xFFFFu, 0xFFFFu, 0) || !pl755_igual(g[38], 0, 0, 0, 0xFFFFFFFFu) ||
        !pl755_igual(g[39], 0x0C0D0E0Fu, 0x10111213u, 0x14151617u, 0xC0C0C0C0u) ||
        !pl755_igual(g[25], 0x00010203u, 0x04050607u, 0x08090A0Bu, 0xC0C0C0C0u) ||
        !pl755_igual(g[32], 0, 0xFFFFFFFFu, 0, 0xFFFFFFFFu) || !pl755_igual(g[37], 0, 0, 0xFFFFFFFFu, 0xFFFFFFFFu))
        return 0;
    double esc[3];
    int32_t vies[3];
    for (int c = 0; c < 3; c++) {
        const uint32_t s = g[34]._u32[c];
        if (s < 8u || s > 31u || g[17]._u32[c] != s - 32u) return 0;
        vies[c] = (int32_t)g[31]._u32[c];
        if (vies[c] > (1 << 20) || vies[c] < -(1 << 20)) return 0;
        esc[c] = 1.0;
        for (uint32_t k = s; k < 32u; k++) esc[c] *= 0.5;
    }
    const uint32_t n = (0u - off) / 32u, ent = g[27]._u32[0] - 48u, out = g[36]._u32[0] + off;
    if (g[29]._u32[0] != g[36]._u32[0] + 16u) return 0;
    if ((out & 15u) || ent > SPU_LS_SIZE || ent + 24u * n + 64u > SPU_LS_SIZE || out + 32u * n > SPU_LS_SIZE) return 0;
    if (out < ent + 24u * n + 64u && ent < out + 32u * n) return 0;   /* o original lê duas voltas à frente */
    uint8_t* ls = SPU_LS_BASE(ctx);
    const uint32_t um = __builtin_bswap32(g[35]._u32[3]);
#if defined(__SSE4_1__) || defined(__AVX2__)
    /* um vértice por registro: os 3 u16 big-endian em 3 faixas, + viés, inteiro -> float (exato, |v| < 2^24) e vezes
     * a potência de 2 (exato), w = 1,0, de volta para big-endian */
    {
        const __m128i sel = _mm_setr_epi8(1, 0, -1, -1, 3, 2, -1, -1, 5, 4, -1, -1, -1, -1, -1, -1);
        const __m128i bs = _mm_setr_epi8(3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12);
        const __m128i vi = _mm_setr_epi32(vies[0], vies[1], vies[2], 0);
        const __m128 es = _mm_setr_ps((float)esc[0], (float)esc[1], (float)esc[2], 0.0f);
        const __m128 w1 = _mm_castsi128_ps(_mm_setr_epi32(0, 0, 0, (int)g[35]._u32[3]));
        for (uint32_t i = 0; i < 2u * n; i++) {
            const __m128i u = _mm_shuffle_epi8(_mm_loadu_si128((const __m128i*)(ls + ent + 12u * i)), sel);
            __m128 f = _mm_mul_ps(_mm_cvtepi32_ps(_mm_add_epi32(u, vi)), es);
            f = _mm_blend_ps(f, w1, 8);
            _mm_storeu_si128((__m128i*)(ls + out + 16u * i), _mm_shuffle_epi8(_mm_castps_si128(f), bs));
        }
        g[3] = spu_il(0);
        g[1] = spu_ai(g[1], 224);
        return 1;
    }
#endif
    for (uint32_t i = 0; i < 2u * n; i++) {
        const uint8_t* p = ls + ent + 12u * i;
        uint32_t o[4];
        for (int c = 0; c < 3; c++) {
            const int32_t v = (int32_t)((((uint32_t)p[2 * c] << 8) | p[2 * c + 1]) + (uint32_t)vies[c]);
            const float f = (float)((double)v * esc[c]);   /* exato: |v| < 2^24 */
            uint32_t w; memcpy(&w, &f, 4);
            o[c] = __builtin_bswap32(w);
        }
        o[3] = um;
        memcpy(ls + out + 16u * i, o, 16);
    }
    g[3] = spu_il(0);
    g[1] = spu_ai(g[1], 224);
    return 1;
}
#endif
