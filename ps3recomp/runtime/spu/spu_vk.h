/* GOWA: operações SPU em 512 bits -- quatro iterações de um laço traduzido
 * por instrução (cada faixa de 128 bits do zmm é o registrador SPU de uma
 * iteração, no mesmo leiaute de bytes do u128). Usadas pelos núcleos gerados
 * por scripts/vetorizar_laco.py.
 *
 * Cada operação dá, faixa por faixa, os mesmos bits do auxiliar de 128 bits
 * (spu_helpers.h, caminho AVX-512 + DAZ), com uma diferença: as contas de
 * float não corrigem o resultado de alcance estendido/saturado na hora --
 * acumulam a máscara em *bad; o grupo marcado é refeito pela transcrição literal
 * (o caminho exato). Conferido contra os auxiliares em out/spudiff/teste_vk.c. */
#ifndef SPU_VK_H
#define SPU_VK_H
#if !defined(__AVX512F__) && defined(GOWA_VK_AVX2)
/* GOWA (24/10): sem AVX-512 (x86-64-v3 + SPU_SX_RZ): a mesma API em 2 x 256 bits */
#include "spu_vk_avx2.h"
#else
#include <immintrin.h>
#include <stdio.h>
#include <stdlib.h>
#include "spu_helpers.h"

typedef __m512i vk;
#define VK_RZ (_MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC)

static inline vk vk_bc(u128 v) { __m128i x; memcpy(&x, &v, 16); return _mm512_broadcast_i32x4(x); }
static inline u128 vk_lane(vk v, int l) {
    __m128i x;
    switch (l) {
    case 0: x = _mm512_extracti32x4_epi32(v, 0); break;
    case 1: x = _mm512_extracti32x4_epi32(v, 1); break;
    case 2: x = _mm512_extracti32x4_epi32(v, 2); break;
    default: x = _mm512_extracti32x4_epi32(v, 3); break;
    }
    u128 r; memcpy(&r, &x, 16); return r;
}
static inline vk vk_join(u128 a, u128 b, u128 c, u128 d) {
    __m128i x0, x1, x2, x3; memcpy(&x0, &a, 16); memcpy(&x1, &b, 16); memcpy(&x2, &c, 16); memcpy(&x3, &d, 16);
    const __m256i lo = _mm256_inserti128_si256(_mm256_castsi128_si256(x0), x1, 1);
    const __m256i hi = _mm256_inserti128_si256(_mm256_castsi128_si256(x2), x3, 1);
    return _mm512_inserti64x4(_mm512_castsi256_si512(lo), hi, 1);
}
/* valor da iteração anterior: faixas [prev.3, cur.0, cur.1, cur.2] */
static inline vk vk_prev(vk cur, vk prev) { return _mm512_alignr_epi64(cur, prev, 6); }
/* o mesmo com só a última iteração do grupo anterior guardada (q = faixa 3) */
static inline vk vk_prevq(vk cur, __m128i q) { return _mm512_alignr_epi64(cur, _mm512_broadcast_i32x4(q), 6); }
static inline __m128i vk_x(vk v, int l) {
    switch (l) {
    case 0: return _mm512_castsi512_si128(v);
    case 1: return _mm512_extracti32x4_epi32(v, 1);
    case 2: return _mm512_extracti32x4_epi32(v, 2);
    default: return _mm512_extracti32x4_epi32(v, 3);
    }
}
static inline vk vk_joinx(__m128i a, __m128i b, __m128i c, __m128i d) {
    const __m256i lo = _mm256_inserti128_si256(_mm256_castsi128_si256(a), b, 1);
    const __m256i hi = _mm256_inserti128_si256(_mm256_castsi128_si256(c), d, 1);
    return _mm512_inserti64x4(_mm512_castsi256_si512(lo), hi, 1);
}
/* transposição 4x4 de faixas de 128 bits: r_j.faixa_i = (a,b,c,d)_i.faixa_j */
static inline void vk_t4(vk a, vk b, vk c, vk d, vk* r0, vk* r1, vk* r2, vk* r3) {
    const vk t0 = _mm512_shuffle_i64x2(a, b, 0x44), t1 = _mm512_shuffle_i64x2(a, b, 0xEE);
    const vk t2 = _mm512_shuffle_i64x2(c, d, 0x44), t3 = _mm512_shuffle_i64x2(c, d, 0xEE);
    *r0 = _mm512_shuffle_i64x2(t0, t2, 0x88); *r1 = _mm512_shuffle_i64x2(t0, t2, 0xDD);
    *r2 = _mm512_shuffle_i64x2(t1, t3, 0x88); *r3 = _mm512_shuffle_i64x2(t1, t3, 0xDD);
}
/* indução: faixa L = base + s*(L+1) em cada palavra (ai acumulado por iteração) */
static inline vk vk_passo(__m128i base, int32_t s) {
    return _mm512_add_epi32(_mm512_broadcast_i32x4(base),
                            _mm512_setr_epi32(s, s, s, s, 2 * s, 2 * s, 2 * s, 2 * s, 3 * s, 3 * s, 3 * s, 3 * s, 4 * s, 4 * s, 4 * s, 4 * s));
}
static inline __m128i vk_u2x(u128 v) { __m128i x; memcpy(&x, &v, 16); return x; }
static inline u128 vk_x2u(__m128i x) { u128 v; memcpy(&v, &x, 16); return v; }
/* LS: cada palavra de 32 bits é big-endian (spu_ls_read128/write128 com SPU_LS_FAST) */
static inline vk vk__bswap32(vk v) {
    return _mm512_shuffle_epi8(v, _mm512_broadcast_i32x4(_mm_setr_epi8(3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12)));
}
/* quatro leituras de 16 bytes (endereços já mascarados) -> uma por faixa */
static inline vk vk_ld4(const uint8_t* ls, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    const __m256i lo = _mm256_inserti128_si256(_mm256_castsi128_si256(_mm_load_si128((const __m128i*)(ls + a0))),
                                               _mm_load_si128((const __m128i*)(ls + a1)), 1);
    const __m256i hi = _mm256_inserti128_si256(_mm256_castsi128_si256(_mm_load_si128((const __m128i*)(ls + a2))),
                                               _mm_load_si128((const __m128i*)(ls + a3)), 1);
    return vk__bswap32(_mm512_inserti64x4(_mm512_castsi256_si512(lo), hi, 1));
}
/* preferred slot (_u32[0]) de cada faixa */
static inline uint32_t vk_slot(vk v, int l) { return vk_lane(v, l)._u32[0]; }
/* quatro leituras de 16 bytes com os endereços (já mascarados) na palavra 0 de cada
 * faixa de a (scripts/vetorizar_chamadas.py) */
static inline vk vk_ld4a(const uint8_t* ls, vk a) {
    const uint32_t a0 = (uint32_t)_mm_cvtsi128_si32(_mm512_castsi512_si128(a));
    const uint32_t a1 = (uint32_t)_mm_cvtsi128_si32(_mm512_extracti32x4_epi32(a, 1));
    const uint32_t a2 = (uint32_t)_mm_cvtsi128_si32(_mm512_extracti32x4_epi32(a, 2));
    const uint32_t a3 = (uint32_t)_mm_cvtsi128_si32(_mm512_extracti32x4_epi32(a, 3));
    return vk_ld4(ls, a0, a1, a2, a3);
}

/* ---- float (truncamento, DAZ, saída achatada: expoente 0 -> +0) ---- */
#if defined(SPU_SX_DAZ)
static inline __m512 vk__in(vk x) { return _mm512_castsi512_ps(x); }
#else
static inline __m512 vk__in(vk x) { return _mm512_castsi512_ps(_mm512_maskz_mov_epi32(_mm512_test_epi32_mask(x, _mm512_set1_epi32(0x7F800000)), x)); }
#endif
/* saída: máscara das palavras de alcance estendido/saturado (|r| >= 0x7F7FFFFF;
 * com o sinal tirado os dois lados são >= 0, então a comparação com sinal > 0x7F7FFFFE
 * é a mesma e sai numa instrução -- o cmpge sem sinal virava vpmaxud + vpcmpeqd) e,
 * se não for cru, o expoente 0 achatado para +0. Cru: valor que só vai para outra
 * conta de float, comparação de float ou cflts/cfltu (a mesma regra do
 * scripts/flush_preguicoso.py) -- com DAZ o denormal é lido como zero. */
static inline vk vk__outc(__m512 r, __mmask16* bad, int cru) {
    const vk ri = _mm512_castps_si512(r);
    *bad |= _mm512_cmpgt_epi32_mask(_mm512_and_si512(ri, _mm512_set1_epi32(0x7FFFFFFF)), _mm512_set1_epi32(0x7F7FFFFE));
    return cru ? ri : _mm512_maskz_mov_epi32(_mm512_test_epi32_mask(ri, _mm512_set1_epi32(0x7F800000)), ri);
}
static inline vk vk__out(__m512 r, __mmask16* bad) { return vk__outc(r, bad, 0); }
#define VK_NEG(x, s) _mm512_xor_si512((x), _mm512_set1_epi32((int)(s)))
/* versões rápidas: resultado + máscara das palavras de alcance estendido/saturado */
static inline vk vk__fa_mc(vk a, vk b, __mmask16* m, int cru)  { return vk__outc(_mm512_add_round_ps(vk__in(a), vk__in(b), VK_RZ), m, cru); }
static inline vk vk__fs_mc(vk a, vk b, __mmask16* m, int cru)  { return vk__outc(_mm512_add_round_ps(vk__in(a), vk__in(VK_NEG(b, 0x80000000u)), VK_RZ), m, cru); }
static inline vk vk__fm_mc(vk a, vk b, __mmask16* m, int cru)  { return vk__outc(_mm512_mul_round_ps(vk__in(a), vk__in(b), VK_RZ), m, cru); }
static inline vk vk__fma_mc(vk a, vk b, vk c, __mmask16* m, int cru)  { return vk__outc(_mm512_fmadd_round_ps(vk__in(a), vk__in(b), vk__in(c), VK_RZ), m, cru); }
static inline vk vk__fms_mc(vk a, vk b, vk c, __mmask16* m, int cru)  { return vk__outc(_mm512_fmadd_round_ps(vk__in(a), vk__in(b), vk__in(VK_NEG(c, 0x80000000u)), VK_RZ), m, cru); }
static inline vk vk__fnms_mc(vk a, vk b, vk c, __mmask16* m, int cru) { return vk__outc(_mm512_fmadd_round_ps(vk__in(VK_NEG(a, 0x80000000u)), vk__in(b), vk__in(c), VK_RZ), m, cru); }
static inline vk vk__fa_m(vk a, vk b, __mmask16* m)  { return vk__fa_mc(a, b, m, 0); }
static inline vk vk__fs_m(vk a, vk b, __mmask16* m)  { return vk__fs_mc(a, b, m, 0); }
static inline vk vk__fm_m(vk a, vk b, __mmask16* m)  { return vk__fm_mc(a, b, m, 0); }
static inline vk vk__fma_m(vk a, vk b, vk c, __mmask16* m)  { return vk__fma_mc(a, b, c, m, 0); }
static inline vk vk__fms_m(vk a, vk b, vk c, __mmask16* m)  { return vk__fms_mc(a, b, c, m, 0); }
static inline vk vk__fnms_m(vk a, vk b, vk c, __mmask16* m) { return vk__fnms_mc(a, b, c, m, 0); }
/* As contas acumulam em *bad a máscara das palavras de alcance estendido; o núcleo
 * para no fim do grupo e o invólucro faz esse grupo com a transcrição literal (o
 * caminho exato) e chama o núcleo de novo. (21/10: refazer as faixas na hora com
 * uma função fora de linha deixava cada conta com uma chamada no laço -- no ABI do
 * Windows os zmm não sobrevivem a uma chamada, e o 7BB4 foi de 1.445 para 4.610
 * instruções, com 1.227 derramamentos: -7% de FPS.) _r: saída crua (vk__outc). */
static inline vk vk_fa(vk a, vk b, __mmask16* bad)  { return vk__fa_mc(a, b, bad, 0); }
static inline vk vk_fs(vk a, vk b, __mmask16* bad)  { return vk__fs_mc(a, b, bad, 0); }
static inline vk vk_fm(vk a, vk b, __mmask16* bad)  { return vk__fm_mc(a, b, bad, 0); }
static inline vk vk_fma(vk a, vk b, vk c, __mmask16* bad)  { return vk__fma_mc(a, b, c, bad, 0); }
static inline vk vk_fms(vk a, vk b, vk c, __mmask16* bad)  { return vk__fms_mc(a, b, c, bad, 0); }
static inline vk vk_fnms(vk a, vk b, vk c, __mmask16* bad) { return vk__fnms_mc(a, b, c, bad, 0); }
static inline vk vk_fa_r(vk a, vk b, __mmask16* bad)  { return vk__fa_mc(a, b, bad, 1); }
static inline vk vk_fs_r(vk a, vk b, __mmask16* bad)  { return vk__fs_mc(a, b, bad, 1); }
static inline vk vk_fm_r(vk a, vk b, __mmask16* bad)  { return vk__fm_mc(a, b, bad, 1); }
static inline vk vk_fma_r(vk a, vk b, vk c, __mmask16* bad)  { return vk__fma_mc(a, b, c, bad, 1); }
static inline vk vk_fms_r(vk a, vk b, vk c, __mmask16* bad)  { return vk__fms_mc(a, b, c, bad, 1); }
static inline vk vk_fnms_r(vk a, vk b, vk c, __mmask16* bad) { return vk__fnms_mc(a, b, c, bad, 1); }

/* ---- comparações de float: chave +-magnitude, expoente 0 = 0 ---- */
static inline vk vk__key(vk x) {
    const __mmask16 nz = _mm512_test_epi32_mask(x, _mm512_set1_epi32(0x7F800000));
    const vk mag = _mm512_and_si512(x, _mm512_set1_epi32(0x7FFFFFFF));
    const vk neg = _mm512_srai_epi32(x, 31);
    return _mm512_maskz_mov_epi32(nz, _mm512_sub_epi32(_mm512_xor_si512(mag, neg), neg));
}
static inline vk vk__mag(vk x) {
    return _mm512_maskz_mov_epi32(_mm512_test_epi32_mask(x, _mm512_set1_epi32(0x7F800000)), _mm512_and_si512(x, _mm512_set1_epi32(0x7FFFFFFF)));
}
static inline vk vk_fcgt(vk a, vk b)  { return _mm512_movm_epi32(_mm512_cmpgt_epi32_mask(vk__key(a), vk__key(b))); }
static inline vk vk_fceq(vk a, vk b)  { return _mm512_movm_epi32(_mm512_cmpeq_epi32_mask(vk__key(a), vk__key(b))); }
static inline vk vk_fcmgt(vk a, vk b) { return _mm512_movm_epi32(_mm512_cmpgt_epi32_mask(vk__mag(a), vk__mag(b))); }
static inline vk vk_fcmeq(vk a, vk b) { return _mm512_movm_epi32(_mm512_cmpeq_epi32_mask(vk__mag(a), vk__mag(b))); }

/* ---- inteiros e bits ---- */
static inline vk vk_and(vk a, vk b)  { return _mm512_and_si512(a, b); }
static inline vk vk_or(vk a, vk b)   { return _mm512_or_si512(a, b); }
static inline vk vk_xor(vk a, vk b)  { return _mm512_xor_si512(a, b); }
static inline vk vk_andc(vk a, vk b) { return _mm512_andnot_si512(b, a); }
static inline vk vk_nor(vk a, vk b)  { return _mm512_ternarylogic_epi32(a, b, b, 0x03); }
static inline vk vk_nand(vk a, vk b) { return _mm512_ternarylogic_epi32(a, b, b, 0x3F); }
static inline vk vk_orc(vk a, vk b)  { return _mm512_ternarylogic_epi32(a, b, b, 0xF3); }
static inline vk vk_selb(vk a, vk b, vk c) { return _mm512_ternarylogic_epi32(c, b, a, 0xCA); }
static inline vk vk_a(vk a, vk b)    { return _mm512_add_epi32(a, b); }
static inline vk vk_sf(vk a, vk b)   { return _mm512_sub_epi32(b, a); }
static inline vk vk_ai(vk a, int32_t i)  { return _mm512_add_epi32(a, _mm512_set1_epi32(i)); }
static inline vk vk_sfi(vk a, int32_t i) { return _mm512_sub_epi32(_mm512_set1_epi32(i), a); }
static inline vk vk_andi(vk a, int32_t i) { return _mm512_and_si512(a, _mm512_set1_epi32(i)); }
static inline vk vk_ori(vk a, int32_t i)  { return _mm512_or_si512(a, _mm512_set1_epi32(i)); }
static inline vk vk_xori(vk a, int32_t i) { return _mm512_xor_si512(a, _mm512_set1_epi32(i)); }
static inline vk vk_ceq(vk a, vk b)  { return _mm512_movm_epi32(_mm512_cmpeq_epi32_mask(a, b)); }
static inline vk vk_cgt(vk a, vk b)  { return _mm512_movm_epi32(_mm512_cmpgt_epi32_mask(a, b)); }
static inline vk vk_clgt(vk a, vk b) { return _mm512_movm_epi32(_mm512_cmpgt_epu32_mask(a, b)); }
static inline vk vk_ceqi(vk a, int32_t i)  { return _mm512_movm_epi32(_mm512_cmpeq_epi32_mask(a, _mm512_set1_epi32(i))); }
static inline vk vk_cgti(vk a, int32_t i)  { return _mm512_movm_epi32(_mm512_cmpgt_epi32_mask(a, _mm512_set1_epi32(i))); }
static inline vk vk_clgti(vk a, int32_t i) { return _mm512_movm_epi32(_mm512_cmpgt_epu32_mask(a, _mm512_set1_epi32(i))); }

/* ---- deslocamentos e rotações ---- */
static inline vk vk_andbi(vk a, int32_t i) { return _mm512_and_si512(a, _mm512_set1_epi8((char)(uint8_t)i)); }
static inline vk vk_clgth(vk a, vk b) { return _mm512_movm_epi16(_mm512_cmpgt_epu16_mask(a, b)); }
static inline vk vk_ceqh(vk a, vk b)  { return _mm512_movm_epi16(_mm512_cmpeq_epi16_mask(a, b)); }
static inline vk vk_cgth(vk a, vk b)  { return _mm512_movm_epi16(_mm512_cmpgt_epi16_mask(a, b)); }
static inline vk vk_shli(vk a, int32_t s)  { s &= 0x3F; return s > 31 ? _mm512_setzero_si512() : _mm512_sll_epi32(a, _mm_cvtsi32_si128(s)); }
static inline vk vk_rotmi(vk a, int32_t i7) { const int s = (0 - i7) & 0x3F; return s > 31 ? _mm512_setzero_si512() : _mm512_srl_epi32(a, _mm_cvtsi32_si128(s)); }
/* deslocamento aritmético: contagem >= 32 enche com o sinal (o mesmo do spu_rotmai_ref) */
static inline vk vk_rotmai(vk a, int32_t i7) { return _mm512_sra_epi32(a, _mm_cvtsi32_si128((0 - i7) & 0x3F)); }
static inline vk vk_roti(vk a, int32_t s)  { s &= 31; return s ? _mm512_or_si512(_mm512_sll_epi32(a, _mm_cvtsi32_si128(s)), _mm512_srl_epi32(a, _mm_cvtsi32_si128(32 - s))) : a; }
static inline vk vk_rotm(vk a, vk b) { return _mm512_srlv_epi32(a, _mm512_and_si512(_mm512_sub_epi32(_mm512_setzero_si512(), b), _mm512_set1_epi32(0x3F))); }
static inline vk vk_shl(vk a, vk b)  { return _mm512_sllv_epi32(a, _mm512_and_si512(b, _mm512_set1_epi32(0x3F))); }
/* rotação/deslocamento do quadword em bits (0..7): palavra i com os bits altos da i+1 */
static inline vk vk_rotqbii(vk a, int32_t s) {
    s &= 7;
    if (!s) return a;
    const vk nx = _mm512_shuffle_epi32(a, (_MM_PERM_ENUM)0x39);              /* palavras [1,2,3,0] */
    return _mm512_or_si512(_mm512_sll_epi32(a, _mm_cvtsi32_si128(s)), _mm512_srl_epi32(nx, _mm_cvtsi32_si128(32 - s)));
}
static inline vk vk_shlqbii(vk a, int32_t s) {
    s &= 7;
    if (!s) return a;
    const vk nx = _mm512_maskz_shuffle_epi32((__mmask16)0x7777, a, (_MM_PERM_ENUM)0x39);   /* [1,2,3,0] com a 4ª = 0 */
    return _mm512_or_si512(_mm512_sll_epi32(a, _mm_cvtsi32_si128(s)), _mm512_srl_epi32(nx, _mm_cvtsi32_si128(32 - s)));
}
/* bytes: o byte SPU P mora no byte h = P^3 da faixa; idx[h] = origem (0x80 = zero) */
static inline vk vk__perm_bytes(vk a, const int8_t idx[16]) {
    __m128i x; memcpy(&x, idx, 16);
    return _mm512_shuffle_epi8(a, _mm512_broadcast_i32x4(x));
}
static inline vk vk_rotqbyi(vk a, int32_t s) {
    int8_t idx[16]; s &= 0x0F;
    for (int h = 0; h < 16; h++) idx[h] = (int8_t)((((h ^ 3) + s) & 15) ^ 3);
    return vk__perm_bytes(a, idx);
}
static inline vk vk_shlqbyi(vk a, int32_t s) {
    int8_t idx[16]; s &= 0x1F;
    for (int h = 0; h < 16; h++) { const int q = (h ^ 3) + s; idx[h] = (int8_t)(q < 16 ? (q ^ 3) : 0x80); }
    return vk__perm_bytes(a, idx);
}
static inline vk vk_rotqmbyi(vk a, int32_t i7) {
    int8_t idx[16]; const int s = (0 - i7) & 0x1F;
    if (s >= 16) return _mm512_setzero_si512();
    for (int h = 0; h < 16; h++) { const int q = (h ^ 3) - s; idx[h] = (int8_t)(q >= 0 ? (q ^ 3) : 0x80); }
    return vk__perm_bytes(a, idx);
}
/* rotqby: a contagem de cada faixa vem do byte baixo da palavra 0 dela */
static inline vk vk_rotqby(vk a, vk b) {
    const vk n = _mm512_and_si512(_mm512_shuffle_epi8(b, _mm512_setzero_si512()), _mm512_set1_epi8(0x0F));
    const vk h3 = _mm512_broadcast_i32x4(_mm_setr_epi8(3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12));
    const vk idx = _mm512_xor_si512(_mm512_and_si512(_mm512_add_epi8(h3, n), _mm512_set1_epi8(0x0F)), _mm512_set1_epi8(3));
    return _mm512_shuffle_epi8(a, idx);
}
/* ---- mais formas com contagem/escala por faixa (21/10) ---- */
/* preferred word de cada faixa repetida nas 4 palavras da faixa */
static inline vk vk__pw(vk b) { return _mm512_shuffle_epi32(b, (_MM_PERM_ENUM)0x00); }
/* rotqmbybi: desloca o quadword (n = (0 - (b.w0 >> 3)) & 0x1F) bytes para a direita, zeros
 * entrando; n >= 16 -> 0. Byte SPU i <- byte SPU i - n (host h = SPU h^3) */
static inline vk vk_rotqmbybi(vk a, vk b) {
    const vk n = _mm512_and_si512(_mm512_sub_epi32(_mm512_setzero_si512(), _mm512_srai_epi32(vk__pw(b), 3)), _mm512_set1_epi32(0x1F));
    const vk nb = _mm512_shuffle_epi8(n, _mm512_setzero_si512());            /* n em todos os bytes da faixa */
    const vk h3 = _mm512_broadcast_i32x4(_mm_setr_epi8(3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12));
    return _mm512_shuffle_epi8(a, _mm512_xor_si512(_mm512_sub_epi8(h3, nb), _mm512_set1_epi8(3)));   /* < 0: bit 7 -> 0 */
}
/* rotqmbi: desloca o quadword (n = (0 - b.w0) & 7) bits para a direita, zeros entrando */
static inline vk vk_rotqmbi(vk a, vk b) {
    const vk n = _mm512_and_si512(_mm512_sub_epi32(_mm512_setzero_si512(), vk__pw(b)), _mm512_set1_epi32(7));
    const vk prev = _mm512_bslli_epi128(a, 4);                                 /* palavra i <- palavra i-1, a 0 <- 0 */
    return _mm512_or_si512(_mm512_srlv_epi32(a, n), _mm512_sllv_epi32(prev, _mm512_sub_epi32(_mm512_set1_epi32(32), n)));
}
static inline vk vk_rotma(vk a, vk b) { return _mm512_srav_epi32(a, _mm512_and_si512(_mm512_sub_epi32(_mm512_setzero_si512(), b), _mm512_set1_epi32(0x3F))); }
static inline vk vk_rothm(vk a, vk b) { return _mm512_srlv_epi16(a, _mm512_and_si512(_mm512_sub_epi16(_mm512_setzero_si512(), b), _mm512_set1_epi16(0x1F))); }
static inline vk vk_rot(vk a, vk b)   { return _mm512_rolv_epi32(a, _mm512_and_si512(b, _mm512_set1_epi32(31))); }
static inline vk vk_sfh(vk a, vk b)   { return _mm512_sub_epi16(b, a); }
static inline vk vk_ah(vk a, vk b)    { return _mm512_add_epi16(a, b); }
static inline vk vk_cntb(vk a)        { return _mm512_popcnt_epi8(a); }
/* gb/gbh: o bit 0 de cada palavra/meia palavra juntado no preferred word (resto 0) */
static inline vk vk__soma_faixa16(vk m) {          /* soma das 8 meias palavras (< 256 cada) de cada faixa */
    const vk sd = _mm512_sad_epu8(m, _mm512_setzero_si512());
    return _mm512_maskz_mov_epi32(0x1111, _mm512_add_epi64(sd, _mm512_bsrli_epi128(sd, 8)));
}
static inline vk vk_gbh(vk a) {
    const vk w = _mm512_broadcast_i32x4(_mm_setr_epi16(64, 128, 16, 32, 4, 8, 1, 2));
    return vk__soma_faixa16(_mm512_mullo_epi16(_mm512_and_si512(a, _mm512_set1_epi16(1)), w));
}
static inline vk vk_gb(vk a) {
    const vk w = _mm512_broadcast_i32x4(_mm_setr_epi32(8, 4, 2, 1));
    return vk__soma_faixa16(_mm512_mullo_epi32(_mm512_and_si512(a, _mm512_set1_epi32(1)), w));
}
/* csflt/cuflt: inteiro -> float * 2^(i8-155), truncado (o corte de 24 bits do spu__sf_pack);
 * a escala vai direto no campo de expoente: < 1 -> 0, > 255 -> sinal | 0x7FFFFFFF, e 255 é o
 * padrão de bits que a SPU dá (alcance estendido) -- exato sem caminho lento */
static inline vk vk__cxflt(vk fv, int i8) {
    const vk be = _mm512_and_si512(_mm512_srli_epi32(fv, 23), _mm512_set1_epi32(0xFF));
    const vk nb = _mm512_add_epi32(be, _mm512_set1_epi32((i8 & 0xFF) - 155));
    const vk sg = _mm512_and_si512(fv, _mm512_set1_epi32((int)0x80000000u));
    vk r = _mm512_or_si512(_mm512_and_si512(fv, _mm512_set1_epi32((int)0x807FFFFFu)), _mm512_slli_epi32(nb, 23));
    r = _mm512_mask_mov_epi32(r, _mm512_cmpgt_epi32_mask(nb, _mm512_set1_epi32(255)), _mm512_or_si512(sg, _mm512_set1_epi32(0x7FFFFFFF)));
    const __mmask16 zero = _mm512_cmpeq_epi32_mask(be, _mm512_setzero_si512()) | _mm512_cmplt_epi32_mask(nb, _mm512_set1_epi32(1));
    return _mm512_maskz_mov_epi32((__mmask16)~zero, r);
}
static inline vk vk_csflt(vk a, int32_t i8) { return vk__cxflt(_mm512_castps_si512(_mm512_cvt_roundepi32_ps(a, VK_RZ)), i8); }
static inline vk vk_cuflt(vk a, int32_t i8) { return vk__cxflt(_mm512_castps_si512(_mm512_cvt_roundepu32_ps(a, VK_RZ)), i8); }
/* cflts/cfltu: valor * 2^(173-i8), truncado, saturado (mesma conta do spu__cfl_v) */
static inline vk vk__cfl(vk x, int i8, int uns) {
    const vk be = _mm512_and_si512(_mm512_srli_epi32(x, 23), _mm512_set1_epi32(0xFF));
    const vk m  = _mm512_or_si512(_mm512_and_si512(x, _mm512_set1_epi32(0x7FFFFF)), _mm512_set1_epi32(0x800000));
    const vk sh = _mm512_add_epi32(be, _mm512_set1_epi32(173 - (i8 & 0xFF) - 150));
    const vk v  = _mm512_mask_mov_epi32(_mm512_srlv_epi32(m, _mm512_sub_epi32(_mm512_setzero_si512(), sh)),
                                        _mm512_cmpgt_epi32_mask(sh, _mm512_set1_epi32(-1)), _mm512_sllv_epi32(m, sh));
    const vk neg = _mm512_srai_epi32(x, 31);
    const __mmask16 isz = _mm512_cmpeq_epi32_mask(be, _mm512_setzero_si512());
    vk r;
    if (uns) {
        r = _mm512_mask_mov_epi32(v, _mm512_cmpgt_epi32_mask(sh, _mm512_set1_epi32(8)), _mm512_set1_epi32(-1));
        r = _mm512_maskz_mov_epi32((__mmask16)~(isz | _mm512_movepi32_mask(neg)), r);
    } else {
        const vk sat = _mm512_xor_si512(_mm512_set1_epi32(0x7FFFFFFF), neg);
        r = _mm512_sub_epi32(_mm512_xor_si512(v, neg), neg);
        r = _mm512_mask_mov_epi32(r, _mm512_cmpgt_epi32_mask(sh, _mm512_set1_epi32(7)), sat);
        r = _mm512_maskz_mov_epi32((__mmask16)~isz, r);
    }
    return r;
}
static inline vk vk_cflts(vk a, int32_t i8) { return vk__cfl(a, i8, 0); }
static inline vk vk_cfltu(vk a, int32_t i8) { return vk__cfl(a, i8, 1); }

/* ---- shufb por faixa: a mesma regra do spu_shufb (vpermi2b), o índice
 * levado para a faixa de 16 bytes da iteração dentro das tabelas de 64 ---- */
static inline vk vk_shufb(vk a, vk b, vk c) {
    const vk base = _mm512_set_epi32(0x30303030, 0x30303030, 0x30303030, 0x30303030,
                                     0x20202020, 0x20202020, 0x20202020, 0x20202020,
                                     0x10101010, 0x10101010, 0x10101010, 0x10101010,
                                     0, 0, 0, 0);
    const vk lo = _mm512_and_si512(_mm512_xor_si512(c, _mm512_set1_epi8(3)), _mm512_set1_epi8(0x0F));
    const vk sel = _mm512_slli_epi16(_mm512_and_si512(c, _mm512_set1_epi8(0x10)), 2);   /* bit 4 -> bit 6: tabela b */
    const vk idx = _mm512_or_si512(_mm512_or_si512(lo, base), sel);
    vk r = _mm512_permutex2var_epi8(a, idx, b);
    const vk lut = _mm512_broadcast_i32x4(_mm_setr_epi8(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                                        (char)0xFF, (char)0xFF, (char)0x80, (char)0x80));
    const vk sp = _mm512_shuffle_epi8(lut, _mm512_and_si512(_mm512_srli_epi16(c, 4), _mm512_set1_epi8(0x0F)));
    return _mm512_mask_mov_epi8(r, _mm512_movepi8_mask(c), sp);
}

/* ---- estimativas ---- */
static inline vk vk_frsqest(vk x) {
    const __m512i t0 = _mm512_loadu_si512((const void*)&spu_frsqest_fraction_lut[0]);
    const __m512i t1 = _mm512_loadu_si512((const void*)&spu_frsqest_fraction_lut[16]);
    const __m512i t2 = _mm512_loadu_si512((const void*)&spu_frsqest_fraction_lut[32]);
    const __m512i t3 = _mm512_loadu_si512((const void*)&spu_frsqest_fraction_lut[48]);
    const vk idx = _mm512_and_si512(_mm512_srli_epi32(x, 18), _mm512_set1_epi32(0x3F));
    const vk lo = _mm512_permutex2var_epi32(t0, idx, t1);
    const vk hi = _mm512_permutex2var_epi32(t2, idx, t3);
    const vk frac = _mm512_and_si512(_mm512_mask_mov_epi32(lo, _mm512_test_epi32_mask(idx, _mm512_set1_epi32(0x20)), hi),
                                     _mm512_set1_epi32(0x7FFFFF));
    const vk exp = _mm512_and_si512(_mm512_srli_epi32(x, 23), _mm512_set1_epi32(0xFF));
    vk rexp = _mm512_sub_epi32(_mm512_set1_epi32(190), _mm512_srli_epi32(_mm512_add_epi32(exp, _mm512_set1_epi32(1)), 1));
    rexp = _mm512_mask_mov_epi32(rexp, _mm512_cmpeq_epi32_mask(exp, _mm512_setzero_si512()), _mm512_set1_epi32(0xFF));
    return _mm512_or_si512(_mm512_slli_epi32(rexp, 23), frac);
}
static inline vk vk_frest(vk x) {
    const __m512i t0 = _mm512_loadu_si512((const void*)&spu_frest_fraction_lut[0]);
    const __m512i t1 = _mm512_loadu_si512((const void*)&spu_frest_fraction_lut[16]);
    const vk idx = _mm512_and_si512(_mm512_srli_epi32(x, 18), _mm512_set1_epi32(0x1F));
    const vk frac = _mm512_and_si512(_mm512_permutex2var_epi32(t0, idx, t1), _mm512_set1_epi32(0x7FFFFF));
    const vk exp = _mm512_and_si512(_mm512_srli_epi32(x, 23), _mm512_set1_epi32(0xFF));
    vk rexp = _mm512_sub_epi32(_mm512_set1_epi32(253), exp);
    rexp = _mm512_mask_mov_epi32(rexp, _mm512_cmpge_epu32_mask(exp, _mm512_set1_epi32(253)), _mm512_setzero_si512());
    rexp = _mm512_mask_mov_epi32(rexp, _mm512_cmpeq_epi32_mask(exp, _mm512_setzero_si512()), _mm512_set1_epi32(255));
    return _mm512_or_si512(_mm512_and_si512(x, _mm512_set1_epi32((int)0x80000000u)),
                           _mm512_or_si512(_mm512_slli_epi32(rexp, 23), frac));
}
/* fi: o auxiliar faz m = (base << 19) - step*y em 64 bits; se m < 2^32, m <<= 1;
 * fração = (m >> 9) & 0x7FFFFF. Aqui tudo em 32 bits: base << 19 é múltiplo de
 * 256, então m >> 8 = (base << 11) - ceil(step*y / 256), e m >> 9 = (m >> 8) >> 1;
 * m < 2^32 <=> m >> 8 < 2^24. (Antes: duas metades de 64 bits e as máscaras
 * juntadas pela pilha -- falha de encaminhamento.) */
static inline vk vk_fi(vk a, vk b) {
    const vk exp = _mm512_and_si512(_mm512_srli_epi32(b, 23), _mm512_set1_epi32(0xFF));
    const vk base32 = _mm512_or_si512(_mm512_and_si512(_mm512_srli_epi32(b, 10), _mm512_set1_epi32(0x1FFF)), _mm512_set1_epi32(0x2000));
    const vk step32 = _mm512_and_si512(b, _mm512_set1_epi32(0x3FF));
    const vk y32 = _mm512_and_si512(a, _mm512_set1_epi32(0x7FFFF));
    const vk prod = _mm512_mullo_epi32(step32, y32);                                     /* < 2^29 */
    const vk m8 = _mm512_sub_epi32(_mm512_slli_epi32(base32, 11),
                                   _mm512_srli_epi32(_mm512_add_epi32(prod, _mm512_set1_epi32(255)), 8));
    const __mmask16 lo16 = _mm512_cmplt_epu32_mask(m8, _mm512_set1_epi32(1 << 24));      /* m < 2^32 */
    const vk frac = _mm512_and_si512(_mm512_mask_mov_epi32(_mm512_srli_epi32(m8, 1), lo16, m8), _mm512_set1_epi32(0x7FFFFF));
    const vk e2 = _mm512_mask_sub_epi32(exp, lo16, exp, _mm512_set1_epi32(1));       /* exp - 1 onde normalizou */
    const vk r = _mm512_or_si512(_mm512_and_si512(b, _mm512_set1_epi32((int)0x80000000u)),
                                 _mm512_or_si512(_mm512_slli_epi32(e2, 23), frac));
    const __mmask16 zero = _mm512_cmpeq_epi32_mask(exp, _mm512_setzero_si512()) | _mm512_cmpeq_epi32_mask(e2, _mm512_setzero_si512());
    return _mm512_maskz_mov_epi32((__mmask16)~zero, r);
}
/* Conflito de memória dentro de um grupo: alguma leitura da iteração L lê o
 * quadword que a iteração j < L grava? (as gravações do grupo ficam para o fim.)
 * Endereços já alinhados a 16 e mascarados para a LS, então igualdade = mesmo
 * quadword. sta/lda: [n][4], uma coluna por iteração do grupo. */
/* o mesmo só com as nv primeiras iterações do grupo válidas (grupo parcial do fim):
 * iteração j gravando, L = j + d lendo, L < nv */
static inline int vk_conflito_n(const uint32_t (*sta)[4], int nst, const uint32_t (*lda)[4], int nld, uint32_t nv)
{
    const unsigned r1 = nv > 1 ? (1u << (nv - 1)) - 1u : 0u, r2 = nv > 2 ? (1u << (nv - 2)) - 1u : 0u, r3 = nv > 3 ? 1u : 0u;
    const __mmask16 k1 = (__mmask16)(r1 * 0x1111u), k2 = (__mmask16)(r2 * 0x1111u), k3 = (__mmask16)(r3 * 0x1111u);
    __mmask16 m = 0;
    for (int b = 0; b < nld; b += 4) {
        const int b1 = b + 1 < nld ? b + 1 : b, b2 = b + 2 < nld ? b + 2 : b1, b3 = b + 3 < nld ? b + 3 : b2;
        const vk ld = vk_joinx(_mm_loadu_si128((const __m128i*)lda[b]), _mm_loadu_si128((const __m128i*)lda[b1]),
                               _mm_loadu_si128((const __m128i*)lda[b2]), _mm_loadu_si128((const __m128i*)lda[b3]));
        const vk l1 = _mm512_bsrli_epi128(ld, 4), l2 = _mm512_bsrli_epi128(ld, 8), l3 = _mm512_bsrli_epi128(ld, 12);
        for (int a = 0; a < nst; a++) {
            const vk s = _mm512_broadcast_i32x4(_mm_loadu_si128((const __m128i*)sta[a]));
            m |= _mm512_mask_cmpeq_epi32_mask(k1, s, l1);
            m |= _mm512_mask_cmpeq_epi32_mask(k2, s, l2);
            m |= _mm512_mask_cmpeq_epi32_mask(k3, s, l3);
        }
    }
    return m != 0;
}
/* faixa l em tempo de execução sem desvio: índice de vpermq montado uma vez por grupo */
static inline vk vk_xsel(uint32_t l) { return _mm512_add_epi64(_mm512_set1_epi64((long long)(2 * l)), _mm512_setr_epi64(0, 1, 0, 1, 0, 1, 0, 1)); }
static inline __m128i vk_xs(vk v, vk sel) { return _mm512_castsi512_si128(_mm512_permutexvar_epi64(sel, v)); }
/* faixa l (em tempo de execução) de um vk, como xmm */
static inline __m128i vk_xl(vk v, uint32_t l)
{
    switch (l) {
    case 0: return vk_x(v, 0);
    case 1: return vk_x(v, 1);
    case 2: return vk_x(v, 2);
    default: return vk_x(v, 3);
    }
}
static inline int vk_conflito(const uint32_t (*sta)[4], int nst, const uint32_t (*lda)[4], int nld)
{
    __mmask16 m = 0;
    for (int b = 0; b < nld; b += 4) {
        /* até 4 leituras num zmm; as que faltam repetem a última (duplicata inofensiva) */
        const int b1 = b + 1 < nld ? b + 1 : b, b2 = b + 2 < nld ? b + 2 : b1, b3 = b + 3 < nld ? b + 3 : b2;
        const vk ld = vk_joinx(_mm_loadu_si128((const __m128i*)lda[b]), _mm_loadu_si128((const __m128i*)lda[b1]),
                               _mm_loadu_si128((const __m128i*)lda[b2]), _mm_loadu_si128((const __m128i*)lda[b3]));
        /* dentro de cada faixa: dword j = leitura da iteração j+d */
        const vk l1 = _mm512_bsrli_epi128(ld, 4), l2 = _mm512_bsrli_epi128(ld, 8), l3 = _mm512_bsrli_epi128(ld, 12);
        for (int a = 0; a < nst; a++) {
            const vk s = _mm512_broadcast_i32x4(_mm_loadu_si128((const __m128i*)sta[a]));
            m |= _mm512_mask_cmpeq_epi32_mask(0x7777, s, l1);
            m |= _mm512_mask_cmpeq_epi32_mask(0x3333, s, l2);
            m |= _mm512_mask_cmpeq_epi32_mask(0x1111, s, l3);
        }
    }
    return m != 0;
}
/* encaminhamento dentro da iteração: máscara das palavras das faixas em que a
 * gravação e a leitura caem no mesmo quadword (sa/la: um endereço por faixa) */
static inline __mmask16 vk_mesmo_end(const uint32_t sa[4], const uint32_t la[4]) {
    const unsigned m4 = (unsigned)_mm_cmpeq_epi32_mask(_mm_loadu_si128((const __m128i*)sa), _mm_loadu_si128((const __m128i*)la));
    return (__mmask16)(_pdep_u32(m4, 0x1111u) * 0xFu);
}
/* GOWA_VK_CONFDBG=1: par gravação -> leitura da mesma iteração (comandos do corpo)
 * que caiu no mesmo quadword -- poucas vezes por par */
static __attribute__((noinline, cold)) void vk_par_mostra(const char* nome, int* vezes, int kst, int kld, uint32_t end)
{
    static int on = -1;
    if (on < 0) on = getenv("GOWA_VK_CONFDBG") ? 1 : 0;
    if (!on || *vezes >= 8) return;
    (*vezes)++;
    fprintf(stderr, "[vk] par %s: gravação (comando %d) e leitura (comando %d) da mesma iteração em 0x%05X\n", nome, kst, kld, end);
}
/* GOWA_VK_CONFDBG=1 (diagnóstico): mostra o primeiro par em conflito de um grupo
 * (gravação a da iteração j, leitura b da iteração L > j), poucas vezes por núcleo. */
static inline void vk_conflito_mostra(const char* nome, int* vezes, const uint32_t (*sta)[4], int nst, const uint32_t (*lda)[4], int nld)
{
    static int on = -1;
    if (on < 0) on = getenv("GOWA_VK_CONFDBG") ? 1 : 0;
    if (!on || *vezes >= 6) return;
    for (int a = 0; a < nst; a++) for (int b = 0; b < nld; b++)
        for (int j = 0; j < 3; j++) for (int L = j + 1; L < 4; L++)
            if (sta[a][j] == lda[b][L]) {
                (*vezes)++;
                fprintf(stderr, "[vk] conflito %s: gravação %d (iter %d) = leitura %d (iter %d) em 0x%05X; gravações %05X %05X %05X %05X, leituras %05X %05X %05X %05X\n",
                        nome, a, j, b, L, sta[a][j], sta[a][0], sta[a][1], sta[a][2], sta[a][3], lda[b][0], lda[b][1], lda[b][2], lda[b][3]);
                return;
            }
}
#endif /* !__AVX512F__ && GOWA_VK_AVX2 */
#endif /* SPU_VK_H */
