/* GOWA (02/11, EDGE nativo, marco E4): três laços da 0x73E0 do job 0x72DF80. Cada nativo faz as voltas 0..n-2 com
 * as mesmas leituras e gravações na LS, na mesma ordem dentro da volta (então sobreposições dão o mesmo resultado),
 * deixa os registradores que passam de uma volta à outra como o laço os deixaria e salta para o rótulo: a última volta
 * fica com o traduzido, que monta os temporários de saída. n < 2 e estados fora do comum voltam 0.
 *   75D8: lista de índices u16 (r80 + r14, um quadword por volta, lido uma volta à frente); o quadword em r8 já está
 *         deslocado 4 bits (shlqbii) e as palavras dele e as rodadas de 16 bits (roti) são 8 endereços: grava r10 em
 *         r81 + cada um. r9/r11/r12 = r8 rodado 4/8/12 bytes; r13 conta as voltas, r14 avança 16.
 *   7670: tabela de deslocamentos: soma corrente r11 (4 faixas, spu_a) dos quadwords em r19/r18/r17/r16 + r9; antes de
 *         somar cada um grava o contador (r15, r14, r13, r12; +64 por volta) em r81 + soma. Sai quando r10 + 64 >= 0.
 *   7828: no lugar (lê 64 bytes à frente): cada quadword com a palavra 3 trocada pela do contador (r12, r11, r10, r9;
 *         +64 por volta), gravado em r23/r24/r22/r21 + r6. Sai quando r7 + 64 = 0.
 * GOWA_EDGE_NATIVO=0 desliga. Ganchos 'antes' dos rótulos, saltando para o próprio rótulo (edge_nativo.py). */
#ifndef GOWA_FN_000073E0_LACOS_H
#define GOWA_FN_000073E0_LACOS_H
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

#define L73_A16(a) ((a) & SPU_LS_MASK & ~0xFu)
static inline int l73_iguais(u128 a, u128 b) { return !memcmp(&a, &b, 16); }
static inline u128 l73_soma(u128 a, int32_t k) { return spu_ai(a, k); }
/* u128 (faixa i = _u32[i]) <-> 16 bytes da LS (big-endian) */
static inline __m128i l73_be(__m128i v) { return _mm_shuffle_epi8(v, _mm_setr_epi8(3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12)); }
static inline __m128i l73_m(u128 a) { __m128i v; memcpy(&v, &a, 16); return v; }
static inline u128 l73_u(__m128i v) { u128 a; memcpy(&a, &v, 16); return a; }

/* ---- 75D8 ---- */
static int fn_000073E0_75d8_corpo(spu_context* ctx)
{
    if (!gowa_edge_nativo_on()) return 0;
    u128* g = ctx->gpr;
    const uint32_t n = g[13]._u32[0];
    if (n < 2u || n > 0x4000u) return 0;
    if (!l73_iguais(g[9], spu_rotqbyi(g[8], 4)) || !l73_iguais(g[11], spu_rotqbyi(g[8], 8)) || !l73_iguais(g[12], spu_rotqbyi(g[8], 12)))
        return 0;
    uint8_t* ls = SPU_LS_BASE(ctx);
    const uint32_t t = g[81]._u32[0], base = g[80]._u32[0];
    uint32_t off = g[14]._u32[0];
    const __m128i V = l73_be(l73_m(g[10]));
    uint32_t c0 = g[8]._u32[0], c1 = g[8]._u32[1], c2 = g[8]._u32[2], c3 = g[8]._u32[3];
    for (uint32_t it = 0; it + 1u < n; it++) {
        uint32_t w[4];
        memcpy(w, ls + L73_A16(base + off), 16);
        const uint32_t n0 = __builtin_bswap32(w[0]), n1 = __builtin_bswap32(w[1]), n2 = __builtin_bswap32(w[2]), n3 = __builtin_bswap32(w[3]);
        const uint32_t e[8] = { c0, c1, c2, c3, (c0 << 16) | (c0 >> 16), (c1 << 16) | (c1 >> 16), (c2 << 16) | (c2 >> 16), (c3 << 16) | (c3 >> 16) };
        for (int k = 0; k < 8; k++) _mm_storeu_si128((__m128i*)(ls + L73_A16(t + e[k])), V);
        off += 16u;
        c0 = (n0 << 4) | (n1 >> 28); c1 = (n1 << 4) | (n2 >> 28); c2 = (n2 << 4) | (n3 >> 28); c3 = n3 << 4;
    }
    u128 r8; r8._u32[0] = c0; r8._u32[1] = c1; r8._u32[2] = c2; r8._u32[3] = c3;
    g[8] = r8;
    g[9] = spu_rotqbyi(r8, 4);
    g[11] = spu_rotqbyi(r8, 8);
    g[12] = spu_rotqbyi(r8, 12);
    g[13] = l73_soma(g[13], -(int32_t)(n - 1u));
    g[14] = l73_soma(g[14], (int32_t)(16u * (n - 1u)));
    return 1;
}
static __attribute__((noinline)) int fn_000073E0_75d8_nativo(spu_context* ctx)
{
    static volatile long nn[2];
    const int r = fn_000073E0_75d8_corpo(ctx);
    gowa_edge_conta("73E0/75D8", nn, r);
    return r;
}

/* ---- 7670 ---- */
static int fn_000073E0_7670_corpo(spu_context* ctx)
{
    if (!gowa_edge_nativo_on()) return 0;
    u128* g = ctx->gpr;
    if (!l73_iguais(g[9], g[10])) return 0;
    const int32_t x = (int32_t)g[10]._u32[0];
    if (x >= -64) return 0;                                   /* uma volta só */
    const uint32_t n = (uint32_t)((-(int64_t)x + 63) / 64);
    if (n > 0x2000u) return 0;
    uint8_t* ls = SPU_LS_BASE(ctx);
    const uint32_t t = g[81]._u32[0], a0 = g[19]._u32[0], a1 = g[18]._u32[0], a2 = g[17]._u32[0], a3 = g[16]._u32[0];
    uint32_t off = g[9]._u32[0];
    __m128i s = l73_m(g[11]);
    __m128i k0 = l73_m(g[15]), k1 = l73_m(g[14]), k2 = l73_m(g[13]), k3 = l73_m(g[12]);
    const __m128i d64 = _mm_set1_epi32(64);
    for (uint32_t it = 0; it + 1u < n; it++) {
        const __m128i q0 = l73_be(_mm_loadu_si128((const __m128i*)(ls + L73_A16(a0 + off))));
        const __m128i q1 = l73_be(_mm_loadu_si128((const __m128i*)(ls + L73_A16(a1 + off))));
        const __m128i q2 = l73_be(_mm_loadu_si128((const __m128i*)(ls + L73_A16(a2 + off))));
        const __m128i q3 = l73_be(_mm_loadu_si128((const __m128i*)(ls + L73_A16(a3 + off))));
        _mm_storeu_si128((__m128i*)(ls + L73_A16(t + (uint32_t)_mm_cvtsi128_si32(s))), l73_be(k0));
        s = _mm_add_epi32(s, q0);
        _mm_storeu_si128((__m128i*)(ls + L73_A16(t + (uint32_t)_mm_cvtsi128_si32(s))), l73_be(k1));
        s = _mm_add_epi32(s, q1);
        _mm_storeu_si128((__m128i*)(ls + L73_A16(t + (uint32_t)_mm_cvtsi128_si32(s))), l73_be(k2));
        s = _mm_add_epi32(s, q2);
        _mm_storeu_si128((__m128i*)(ls + L73_A16(t + (uint32_t)_mm_cvtsi128_si32(s))), l73_be(k3));
        s = _mm_add_epi32(s, q3);
        k0 = _mm_add_epi32(k0, d64); k1 = _mm_add_epi32(k1, d64); k2 = _mm_add_epi32(k2, d64); k3 = _mm_add_epi32(k3, d64);
        off += 64u;
    }
    g[11] = l73_u(s);
    g[15] = l73_u(k0); g[14] = l73_u(k1); g[13] = l73_u(k2); g[12] = l73_u(k3);
    g[10] = l73_soma(g[10], (int32_t)(64u * (n - 1u)));
    g[9] = g[10];
    return 1;
}
static __attribute__((noinline)) int fn_000073E0_7670_nativo(spu_context* ctx)
{
    static volatile long nn[2];
    const int r = fn_000073E0_7670_corpo(ctx);
    gowa_edge_conta("73E0/7670", nn, r);
    return r;
}

/* ---- 7828 ---- */
static int fn_000073E0_7828_corpo(spu_context* ctx)
{
    if (!gowa_edge_nativo_on()) return 0;
    u128* g = ctx->gpr;
    if (g[8]._u32[0] || g[8]._u32[1] || g[8]._u32[2] || g[8]._u32[3] != 0xFFFFFFFFu || !l73_iguais(g[6], g[7])) return 0;
    const int32_t x = (int32_t)g[7]._u32[0];
    if (x >= -64 || (x & 63)) return 0;                      /* uma volta só, ou não chega a 0 */
    const uint32_t n = (uint32_t)(-(int64_t)x / 64);
    if (n > 0x2000u) return 0;
    uint8_t* ls = SPU_LS_BASE(ctx);
    const uint32_t lr[4] = { g[20]._u32[0], g[19]._u32[0], g[14]._u32[0], g[13]._u32[0] };
    const uint32_t gr[4] = { g[23]._u32[0], g[24]._u32[0], g[22]._u32[0], g[21]._u32[0] };
    __m128i p[4] = { l73_be(l73_m(g[17])), l73_be(l73_m(g[16])), l73_be(l73_m(g[15])), l73_be(l73_m(g[18])) };   /* bytes da LS */
    uint32_t c[4] = { g[12]._u32[3], g[11]._u32[3], g[10]._u32[3], g[9]._u32[3] };
    uint32_t off = g[6]._u32[0];
    for (uint32_t it = 0; it + 1u < n; it++) {
        __m128i o[4];
        for (int j = 0; j < 4; j++) {
            o[j] = _mm_insert_epi32(p[j], (int)__builtin_bswap32(c[j]), 3);
            p[j] = _mm_loadu_si128((const __m128i*)(ls + L73_A16(lr[j] + off)));
        }
        for (int j = 0; j < 4; j++) _mm_storeu_si128((__m128i*)(ls + L73_A16(gr[j] + off)), o[j]);
        for (int j = 0; j < 4; j++) c[j] += 64u;
        off += 64u;
    }
    g[17] = l73_u(l73_be(p[0])); g[16] = l73_u(l73_be(p[1])); g[15] = l73_u(l73_be(p[2])); g[18] = l73_u(l73_be(p[3]));
    const int32_t dk = (int32_t)(64u * (n - 1u));
    g[12] = l73_soma(g[12], dk); g[11] = l73_soma(g[11], dk); g[10] = l73_soma(g[10], dk); g[9] = l73_soma(g[9], dk);
    g[7] = l73_soma(g[7], dk);
    g[6] = g[7];
    return 1;
}
static __attribute__((noinline)) int fn_000073E0_7828_nativo(spu_context* ctx)
{
    static volatile long nn[2];
    const int r = fn_000073E0_7828_corpo(ctx);
    gowa_edge_conta("73E0/7828", nn, r);
    return r;
}
#undef L73_A16
#endif
