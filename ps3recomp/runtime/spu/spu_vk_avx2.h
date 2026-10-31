/* GOWA (24/10): spu_vk.h for machines without AVX-512 (x86-64-v3 + SPU_SX_RZ). The same
 * API -- four iterations of a lifted loop per "instruction", one 128-bit lane per
 * iteration -- with vk as two 256-bit halves (lanes 0-1 in l, 2-3 in h) and the 16-bit
 * masks (one bit per 32-bit word) built with movemask. The float ops rely on the SPU host
 * thread's MXCSR round-toward-zero (SPU_SX_RZ, spu_helpers.h) instead of {rz-sae}. The
 * AVX-512 intrinsics the generated kernels (scripts/vetorizar_laco.py) call directly are
 * mapped at the end. Lane for lane the same bits as spu_vk.h (out/spudiff/teste_vk.c
 * built for x86-64-v3). Used only when GOWA_VK_AVX2 is defined (spu_helpers.h). */
#ifndef SPU_VK_AVX2_H
#define SPU_VK_AVX2_H
#include <immintrin.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "spu_helpers.h"

/* clang before 19 in MSVC mode (the clang-cl of older VS 2022 installs) leaves avx512fintrin.h
 * out of immintrin.h without -mavx512f, and the mask types come from it (report 498) */
#if defined(__clang__) && !defined(__AVX512FINTRIN_H)
typedef unsigned char __mmask8;
typedef unsigned short __mmask16;
#endif

typedef struct { __m256i l, h; } vk;
#define VK__I static inline __attribute__((always_inline))

VK__I vk vk__mk(__m256i l, __m256i h) { vk r; r.l = l; r.h = h; return r; }
VK__I vk vk__bc256(__m256i x) { return vk__mk(x, x); }
VK__I __m256i vk__b128(__m128i x) { return _mm256_broadcastsi128_si256(x); }
VK__I vk vk__set1(int32_t i) { return vk__bc256(_mm256_set1_epi32(i)); }
VK__I vk vk__zero(void) { return vk__bc256(_mm256_setzero_si256()); }
#define VK__OP2(nome, f) VK__I vk nome(vk a, vk b) { return vk__mk(f(a.l, b.l), f(a.h, b.h)); }
/* masks: bit i = 32-bit word i (word i of lane i/4) */
VK__I __mmask16 vk__m(vk c) {
    return (__mmask16)((unsigned)_mm256_movemask_ps(_mm256_castsi256_ps(c.l)) |
                       ((unsigned)_mm256_movemask_ps(_mm256_castsi256_ps(c.h)) << 8));
}
VK__I __m256i vk__x8(unsigned m) {
    const __m256i bits = _mm256_setr_epi32(1, 2, 4, 8, 16, 32, 64, 128);
    return _mm256_cmpeq_epi32(_mm256_and_si256(_mm256_set1_epi32((int)m), bits), bits);
}
VK__I vk vk__xm(__mmask16 m) { return vk__mk(vk__x8(m & 0xFFu), vk__x8((unsigned)m >> 8)); }
VK__I vk vk__blend(vk a, vk b, vk sel) { return vk__mk(_mm256_blendv_epi8(a.l, b.l, sel.l), _mm256_blendv_epi8(a.h, b.h, sel.h)); }
VK__I vk vk__ones(void) { return vk__set1(-1); }

static inline vk vk_bc(u128 v) { __m128i x; memcpy(&x, &v, 16); return vk__bc256(vk__b128(x)); }
static inline __m128i vk_x(vk v, int l) {
    switch (l) {
    case 0: return _mm256_castsi256_si128(v.l);
    case 1: return _mm256_extracti128_si256(v.l, 1);
    case 2: return _mm256_castsi256_si128(v.h);
    default: return _mm256_extracti128_si256(v.h, 1);
    }
}
static inline u128 vk_lane(vk v, int l) { const __m128i x = vk_x(v, l); u128 r; memcpy(&r, &x, 16); return r; }
static inline vk vk_joinx(__m128i a, __m128i b, __m128i c, __m128i d) {
    return vk__mk(_mm256_inserti128_si256(_mm256_castsi128_si256(a), b, 1), _mm256_inserti128_si256(_mm256_castsi128_si256(c), d, 1));
}
static inline vk vk_join(u128 a, u128 b, u128 c, u128 d) {
    __m128i x0, x1, x2, x3; memcpy(&x0, &a, 16); memcpy(&x1, &b, 16); memcpy(&x2, &c, 16); memcpy(&x3, &d, 16);
    return vk_joinx(x0, x1, x2, x3);
}
/* previous iteration's value: lanes [prev.3, cur.0, cur.1, cur.2] */
static inline vk vk_prev(vk cur, vk prev) {
    return vk__mk(_mm256_permute2x128_si256(prev.h, cur.l, 0x21), _mm256_permute2x128_si256(cur.l, cur.h, 0x21));
}
static inline vk vk_prevq(vk cur, __m128i q) {
    return vk__mk(_mm256_inserti128_si256(_mm256_castsi128_si256(q), _mm256_castsi256_si128(cur.l), 1),
                  _mm256_permute2x128_si256(cur.l, cur.h, 0x21));
}
static inline void vk_t4(vk a, vk b, vk c, vk d, vk* r0, vk* r1, vk* r2, vk* r3) {
    *r0 = vk__mk(_mm256_permute2x128_si256(a.l, b.l, 0x20), _mm256_permute2x128_si256(c.l, d.l, 0x20));
    *r1 = vk__mk(_mm256_permute2x128_si256(a.l, b.l, 0x31), _mm256_permute2x128_si256(c.l, d.l, 0x31));
    *r2 = vk__mk(_mm256_permute2x128_si256(a.h, b.h, 0x20), _mm256_permute2x128_si256(c.h, d.h, 0x20));
    *r3 = vk__mk(_mm256_permute2x128_si256(a.h, b.h, 0x31), _mm256_permute2x128_si256(c.h, d.h, 0x31));
}
static inline vk vk_passo(__m128i base, int32_t s) {
    const __m256i b = vk__b128(base);
    return vk__mk(_mm256_add_epi32(b, _mm256_setr_epi32(s, s, s, s, 2 * s, 2 * s, 2 * s, 2 * s)),
                  _mm256_add_epi32(b, _mm256_setr_epi32(3 * s, 3 * s, 3 * s, 3 * s, 4 * s, 4 * s, 4 * s, 4 * s)));
}
static inline __m128i vk_u2x(u128 v) { __m128i x; memcpy(&x, &v, 16); return x; }
static inline u128 vk_x2u(__m128i x) { u128 v; memcpy(&v, &x, 16); return v; }
VK__I __m256i vk__bsw(__m256i v) {
    return _mm256_shuffle_epi8(v, _mm256_setr_epi8(3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12,
                                                   3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12));
}
static inline vk vk__bswap32(vk v) { return vk__mk(vk__bsw(v.l), vk__bsw(v.h)); }
static inline vk vk_ld4(const uint8_t* ls, uint32_t a0, uint32_t a1, uint32_t a2, uint32_t a3) {
    return vk__bswap32(vk_joinx(_mm_load_si128((const __m128i*)(ls + a0)), _mm_load_si128((const __m128i*)(ls + a1)),
                                _mm_load_si128((const __m128i*)(ls + a2)), _mm_load_si128((const __m128i*)(ls + a3))));
}
static inline uint32_t vk_slot(vk v, int l) { return (uint32_t)_mm_cvtsi128_si32(vk_x(v, l)); }
static inline vk vk_ld4a(const uint8_t* ls, vk a) {
    return vk_ld4(ls, (uint32_t)_mm_cvtsi128_si32(vk_x(a, 0)), (uint32_t)_mm_cvtsi128_si32(vk_x(a, 1)),
                  (uint32_t)_mm_cvtsi128_si32(vk_x(a, 2)), (uint32_t)_mm_cvtsi128_si32(vk_x(a, 3)));
}

/* ---- float: truncation from MXCSR (SPU_SX_RZ), DAZ, flushed output ---- */
VK__I __m256 vk__in1(__m256i x) {
#if defined(SPU_SX_DAZ)
    return _mm256_castsi256_ps(x);
#else
    return _mm256_castsi256_ps(_mm256_andnot_si256(_mm256_cmpeq_epi32(_mm256_and_si256(x, _mm256_set1_epi32(0x7F800000)), _mm256_setzero_si256()), x));
#endif
}
VK__I __m256i vk__out1(__m256 r, __m256i* badv, int cru) {
    const __m256i ri = _mm256_castps_si256(r);
    *badv = _mm256_or_si256(*badv, _mm256_cmpgt_epi32(_mm256_and_si256(ri, _mm256_set1_epi32(0x7FFFFFFF)), _mm256_set1_epi32(0x7F7FFFFE)));
    return cru ? ri : _mm256_andnot_si256(_mm256_cmpeq_epi32(_mm256_and_si256(ri, _mm256_set1_epi32(0x7F800000)), _mm256_setzero_si256()), ri);
}
#define VK__F2(nome, op, nb, cru) \
    static inline vk nome(vk a, vk b, __mmask16* bad) { \
        __m256i bl = _mm256_setzero_si256(), bh = _mm256_setzero_si256(); \
        const __m256i yl = _mm256_xor_si256(b.l, _mm256_set1_epi32((int)(nb))), yh = _mm256_xor_si256(b.h, _mm256_set1_epi32((int)(nb))); \
        const vk r = vk__mk(vk__out1(op(vk__in1(a.l), vk__in1(yl)), &bl, cru), vk__out1(op(vk__in1(a.h), vk__in1(yh)), &bh, cru)); \
        *bad |= vk__m(vk__mk(bl, bh)); return r; }
#define VK__F3(nome, na, nc, cru) \
    static inline vk nome(vk a, vk b, vk c, __mmask16* bad) { \
        __m256i bl = _mm256_setzero_si256(), bh = _mm256_setzero_si256(); \
        const __m256i al = _mm256_xor_si256(a.l, _mm256_set1_epi32((int)(na))), ah = _mm256_xor_si256(a.h, _mm256_set1_epi32((int)(na))); \
        const __m256i cl = _mm256_xor_si256(c.l, _mm256_set1_epi32((int)(nc))), ch = _mm256_xor_si256(c.h, _mm256_set1_epi32((int)(nc))); \
        const vk r = vk__mk(vk__out1(_mm256_fmadd_ps(vk__in1(al), vk__in1(b.l), vk__in1(cl)), &bl, cru), \
                            vk__out1(_mm256_fmadd_ps(vk__in1(ah), vk__in1(b.h), vk__in1(ch)), &bh, cru)); \
        *bad |= vk__m(vk__mk(bl, bh)); return r; }
VK__F2(vk_fa, _mm256_add_ps, 0u, 0)
VK__F2(vk_fs, _mm256_add_ps, 0x80000000u, 0)
VK__F2(vk_fm, _mm256_mul_ps, 0u, 0)
VK__F2(vk_fa_r, _mm256_add_ps, 0u, 1)
VK__F2(vk_fs_r, _mm256_add_ps, 0x80000000u, 1)
VK__F2(vk_fm_r, _mm256_mul_ps, 0u, 1)
VK__F3(vk_fma, 0u, 0u, 0)
VK__F3(vk_fms, 0u, 0x80000000u, 0)
VK__F3(vk_fnms, 0x80000000u, 0u, 0)
VK__F3(vk_fma_r, 0u, 0u, 1)
VK__F3(vk_fms_r, 0u, 0x80000000u, 1)
VK__F3(vk_fnms_r, 0x80000000u, 0u, 1)
#undef VK__F2
#undef VK__F3
static inline vk vk__fa_m(vk a, vk b, __mmask16* m)  { return vk_fa(a, b, m); }
static inline vk vk__fs_m(vk a, vk b, __mmask16* m)  { return vk_fs(a, b, m); }
static inline vk vk__fm_m(vk a, vk b, __mmask16* m)  { return vk_fm(a, b, m); }
static inline vk vk__fma_m(vk a, vk b, vk c, __mmask16* m)  { return vk_fma(a, b, c, m); }
static inline vk vk__fms_m(vk a, vk b, vk c, __mmask16* m)  { return vk_fms(a, b, c, m); }
static inline vk vk__fnms_m(vk a, vk b, vk c, __mmask16* m) { return vk_fnms(a, b, c, m); }

/* ---- float compares: +-magnitude key, exponent 0 = 0 ---- */
VK__I __m256i vk__key1(__m256i x) {
    const __m256i z = _mm256_cmpeq_epi32(_mm256_and_si256(x, _mm256_set1_epi32(0x7F800000)), _mm256_setzero_si256());
    const __m256i mag = _mm256_and_si256(x, _mm256_set1_epi32(0x7FFFFFFF)), neg = _mm256_srai_epi32(x, 31);
    return _mm256_andnot_si256(z, _mm256_sub_epi32(_mm256_xor_si256(mag, neg), neg));
}
VK__I __m256i vk__mag1(__m256i x) {
    const __m256i z = _mm256_cmpeq_epi32(_mm256_and_si256(x, _mm256_set1_epi32(0x7F800000)), _mm256_setzero_si256());
    return _mm256_andnot_si256(z, _mm256_and_si256(x, _mm256_set1_epi32(0x7FFFFFFF)));
}
static inline vk vk_fcgt(vk a, vk b)  { return vk__mk(_mm256_cmpgt_epi32(vk__key1(a.l), vk__key1(b.l)), _mm256_cmpgt_epi32(vk__key1(a.h), vk__key1(b.h))); }
static inline vk vk_fceq(vk a, vk b)  { return vk__mk(_mm256_cmpeq_epi32(vk__key1(a.l), vk__key1(b.l)), _mm256_cmpeq_epi32(vk__key1(a.h), vk__key1(b.h))); }
static inline vk vk_fcmgt(vk a, vk b) { return vk__mk(_mm256_cmpgt_epi32(vk__mag1(a.l), vk__mag1(b.l)), _mm256_cmpgt_epi32(vk__mag1(a.h), vk__mag1(b.h))); }
static inline vk vk_fcmeq(vk a, vk b) { return vk__mk(_mm256_cmpeq_epi32(vk__mag1(a.l), vk__mag1(b.l)), _mm256_cmpeq_epi32(vk__mag1(a.h), vk__mag1(b.h))); }

/* ---- integers and bits ---- */
VK__OP2(vk_and, _mm256_and_si256)
VK__OP2(vk_or, _mm256_or_si256)
VK__OP2(vk_xor, _mm256_xor_si256)
VK__OP2(vk_a, _mm256_add_epi32)
VK__I __m256i vk__andc1(__m256i a, __m256i b) { return _mm256_andnot_si256(b, a); }
VK__OP2(vk_andc, vk__andc1)
VK__I __m256i vk__not1(__m256i a) { return _mm256_xor_si256(a, _mm256_set1_epi32(-1)); }
static inline vk vk_nor(vk a, vk b)  { return vk__mk(vk__not1(_mm256_or_si256(a.l, b.l)), vk__not1(_mm256_or_si256(a.h, b.h))); }
static inline vk vk_nand(vk a, vk b) { return vk__mk(vk__not1(_mm256_and_si256(a.l, b.l)), vk__not1(_mm256_and_si256(a.h, b.h))); }
static inline vk vk_orc(vk a, vk b)  { return vk__mk(_mm256_or_si256(a.l, vk__not1(b.l)), _mm256_or_si256(a.h, vk__not1(b.h))); }
/* selb: c ? b : a, bit by bit */
static inline vk vk_selb(vk a, vk b, vk c) {
    return vk__mk(_mm256_or_si256(_mm256_and_si256(c.l, b.l), _mm256_andnot_si256(c.l, a.l)),
                  _mm256_or_si256(_mm256_and_si256(c.h, b.h), _mm256_andnot_si256(c.h, a.h)));
}
static inline vk vk_sf(vk a, vk b)   { return vk__mk(_mm256_sub_epi32(b.l, a.l), _mm256_sub_epi32(b.h, a.h)); }
static inline vk vk_ai(vk a, int32_t i)  { return vk_a(a, vk__set1(i)); }
static inline vk vk_sfi(vk a, int32_t i) { return vk_sf(a, vk__set1(i)); }
static inline vk vk_andi(vk a, int32_t i) { return vk_and(a, vk__set1(i)); }
static inline vk vk_ori(vk a, int32_t i)  { return vk_or(a, vk__set1(i)); }
static inline vk vk_xori(vk a, int32_t i) { return vk_xor(a, vk__set1(i)); }
VK__OP2(vk_ceq, _mm256_cmpeq_epi32)
VK__OP2(vk_cgt, _mm256_cmpgt_epi32)
VK__I __m256i vk__cgtu1(__m256i a, __m256i b) {
    const __m256i s = _mm256_set1_epi32((int)0x80000000u);
    return _mm256_cmpgt_epi32(_mm256_xor_si256(a, s), _mm256_xor_si256(b, s));
}
VK__OP2(vk_clgt, vk__cgtu1)
static inline vk vk_ceqi(vk a, int32_t i)  { return vk_ceq(a, vk__set1(i)); }
static inline vk vk_cgti(vk a, int32_t i)  { return vk_cgt(a, vk__set1(i)); }
static inline vk vk_clgti(vk a, int32_t i) { return vk_clgt(a, vk__set1(i)); }

/* ---- shifts and rotates ---- */
static inline vk vk_andbi(vk a, int32_t i) { return vk_and(a, vk__bc256(_mm256_set1_epi8((char)(uint8_t)i))); }
VK__I __m256i vk__cgtuh1(__m256i a, __m256i b) {
    const __m256i s = _mm256_set1_epi16((short)0x8000);
    return _mm256_cmpgt_epi16(_mm256_xor_si256(a, s), _mm256_xor_si256(b, s));
}
VK__OP2(vk_clgth, vk__cgtuh1)
VK__OP2(vk_ceqh, _mm256_cmpeq_epi16)
VK__OP2(vk_cgth, _mm256_cmpgt_epi16)
static inline vk vk_shli(vk a, int32_t s)  { s &= 0x3F; if (s > 31) return vk__zero(); const __m128i c = _mm_cvtsi32_si128(s); return vk__mk(_mm256_sll_epi32(a.l, c), _mm256_sll_epi32(a.h, c)); }
static inline vk vk_rotmi(vk a, int32_t i7) { const int s = (0 - i7) & 0x3F; if (s > 31) return vk__zero(); const __m128i c = _mm_cvtsi32_si128(s); return vk__mk(_mm256_srl_epi32(a.l, c), _mm256_srl_epi32(a.h, c)); }
static inline vk vk_rotmai(vk a, int32_t i7) { const __m128i c = _mm_cvtsi32_si128((0 - i7) & 0x3F); return vk__mk(_mm256_sra_epi32(a.l, c), _mm256_sra_epi32(a.h, c)); }
static inline vk vk_roti(vk a, int32_t s) {
    s &= 31; if (!s) return a;
    const __m128i c = _mm_cvtsi32_si128(s), d = _mm_cvtsi32_si128(32 - s);
    return vk__mk(_mm256_or_si256(_mm256_sll_epi32(a.l, c), _mm256_srl_epi32(a.l, d)), _mm256_or_si256(_mm256_sll_epi32(a.h, c), _mm256_srl_epi32(a.h, d)));
}
VK__I __m256i vk__rotm1(__m256i a, __m256i b) { return _mm256_srlv_epi32(a, _mm256_and_si256(_mm256_sub_epi32(_mm256_setzero_si256(), b), _mm256_set1_epi32(0x3F))); }
VK__OP2(vk_rotm, vk__rotm1)
VK__I __m256i vk__shl1(__m256i a, __m256i b) { return _mm256_sllv_epi32(a, _mm256_and_si256(b, _mm256_set1_epi32(0x3F))); }
VK__OP2(vk_shl, vk__shl1)
static inline vk vk_rotqbii(vk a, int32_t s) {
    s &= 7; if (!s) return a;
    const __m128i c = _mm_cvtsi32_si128(s), d = _mm_cvtsi32_si128(32 - s);
    const __m256i nl = _mm256_shuffle_epi32(a.l, 0x39), nh = _mm256_shuffle_epi32(a.h, 0x39);
    return vk__mk(_mm256_or_si256(_mm256_sll_epi32(a.l, c), _mm256_srl_epi32(nl, d)), _mm256_or_si256(_mm256_sll_epi32(a.h, c), _mm256_srl_epi32(nh, d)));
}
static inline vk vk_shlqbii(vk a, int32_t s) {
    s &= 7; if (!s) return a;
    const __m128i c = _mm_cvtsi32_si128(s), d = _mm_cvtsi32_si128(32 - s);
    const __m256i k = _mm256_setr_epi32(-1, -1, -1, 0, -1, -1, -1, 0);
    const __m256i nl = _mm256_and_si256(_mm256_shuffle_epi32(a.l, 0x39), k), nh = _mm256_and_si256(_mm256_shuffle_epi32(a.h, 0x39), k);
    return vk__mk(_mm256_or_si256(_mm256_sll_epi32(a.l, c), _mm256_srl_epi32(nl, d)), _mm256_or_si256(_mm256_sll_epi32(a.h, c), _mm256_srl_epi32(nh, d)));
}
static inline vk vk__perm_bytes(vk a, const int8_t idx[16]) {
    __m128i x; memcpy(&x, idx, 16);
    const __m256i i = vk__b128(x);
    return vk__mk(_mm256_shuffle_epi8(a.l, i), _mm256_shuffle_epi8(a.h, i));
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
    if (s >= 16) return vk__zero();
    for (int h = 0; h < 16; h++) { const int q = (h ^ 3) - s; idx[h] = (int8_t)(q >= 0 ? (q ^ 3) : 0x80); }
    return vk__perm_bytes(a, idx);
}
VK__I __m256i vk__h3(void) {
    return _mm256_setr_epi8(3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12, 3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12);
}
VK__I __m256i vk__rotqby1(__m256i a, __m256i b) {
    const __m256i n = _mm256_and_si256(_mm256_shuffle_epi8(b, _mm256_setzero_si256()), _mm256_set1_epi8(0x0F));
    const __m256i idx = _mm256_xor_si256(_mm256_and_si256(_mm256_add_epi8(vk__h3(), n), _mm256_set1_epi8(0x0F)), _mm256_set1_epi8(3));
    return _mm256_shuffle_epi8(a, idx);
}
VK__OP2(vk_rotqby, vk__rotqby1)
VK__I __m256i vk__pw1(__m256i b) { return _mm256_shuffle_epi32(b, 0x00); }
VK__I __m256i vk__rotqmbybi1(__m256i a, __m256i b) {
    const __m256i n = _mm256_and_si256(_mm256_sub_epi32(_mm256_setzero_si256(), _mm256_srai_epi32(vk__pw1(b), 3)), _mm256_set1_epi32(0x1F));
    const __m256i nb = _mm256_shuffle_epi8(n, _mm256_setzero_si256());
    return _mm256_shuffle_epi8(a, _mm256_xor_si256(_mm256_sub_epi8(vk__h3(), nb), _mm256_set1_epi8(3)));
}
VK__OP2(vk_rotqmbybi, vk__rotqmbybi1)
VK__I __m256i vk__rotqmbi1(__m256i a, __m256i b) {
    const __m256i n = _mm256_and_si256(_mm256_sub_epi32(_mm256_setzero_si256(), vk__pw1(b)), _mm256_set1_epi32(7));
    const __m256i prev = _mm256_bslli_epi128(a, 4);
    return _mm256_or_si256(_mm256_srlv_epi32(a, n), _mm256_sllv_epi32(prev, _mm256_sub_epi32(_mm256_set1_epi32(32), n)));
}
VK__OP2(vk_rotqmbi, vk__rotqmbi1)
VK__I __m256i vk__rotma1(__m256i a, __m256i b) { return _mm256_srav_epi32(a, _mm256_and_si256(_mm256_sub_epi32(_mm256_setzero_si256(), b), _mm256_set1_epi32(0x3F))); }
VK__OP2(vk_rotma, vk__rotma1)
/* rothm: 16-bit logical right shift by (0 - b) & 0x1F, per halfword (no vpsrlvw in AVX2) */
VK__I __m256i vk__rothm1(__m256i a, __m256i b) {
    const __m256i n = _mm256_and_si256(_mm256_sub_epi16(_mm256_setzero_si256(), b), _mm256_set1_epi16(0x1F));
    const __m256i lo16 = _mm256_set1_epi32(0xFFFF);
    const __m256i lo = _mm256_srlv_epi32(_mm256_and_si256(a, lo16), _mm256_and_si256(n, lo16));
    const __m256i hi = _mm256_slli_epi32(_mm256_srlv_epi32(_mm256_srli_epi32(a, 16), _mm256_srli_epi32(n, 16)), 16);
    return _mm256_or_si256(_mm256_and_si256(lo, lo16), hi);
}
VK__OP2(vk_rothm, vk__rothm1)
VK__I __m256i vk__rot1(__m256i a, __m256i b) {
    const __m256i s = _mm256_and_si256(b, _mm256_set1_epi32(31));
    return _mm256_or_si256(_mm256_sllv_epi32(a, s), _mm256_srlv_epi32(a, _mm256_sub_epi32(_mm256_set1_epi32(32), s)));
}
VK__OP2(vk_rot, vk__rot1)
static inline vk vk_sfh(vk a, vk b) { return vk__mk(_mm256_sub_epi16(b.l, a.l), _mm256_sub_epi16(b.h, a.h)); }
VK__OP2(vk_ah, _mm256_add_epi16)
VK__I __m256i vk__cntb1(__m256i a) {
    const __m256i lut = _mm256_setr_epi8(0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4, 0, 1, 1, 2, 1, 2, 2, 3, 1, 2, 2, 3, 2, 3, 3, 4);
    const __m256i m = _mm256_set1_epi8(0x0F);
    return _mm256_add_epi8(_mm256_shuffle_epi8(lut, _mm256_and_si256(a, m)), _mm256_shuffle_epi8(lut, _mm256_and_si256(_mm256_srli_epi16(a, 4), m)));
}
static inline vk vk_cntb(vk a) { return vk__mk(vk__cntb1(a.l), vk__cntb1(a.h)); }
VK__I __m256i vk__soma1(__m256i m) {
    const __m256i sd = _mm256_sad_epu8(m, _mm256_setzero_si256());
    return _mm256_and_si256(_mm256_add_epi64(sd, _mm256_bsrli_epi128(sd, 8)), _mm256_setr_epi32(-1, 0, 0, 0, -1, 0, 0, 0));
}
static inline vk vk_gbh(vk a) {
    const __m256i w = _mm256_setr_epi16(64, 128, 16, 32, 4, 8, 1, 2, 64, 128, 16, 32, 4, 8, 1, 2);
    const __m256i o = _mm256_set1_epi16(1);
    return vk__mk(vk__soma1(_mm256_mullo_epi16(_mm256_and_si256(a.l, o), w)), vk__soma1(_mm256_mullo_epi16(_mm256_and_si256(a.h, o), w)));
}
static inline vk vk_gb(vk a) {
    const __m256i w = _mm256_setr_epi32(8, 4, 2, 1, 8, 4, 2, 1);
    const __m256i o = _mm256_set1_epi32(1);
    return vk__mk(vk__soma1(_mm256_mullo_epi32(_mm256_and_si256(a.l, o), w)), vk__soma1(_mm256_mullo_epi32(_mm256_and_si256(a.h, o), w)));
}
VK__I __m256i vk__cxflt1(__m256i fv, int i8) {
    const __m256i be = _mm256_and_si256(_mm256_srli_epi32(fv, 23), _mm256_set1_epi32(0xFF));
    const __m256i nb = _mm256_add_epi32(be, _mm256_set1_epi32((i8 & 0xFF) - 155));
    const __m256i sg = _mm256_and_si256(fv, _mm256_set1_epi32((int)0x80000000u));
    __m256i r = _mm256_or_si256(_mm256_and_si256(fv, _mm256_set1_epi32((int)0x807FFFFFu)), _mm256_slli_epi32(nb, 23));
    r = _mm256_blendv_epi8(r, _mm256_or_si256(sg, _mm256_set1_epi32(0x7FFFFFFF)), _mm256_cmpgt_epi32(nb, _mm256_set1_epi32(255)));
    const __m256i zero = _mm256_or_si256(_mm256_cmpeq_epi32(be, _mm256_setzero_si256()), _mm256_cmpgt_epi32(_mm256_set1_epi32(1), nb));
    return _mm256_andnot_si256(zero, r);
}
/* integer -> float under MXCSR round-toward-zero (SPU_SX_RZ): signed in one conversion;
 * unsigned as hi16 * 65536 + lo16 in one FMA (both halves exact, one rounding) */
VK__I __m256i vk__csflt1(__m256i a) { return _mm256_castps_si256(_mm256_cvtepi32_ps(a)); }
VK__I __m256i vk__cuflt1(__m256i a) {
    const __m256 hi = _mm256_cvtepi32_ps(_mm256_srli_epi32(a, 16));
    const __m256 lo = _mm256_cvtepi32_ps(_mm256_and_si256(a, _mm256_set1_epi32(0xFFFF)));
    return _mm256_castps_si256(_mm256_fmadd_ps(hi, _mm256_set1_ps(65536.0f), lo));
}
static inline vk vk_csflt(vk a, int32_t i8) { return vk__mk(vk__cxflt1(vk__csflt1(a.l), i8), vk__cxflt1(vk__csflt1(a.h), i8)); }
static inline vk vk_cuflt(vk a, int32_t i8) { return vk__mk(vk__cxflt1(vk__cuflt1(a.l), i8), vk__cxflt1(vk__cuflt1(a.h), i8)); }
VK__I __m256i vk__cfl1(__m256i x, int i8, int uns) {
    const __m256i be = _mm256_and_si256(_mm256_srli_epi32(x, 23), _mm256_set1_epi32(0xFF));
    const __m256i m  = _mm256_or_si256(_mm256_and_si256(x, _mm256_set1_epi32(0x7FFFFF)), _mm256_set1_epi32(0x800000));
    const __m256i sh = _mm256_add_epi32(be, _mm256_set1_epi32(173 - (i8 & 0xFF) - 150));
    const __m256i v  = _mm256_blendv_epi8(_mm256_srlv_epi32(m, _mm256_sub_epi32(_mm256_setzero_si256(), sh)),
                                          _mm256_sllv_epi32(m, sh), _mm256_cmpgt_epi32(sh, _mm256_set1_epi32(-1)));
    const __m256i neg = _mm256_srai_epi32(x, 31);
    const __m256i isz = _mm256_cmpeq_epi32(be, _mm256_setzero_si256());
    __m256i r;
    if (uns) {
        r = _mm256_blendv_epi8(v, _mm256_set1_epi32(-1), _mm256_cmpgt_epi32(sh, _mm256_set1_epi32(8)));
        r = _mm256_andnot_si256(_mm256_or_si256(isz, neg), r);
    } else {
        const __m256i sat = _mm256_xor_si256(_mm256_set1_epi32(0x7FFFFFFF), neg);
        r = _mm256_sub_epi32(_mm256_xor_si256(v, neg), neg);
        r = _mm256_blendv_epi8(r, sat, _mm256_cmpgt_epi32(sh, _mm256_set1_epi32(7)));
        r = _mm256_andnot_si256(isz, r);
    }
    return r;
}
static inline vk vk_cflts(vk a, int32_t i8) { return vk__mk(vk__cfl1(a.l, i8, 0), vk__cfl1(a.h, i8, 0)); }
static inline vk vk_cfltu(vk a, int32_t i8) { return vk__mk(vk__cfl1(a.l, i8, 1), vk__cfl1(a.h, i8, 1)); }

/* ---- shufb per lane: two pshufb (a, b) blended on bit 4, then the special bytes ---- */
VK__I __m256i vk__shufb1(__m256i a, __m256i b, __m256i c) {
    const __m256i lo = _mm256_and_si256(_mm256_xor_si256(c, _mm256_set1_epi8(3)), _mm256_set1_epi8(0x0F));
    const __m256i r = _mm256_blendv_epi8(_mm256_shuffle_epi8(a, lo), _mm256_shuffle_epi8(b, lo), _mm256_slli_epi16(c, 3));
    const __m256i lut = _mm256_setr_epi8(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, (char)0xFF, (char)0xFF, (char)0x80, (char)0x80,
                                         0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, (char)0xFF, (char)0xFF, (char)0x80, (char)0x80);
    const __m256i sp = _mm256_shuffle_epi8(lut, _mm256_and_si256(_mm256_srli_epi16(c, 4), _mm256_set1_epi8(0x0F)));
    return _mm256_blendv_epi8(r, sp, c);
}
static inline vk vk_shufb(vk a, vk b, vk c) { return vk__mk(vk__shufb1(a.l, b.l, c.l), vk__shufb1(a.h, b.h, c.h)); }

/* ---- estimates (gathers for the 64/32-entry tables) ---- */
VK__I __m256i vk__frsqest1(__m256i x) {
    const __m256i idx = _mm256_and_si256(_mm256_srli_epi32(x, 18), _mm256_set1_epi32(0x3F));
    const __m256i frac = _mm256_and_si256(_mm256_i32gather_epi32((const int*)spu_frsqest_fraction_lut, idx, 4), _mm256_set1_epi32(0x7FFFFF));
    const __m256i exp = _mm256_and_si256(_mm256_srli_epi32(x, 23), _mm256_set1_epi32(0xFF));
    __m256i rexp = _mm256_sub_epi32(_mm256_set1_epi32(190), _mm256_srli_epi32(_mm256_add_epi32(exp, _mm256_set1_epi32(1)), 1));
    rexp = _mm256_blendv_epi8(rexp, _mm256_set1_epi32(0xFF), _mm256_cmpeq_epi32(exp, _mm256_setzero_si256()));
    return _mm256_or_si256(_mm256_slli_epi32(rexp, 23), frac);
}
static inline vk vk_frsqest(vk x) { return vk__mk(vk__frsqest1(x.l), vk__frsqest1(x.h)); }
VK__I __m256i vk__frest1(__m256i x) {
    const __m256i idx = _mm256_and_si256(_mm256_srli_epi32(x, 18), _mm256_set1_epi32(0x1F));
    const __m256i frac = _mm256_and_si256(_mm256_i32gather_epi32((const int*)spu_frest_fraction_lut, idx, 4), _mm256_set1_epi32(0x7FFFFF));
    const __m256i exp = _mm256_and_si256(_mm256_srli_epi32(x, 23), _mm256_set1_epi32(0xFF));
    __m256i rexp = _mm256_sub_epi32(_mm256_set1_epi32(253), exp);
    rexp = _mm256_blendv_epi8(rexp, _mm256_setzero_si256(), _mm256_cmpgt_epi32(exp, _mm256_set1_epi32(252)));   /* exp in 0..255 */
    rexp = _mm256_blendv_epi8(rexp, _mm256_set1_epi32(255), _mm256_cmpeq_epi32(exp, _mm256_setzero_si256()));
    return _mm256_or_si256(_mm256_and_si256(x, _mm256_set1_epi32((int)0x80000000u)), _mm256_or_si256(_mm256_slli_epi32(rexp, 23), frac));
}
static inline vk vk_frest(vk x) { return vk__mk(vk__frest1(x.l), vk__frest1(x.h)); }
VK__I __m256i vk__fi1(__m256i a, __m256i b) {
    const __m256i exp = _mm256_and_si256(_mm256_srli_epi32(b, 23), _mm256_set1_epi32(0xFF));
    const __m256i base32 = _mm256_or_si256(_mm256_and_si256(_mm256_srli_epi32(b, 10), _mm256_set1_epi32(0x1FFF)), _mm256_set1_epi32(0x2000));
    const __m256i prod = _mm256_mullo_epi32(_mm256_and_si256(b, _mm256_set1_epi32(0x3FF)), _mm256_and_si256(a, _mm256_set1_epi32(0x7FFFF)));
    const __m256i m8 = _mm256_sub_epi32(_mm256_slli_epi32(base32, 11), _mm256_srli_epi32(_mm256_add_epi32(prod, _mm256_set1_epi32(255)), 8));
    const __m256i lo = _mm256_cmpgt_epi32(_mm256_set1_epi32(1 << 24), m8);   /* m8 in [0, 2^25) */
    const __m256i frac = _mm256_and_si256(_mm256_blendv_epi8(_mm256_srli_epi32(m8, 1), m8, lo), _mm256_set1_epi32(0x7FFFFF));
    const __m256i e2 = _mm256_add_epi32(exp, lo);                            /* exp - 1 where normalised */
    const __m256i r = _mm256_or_si256(_mm256_and_si256(b, _mm256_set1_epi32((int)0x80000000u)), _mm256_or_si256(_mm256_slli_epi32(e2, 23), frac));
    return _mm256_andnot_si256(_mm256_or_si256(_mm256_cmpeq_epi32(exp, _mm256_setzero_si256()), _mm256_cmpeq_epi32(e2, _mm256_setzero_si256())), r);
}
static inline vk vk_fi(vk a, vk b) { return vk__mk(vk__fi1(a.l, b.l), vk__fi1(a.h, b.h)); }

/* ---- memory conflicts inside a group (same rules as spu_vk.h) ---- */
VK__I unsigned vk__cmpm(vk s, vk l) { return vk__m(vk_ceq(s, l)); }
static inline vk vk__bsr(vk v, int n) {
    switch (n) {
    case 4: return vk__mk(_mm256_bsrli_epi128(v.l, 4), _mm256_bsrli_epi128(v.h, 4));
    case 8: return vk__mk(_mm256_bsrli_epi128(v.l, 8), _mm256_bsrli_epi128(v.h, 8));
    default: return vk__mk(_mm256_bsrli_epi128(v.l, 12), _mm256_bsrli_epi128(v.h, 12));
    }
}
static inline int vk_conflito_n(const uint32_t (*sta)[4], int nst, const uint32_t (*lda)[4], int nld, uint32_t nv)
{
    const unsigned r1 = nv > 1 ? (1u << (nv - 1)) - 1u : 0u, r2 = nv > 2 ? (1u << (nv - 2)) - 1u : 0u, r3 = nv > 3 ? 1u : 0u;
    const unsigned k1 = r1 * 0x1111u, k2 = r2 * 0x1111u, k3 = r3 * 0x1111u;
    unsigned m = 0;
    for (int b = 0; b < nld; b += 4) {
        const int b1 = b + 1 < nld ? b + 1 : b, b2 = b + 2 < nld ? b + 2 : b1, b3 = b + 3 < nld ? b + 3 : b2;
        const vk ld = vk_joinx(_mm_loadu_si128((const __m128i*)lda[b]), _mm_loadu_si128((const __m128i*)lda[b1]),
                               _mm_loadu_si128((const __m128i*)lda[b2]), _mm_loadu_si128((const __m128i*)lda[b3]));
        const vk l1 = vk__bsr(ld, 4), l2 = vk__bsr(ld, 8), l3 = vk__bsr(ld, 12);
        for (int a = 0; a < nst; a++) {
            const vk s = vk__bc256(vk__b128(_mm_loadu_si128((const __m128i*)sta[a])));
            m |= vk__cmpm(s, l1) & k1; m |= vk__cmpm(s, l2) & k2; m |= vk__cmpm(s, l3) & k3;
        }
    }
    return m != 0;
}
static inline int vk_conflito(const uint32_t (*sta)[4], int nst, const uint32_t (*lda)[4], int nld)
{
    unsigned m = 0;
    for (int b = 0; b < nld; b += 4) {
        const int b1 = b + 1 < nld ? b + 1 : b, b2 = b + 2 < nld ? b + 2 : b1, b3 = b + 3 < nld ? b + 3 : b2;
        const vk ld = vk_joinx(_mm_loadu_si128((const __m128i*)lda[b]), _mm_loadu_si128((const __m128i*)lda[b1]),
                               _mm_loadu_si128((const __m128i*)lda[b2]), _mm_loadu_si128((const __m128i*)lda[b3]));
        const vk l1 = vk__bsr(ld, 4), l2 = vk__bsr(ld, 8), l3 = vk__bsr(ld, 12);
        for (int a = 0; a < nst; a++) {
            const vk s = vk__bc256(vk__b128(_mm_loadu_si128((const __m128i*)sta[a])));
            m |= vk__cmpm(s, l1) & 0x7777u; m |= vk__cmpm(s, l2) & 0x3333u; m |= vk__cmpm(s, l3) & 0x1111u;
        }
    }
    return m != 0;
}
static inline __mmask16 vk_mesmo_end(const uint32_t sa[4], const uint32_t la[4]) {
    const unsigned m4 = (unsigned)_mm_movemask_ps(_mm_castsi128_ps(_mm_cmpeq_epi32(_mm_loadu_si128((const __m128i*)sa), _mm_loadu_si128((const __m128i*)la))));
    return (__mmask16)(_pdep_u32(m4, 0x1111u) * 0xFu);
}
static inline vk vk_xsel(uint32_t l) { vk r = vk__zero(); r.l = _mm256_set1_epi32((int)l); return r; }
static inline __m128i vk_xs(vk v, vk sel) { return vk_x(v, (int)((uint32_t)_mm_cvtsi128_si32(_mm256_castsi256_si128(sel.l)) & 3u)); }
static inline __m128i vk_xl(vk v, uint32_t l) { return vk_x(v, (int)(l & 3u)); }
static __attribute__((noinline, cold)) void vk_par_mostra(const char* nome, int* vezes, int kst, int kld, uint32_t end)
{
    static int on = -1;
    if (on < 0) on = getenv("GOWA_VK_CONFDBG") ? 1 : 0;
    if (!on || *vezes >= 8) return;
    (*vezes)++;
    fprintf(stderr, "[vk] par %s: gravação (comando %d) e leitura (comando %d) da mesma iteração em 0x%05X\n", nome, kst, kld, end);
}
static inline void vk_conflito_mostra(const char* nome, int* vezes, const uint32_t (*sta)[4], int nst, const uint32_t (*lda)[4], int nld)
{
    static int on = -1;
    if (on < 0) on = getenv("GOWA_VK_CONFDBG") ? 1 : 0;
    if (!on || *vezes >= 6) return;
    for (int a = 0; a < nst; a++) for (int b = 0; b < nld; b++)
        for (int j = 0; j < 3; j++) for (int L = j + 1; L < 4; L++)
            if (sta[a][j] == lda[b][L]) {
                (*vezes)++;
                fprintf(stderr, "[vk] conflito %s: gravação %d (iter %d) = leitura %d (iter %d) em 0x%05X\n", nome, a, j, b, L, sta[a][j]);
                return;
            }
}

/* ---- the AVX-512 intrinsics the generated kernels call directly ---- */
VK__I vk vke_bcx(__m128i x) { return vk__bc256(vk__b128(x)); }
VK__I vk vke_sub(vk a, vk b) { return vk__mk(_mm256_sub_epi32(a.l, b.l), _mm256_sub_epi32(a.h, b.h)); }
VK__I vk vke_mask_mov(vk src, __mmask16 k, vk a) { return vk__blend(src, a, vk__xm(k)); }
VK__I __mmask16 vke_mask_cmplt_epu32(__mmask16 k, vk a, vk b) { return (__mmask16)(k & vk__m(vk_clgt(b, a))); }
VK__I void vke_storeu(void* p, vk v) { _mm256_storeu_si256((__m256i*)p, v.l); _mm256_storeu_si256((__m256i*)p + 1, v.h); }
VK__I vk vke_loadu(const void* p) { return vk__mk(_mm256_loadu_si256((const __m256i*)p), _mm256_loadu_si256((const __m256i*)p + 1)); }
VK__I vk vke_setr(int e0, int e1, int e2, int e3, int e4, int e5, int e6, int e7,
                  int e8, int e9, int e10, int e11, int e12, int e13, int e14, int e15) {
    return vk__mk(_mm256_setr_epi32(e0, e1, e2, e3, e4, e5, e6, e7), _mm256_setr_epi32(e8, e9, e10, e11, e12, e13, e14, e15));
}
VK__I __m128i vke_cast128(vk v) { return _mm256_castsi256_si128(v.l); }
VK__I vk vke_permvar(vk idx, vk v) {
    uint32_t t[16], i[16], r[16];
    vke_storeu(t, v); vke_storeu(i, idx);
    for (int k = 0; k < 16; k++) r[k] = t[i[k] & 15u];
    return vke_loadu(r);
}
VK__I __mmask8 vke_mm_cmpeq_mask(__m128i a, __m128i b) { return (__mmask8)_mm_movemask_ps(_mm_castsi128_ps(_mm_cmpeq_epi32(a, b))); }
#define _mm512_broadcast_i32x4(x)            vke_bcx(x)
#define _mm512_add_epi32(a, b)               vk_a((a), (b))
#define _mm512_sub_epi32(a, b)               vke_sub((a), (b))
#define _mm512_and_si512(a, b)               vk_and((a), (b))
#define _mm512_or_si512(a, b)                vk_or((a), (b))
#define _mm512_set1_epi32(i)                 vk__set1((int32_t)(i))
#define _mm512_mask_mov_epi32(s, k, a)       vke_mask_mov((s), (k), (a))
#define _mm512_mask_cmplt_epu32_mask(k, a, b) vke_mask_cmplt_epu32((k), (a), (b))
#define _mm512_setzero_si512()               vk__zero()
#define _mm512_storeu_si512(p, v)            vke_storeu((p), (v))
#define _mm512_loadu_si512(p)                vke_loadu(p)
#define _mm512_setr_epi32                    vke_setr
#define _mm512_castsi512_si128(v)            vke_cast128(v)
#define _mm512_permutexvar_epi32(i, v)       vke_permvar((i), (v))
#define _mm_cmpeq_epi32_mask(a, b)           vke_mm_cmpeq_mask((a), (b))
#undef VK__OP2
#endif /* SPU_VK_AVX2_H */
