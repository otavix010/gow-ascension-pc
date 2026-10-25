/* GOWA (21/10): as contas de float da SPU em 512 bits pelo CAMINHO EXATO, sem chamada --
 * para os núcleos vk consertarem as palavras de alcance estendido/saturado ali mesmo (uma
 * chamada, até fria e nunca feita, derrama os zmm do laço inteiro no ABI do Windows).
 *
 * Todo float da SPU (expoente 255 incluso: binade comum, até (2 - 2^-23) * 2^128; expoente 0
 * = zero) cabe exato em double. Produto de duas mantissas de 24 bits: exato em 53 bits.
 * Conta em double com arredondamento para zero ({rz-sae}): o resultado exato truncado a 53
 * bits; depois cortado a 24 bits = o exato truncado a 24 bits direto (truncar duas vezes, a
 * segunda mais curta, é truncar uma vez). Volta: expoente acima de 255 satura em +-Smax
 * (0x7FFFFFFF com o sinal), abaixo de 1 vira +0 (todo zero da SPU é +0). Os expoentes ficam
 * entre -252 e 258: nada de denormal nem infinito em double, o DAZ/FTZ não interfere.
 * Conferido palavra a palavra contra spu__fa_any/spu__fm_any/spu__fma_any
 * (out/spudiff/teste_vk_exato.c). */
#ifndef SPU_VK_EXATO_H
#define SPU_VK_EXATO_H
#include <immintrin.h>
#if !defined(__AVX512F__) && defined(GOWA_VK_AVX2)
/* GOWA (24/10): sem AVX-512 -- a mesma conta em 4 doubles por vez (__m256d), com o
 * arredondamento para zero vindo do MXCSR da thread SPU (SPU_SX_RZ) em vez do {rz-sae}. */
static inline __m256d vkx__d4(__m128i w) {      /* 4 palavras SPU -> 4 doubles exatos */
    const __m256i u = _mm256_cvtepu32_epi64(w);
    const __m256i e = _mm256_and_si256(_mm256_srli_epi64(u, 23), _mm256_set1_epi64x(0xFF));
    const __m256i s = _mm256_slli_epi64(_mm256_srli_epi64(u, 31), 63);
    const __m256i f = _mm256_slli_epi64(_mm256_and_si256(u, _mm256_set1_epi64x(0x7FFFFF)), 29);
    const __m256i de = _mm256_slli_epi64(_mm256_add_epi64(e, _mm256_set1_epi64x(1023 - 127)), 52);
    const __m256i d = _mm256_or_si256(_mm256_or_si256(s, de), f);
    return _mm256_castsi256_pd(_mm256_andnot_si256(_mm256_cmpeq_epi64(e, _mm256_setzero_si256()), d));
}
static inline __m128i vkx__s4(__m256d x) {      /* 4 doubles -> 4 palavras SPU */
    const __m256i u = _mm256_castpd_si256(x);
    const __m256i be = _mm256_sub_epi64(_mm256_and_si256(_mm256_srli_epi64(u, 52), _mm256_set1_epi64x(0x7FF)),
                                        _mm256_set1_epi64x(1023 - 127));
    const __m256i s = _mm256_and_si256(_mm256_srli_epi64(u, 32), _mm256_set1_epi64x(0x80000000));
    const __m256i f = _mm256_and_si256(_mm256_srli_epi64(u, 29), _mm256_set1_epi64x(0x7FFFFF));
    __m256i r = _mm256_or_si256(_mm256_or_si256(s, _mm256_slli_epi64(be, 23)), f);
    r = _mm256_blendv_epi8(r, _mm256_or_si256(s, _mm256_set1_epi64x(0x7FFFFFFF)), _mm256_cmpgt_epi64(be, _mm256_set1_epi64x(255)));
    r = _mm256_and_si256(r, _mm256_cmpgt_epi64(be, _mm256_setzero_si256()));
    return _mm256_castsi256_si128(_mm256_permutevar8x32_epi32(r, _mm256_setr_epi32(0, 2, 4, 6, 0, 2, 4, 6)));
}
#define VKX_OP2(nome, expr) \
    static inline vk nome(vk a, vk b) { \
        __m256d x, y; __m128i o[4]; \
        for (int g = 0; g < 4; g++) { x = vkx__d4(vk_x(a, g)); y = vkx__d4(vk_x(b, g)); o[g] = vkx__s4(expr); } \
        return vk_joinx(o[0], o[1], o[2], o[3]); }
#define VKX_OP3(nome, expr) \
    static inline vk nome(vk a, vk b, vk c) { \
        __m256d x, y, z; __m128i o[4]; \
        for (int g = 0; g < 4; g++) { x = vkx__d4(vk_x(a, g)); y = vkx__d4(vk_x(b, g)); z = vkx__d4(vk_x(c, g)); o[g] = vkx__s4(expr); } \
        return vk_joinx(o[0], o[1], o[2], o[3]); }
VKX_OP2(vkx_fa, _mm256_add_pd(x, y))
VKX_OP2(vkx_fs, _mm256_sub_pd(x, y))
VKX_OP2(vkx_fm, _mm256_mul_pd(x, y))
VKX_OP3(vkx_fma, _mm256_fmadd_pd(x, y, z))
VKX_OP3(vkx_fms, _mm256_fmsub_pd(x, y, z))
VKX_OP3(vkx_fnms, _mm256_fnmadd_pd(x, y, z))
#undef VKX_OP2
#undef VKX_OP3
#else

#define VKX_RZ (_MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC)

/* 8 palavras SPU (u32) -> 8 doubles exatos */
static inline __m512d vkx__d(__m256i w) {
    const __m512i u = _mm512_cvtepu32_epi64(w);
    const __m512i e = _mm512_and_si512(_mm512_srli_epi64(u, 23), _mm512_set1_epi64(0xFF));
    const __m512i s = _mm512_slli_epi64(_mm512_srli_epi64(u, 31), 63);
    const __m512i f = _mm512_slli_epi64(_mm512_and_si512(u, _mm512_set1_epi64(0x7FFFFF)), 29);
    const __m512i de = _mm512_slli_epi64(_mm512_add_epi64(e, _mm512_set1_epi64(1023 - 127)), 52);
    const __m512i d = _mm512_or_si512(_mm512_or_si512(s, de), f);
    return _mm512_castsi512_pd(_mm512_maskz_mov_epi64(_mm512_test_epi64_mask(e, e), d));
}
/* 8 doubles (finitos) -> 8 palavras SPU: truncado a 24 bits, satura, +0 abaixo de Smin */
static inline __m256i vkx__s(__m512d x) {
    const __m512i u = _mm512_castpd_si512(x);
    const __m512i be = _mm512_sub_epi64(_mm512_and_si512(_mm512_srli_epi64(u, 52), _mm512_set1_epi64(0x7FF)),
                                        _mm512_set1_epi64(1023 - 127));
    const __m512i s = _mm512_and_si512(_mm512_srli_epi64(u, 32), _mm512_set1_epi64(0x80000000));
    const __m512i f = _mm512_and_si512(_mm512_srli_epi64(u, 29), _mm512_set1_epi64(0x7FFFFF));
    __m512i r = _mm512_or_si512(_mm512_or_si512(s, _mm512_slli_epi64(be, 23)), f);
    r = _mm512_mask_mov_epi64(r, _mm512_cmpgt_epi64_mask(be, _mm512_set1_epi64(255)),
                              _mm512_or_si512(s, _mm512_set1_epi64(0x7FFFFFFF)));
    r = _mm512_maskz_mov_epi64(_mm512_cmpgt_epi64_mask(be, _mm512_setzero_si512()), r);
    return _mm512_cvtepi64_epi32(r);
}
static inline __m256i vkx__lo(__m512i v) { return _mm512_castsi512_si256(v); }
static inline __m256i vkx__hi(__m512i v) { return _mm512_extracti64x4_epi64(v, 1); }
static inline __m512i vkx__junta(__m256i lo, __m256i hi) { return _mm512_inserti64x4(_mm512_castsi256_si512(lo), hi, 1); }

#define VKX_OP2(nome, expr) \
    static inline __m512i nome(__m512i a, __m512i b) { \
        __m512d x, y; \
        x = vkx__d(vkx__lo(a)); y = vkx__d(vkx__lo(b)); const __m256i lo = vkx__s(expr); \
        x = vkx__d(vkx__hi(a)); y = vkx__d(vkx__hi(b)); const __m256i hi = vkx__s(expr); \
        return vkx__junta(lo, hi); }
#define VKX_OP3(nome, expr) \
    static inline __m512i nome(__m512i a, __m512i b, __m512i c) { \
        __m512d x, y, z; \
        x = vkx__d(vkx__lo(a)); y = vkx__d(vkx__lo(b)); z = vkx__d(vkx__lo(c)); const __m256i lo = vkx__s(expr); \
        x = vkx__d(vkx__hi(a)); y = vkx__d(vkx__hi(b)); z = vkx__d(vkx__hi(c)); const __m256i hi = vkx__s(expr); \
        return vkx__junta(lo, hi); }
VKX_OP2(vkx_fa, _mm512_add_round_pd(x, y, VKX_RZ))
VKX_OP2(vkx_fs, _mm512_sub_round_pd(x, y, VKX_RZ))
VKX_OP2(vkx_fm, _mm512_mul_round_pd(x, y, VKX_RZ))
VKX_OP3(vkx_fma, _mm512_fmadd_round_pd(x, y, z, VKX_RZ))
VKX_OP3(vkx_fms, _mm512_fmsub_round_pd(x, y, z, VKX_RZ))
VKX_OP3(vkx_fnms, _mm512_fnmadd_round_pd(x, y, z, VKX_RZ))
#undef VKX_OP2
#undef VKX_OP3
#endif /* !__AVX512F__ && GOWA_VK_AVX2 */
#endif
