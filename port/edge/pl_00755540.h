/* GOWA (30/10, EDGE nativo, marco E3): o laço 0x30310 do plugin 0x755540 (posições comprimidas -> float4).
 *
 * Cada vértice ocupa 12 bytes no fluxo (9 no 0x754B80; a partir de r27 - 4 * passo: o laço lê 2 voltas à frente) e a posição são os 3
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

static inline int pl755_igual(u128 a, uint32_t w0, uint32_t w1, uint32_t w2, uint32_t w3)
{
    return a._u32[0] == w0 && a._u32[1] == w1 && a._u32[2] == w2 && a._u32[3] == w3;
}

/* os nv vértices: os 3 u16 big-endian + viés, inteiro -> float (exato, |v| < 2^24) vezes a potência de 2 (exato), w =
 * a palavra wbits; passo = bytes por vértice na entrada */
static void pl755_escreve(uint8_t* ls, uint32_t ent, uint32_t out, uint32_t nv, uint32_t passo, const int32_t* vies,
                          const double* esc, uint32_t wbits)
{
#if defined(__SSE4_1__) || defined(__AVX2__)
    const __m128i sel = _mm_setr_epi8(1, 0, -1, -1, 3, 2, -1, -1, 5, 4, -1, -1, -1, -1, -1, -1);
    const __m128i bs = _mm_setr_epi8(3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12);
    const __m128i vi = _mm_setr_epi32(vies[0], vies[1], vies[2], 0);
    const __m128 es = _mm_setr_ps((float)esc[0], (float)esc[1], (float)esc[2], 0.0f);
    const __m128 w1 = _mm_castsi128_ps(_mm_setr_epi32(0, 0, 0, (int)wbits));
    for (uint32_t i = 0; i < nv; i++) {
        const __m128i u = _mm_shuffle_epi8(_mm_loadu_si128((const __m128i*)(ls + ent + passo * i)), sel);
        __m128 f = _mm_mul_ps(_mm_cvtepi32_ps(_mm_add_epi32(u, vi)), es);
        f = _mm_blend_ps(f, w1, 8);
        _mm_storeu_si128((__m128i*)(ls + out + 16u * i), _mm_shuffle_epi8(_mm_castps_si128(f), bs));
    }
#else
    const uint32_t um = __builtin_bswap32(wbits);
    for (uint32_t i = 0; i < nv; i++) {
        const uint8_t* p = ls + ent + passo * i;
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
#endif
}

/* passo = bytes por vértice no fluxo (12 no 0x755540, 9 no 0x754B80: o mesmo laço, com o controle do vértice B do par
 * em r39 = os bytes passo..passo+11 e a entrada duas voltas atrás de r27) */
static int pl755_laco(spu_context* ctx, uint32_t passo)
{
    if (!gowa_edge_nativo_on()) return 0;
    u128* g = ctx->gpr;
    const uint32_t off = g[19]._u32[0];
    if (!off || (off & 31u) || (0u - off) > 0x20000u || g[18]._u32[0] != off) return 0;
    /* o formato das fotos: 12 bytes por vértice, 3 x u16 no começo, máscara 0xFFFF, w = 1,0 */
    if (!pl755_igual(g[23], 0xFFFFFFD0u, 0xFFFFFFB7u, 0, 0) || !pl755_igual(g[24], 0xFFFFFFF0u, 0xFFFFFFE0u, 0xFFFFFFD0u, 0xFFFFFFB7u) ||
        !pl755_igual(g[26], 0xFFFFFFE0u, 0xFFFFFFD0u, 0xFFFFFFB7u, 0) || !pl755_igual(g[28], 0xFFFFFFB7u, 0, 0, 0) ||
        !pl755_igual(g[33], 0xFFFFu, 0xFFFFu, 0xFFFFu, 0) || !pl755_igual(g[38], 0, 0, 0, 0xFFFFFFFFu) ||
        !pl755_igual(g[39], passo * 0x01010101u + 0x00010203u, passo * 0x01010101u + 0x04050607u, passo * 0x01010101u + 0x08090A0Bu, 0xC0C0C0C0u) ||
        !pl755_igual(g[25], 0x00010203u, 0x04050607u, 0x08090A0Bu, 0xC0C0C0C0u) ||
        !pl755_igual(g[32], 0, 0xFFFFFFFFu, 0, 0xFFFFFFFFu) || !pl755_igual(g[37], 0, 0, 0xFFFFFFFFu, 0xFFFFFFFFu))
        return 0;
    double esc[3];
    int32_t vies[3];
    for (int c = 0; c < 3; c++) {
        const uint32_t s = g[34]._u32[c];
        if (s < 8u || s > 31u || g[17]._u32[c] != s - 32u) return 0;
        vies[c] = (int32_t)g[31]._u32[c];
        if (vies[c] >= (1 << 23) || vies[c] <= -(1 << 23)) return 0;
        esc[c] = 1.0;
        for (uint32_t k = s; k < 32u; k++) esc[c] *= 0.5;
    }
    const uint32_t n = (0u - off) / 32u, ent = g[27]._u32[0] - 4u * passo, out = g[36]._u32[0] + off, tam = 2u * passo * n + 4u * passo + 48u;
    if (g[29]._u32[0] != g[36]._u32[0] + 16u) return 0;
    if ((out & 15u) || ent > SPU_LS_SIZE || ent + tam > SPU_LS_SIZE || out + 32u * n > SPU_LS_SIZE) return 0;
    if (out < ent + tam && ent < out + 32u * n) return 0;   /* o original lê duas voltas à frente */
    pl755_escreve(SPU_LS_BASE(ctx), ent, out, 2u * n, passo, vies, esc, g[35]._u32[3]);
    g[3] = spu_il(0);
    g[1] = spu_ai(g[1], 224);
    return 1;
}

/* 02/11: o plugin inteiro, da entrada 0x30014 (o job chama direto nela: o 0x30000 nunca roda; quem chama já deixou
 * r29 = 17, r25 = 19, r38 = 18, r75 = o quadword em r6 + 0x10 e o r1 em r1 - 0xE0). A preparação, no formato das
 * gravações: o descritor (r6, sem alinhamento) dá por componente c a largura do campo (bytes 16 + 2c e 17 + 2c,
 * inteiros + fracionários; só 16 = os 3 u16) e s = 32 - fracionários, e o viés nas palavras 0, 4 e 8; o vértice vai
 * de r4 com o passo; n = r5. Refaz: a tabela de constantes do plugin no quadro (r1 - 224 + 0x20..0xD0), o atributo 1
 * no primeiro zero da lista (r3 + 0x160) e o ponteiro daquela vaga na palavra 3 de r3 + 0x50; a saída é esse ponteiro,
 * 16 bytes por vértice, os vértices aos pares. Sai com r3 = 0. */
static inline uint8_t pl755_byte(const uint8_t* ls, uint32_t a) { return ls[a & SPU_LS_MASK]; }
static inline uint32_t pl755_palavra(const uint8_t* ls, uint32_t a)   /* lqd + rotqby: a palavra dá a volta no quadword */
{
    const uint32_t q = a & SPU_LS_MASK & ~15u;
    uint32_t w = 0;
    for (uint32_t i = 0; i < 4; i++) w = (w << 8) | ls[q + ((a + i) & 15u)];
    return w;
}
static int pl755_inteiro(spu_context* ctx, uint32_t passo)
{
    if (!gowa_edge_nativo_on()) return 0;
    u128* g = ctx->gpr;
    uint8_t* ls = SPU_LS_BASE(ctx);
    const uint32_t r1 = g[1]._u32[0] & SPU_LS_MASK, r3 = g[3]._u32[0] & SPU_LS_MASK, r4 = g[4]._u32[0] & SPU_LS_MASK,
                   n = g[5]._u32[0], r6 = g[6]._u32[0];
    if (g[29]._u32[0] != 17u || g[25]._u32[0] != 19u || g[38]._u32[0] != 18u || (r3 & 15u) || r3 + 0x170u > SPU_LS_SIZE ||
        r1 < 0x200u || !n || n > 4096u)
        return 0;
    { const u128 q = spu_ls_read128(ctx, r6 + 0x10u); if (memcmp(&q, &g[75], 16)) return 0; }
    int32_t vies[3];
    double esc[3];
    for (uint32_t c = 0; c < 3; c++) {
        const uint32_t fr = pl755_byte(ls, r6 + 17u + 2u * c);
        if (pl755_byte(ls, r6 + 16u + 2u * c) + fr != 16u || fr < 1u || fr > 24u) return 0;
        vies[c] = (int32_t)pl755_palavra(ls, r6 + 4u * c);
        if (vies[c] >= (1 << 23) || vies[c] <= -(1 << 23)) return 0;
        esc[c] = 1.0;
        for (uint32_t k = 0; k < fr; k++) esc[c] *= 0.5;
    }
    int k = 0;
    while (k < 16 && ls[r3 + 0x160u + (uint32_t)k]) k++;
    if (k == 16) return 0;
    const uint32_t ptr = pl755_palavra(ls, r3 + 16u + 4u * (uint32_t)k), out = ptr & SPU_LS_MASK;
    const uint32_t nv = (n + 1u) & ~1u, tam = passo * nv + 4u * passo + 64u, fq = (r1 - 224u) & ~15u;
    if ((out & 15u) || out + 16u * nv > SPU_LS_SIZE || r4 + tam > SPU_LS_SIZE) return 0;
    if ((out < r4 + tam && r4 < out + 16u * nv) || (out < r1 && fq < out + 16u * nv) || (out < r3 + 0x170u && r3 < out + 16u * nv))
        return 0;
    /* a tabela de constantes no quadro (a de 0x30420 não vai) */
    static const uint16_t de[12] = { 0x400, 0x410, 0x430, 0x440, 0x450, 0x460, 0x470, 0x480, 0x490, 0x4A0, 0x4B0, 0x4C0 };
    for (uint32_t i = 0; i < 12; i++)
        spu_ls_write128(ctx, fq + 0x20u + 16u * i, spu_ls_read128(ctx, (uint32_t)(0x30000 + de[i] + g_spu_hot_reloc)));
    const uint32_t wbits = spu_ls_read128(ctx, (uint32_t)(0x304D0 + g_spu_hot_reloc))._u32[3];
    ls[r3 + 0x160u + (uint32_t)k] = 1;
    ls[r3 + 0x5C] = (uint8_t)(ptr >> 24); ls[r3 + 0x5D] = (uint8_t)(ptr >> 16); ls[r3 + 0x5E] = (uint8_t)(ptr >> 8); ls[r3 + 0x5F] = (uint8_t)ptr;
    pl755_escreve(ls, r4, out, nv, passo, vies, esc, wbits);
    g[3] = spu_il(0);
    return 1;
}

static __attribute__((noinline)) int pl_00755540_nativo(spu_context* ctx)
{
    static volatile long n[2];
    const int r = pl755_laco(ctx, 12u);
    gowa_edge_conta("pl755", n, r);
    return r;
}

/* 02/11: o 0x754B80 (vértice de 9 bytes: os 3 u16 da posição + 3 bytes que este plugin não usa) */
static __attribute__((noinline)) int pl_00754B80_nativo(spu_context* ctx)
{
    static volatile long n[2];
    const int r = pl755_laco(ctx, 9u);
    gowa_edge_conta("pl754B80", n, r);
    return r;
}
static __attribute__((noinline)) int pl_00755540_inteiro(spu_context* ctx)
{
    static volatile long n[2];
    const int r = pl755_inteiro(ctx, 12u);
    gowa_edge_conta("pl755i", n, r);
    return r;
}

static __attribute__((noinline)) int pl_00754B80_inteiro(spu_context* ctx)
{
    static volatile long n[2];
    const int r = pl755_inteiro(ctx, 9u);
    gowa_edge_conta("pl754B80i", n, r);
    return r;
}
#endif
