/*
 * ps3recomp - SPU instruction semantics helpers
 *
 * Pure-C, header-only implementation of the per-instruction semantics used
 * by lifter-generated code. Extracted from spu_lifter.py so the helpers
 * have one source of truth and can be unit-tested directly (see
 * runtime/spu/tests/test_spu_helpers.c).
 *
 * Each helper is `static inline u128 spu_<mnemonic>(...)`. Naming, lane
 * widths and big-endian conventions match runtime/spu/spu_context.h.
 */

#ifndef SPU_HELPERS_H
#define SPU_HELPERS_H

#include "spu_context.h"
#include <string.h>
#include <math.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef _MSC_VER
#include <intrin.h>
static inline int spu_clz32(uint32_t x) {
    unsigned long idx;
    if (!x) return 32;
    _BitScanReverse(&idx, x);
    return 31 - (int)idx;
}
#else
static inline int spu_clz32(uint32_t x) { return x ? __builtin_clz(x) : 32; }
#endif

/* Architectural SPU barrier -- the lifted `sync`/`dsync`/`syncc` emission.
 * SPU ISA v1.2 §13 (+ the per-instruction pages): order all earlier LS and
 * channel accesses before later ones; LS must be consistent "if it were\n * observed by another entity" (Table 13-6 is exactly this runtime's
 * host-thread producer -> SPU consumer handoff). A no-op is neither a
 * compiler nor a CPU barrier -- MSVC at /O2 may reorder plain LS accesses
 * across an empty statement (the PPU twin of this bug is LESSONS #11d). A
 * full fence: the locked exchange forces total order and is opaque to the
 * optimizer. */
#ifdef _MSC_VER
static inline void spu_arch_fence(void) { long spu_fence_tmp = 0; _InterlockedExchange(&spu_fence_tmp, 1); }
#else
static inline void spu_arch_fence(void) { __atomic_thread_fence(__ATOMIC_SEQ_CST); }
#endif

/* SPU byte position -> our _u8 index.
 * Our u128 is HOST-NATIVE little-endian (_u32[i]=SPU word i as a value via
 * spu_ls_read128's big-endian load), so within each 4-byte word the _u8[]
 * bytes are reversed vs SPU big-endian byte order. Any op defined in terms of
 * SPU *byte positions* (quadword byte rotates/shifts, gen-controls, shuffle
 * insertion) must map SPU byte P to our index W(P). Word-aligned operations
 * are unaffected (W is identity modulo the within-word reversal). */
#define SPU_W(P) (((P) & ~3) | (3 - ((P) & 3)))

#if defined(__ARM_NEON) && !defined(SPU_SCALAR_HELPERS) && !defined(SPU_SCALAR_INT)
#include <arm_neon.h>
/* NEON forms of the hot integer ops. Each scalar definition stays as spu_X_ref
 * (tests/test_spu_int_neon.c compares them). */
static inline uint32x4_t spu__ld(u128 a) { return vld1q_u32(a._u32); }
static inline u128 spu__st(uint32x4_t v) { u128 r; vst1q_u32(r._u32, v); return r; }
/* Byte permute in SPU byte order: result SPU byte i = a's SPU byte I[i], or 0
 * when I[i] >= 16. SPU byte P is host byte P^3, so the host-order table index
 * is rev32(I) ^ 3 (out-of-range stays out of range). */
static inline u128 spu__perm(u128 a, uint8x16_t I) {
    uint8x16_t ih = veorq_u8(vrev32q_u8(I), vdupq_n_u8(3));
    u128 r; vst1q_u8(r._u8, vqtbl1q_u8(vld1q_u8(a._u8), ih)); return r;
}
static inline uint8x16_t spu__iota(void) {
    static const uint8_t k[16] = {0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15};
    return vld1q_u8(k);
}
#endif

/* GOWA (23/10): SPU__XV -- the integer operations as explicit SSE/AVX-512 on x86
 * (scripts/spu_ops_xv.py; the _ref loops were usually, but not always, vectorized
 * by the compiler). SPU_NO_XV turns them off. */
#if (defined(__x86_64__) || defined(_M_X64)) && defined(__SSE4_1__) && \
    ((defined(__AVX512F__) && defined(__AVX512VL__) && defined(__AVX512BW__)) || defined(__AVX2__)) && \
    !defined(__ARM_NEON) && \
    !defined(SPU_SCALAR_HELPERS) && !defined(SPU_SCALAR_INT) && !defined(SPU_NO_XV)
#define SPU__XV 1
#include <immintrin.h>
static inline __m128i spu__xv_ld(u128 v) { __m128i r; memcpy(&r, &v, 16); return r; }
static inline u128 spu__xv_st(__m128i v) { u128 r; memcpy(&r, &v, 16); return r; }
/* GOWA (24/10): the XV forms that need AVX-512 -- unsigned compares, eqv, cg/bg,
 * variable rotates and the 16-bit variable shifts -- go through these, so the block
 * also builds for x86-64-v3 (AVX2), where they are emulated. Checked against the
 * _ref forms by scratchpad t_xv.c built both ways. */
#if defined(__AVX512F__) && defined(__AVX512VL__) && defined(__AVX512BW__)
#define spu__xv_movm32(m) _mm_movm_epi32(m)
#define spu__xv_movm16(m) _mm_movm_epi16(m)
#define spu__xv_movm8(m)  _mm_movm_epi8(m)
#define spu__xv_cmpgtu32(a, b) _mm_cmpgt_epu32_mask(a, b)
#define spu__xv_cmpgtu16(a, b) _mm_cmpgt_epu16_mask(a, b)
#define spu__xv_cmpgtu8(a, b)  _mm_cmpgt_epu8_mask(a, b)
#define spu__xv_eqv(a, b)    _mm_ternarylogic_epi32(a, b, b, 0xC3)
#define spu__xv_cg(a, b)     _mm_maskz_mov_epi32(_mm_cmplt_epu32_mask(_mm_add_epi32(a, b), a), _mm_set1_epi32(1))
#define spu__xv_bg(a, b)     _mm_maskz_mov_epi32(_mm_cmpge_epu32_mask(b, a), _mm_set1_epi32(1))
#define spu__xv_rolv32(a, b) _mm_rolv_epi32(a, b)
#define spu__xv_sllv16(a, b) _mm_sllv_epi16(a, b)
#define spu__xv_srlv16(a, b) _mm_srlv_epi16(a, b)
#define spu__xv_srav16(a, b) _mm_srav_epi16(a, b)
#else
static inline __m128i spu__xv_movm32(__m128i m) { return m; }
static inline __m128i spu__xv_movm16(__m128i m) { return m; }
static inline __m128i spu__xv_movm8(__m128i m)  { return m; }
static inline __m128i spu__xv_cmpgtu32(__m128i a, __m128i b) {
    const __m128i s = _mm_set1_epi32((int)0x80000000u); return _mm_cmpgt_epi32(_mm_xor_si128(a, s), _mm_xor_si128(b, s)); }
static inline __m128i spu__xv_cmpgtu16(__m128i a, __m128i b) {
    const __m128i s = _mm_set1_epi16((short)0x8000); return _mm_cmpgt_epi16(_mm_xor_si128(a, s), _mm_xor_si128(b, s)); }
static inline __m128i spu__xv_cmpgtu8(__m128i a, __m128i b) {
    const __m128i s = _mm_set1_epi8((char)0x80); return _mm_cmpgt_epi8(_mm_xor_si128(a, s), _mm_xor_si128(b, s)); }
static inline __m128i spu__xv_eqv(__m128i a, __m128i b) { return _mm_xor_si128(_mm_xor_si128(a, b), _mm_set1_epi32(-1)); }
/* carry of a + b: (a + b) <u a; borrow of b - a: b >=u a, i.e. not a >u b */
static inline __m128i spu__xv_cg(__m128i a, __m128i b) { return _mm_srli_epi32(spu__xv_cmpgtu32(a, _mm_add_epi32(a, b)), 31); }
static inline __m128i spu__xv_bg(__m128i a, __m128i b) { return _mm_srli_epi32(_mm_xor_si128(spu__xv_cmpgtu32(a, b), _mm_set1_epi32(-1)), 31); }
static inline __m128i spu__xv_rolv32(__m128i a, __m128i b) {
    const __m128i n = _mm_and_si128(b, _mm_set1_epi32(31));
    return _mm_or_si128(_mm_sllv_epi32(a, n), _mm_srlv_epi32(a, _mm_sub_epi32(_mm_set1_epi32(32), n))); }
/* 16-bit lanes through the 32-bit variable shifts, low and high halfword apart;
 * a count above 15 gives 0 (logical) or the sign (arithmetic), as vpsxxvw */
static inline __m128i spu__xv_sllv16(__m128i a, __m128i n) {
    const __m128i lo = _mm_set1_epi32(0xFFFF);
    return _mm_or_si128(_mm_and_si128(_mm_sllv_epi32(_mm_and_si128(a, lo), _mm_and_si128(n, lo)), lo),
                        _mm_sllv_epi32(_mm_andnot_si128(lo, a), _mm_srli_epi32(n, 16))); }
static inline __m128i spu__xv_srlv16(__m128i a, __m128i n) {
    const __m128i lo = _mm_set1_epi32(0xFFFF);
    return _mm_or_si128(_mm_srlv_epi32(_mm_and_si128(a, lo), _mm_and_si128(n, lo)),
                        _mm_andnot_si128(lo, _mm_srlv_epi32(_mm_andnot_si128(lo, a), _mm_srli_epi32(n, 16)))); }
static inline __m128i spu__xv_srav16(__m128i a, __m128i n) {
    const __m128i lo = _mm_set1_epi32(0xFFFF);
    return _mm_or_si128(_mm_srli_epi32(_mm_srav_epi32(_mm_slli_epi32(a, 16), _mm_and_si128(n, lo)), 16),
                        _mm_andnot_si128(lo, _mm_srav_epi32(a, _mm_srli_epi32(n, 16)))); }
#endif
#endif

/* Integer ops with a NEON form further down (end of file); the scalar definitions are
 * spu_X_ref, checked against the NEON forms by tests/test_spu_int_neon.c. */
static inline u128 spu_absdb(u128 a, u128 b);
static inline u128 spu_avgb(u128 a, u128 b);
static inline u128 spu_ceqb(u128 a, u128 b);
static inline u128 spu_cgtb(u128 a, u128 b);
static inline u128 spu_clgtb(u128 a, u128 b);
static inline u128 spu_ceqh(u128 a, u128 b);
static inline u128 spu_cgth(u128 a, u128 b);
static inline u128 spu_clgth(u128 a, u128 b);
static inline u128 spu_ah(u128 a, u128 b);
static inline u128 spu_sfh(u128 a, u128 b);
static inline u128 spu_shlh(u128 a, u128 b);
static inline u128 spu_roth(u128 a, u128 b);
static inline u128 spu_rothm(u128 a, u128 b);
static inline u128 spu_rothma(u128 a, u128 b);
static inline u128 spu_shl(u128 a, u128 b);
static inline u128 spu_rot(u128 a, u128 b);
static inline u128 spu_rotm(u128 a, u128 b);
static inline u128 spu_rotma(u128 a, u128 b);
static inline u128 spu_cg(u128 a, u128 b);
static inline u128 spu_bg(u128 a, u128 b);
static inline u128 spu_mpy(u128 a, u128 b);
static inline u128 spu_mpyu(u128 a, u128 b);
static inline u128 spu_mpyh(u128 a, u128 b);
static inline u128 spu_mpyhh(u128 a, u128 b);
static inline u128 spu_mpyhhu(u128 a, u128 b);
static inline u128 spu_mpys(u128 a, u128 b);
static inline u128 spu_sumb(u128 a, u128 b);
static inline u128 spu_ceqbi(u128 a, int32_t imm);
static inline u128 spu_cgtbi(u128 a, int32_t imm);
static inline u128 spu_clgtbi(u128 a, int32_t imm);
static inline u128 spu_ceqhi(u128 a, int32_t imm);
static inline u128 spu_cgthi(u128 a, int32_t imm);
static inline u128 spu_clgthi(u128 a, int32_t imm);
static inline u128 spu_ahi(u128 a, int32_t imm);
static inline u128 spu_sfhi(u128 a, int32_t imm);
static inline u128 spu_mpyi(u128 a, int32_t imm);
static inline u128 spu_mpyui(u128 a, int32_t imm);
static inline u128 spu_rothi(u128 a, int sh);
static inline u128 spu_roti(u128 a, int sh);
static inline u128 spu_rotmahi(u128 a, int i7);
static inline u128 spu_rotmai(u128 a, int i7);
static inline u128 spu_mpya(u128 a, u128 b, u128 c);
static inline u128 spu_mpyhha(u128 a, u128 b, u128 t);
static inline u128 spu_mpyhhau(u128 a, u128 b, u128 t);
static inline u128 spu_addx(u128 a, u128 b, u128 t);
static inline u128 spu_sfx(u128 a, u128 b, u128 t);
static inline u128 spu_cgx(u128 a, u128 b, u128 t);
static inline u128 spu_bgx(u128 a, u128 b, u128 t);
static inline u128 spu_cntb(u128 a);
static inline u128 spu_xsbh(u128 a);
static inline u128 spu_xshw(u128 a);
static inline u128 spu_clz(u128 a);
static inline u128 spu_xswd(u128 a);
static inline u128 spu_orx(u128 a);
static inline u128 spu_gb(u128 a);
static inline u128 spu_gbh(u128 a);
static inline u128 spu_gbb(u128 a);
static inline u128 spu_fsm(u128 a);
static inline u128 spu_fsmh(u128 a);
static inline u128 spu_fsmb(u128 a);

/* ---- constructors ---- */
static inline u128 spu_splat_u32(uint32_t v) {
    u128 r; r._u32[0]=v; r._u32[1]=v; r._u32[2]=v; r._u32[3]=v; return r;
}
/* Link register value for brsl/bisl/bisled: the SPU writes the return address
 * into the PREFERRED word (word 0) and ZEROS the other three slots (matches
 * RPCS3 v128::from32r, SPUInterpreter.cpp BRSL). Splatting it (the old bug)
 * corrupts any code that saves/reloads/operates on the full 128-bit link. */
/* An SPU instruction the lifter could not translate. It emits a bare comment,
 * so an unsupported opcode on a live path silently does NOTHING and the guest
 * computes garbage from there on -- which surfaces much later as a wild DMA or
 * atomic address with no hint of where it came from. Most of these are data
 * embedded in .text that never executes; this fires only if one actually runs.
 * One line per site. */
static inline void spu_unsupported(uint32_t pc, const char* mnemonic)
{
    static int warned;
    if (warned < 64) {
        warned++;
        fprintf(stderr, "[spu] UNSUPPORTED %s at LS 0x%05X -- executed, "
                        "result is wrong from here\n", mnemonic, pc);
        fflush(stderr);
    }
}

static inline u128 spu_link(uint32_t addr) {
    u128 r; r._u32[0]=addr; r._u32[1]=0; r._u32[2]=0; r._u32[3]=0; return r;
}
/* Preferred-slot scalar: value in word 0, remaining slots ZERO. CBEA's scalar
 * channel convention -- rchcnt returns the count this way (RPCS3 measured
 * {1,0,0,0} where the old splat gave {1,1,1,1}). Same bug class as spu_link. */
static inline u128 spu_pref_u32(uint32_t v) {
    u128 r; r._u32[0]=v; r._u32[1]=0; r._u32[2]=0; r._u32[3]=0; return r;
}
static inline u128 spu_splat_u16(uint16_t v) {
    u128 r; for (int i=0;i<8;i++) r._u16[i]=v; return r;
}
static inline u128 spu_splat_u8(uint8_t v) {
    u128 r; for (int i=0;i<16;i++) r._u8[i]=v; return r;
}
static inline u128 spu_zero(void) { u128 r; memset(&r,0,sizeof r); return r; }

/* ---- integer arithmetic (SIMD) ---- */
static inline u128 spu_a_ref(u128 a, u128 b)  { u128 r; for(int i=0;i<4;i++) r._u32[i]=a._u32[i]+b._u32[i]; return r; }
#if defined(__ARM_NEON) && !defined(SPU_SCALAR_HELPERS) && !defined(SPU_SCALAR_INT)
static inline u128 spu_a(u128 a, u128 b) {
    return spu__st(vaddq_u32(spu__ld(a), spu__ld(b)));
}
#else
#if defined(SPU__XV)
static inline u128 spu_a(u128 a, u128 b) { const __m128i A = spu__xv_ld(a); const __m128i B = spu__xv_ld(b); return spu__xv_st(_mm_add_epi32(A, B)); }
#else
static inline u128 spu_a(u128 a, u128 b) { return spu_a_ref(a, b); }
#endif
#endif
static inline u128 spu_sf_ref(u128 a, u128 b) { u128 r; for(int i=0;i<4;i++) r._u32[i]=b._u32[i]-a._u32[i]; return r; }
#if defined(__ARM_NEON) && !defined(SPU_SCALAR_HELPERS) && !defined(SPU_SCALAR_INT)
static inline u128 spu_sf(u128 a, u128 b) {
    return spu__st(vsubq_u32(spu__ld(b), spu__ld(a)));
}
#else
#if defined(SPU__XV)
static inline u128 spu_sf(u128 a, u128 b) { const __m128i A = spu__xv_ld(a); const __m128i B = spu__xv_ld(b); return spu__xv_st(_mm_sub_epi32(B, A)); }
#else
static inline u128 spu_sf(u128 a, u128 b) { return spu_sf_ref(a, b); }
#endif
#endif
static inline u128 spu_ah_ref(u128 a, u128 b) { u128 r; for(int i=0;i<8;i++) r._u16[i]=a._u16[i]+b._u16[i]; return r; }
static inline u128 spu_sfh_ref(u128 a, u128 b){ u128 r; for(int i=0;i<8;i++) r._u16[i]=b._u16[i]-a._u16[i]; return r; }
static inline u128 spu_ai_ref(u128 a, int32_t imm) { u128 r; for(int i=0;i<4;i++) r._u32[i]=a._u32[i]+(uint32_t)imm; return r; }
#if defined(__ARM_NEON) && !defined(SPU_SCALAR_HELPERS) && !defined(SPU_SCALAR_INT)
static inline u128 spu_ai(u128 a, int32_t imm) {
    return spu__st(vaddq_u32(spu__ld(a), vdupq_n_u32((uint32_t)imm)));
}
#else
#if defined(SPU__XV)
static inline u128 spu_ai(u128 a, int32_t imm) { const __m128i A = spu__xv_ld(a); return spu__xv_st(_mm_add_epi32(A, _mm_set1_epi32(imm))); }
#else
static inline u128 spu_ai(u128 a, int32_t imm) { return spu_ai_ref(a, imm); }
#endif
#endif
static inline u128 spu_ahi_ref(u128 a, int32_t imm){ u128 r; for(int i=0;i<8;i++) r._u16[i]=a._u16[i]+(uint16_t)imm; return r; }
static inline u128 spu_sfi_ref(u128 a, int32_t imm) { u128 r; for(int i=0;i<4;i++) r._u32[i]=(uint32_t)imm-a._u32[i]; return r; }
#if defined(__ARM_NEON) && !defined(SPU_SCALAR_HELPERS) && !defined(SPU_SCALAR_INT)
static inline u128 spu_sfi(u128 a, int32_t imm) {
    return spu__st(vsubq_u32(vdupq_n_u32((uint32_t)imm), spu__ld(a)));
}
#else
#if defined(SPU__XV)
static inline u128 spu_sfi(u128 a, int32_t imm) { const __m128i A = spu__xv_ld(a); return spu__xv_st(_mm_sub_epi32(_mm_set1_epi32(imm), A)); }
#else
static inline u128 spu_sfi(u128 a, int32_t imm) { return spu_sfi_ref(a, imm); }
#endif
#endif
static inline u128 spu_sfhi_ref(u128 a, int32_t imm){ u128 r; for(int i=0;i<8;i++) r._u16[i]=(uint16_t)imm-a._u16[i]; return r; }

/* ---- multiply (low halfword of each word × ... -> 32-bit, per SPU mpy) ----
 * Sub-lane indexing assumes a little-endian host (matches the recompiler's
 * target). The "low halfword of word i" in SPU BE semantics is _s16[2i] on
 * an LE host (NOT _s16[2i+1], which would be correct on a BE host). */
static inline u128 spu_mpy_ref(u128 a, u128 b)  { u128 r; for(int i=0;i<4;i++) r._s32[i]=(int32_t)a._s16[i*2]*(int32_t)b._s16[i*2]; return r; }
/* mpya: 16x16 signed multiply of low halves + add rc (per word, RRR form). */
static inline u128 spu_mpya_ref(u128 a, u128 b, u128 c) { u128 r; for(int i=0;i<4;i++) r._s32[i]=(int32_t)a._s16[i*2]*(int32_t)b._s16[i*2]+c._s32[i]; return r; }
/* sfx: extended subtract rb-ra-1+carry; carry-in = low bit of old rt (RT is 3rd src). */
static inline u128 spu_sfx_ref(u128 a, u128 b, u128 t) { u128 r; for(int i=0;i<4;i++) r._u32[i]=b._u32[i]+~a._u32[i]+(t._u32[i]&1u); return r; }
static inline u128 spu_mpyu_ref(u128 a, u128 b) { u128 r; for(int i=0;i<4;i++) r._u32[i]=(uint32_t)a._u16[i*2]*(uint32_t)b._u16[i*2]; return r; }
static inline u128 spu_mpyi_ref(u128 a, int32_t imm) { u128 r; for(int i=0;i<4;i++) r._s32[i]=(int32_t)a._s16[i*2]*(int16_t)imm; return r; }

/* ---- bitwise logic (whole 128 bits) ---- */
#if (defined(__SSE2__) || defined(_M_X64)) && !defined(__ARM_NEON) && !defined(SPU_SCALAR_HELPERS) && !defined(SPU_SCALAR_INT) && !defined(SPU_NO_VEC_LOGIC)
/* GOWA: as operações de bits como UMA instrução vetorial no x86 (eram as
 * formas _ref: duas operações de 64 bits em registradores inteiros, gravadas
 * em duas metades -- e a próxima leitura vetorial do registrador não
 * aproveitava as duas gravações: o encaminhamento store->load falha, ~10+
 * ciclos). or/selb/and estão entre as operações mais comuns do código SPU
 * traduzido. Bits são bits: idênticas às _ref. SPU_NO_VEC_LOGIC volta. */
#define SPU__XL 1
#if defined(__AVX512F__) && defined(__AVX512VL__)
#include <immintrin.h>
#else
#include <emmintrin.h>
#endif
static inline __m128i spu__xl_ld(u128 v) { __m128i r; memcpy(&r, &v, 16); return r; }
static inline u128 spu__xl_st(__m128i v) { u128 r; memcpy(&r, &v, 16); return r; }
#endif
static inline u128 spu_and_ref(u128 a, u128 b) { u128 r; r._u64[0]=a._u64[0]&b._u64[0]; r._u64[1]=a._u64[1]&b._u64[1]; return r; }
#if defined(__ARM_NEON) && !defined(SPU_SCALAR_HELPERS) && !defined(SPU_SCALAR_INT)
static inline u128 spu_and(u128 a, u128 b) {
    return spu__st(vandq_u32(spu__ld(a), spu__ld(b)));
}
#elif defined(SPU__XL)
static inline u128 spu_and(u128 a, u128 b) { return spu__xl_st(_mm_and_si128(spu__xl_ld(a), spu__xl_ld(b))); }
#else
static inline u128 spu_and(u128 a, u128 b) { return spu_and_ref(a, b); }
#endif
static inline u128 spu_or_ref(u128 a, u128 b)  { u128 r; r._u64[0]=a._u64[0]|b._u64[0]; r._u64[1]=a._u64[1]|b._u64[1]; return r; }
#if defined(__ARM_NEON) && !defined(SPU_SCALAR_HELPERS) && !defined(SPU_SCALAR_INT)
static inline u128 spu_or(u128 a, u128 b) {
    return spu__st(vorrq_u32(spu__ld(a), spu__ld(b)));
}
#elif defined(SPU__XL)
static inline u128 spu_or(u128 a, u128 b) { return spu__xl_st(_mm_or_si128(spu__xl_ld(a), spu__xl_ld(b))); }
#else
static inline u128 spu_or(u128 a, u128 b) { return spu_or_ref(a, b); }
#endif
static inline u128 spu_xor_ref(u128 a, u128 b) { u128 r; r._u64[0]=a._u64[0]^b._u64[0]; r._u64[1]=a._u64[1]^b._u64[1]; return r; }
#if defined(__ARM_NEON) && !defined(SPU_SCALAR_HELPERS) && !defined(SPU_SCALAR_INT)
static inline u128 spu_xor(u128 a, u128 b) {
    return spu__st(veorq_u32(spu__ld(a), spu__ld(b)));
}
#elif defined(SPU__XL)
static inline u128 spu_xor(u128 a, u128 b) { return spu__xl_st(_mm_xor_si128(spu__xl_ld(a), spu__xl_ld(b))); }
#else
static inline u128 spu_xor(u128 a, u128 b) { return spu_xor_ref(a, b); }
#endif
static inline u128 spu_nand_ref(u128 a, u128 b){ u128 r; r._u64[0]=~(a._u64[0]&b._u64[0]); r._u64[1]=~(a._u64[1]&b._u64[1]); return r; }
static inline u128 spu_nor_ref(u128 a, u128 b) { u128 r; r._u64[0]=~(a._u64[0]|b._u64[0]); r._u64[1]=~(a._u64[1]|b._u64[1]); return r; }
static inline u128 spu_andc_ref(u128 a, u128 b){ u128 r; r._u64[0]=a._u64[0]&~b._u64[0]; r._u64[1]=a._u64[1]&~b._u64[1]; return r; }
static inline u128 spu_orc_ref(u128 a, u128 b) { u128 r; r._u64[0]=a._u64[0]|~b._u64[0]; r._u64[1]=a._u64[1]|~b._u64[1]; return r; }
#if defined(SPU__XL)
static inline u128 spu_andc(u128 a, u128 b) { return spu__xl_st(_mm_andnot_si128(spu__xl_ld(b), spu__xl_ld(a))); }   /* a & ~b */
#if defined(__AVX512F__) && defined(__AVX512VL__)
/* vpternlogd(A, B, B, imm): bit do resultado = imm[(a<<2)|(b<<1)|b] */
static inline u128 spu_nand(u128 a, u128 b) { const __m128i A = spu__xl_ld(a), B = spu__xl_ld(b); return spu__xl_st(_mm_ternarylogic_epi32(A, B, B, 0x3F)); }  /* ~(a&b) */
static inline u128 spu_nor(u128 a, u128 b)  { const __m128i A = spu__xl_ld(a), B = spu__xl_ld(b); return spu__xl_st(_mm_ternarylogic_epi32(A, B, B, 0x03)); }  /* ~(a|b) */
static inline u128 spu_orc(u128 a, u128 b)  { const __m128i A = spu__xl_ld(a), B = spu__xl_ld(b); return spu__xl_st(_mm_ternarylogic_epi32(A, B, B, 0xF3)); }  /* a|~b */
#else
static inline u128 spu_nand(u128 a, u128 b) { return spu__xl_st(_mm_xor_si128(_mm_and_si128(spu__xl_ld(a), spu__xl_ld(b)), _mm_set1_epi32(-1))); }
static inline u128 spu_nor(u128 a, u128 b)  { return spu__xl_st(_mm_xor_si128(_mm_or_si128(spu__xl_ld(a), spu__xl_ld(b)), _mm_set1_epi32(-1))); }
static inline u128 spu_orc(u128 a, u128 b)  { return spu__xl_st(_mm_or_si128(spu__xl_ld(a), _mm_xor_si128(spu__xl_ld(b), _mm_set1_epi32(-1)))); }
#endif
#else
static inline u128 spu_nand(u128 a, u128 b) { return spu_nand_ref(a, b); }
static inline u128 spu_nor(u128 a, u128 b)  { return spu_nor_ref(a, b); }
static inline u128 spu_andc(u128 a, u128 b) { return spu_andc_ref(a, b); }
static inline u128 spu_orc(u128 a, u128 b)  { return spu_orc_ref(a, b); }
#endif
static inline u128 spu_andi_ref(u128 a, int32_t imm){ u128 r; for(int i=0;i<4;i++) r._u32[i]=a._u32[i]&(uint32_t)imm; return r; }
#if defined(__ARM_NEON) && !defined(SPU_SCALAR_HELPERS) && !defined(SPU_SCALAR_INT)
static inline u128 spu_andi(u128 a, int32_t imm) {
    return spu__st(vandq_u32(spu__ld(a), vdupq_n_u32((uint32_t)imm)));
}
#else
#if defined(SPU__XV)
static inline u128 spu_andi(u128 a, int32_t imm) { const __m128i A = spu__xv_ld(a); return spu__xv_st(_mm_and_si128(A, _mm_set1_epi32(imm))); }
#else
static inline u128 spu_andi(u128 a, int32_t imm) { return spu_andi_ref(a, imm); }
#endif
#endif
static inline u128 spu_ori_ref(u128 a, int32_t imm) { u128 r; for(int i=0;i<4;i++) r._u32[i]=a._u32[i]|(uint32_t)imm; return r; }
#if defined(__ARM_NEON) && !defined(SPU_SCALAR_HELPERS) && !defined(SPU_SCALAR_INT)
static inline u128 spu_ori(u128 a, int32_t imm) {
    return spu__st(vorrq_u32(spu__ld(a), vdupq_n_u32((uint32_t)imm)));
}
#else
#if defined(SPU__XV)
static inline u128 spu_ori(u128 a, int32_t imm) { const __m128i A = spu__xv_ld(a); return spu__xv_st(_mm_or_si128(A, _mm_set1_epi32(imm))); }
#else
static inline u128 spu_ori(u128 a, int32_t imm) { return spu_ori_ref(a, imm); }
#endif
#endif
static inline u128 spu_xori_ref(u128 a, int32_t imm){ u128 r; for(int i=0;i<4;i++) r._u32[i]=a._u32[i]^(uint32_t)imm; return r; }
#if defined(__ARM_NEON) && !defined(SPU_SCALAR_HELPERS) && !defined(SPU_SCALAR_INT)
static inline u128 spu_xori(u128 a, int32_t imm) {
    return spu__st(veorq_u32(spu__ld(a), vdupq_n_u32((uint32_t)imm)));
}
#else
#if defined(SPU__XV)
static inline u128 spu_xori(u128 a, int32_t imm) { const __m128i A = spu__xv_ld(a); return spu__xv_st(_mm_xor_si128(A, _mm_set1_epi32(imm))); }
#else
static inline u128 spu_xori(u128 a, int32_t imm) { return spu_xori_ref(a, imm); }
#endif
#endif

/* ---- count leading zeros / population count per byte ---- */
static inline u128 spu_clz_ref(u128 a)  { u128 r; for(int i=0;i<4;i++) r._u32[i]=(uint32_t)spu_clz32(a._u32[i]); return r; }
static inline u128 spu_cntb_ref(u128 a) { u128 r; for(int i=0;i<16;i++){ uint8_t v=a._u8[i],c=0; while(v){c+=v&1;v>>=1;} r._u8[i]=c; } return r; }

/* ---- compares: all-ones / all-zeros per lane ---- */
static inline u128 spu_ceq_ref(u128 a, u128 b)  { u128 r; for(int i=0;i<4;i++) r._u32[i]=(a._u32[i]==b._u32[i])?0xFFFFFFFFu:0; return r; }
#if defined(__ARM_NEON) && !defined(SPU_SCALAR_HELPERS) && !defined(SPU_SCALAR_INT)
static inline u128 spu_ceq(u128 a, u128 b) {
    return spu__st(vceqq_u32(spu__ld(a), spu__ld(b)));
}
#else
#if defined(SPU__XV)
static inline u128 spu_ceq(u128 a, u128 b) { const __m128i A = spu__xv_ld(a); const __m128i B = spu__xv_ld(b); return spu__xv_st(_mm_cmpeq_epi32(A, B)); }
#else
static inline u128 spu_ceq(u128 a, u128 b) { return spu_ceq_ref(a, b); }
#endif
#endif
static inline u128 spu_ceqh_ref(u128 a, u128 b) { u128 r; for(int i=0;i<8;i++) r._u16[i]=(a._u16[i]==b._u16[i])?0xFFFFu:0; return r; }
static inline u128 spu_ceqb_ref(u128 a, u128 b) { u128 r; for(int i=0;i<16;i++) r._u8[i]=(a._u8[i]==b._u8[i])?0xFFu:0; return r; }
static inline u128 spu_cgt_ref(u128 a, u128 b)  { u128 r; for(int i=0;i<4;i++) r._u32[i]=(a._s32[i]>b._s32[i])?0xFFFFFFFFu:0; return r; }
#if defined(__ARM_NEON) && !defined(SPU_SCALAR_HELPERS) && !defined(SPU_SCALAR_INT)
static inline u128 spu_cgt(u128 a, u128 b) {
    return spu__st(vcgtq_s32(vreinterpretq_s32_u32(spu__ld(a)), vreinterpretq_s32_u32(spu__ld(b))));
}
#else
#if defined(SPU__XV)
static inline u128 spu_cgt(u128 a, u128 b) { const __m128i A = spu__xv_ld(a); const __m128i B = spu__xv_ld(b); return spu__xv_st(_mm_cmpgt_epi32(A, B)); }
#else
static inline u128 spu_cgt(u128 a, u128 b) { return spu_cgt_ref(a, b); }
#endif
#endif
static inline u128 spu_cgth_ref(u128 a, u128 b) { u128 r; for(int i=0;i<8;i++) r._u16[i]=(a._s16[i]>b._s16[i])?0xFFFFu:0; return r; }
static inline u128 spu_cgtb_ref(u128 a, u128 b) { u128 r; for(int i=0;i<16;i++) r._u8[i]=(a._s8[i]>b._s8[i])?0xFFu:0; return r; }
static inline u128 spu_clgt_ref(u128 a, u128 b) { u128 r; for(int i=0;i<4;i++) r._u32[i]=(a._u32[i]>b._u32[i])?0xFFFFFFFFu:0; return r; }
#if defined(__ARM_NEON) && !defined(SPU_SCALAR_HELPERS) && !defined(SPU_SCALAR_INT)
static inline u128 spu_clgt(u128 a, u128 b) {
    return spu__st(vcgtq_u32(spu__ld(a), spu__ld(b)));
}
#else
#if defined(SPU__XV)
static inline u128 spu_clgt(u128 a, u128 b) { const __m128i A = spu__xv_ld(a); const __m128i B = spu__xv_ld(b); return spu__xv_st(spu__xv_movm32(spu__xv_cmpgtu32(A, B))); }
#else
static inline u128 spu_clgt(u128 a, u128 b) { return spu_clgt_ref(a, b); }
#endif
#endif
static inline u128 spu_clgth_ref(u128 a, u128 b){ u128 r; for(int i=0;i<8;i++) r._u16[i]=(a._u16[i]>b._u16[i])?0xFFFFu:0; return r; }
static inline u128 spu_clgtb_ref(u128 a, u128 b){ u128 r; for(int i=0;i<16;i++) r._u8[i]=(a._u8[i]>b._u8[i])?0xFFu:0; return r; }
static inline u128 spu_ceqi_ref(u128 a, int32_t imm) { u128 r; for(int i=0;i<4;i++) r._u32[i]=(a._s32[i]==imm)?0xFFFFFFFFu:0; return r; }
#if defined(__ARM_NEON) && !defined(SPU_SCALAR_HELPERS) && !defined(SPU_SCALAR_INT)
static inline u128 spu_ceqi(u128 a, int32_t imm) {
    return spu__st(vceqq_u32(spu__ld(a), vdupq_n_u32((uint32_t)imm)));
}
#else
#if defined(SPU__XV)
static inline u128 spu_ceqi(u128 a, int32_t imm) { const __m128i A = spu__xv_ld(a); return spu__xv_st(_mm_cmpeq_epi32(A, _mm_set1_epi32(imm))); }
#else
static inline u128 spu_ceqi(u128 a, int32_t imm) { return spu_ceqi_ref(a, imm); }
#endif
#endif
static inline u128 spu_cgti_ref(u128 a, int32_t imm) { u128 r; for(int i=0;i<4;i++) r._u32[i]=(a._s32[i]>imm)?0xFFFFFFFFu:0; return r; }
#if defined(__ARM_NEON) && !defined(SPU_SCALAR_HELPERS) && !defined(SPU_SCALAR_INT)
static inline u128 spu_cgti(u128 a, int32_t imm) {
    return spu__st(vcgtq_s32(vreinterpretq_s32_u32(spu__ld(a)), vdupq_n_s32(imm)));
}
#else
#if defined(SPU__XV)
static inline u128 spu_cgti(u128 a, int32_t imm) { const __m128i A = spu__xv_ld(a); return spu__xv_st(_mm_cmpgt_epi32(A, _mm_set1_epi32(imm))); }
#else
static inline u128 spu_cgti(u128 a, int32_t imm) { return spu_cgti_ref(a, imm); }
#endif
#endif
static inline u128 spu_clgti_ref(u128 a, int32_t imm){ u128 r; for(int i=0;i<4;i++) r._u32[i]=(a._u32[i]>(uint32_t)imm)?0xFFFFFFFFu:0; return r; }
#if defined(__ARM_NEON) && !defined(SPU_SCALAR_HELPERS) && !defined(SPU_SCALAR_INT)
static inline u128 spu_clgti(u128 a, int32_t imm) {
    return spu__st(vcgtq_u32(spu__ld(a), vdupq_n_u32((uint32_t)imm)));
}
#else
#if defined(SPU__XV)
static inline u128 spu_clgti(u128 a, int32_t imm) { const __m128i A = spu__xv_ld(a); return spu__xv_st(spu__xv_movm32(spu__xv_cmpgtu32(A, _mm_set1_epi32(imm)))); }
#else
static inline u128 spu_clgti(u128 a, int32_t imm) { return spu_clgti_ref(a, imm); }
#endif
#endif

/* ---- select / shuffle ---- */
static inline u128 spu_selb_ref(u128 a, u128 b, u128 c) {
    u128 r;
    r._u64[0]=(a._u64[0]&~c._u64[0])|(b._u64[0]&c._u64[0]);
    r._u64[1]=(a._u64[1]&~c._u64[1])|(b._u64[1]&c._u64[1]);
    return r;
}
#if defined(__ARM_NEON) && !defined(SPU_SCALAR_HELPERS) && !defined(SPU_SCALAR_INT)
static inline u128 spu_selb(u128 a, u128 b, u128 c) {
    return spu__st(vbslq_u32(spu__ld(c), spu__ld(b), spu__ld(a)));
}
#elif defined(SPU__XL) && defined(__AVX512F__) && defined(__AVX512VL__)
/* vpternlogd(C, B, A, 0xCA): c ? b : a, bit a bit */
static inline u128 spu_selb(u128 a, u128 b, u128 c) { return spu__xl_st(_mm_ternarylogic_epi32(spu__xl_ld(c), spu__xl_ld(b), spu__xl_ld(a), 0xCA)); }
#elif defined(SPU__XL)
static inline u128 spu_selb(u128 a, u128 b, u128 c) {
    const __m128i C = spu__xl_ld(c);
    return spu__xl_st(_mm_or_si128(_mm_andnot_si128(C, spu__xl_ld(a)), _mm_and_si128(C, spu__xl_ld(b))));
}
#else
static inline u128 spu_selb(u128 a, u128 b, u128 c) { return spu_selb_ref(a, b, c); }
#endif
/* shufb special selectors per Cell BE ISA:
 *   sel & 0xE0 == 0xE0 -> 0x80
 *   sel & 0xC0 == 0xC0 -> 0xFF
 *   sel & 0xC0 == 0x80 -> 0x00
 *   otherwise           -> concat{a,b}[sel & 0x1F] */
#if defined(__SSE4_1__)
#include <smmintrin.h>
/* pshufb version: one of the hottest SPU ops (~11% of GH3's FMOD mixer task
 * as the byte loop). SPU byte P lives at host byte SPU_W(P), i.e. the bytes
 * of each 32-bit word reversed, so swap into SPU order, select, swap back. */
#if defined(__AVX512VBMI__) && defined(__AVX512VL__) && !defined(SPU_SX_NO_AVX512)
/* GOWA: AVX-512 VBMI -- vpermi2b picks from the 32 bytes of {a, b} in one
 * instruction. No swaps: the result's host byte h is SPU byte h^3, and so is
 * the control's, so control byte h steers result byte h; the source SPU byte
 * k (0..31) sits at host index k^3 of the concatenation (a: 0..15, b: 16..31).
 * Special selectors (bit 7 set) by the high nibble: 8..B -> 0x00, C..D ->
 * 0xFF, E..F -> 0x80. Bit-identical to spu_shufb_ref (out/spudiff). */
static inline u128 spu_shufb(u128 a, u128 b, u128 c) {
    __m128i A, B, C; memcpy(&A, &a, 16); memcpy(&B, &b, 16); memcpy(&C, &c, 16);
    __m128i r = _mm_permutex2var_epi8(A, _mm_xor_si128(C, _mm_set1_epi8(3)), B);
    const __m128i lut = _mm_setr_epi8(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                      (char)0xFF, (char)0xFF, (char)0x80, (char)0x80);
    const __m128i sp = _mm_shuffle_epi8(lut, _mm_and_si128(_mm_srli_epi16(C, 4), _mm_set1_epi8(0x0F)));
    r = _mm_mask_mov_epi8(r, _mm_movepi8_mask(C), sp);
    u128 out; memcpy(&out, &r, 16); return out;
}
#else
static inline u128 spu_shufb(u128 a, u128 b, u128 c) {
    /* GOWA (24/10): straight on host byte order. SPU byte P is host byte P^3, so the
     * selector of host byte h is C[h] and it picks host byte (C[h] & 15) ^ 3 -- no
     * swap of the three operands and of the result (10 instructions, were ~18; the
     * same as vk__shufb1 in spu_vk_avx2.h, checked against the old form there).
     * Bit 4 picks b (moved to bit 7 for blendv); bit 7 set gives 0x00 / 0xFF / 0x80
     * by bits 6-5, from a table on the high nibble. */
    __m128i A, B, C; memcpy(&A, &a, 16); memcpy(&B, &b, 16); memcpy(&C, &c, 16);
    const __m128i idx = _mm_and_si128(_mm_xor_si128(C, _mm_set1_epi8(3)), _mm_set1_epi8(0x0F));
    __m128i r = _mm_blendv_epi8(_mm_shuffle_epi8(A, idx), _mm_shuffle_epi8(B, idx), _mm_slli_epi16(C, 3));
    const __m128i lut = _mm_setr_epi8(0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0,
                                      (char)0xFF, (char)0xFF, (char)0x80, (char)0x80);
    r = _mm_blendv_epi8(r, _mm_shuffle_epi8(lut, _mm_and_si128(_mm_srli_epi16(C, 4), _mm_set1_epi8(0x0F))), C);
    u128 out; memcpy(&out, &r, 16); return out;
}
#endif
/* GOWA: the quadword byte shifts/rotates, cntb and gbb by pshufb (they were
 * the scalar byte loops on x86, called out of line from the interpreter and
 * the lifted code). SPU byte P is host byte P^3. Bit-identical to the _ref
 * forms (out/spudiff/teste_bytes_sse.c). */
static inline __m128i spu__xi_ld(u128 v) { __m128i r; memcpy(&r, &v, 16); return r; }
static inline u128 spu__xi_st(__m128i v) { u128 r; memcpy(&r, &v, 16); return r; }
static inline __m128i spu__xi_spuidx(void) {             /* host byte h -> SPU byte h^3 */
    return _mm_setr_epi8(3,2,1,0, 7,6,5,4, 11,10,9,8, 15,14,13,12);
}
/* r[P] = a[P + sh] in SPU byte order, zero where P + sh leaves 0..15 (sh in -31..31) */
static inline u128 spu__xi_shift_bytes(u128 a, int sh) {
    const __m128i s = _mm_add_epi8(spu__xi_spuidx(), _mm_set1_epi8((char)sh));
    const __m128i out = _mm_or_si128(_mm_cmpgt_epi8(s, _mm_set1_epi8(15)), _mm_cmplt_epi8(s, _mm_setzero_si128()));
    const __m128i c = _mm_or_si128(_mm_xor_si128(s, _mm_set1_epi8(3)), out);
    return spu__xi_st(_mm_shuffle_epi8(spu__xi_ld(a), c));
}
/* r[P] = a[(P + sh) & 15] */
static inline u128 spu__xi_rot_bytes(u128 a, int sh) {
    const __m128i s = _mm_and_si128(_mm_add_epi8(spu__xi_spuidx(), _mm_set1_epi8((char)sh)), _mm_set1_epi8(15));
    return spu__xi_st(_mm_shuffle_epi8(spu__xi_ld(a), _mm_xor_si128(s, _mm_set1_epi8(3))));
}
static inline u128 spu__xi_cntb(u128 a) {
    const __m128i lut = _mm_setr_epi8(0,1,1,2, 1,2,2,3, 1,2,2,3, 2,3,3,4), m = _mm_set1_epi8(0x0F);
    const __m128i v = spu__xi_ld(a);
    const __m128i lo = _mm_shuffle_epi8(lut, _mm_and_si128(v, m));
    const __m128i hi = _mm_shuffle_epi8(lut, _mm_and_si128(_mm_srli_epi16(v, 4), m));
    return spu__xi_st(_mm_add_epi8(lo, hi));
}
static inline u128 spu__xi_gbb(u128 a) {
    /* host byte k <- SPU byte 15-k; each byte's bit 0 to bit 7; movemask bit k = SPU byte 15-k */
    const __m128i c = _mm_setr_epi8(12,13,14,15, 8,9,10,11, 4,5,6,7, 0,1,2,3);
    const __m128i v = _mm_slli_epi16(_mm_shuffle_epi8(spu__xi_ld(a), c), 7);
    return spu__xi_st(_mm_cvtsi32_si128(_mm_movemask_epi8(v)));
}
#define SPU__XI 1
#elif defined(__ARM_NEON)
#include <arm_neon.h>
/* NEON version: tbl over the 32-byte {a,b} concatenation. SPU byte P lives at
 * host byte P^3 (each word's bytes reversed), so rev32 into SPU order,
 * select, and rev32 back. */
static inline u128 spu_shufb(u128 a, u128 b, u128 c) {
    uint8x16x2_t t;
    t.val[0] = vrev32q_u8(vld1q_u8(a._u8));
    t.val[1] = vrev32q_u8(vld1q_u8(b._u8));
    const uint8x16_t C = vrev32q_u8(vld1q_u8(c._u8));
    uint8x16_t r = vqtbl2q_u8(t, vandq_u8(C, vdupq_n_u8(0x1F)));
    /* Special selectors: 10x -> 0x00, 110 -> 0xFF, 111 -> 0x80. */
    const uint8x16_t m80 = vtstq_u8(C, vdupq_n_u8(0x80));
    const uint8x16_t mC0 = vceqq_u8(vandq_u8(C, vdupq_n_u8(0xC0)), vdupq_n_u8(0xC0));
    const uint8x16_t mE0 = vceqq_u8(vandq_u8(C, vdupq_n_u8(0xE0)), vdupq_n_u8(0xE0));
    const uint8x16_t special = vbslq_u8(mE0, vdupq_n_u8(0x80), mC0);
    r = vrev32q_u8(vbslq_u8(m80, special, r));
    u128 out; vst1q_u8(out._u8, r); return out;
}
#endif
#if defined(__SSE4_1__) || defined(__ARM_NEON)
/* The byte loop stays as the reference (tests/test_spu_shufb.c checks both). */
static inline u128 spu_shufb_ref(u128 a, u128 b, u128 c) {
    /* shufb is defined on SPU byte positions. Our u128 is host-native LE, so SPU
     * byte P lives at _u8[SPU_W(P)]; map every access (the concat source, the
     * control, and the result) through SPU_W so a control supplied as an immediate
     * (ila/il) or an LS-loaded constant -- already in true SPU byte order -- is
     * interpreted correctly. The cbd/chd/cwd/cdd generators below produce true
     * SPU-byte-order selectors to match. */
    uint8_t cat[32];
    for (int j=0;j<16;j++) cat[j]    = a._u8[SPU_W(j)];   /* concat SPU byte j  = a SPU byte j */
    for (int j=0;j<16;j++) cat[16+j] = b._u8[SPU_W(j)];   /* concat SPU byte 16+j = b SPU byte j */
    u128 r;
    for (int t=0;t<16;t++) {                              /* result SPU byte t */
        uint8_t s = c._u8[SPU_W(t)];                      /* control SPU byte t */
        uint8_t v;
        if      ((s & 0xE0)==0xE0) v=0x80;
        else if ((s & 0xC0)==0xC0) v=0xFF;
        else if ((s & 0xC0)==0x80) v=0x00;
        else                       v=cat[s & 0x1F];       /* concat SPU byte (s & 0x1F) */
        r._u8[SPU_W(t)] = v;
    }
    return r;
}
#else
static inline u128 spu_shufb(u128 a, u128 b, u128 c) {
    /* shufb is defined on SPU byte positions. Our u128 is host-native LE, so SPU
     * byte P lives at _u8[SPU_W(P)]; map every access (the concat source, the
     * control, and the result) through SPU_W so a control supplied as an immediate
     * (ila/il) or an LS-loaded constant -- already in true SPU byte order -- is
     * interpreted correctly. The cbd/chd/cwd/cdd generators below produce true
     * SPU-byte-order selectors to match. */
    uint8_t cat[32];
    for (int j=0;j<16;j++) cat[j]    = a._u8[SPU_W(j)];   /* concat SPU byte j  = a SPU byte j */
    for (int j=0;j<16;j++) cat[16+j] = b._u8[SPU_W(j)];   /* concat SPU byte 16+j = b SPU byte j */
    u128 r;
    for (int t=0;t<16;t++) {                              /* result SPU byte t */
        uint8_t s = c._u8[SPU_W(t)];                      /* control SPU byte t */
        uint8_t v;
        if      ((s & 0xE0)==0xE0) v=0x80;
        else if ((s & 0xC0)==0xC0) v=0xFF;
        else if ((s & 0xC0)==0x80) v=0x00;
        else                       v=cat[s & 0x1F];       /* concat SPU byte (s & 0x1F) */
        r._u8[SPU_W(t)] = v;
    }
    return r;
}
#define spu_shufb_ref spu_shufb
#endif

/* ---- shift / rotate immediate (word lanes) ---- */
static inline u128 spu_shli_ref(u128 a, int sh)  { u128 r; sh&=0x3F; for(int i=0;i<4;i++) r._u32[i]=(sh>31)?0:(a._u32[i]<<sh); return r; }
#if defined(SPU__XV)
static inline u128 spu_shli(u128 a, int sh) { const __m128i A = spu__xv_ld(a); return spu__xv_st(_mm_sll_epi32(A, _mm_cvtsi32_si128(sh & 0x3F))); }
#else
static inline u128 spu_shli(u128 a, int sh) { return spu_shli_ref(a, sh); }
#endif
static inline u128 spu_shlhi_ref(u128 a, int sh) { u128 r; sh&=0x1F; for(int i=0;i<8;i++) r._u16[i]=(sh>15)?0:(uint16_t)(a._u16[i]<<sh); return r; }
#if defined(SPU__XV)
static inline u128 spu_shlhi(u128 a, int sh) { const __m128i A = spu__xv_ld(a); return spu__xv_st(_mm_sll_epi16(A, _mm_cvtsi32_si128(sh & 0x1F))); }
#else
static inline u128 spu_shlhi(u128 a, int sh) { return spu_shlhi_ref(a, sh); }
#endif
static inline u128 spu_roti_ref(u128 a, int sh)  { u128 r; sh&=31; for(int i=0;i<4;i++) r._u32[i]= sh ? ((a._u32[i]<<sh)|(a._u32[i]>>(32-sh))) : a._u32[i]; return r; }
static inline u128 spu_rothi_ref(u128 a, int sh) { u128 r; sh&=15; for(int i=0;i<8;i++) r._u16[i]=(uint16_t)((a._u16[i]<<sh)|(a._u16[i]>>(16-sh))); return r; }
static inline u128 spu_rotmi_ref(u128 a, int i7)  { u128 r; int sh=(0-i7)&0x3F; for(int i=0;i<4;i++) r._u32[i]=(sh>31)?0:(a._u32[i]>>sh); return r; }
#if defined(SPU__XV)
static inline u128 spu_rotmi(u128 a, int i7) { const __m128i A = spu__xv_ld(a); return spu__xv_st(_mm_srl_epi32(A, _mm_cvtsi32_si128((0 - i7) & 0x3F))); }
#else
static inline u128 spu_rotmi(u128 a, int i7) { return spu_rotmi_ref(a, i7); }
#endif
static inline u128 spu_rotmai_ref(u128 a, int i7) { u128 r; int sh=(0-i7)&0x3F; for(int i=0;i<4;i++) r._s32[i]=(sh>31)?(a._s32[i]>>31):(a._s32[i]>>sh); return r; }
static inline u128 spu_rotmhi(u128 a, int i7) { u128 r; int sh=(0-i7)&0x1F; for(int i=0;i<8;i++) r._u16[i]=(sh>15)?0:(uint16_t)(a._u16[i]>>sh); return r; }
static inline u128 spu_shlqbyi_ref(u128 a, int sh) { u128 r=spu_zero(); sh&=0x1F; for(int i=0;i<16;i++){ int s=i+sh; if(s<16) r._u8[SPU_W(i)]=a._u8[SPU_W(s)]; } return r; }
#if defined(__ARM_NEON) && !defined(SPU_SCALAR_HELPERS) && !defined(SPU_SCALAR_INT)
static inline u128 spu_shlqbyi(u128 a, int sh) {
    return spu__perm(a, vaddq_u8(spu__iota(), vdupq_n_u8((uint8_t)(sh & 0x1F))));
}
#else
#if defined(SPU__XI) && !defined(SPU_SCALAR_HELPERS) && !defined(SPU_SCALAR_INT)
static inline u128 spu_shlqbyi(u128 a, int sh) { return spu__xi_shift_bytes(a, sh & 0x1F); }
#else
static inline u128 spu_shlqbyi(u128 a, int sh) { return spu_shlqbyi_ref(a, sh); }
#endif
#endif
static inline u128 spu_rotqbyi_ref(u128 a, int sh) { u128 r; sh&=0x0F; for(int i=0;i<16;i++) r._u8[SPU_W(i)]=a._u8[SPU_W((i+sh)&0x0F)]; return r; }
#if defined(__ARM_NEON) && !defined(SPU_SCALAR_HELPERS) && !defined(SPU_SCALAR_INT)
static inline u128 spu_rotqbyi(u128 a, int sh) {
    return spu__perm(a, vandq_u8(vaddq_u8(spu__iota(), vdupq_n_u8((uint8_t)sh)), vdupq_n_u8(15)));
}
#else
#if defined(SPU__XI) && !defined(SPU_SCALAR_HELPERS) && !defined(SPU_SCALAR_INT)
static inline u128 spu_rotqbyi(u128 a, int sh) { return spu__xi_rot_bytes(a, sh & 0x0F); }
#else
static inline u128 spu_rotqbyi(u128 a, int sh) { return spu_rotqbyi_ref(a, sh); }
#endif
#endif
/* Bit-level quadword shifts/rotates operate on the WHOLE 128-bit big-endian
 * value. Our _u64[0]/_u64[1] are word-SCRAMBLED (host-LE: _u64[0] = word0 |
 * word1<<32, but SPU's MS half is word0<<32 | word1), so native shifts on them
 * corrupt any bits crossing a 32-bit boundary. Assemble the logical hi/lo halves
 * with _u32[0] as most-significant, shift, then disassemble. */
static inline u128 spu_shlqbii_ref(u128 a, int sh) { sh&=7; if(!sh) return a;
    uint64_t hi=((uint64_t)a._u32[0]<<32)|a._u32[1], lo=((uint64_t)a._u32[2]<<32)|a._u32[3];
    uint64_t nhi=(hi<<sh)|(lo>>(64-sh)), nlo=(lo<<sh);
    u128 r; r._u32[0]=(uint32_t)(nhi>>32); r._u32[1]=(uint32_t)nhi; r._u32[2]=(uint32_t)(nlo>>32); r._u32[3]=(uint32_t)nlo; return r; }
static inline u128 spu_rotqbii_ref(u128 a, int sh) { sh&=7; if(!sh) return a;
    uint64_t hi=((uint64_t)a._u32[0]<<32)|a._u32[1], lo=((uint64_t)a._u32[2]<<32)|a._u32[3];
    uint64_t nhi=(hi<<sh)|(lo>>(64-sh)), nlo=(lo<<sh)|(hi>>(64-sh));
    u128 r; r._u32[0]=(uint32_t)(nhi>>32); r._u32[1]=(uint32_t)nhi; r._u32[2]=(uint32_t)(nlo>>32); r._u32[3]=(uint32_t)nlo; return r; }
#if defined(__AVX512VBMI2__) && defined(__AVX512VL__) && defined(SPU__XI) && !defined(SPU_NO_XM)
/* GOWA (23/10): the bit shifts of the whole quadword as one funnel shift per
 * word (VPSHLDVD/VPSHRDVD) against the neighbouring word: word i takes the
 * bits that leave word i+1 (the next, lower-order one; word 0 is the most
 * significant). The 64-bit assembly above was ~24 host instructions and a
 * round trip through the integer registers (rotqbii 95 sites in the hot
 * modules, 8 per iteration of the 0x74C580 plugin loop). Bit-identical to the
 * _ref forms (t_xm.c). */
#define SPU__XQ 1
static inline u128 spu_shlqbii(u128 a, int sh) {
    const __m128i v = spu__xi_ld(a);
    return spu__xi_st(_mm_shldv_epi32(v, _mm_alignr_epi8(_mm_setzero_si128(), v, 4), _mm_set1_epi32(sh & 7)));
}
static inline u128 spu_rotqbii(u128 a, int sh) {
    const __m128i v = spu__xi_ld(a);
    return spu__xi_st(_mm_shldv_epi32(v, _mm_alignr_epi8(v, v, 4), _mm_set1_epi32(sh & 7)));
}
#elif defined(SPU__XI) && !defined(SPU_NO_XM)
/* GOWA (24/10): without VBMI2, the same funnel per word by two SSE shifts against
 * the neighbour moved in by a byte shift; a count of 32 gives 0, so sh = 0 needs
 * no branch. Bit-identical to the _ref forms (scratchpad t_xq.c). */
#define SPU__XQ2 1
static inline u128 spu_shlqbii(u128 a, int sh) {
    const __m128i v = spu__xi_ld(a);
    return spu__xi_st(_mm_or_si128(_mm_sll_epi32(v, _mm_cvtsi32_si128(sh & 7)),
                                   _mm_srl_epi32(_mm_srli_si128(v, 4), _mm_cvtsi32_si128(32 - (sh & 7)))));
}
static inline u128 spu_rotqbii(u128 a, int sh) {
    const __m128i v = spu__xi_ld(a);
    return spu__xi_st(_mm_or_si128(_mm_sll_epi32(v, _mm_cvtsi32_si128(sh & 7)),
                                   _mm_srl_epi32(_mm_shuffle_epi32(v, 0x39), _mm_cvtsi32_si128(32 - (sh & 7)))));
}
#else
static inline u128 spu_shlqbii(u128 a, int sh) { return spu_shlqbii_ref(a, sh); }
static inline u128 spu_rotqbii(u128 a, int sh) { return spu_rotqbii_ref(a, sh); }
#endif

/* ---- SPU floating point --------------------------------------------------------------
 * Written from the Cell BE SPU ISA v1.2 (section 9 and the instruction pages).
 *
 * Single precision is "extended range" (section 9.1), not IEEE:
 *  - biased exponent 255 is an ordinary binade: no Inf, no NaN, and the largest magnitude is
 *    Smax = 0x7FFFFFFF = (2 - 2^-23) * 2^128;
 *  - biased exponent 0 is zero whatever the fraction (no denormals); any zero result is +0;
 *  - truncation (toward zero) is the only rounding mode;
 *  - a result above Smax saturates to +-Smax, one below Smin = 2^-126 becomes +0.
 * Each operation here takes its operands apart into sign, significand and power of two, computes
 * the exact result in integers (64/128-bit), and truncates once in spu__sf_pack. When one term is
 * so far below the other that it lies under the other's last bit, the exact sum is not formed:
 * its truncation is the dominant term's (same sign), or the dominant term less half its last bit
 * (opposite sign) -- which truncates to the same value as the exact result.
 *
 * Double precision (section 9.2) is IEEE with round-to-nearest (FPSCR rounding modes are not
 * modelled: fscrwr is ignored). Implementation choices the ISA leaves open, as the Cell makes
 * them: denormal operands are read as zero with their sign kept, and any NaN result is the
 * default QNaN 0x7FF8000000000000.
 * --------------------------------------------------------------------------------------- */

/* Exact value sign * sig * 2^e2 (sig > 0) as an SPU single: keep the top 24 bits of sig
 * (truncation), then saturate above Smax / flush below Smin. */
static inline uint32_t spu__sf_pack(uint32_t sign, unsigned __int128 sig, int e2) {
    const uint64_t hi = (uint64_t)(sig >> 64), lo = (uint64_t)sig;
    const int top = hi ? 127 - __builtin_clzll(hi) : 63 - __builtin_clzll(lo);   /* sig in [2^top, 2^(top+1)) */
    const int biased = top + e2 + 127;
    if (biased > 255) return sign | 0x7FFFFFFFu;
    if (biased < 1)   return 0;
    const uint32_t m24 = top >= 23 ? (uint32_t)(sig >> (top - 23)) : (uint32_t)(sig << (23 - top));
    return sign | ((uint32_t)biased << 23) | (m24 & 0x7FFFFFu);
}
/* An SPU single as sig * 2^e2 with a 24-bit sig; returns 0 for zero (biased exponent 0). */
static inline int spu__sf_split(uint32_t x, uint32_t* sig, int* e2) {
    const uint32_t be = (x >> 23) & 0xFFu;
    if (!be) return 0;
    *sig = (x & 0x7FFFFFu) | 0x800000u;
    *e2 = (int)be - 150;                                   /* -127 bias, -23 for the integer sig */
    return 1;
}
/* Exact sum of two signed terms s1*m1*2^e1 + s2*m2*2^e2 (m1, m2 < 2^48, both nonzero), truncated. */
static inline uint32_t spu__sf_sum(uint32_t s1, uint64_t m1, int e1, uint32_t s2, uint64_t m2, int e2) {
    if (e1 < e2) { uint32_t ts = s1; s1 = s2; s2 = ts; uint64_t tm = m1; m1 = m2; m2 = tm; int te = e1; e1 = e2; e2 = te; }
    const int d = e1 - e2;                                 /* term 1 has the higher exponent */
    if (d > 79) {                                          /* term 2 < 2^(e1-31): below term 1's last bit */
        if (s1 == s2) return spu__sf_pack(s1, m1, e1);
        return spu__sf_pack(s1, ((unsigned __int128)m1 << 1) - 1, e1 - 1);
    }
    const unsigned __int128 a = (unsigned __int128)m1 << d, b = m2;   /* both at 2^e2, < 2^127 */
    if (s1 == s2) return spu__sf_pack(s1, a + b, e2);
    if (a == b) return 0;
    return a > b ? spu__sf_pack(s1, a - b, e2) : spu__sf_pack(s2, b - a, e2);
}
static inline uint32_t spu__fa_lane(uint32_t x, uint32_t y) {
    uint32_t mx, my; int ex, ey;
    const int nx = spu__sf_split(x, &mx, &ex), ny = spu__sf_split(y, &my, &ey);
    if (!nx) return ny ? y : 0;                            /* a nonzero single is already exact */
    if (!ny) return x;
    return spu__sf_sum(x & 0x80000000u, mx, ex, y & 0x80000000u, my, ey);
}
static inline uint32_t spu__fm_lane(uint32_t x, uint32_t y) {
    uint32_t mx, my; int ex, ey;
    if (!spu__sf_split(x, &mx, &ex) || !spu__sf_split(y, &my, &ey)) return 0;
    return spu__sf_pack((x ^ y) & 0x80000000u, (uint64_t)mx * my, ex + ey);
}
/* (+-a*b) + (+-c), the product exact and unbounded (fma page), one truncation. */
static inline uint32_t spu__fma_lane(uint32_t x, uint32_t y, uint32_t z, uint32_t negp, uint32_t negc) {
    uint32_t mx, my, mz; int ex, ey, ez;
    const int np = spu__sf_split(x, &mx, &ex) & spu__sf_split(y, &my, &ey);
    const int nz = spu__sf_split(z, &mz, &ez);
    const uint32_t sp = ((x ^ y) & 0x80000000u) ^ negp, sz = (z & 0x80000000u) ^ negc;
    if (!np) return nz ? (sz | (z & 0x7FFFFFFFu)) : 0;
    const uint64_t p = (uint64_t)mx * my;
    if (!nz) return spu__sf_pack(sp, p, ex + ey);
    return spu__sf_sum(sp, p, ex + ey, sz, mz, ez);
}
/* Fast path, exact by construction, for the common case where every operand is an ordinary
 * normal single (biased exponent 1..254): the value is formed in double precision and truncated
 * by masking the 29 low significand bits, which is truncation toward zero of the magnitude.
 *  - a product of two 24-bit significands is exact in double;
 *  - a sum is exact in double when the exponents differ by at most 29;
 *  - x*y + z through a double fma is rounded once at 53 bits; that rounding can only change the
 *    24-bit truncation when the double lands exactly on a 24-bit boundary (its 29 low bits all
 *    zero), so such lanes take the exact path.
 * Anything else -- zero, extended, or a result outside the normal range -- returns 0 here,
 * meaning "use the exact path". */
static inline int spu__sf_plain(uint32_t x) { const uint32_t e = (x >> 23) & 0xFFu; return e - 1u < 254u; }
static inline float spu__sf_f(uint32_t x) { float f; memcpy(&f, &x, 4); return f; }
/* d (finite, nonzero) truncated to a single; 0 if outside the normal single range or zero. */
static inline uint32_t spu__sf_from_d(double d, int boundary_check) {
    uint64_t u; memcpy(&u, &d, 8);
    const int e = (int)((u >> 52) & 0x7FFu) - 1023;
    if (e < -126 || e > 127) return 0;                     /* zero, denormal, or not a normal single */
    if (boundary_check && !(u & 0x1FFFFFFFull)) return 0;  /* possibly rounded onto the boundary */
    return (uint32_t)(u >> 32 & 0x80000000u) | ((uint32_t)(e + 127) << 23) | (uint32_t)((u >> 29) & 0x7FFFFFu);
}
static inline uint32_t spu__fa_fast(uint32_t x, uint32_t y) {
    if (!spu__sf_plain(x) || !spu__sf_plain(y)) return 0;
    const int d = (int)((x >> 23) & 0xFFu) - (int)((y >> 23) & 0xFFu);
    if (d > 29 || d < -29) return 0;
    return spu__sf_from_d((double)spu__sf_f(x) + (double)spu__sf_f(y), 0);
}
static inline uint32_t spu__fm_fast(uint32_t x, uint32_t y) {
    if (!spu__sf_plain(x) || !spu__sf_plain(y)) return 0;
    return spu__sf_from_d((double)spu__sf_f(x) * (double)spu__sf_f(y), 0);
}
static inline uint32_t spu__fma_fast(uint32_t x, uint32_t y, uint32_t z, uint32_t negp, uint32_t negc) {
    if (!spu__sf_plain(x) || !spu__sf_plain(y) || !spu__sf_plain(z)) return 0;
    const double p = (double)spu__sf_f(x ^ negp) * (double)spu__sf_f(y);      /* exact */
    /* p + z in double: one IEEE rounding, bit-identical to fma(1.0, p, z) since
     * 1.0 * p is exact -- and without -mfma that libm fma() was a software
     * routine, the single hottest float helper in God of War Ascension. */
    return spu__sf_from_d(p + (double)spu__sf_f(z ^ negc), 1);
}
static inline uint32_t spu__fa_any(uint32_t x, uint32_t y) { const uint32_t f = spu__fa_fast(x, y); return f ? f : spu__fa_lane(x, y); }
static inline uint32_t spu__fm_any(uint32_t x, uint32_t y) { const uint32_t f = spu__fm_fast(x, y); return f ? f : spu__fm_lane(x, y); }
static inline uint32_t spu__fma_any(uint32_t x, uint32_t y, uint32_t z, uint32_t np, uint32_t nc) {
    /* A zero (or denormal, which the SPU reads as zero) factor with a normal
     * addend: the product is a signed zero and the sum is exactly z (with the
     * addend's negation applied). Zero lanes are common -- padding, w = 0 --
     * and used to take the exact slow path, since zero is not "plain". */
    if ((!(x & 0x7F800000u) || !(y & 0x7F800000u)) && spu__sf_plain(z)) return z ^ nc;
    const uint32_t f = spu__fma_fast(x, y, z, np, nc); return f ? f : spu__fma_lane(x, y, z, np, nc); }
#define SPU__SF_V2(name, expr) static inline u128 name(u128 a, u128 b) { u128 r; \
    for (int i = 0; i < 4; i++) { const uint32_t x = a._u32[i], y = b._u32[i]; r._u32[i] = (expr); } return r; }
#define SPU__SF_V3(name, expr) static inline u128 name(u128 a, u128 b, u128 c) { u128 r; \
    for (int i = 0; i < 4; i++) { const uint32_t x = a._u32[i], y = b._u32[i], z = c._u32[i]; r._u32[i] = (expr); } return r; }
/* Per-lane forms (fast path, else exact). */
SPU__SF_V2(spu__fa_s, spu__fa_any(x, y))
SPU__SF_V2(spu__fs_s, spu__fa_any(x, y ^ 0x80000000u))
SPU__SF_V2(spu__fm_s, spu__fm_any(x, y))
SPU__SF_V3(spu__fma_s,  spu__fma_any(x, y, z, 0, 0))
SPU__SF_V3(spu__fms_s,  spu__fma_any(x, y, z, 0, 0x80000000u))
SPU__SF_V3(spu__fnms_s, spu__fma_any(x, y, z, 0x80000000u, 0))
#if defined(__ARM_NEON) && !defined(SPU_SCALAR_HELPERS)
/* The same fast path four lanes at a time, as two f64x2 halves: widen (exact), operate, clear
 * the 29 low significand bits, narrow (exact: the value now fits a single). Each lane is judged
 * on its own: the lanes that qualify keep the vector result, and only the others are redone one
 * by one (zero is not "plain", so a vector with a 0.0 lane -- w = 0, padding -- is common).
 *
 * Inlining: the fast path is forced inline and the per-lane fix-up is kept out of line (noinline,
 * cold). Left to itself the compiler inlined the scalar fallback into spu__nv_fa/fma, found the
 * result too big, and called the whole helper out of line instead -- every fa/fm/fma became a
 * call with the operands moved through general registers (tests/bench/spu: fa 13.4 ns, fma
 * 27.8 ns per op, latency). */
#define SPU__NV_INLINE static inline __attribute__((always_inline))
#define SPU__NV_SLOW   static inline __attribute__((noinline, cold))
SPU__NV_INLINE uint32x4_t spu__nv_bexp(uint32x4_t x) { return vshrq_n_u32(vshlq_n_u32(x, 1), 24); }
SPU__NV_INLINE uint32x4_t spu__nv_plain(uint32x4_t x) {         /* biased exponent 1..254 */
    return vcltq_u32(vsubq_u32(spu__nv_bexp(x), vdupq_n_u32(1)), vdupq_n_u32(254));
}
/* Truncate f64x2 to singles. *ok gets all-ones in the lanes whose result is a normal single and,
 * when err is given (the exact rounding error of the operation that produced d), was not both
 * rounded and on a 24-bit boundary: only there can rounding have crossed the boundary and
 * changed the truncation. */
SPU__NV_INLINE float32x2_t spu__nv_trunc(float64x2_t d, uint32x2_t* ok, const float64x2_t* err) {
    const uint64x2_t u = vreinterpretq_u64_f64(d);
    const int64x2_t e = vsubq_s64(vreinterpretq_s64_u64(vandq_u64(vshrq_n_u64(u, 52), vdupq_n_u64(0x7FF))), vdupq_n_s64(1023));
    uint64x2_t good = vandq_u64(vcgeq_s64(e, vdupq_n_s64(-126)), vcleq_s64(e, vdupq_n_s64(127)));
    if (err) good = vandq_u64(good, vorrq_u64(vtstq_u64(u, vdupq_n_u64(0x1FFFFFFFull)), vceqzq_f64(*err)));
    *ok = vmovn_u64(good);
    return vcvt_f32_f64(vreinterpretq_f64_u64(vandq_u64(u, vdupq_n_u64(~0x1FFFFFFFull))));
}
/* As spu__nv_trunc, but an exact zero result is accepted too; *zm marks those lanes so the caller
 * can clear the sign (the SPU never produces -0). Used once true zeros (+-0.0 words) are fast-path
 * operands: in double, 0 + y = y, x * 0 = +-0, 0 * y + z = z exactly -- the SPU's zero rules --
 * and a zero result of nonzero operands is an exact cancellation, which the SPU returns as +0.
 * The sign mask is computed beside the conversion, so it costs one AND on the result path. */
SPU__NV_INLINE float32x2_t spu__nv_trunc0(float64x2_t d, uint32x2_t* ok, uint32x2_t* zm, const float64x2_t* err) {
    const uint64x2_t u = vreinterpretq_u64_f64(d);
    const uint64x2_t az = vceqzq_u64(vshlq_n_u64(u, 1));                  /* +-0 */
    const int64x2_t e = vsubq_s64(vreinterpretq_s64_u64(vandq_u64(vshrq_n_u64(u, 52), vdupq_n_u64(0x7FF))), vdupq_n_s64(1023));
    uint64x2_t good = vandq_u64(vcgeq_s64(e, vdupq_n_s64(-126)), vcleq_s64(e, vdupq_n_s64(127)));
    if (err) good = vandq_u64(good, vorrq_u64(vtstq_u64(u, vdupq_n_u64(0x1FFFFFFFull)), vceqzq_f64(*err)));
    *ok = vmovn_u64(vorrq_u64(good, az));
    *zm = vmovn_u64(az);
    return vcvt_f32_f64(vreinterpretq_f64_u64(vandq_u64(u, vdupq_n_u64(~0x1FFFFFFFull))));
}
/* A lane the fast path takes: an ordinary single (biased exponent 1..254) or a true zero
 * (+-0.0). Exponent-0 words with a nonzero fraction are zero on the SPU but not in double, so
 * they still go to the fix-up. */
SPU__NV_INLINE uint32x4_t spu__nv_plain0(uint32x4_t x) {
    return vorrq_u32(spu__nv_plain(x), vceqzq_u32(vshlq_n_u32(x, 1)));
}
/* r with the sign cleared in the lanes whose result is zero */
SPU__NV_INLINE uint32x4_t spu__nv_pos0(uint32x4_t r, uint32x2_t zlo, uint32x2_t zhi) {
    return vbicq_u32(r, vandq_u32(vcombine_u32(zlo, zhi), vdupq_n_u32(0x80000000u)));
}
SPU__NV_INLINE int spu__nv_all4(uint32x4_t m) { return vminvq_u32(m) == 0xFFFFFFFFu; }
SPU__NV_INLINE float64x2_t spu__nv_lo(uint32x4_t x) { return vcvt_f64_f32(vget_low_f32(vreinterpretq_f32_u32(x))); }
SPU__NV_INLINE float64x2_t spu__nv_hi(uint32x4_t x) { return vcvt_high_f64_f32(vreinterpretq_f32_u32(x)); }
/* The lanes of r not marked in ok, recomputed one by one (scalar fast path, else exact). They take
 * and return NEON vectors, not u128: a u128 travels in general registers, and merging that return
 * with the fast path's vector result kept every fa/fm/fma result in x-registers, with fmov round
 * trips, on the fast path too (fa 5.3 -> 9.8 ns). */
#define SPU__NV_LANES(v, m) uint32_t m[4]; vst1q_u32(m, v)
SPU__NV_INLINE uint32x4_t spu__nv_vec(float32x2_t lo, float32x2_t hi) { return vreinterpretq_u32_f32(vcombine_f32(lo, hi)); }
SPU__NV_INLINE uint32x4_t spu__nv_zero(uint32x4_t x) { return vceqzq_u32(spu__nv_bexp(x)); }   /* biased exponent 0: zero */
/* SPU_FLOATFIX_STATS=1: count why lanes reach the fix-ups (T-0014) and print the tally every
 * 2^24 lanes. Only the cold fix-ups count, so it costs nothing on the fast path; the counters
 * are one weak global shared by every TU that includes this header. */
enum { SPU_FF_ZX, SPU_FF_ZY, SPU_FF_EXT, SPU_FF_GAP, SPU_FF_RANGE, SPU_FF_BOUNDARY, SPU_FF_N };
__attribute__((weak)) uint64_t spu_floatfix_stats[3][SPU_FF_N];
__attribute__((weak)) uint64_t spu_floatfix_vectors[3];
__attribute__((weak)) int spu_floatfix_on = -1;
__attribute__((weak)) uint64_t spu_floatfix_lanes;
static inline __attribute__((cold, noinline)) void spu__ff_count(int op, int why) {
    static const char* const ops[3] = { "fa/fs", "fm", "fma/fms/fnms" };
    __atomic_fetch_add(&spu_floatfix_stats[op][why], 1, __ATOMIC_RELAXED);
    if ((__atomic_add_fetch(&spu_floatfix_lanes, 1, __ATOMIC_RELAXED) & 0xFFFFFF) != 0) return;
    uint64_t n = 0;
    for (int o = 0; o < 3; o++) for (int k = 0; k < SPU_FF_N; k++) n += __atomic_load_n(&spu_floatfix_stats[o][k], __ATOMIC_RELAXED);
    char buf[640]; int p = snprintf(buf, sizeof buf, "[spu-floatfix] %llu lanes:", (unsigned long long)n);
    for (int o = 0; o < 3; o++)
        p += snprintf(buf + p, sizeof buf - p, "  %s vec=%llu zero-a=%llu zero-b=%llu ext=%llu gap=%llu range=%llu boundary=%llu;",
                      ops[o], (unsigned long long)spu_floatfix_vectors[o],
                      (unsigned long long)spu_floatfix_stats[o][SPU_FF_ZX], (unsigned long long)spu_floatfix_stats[o][SPU_FF_ZY],
                      (unsigned long long)spu_floatfix_stats[o][SPU_FF_EXT], (unsigned long long)spu_floatfix_stats[o][SPU_FF_GAP],
                      (unsigned long long)spu_floatfix_stats[o][SPU_FF_RANGE], (unsigned long long)spu_floatfix_stats[o][SPU_FF_BOUNDARY]);
    fprintf(stderr, "%s\n", buf);
}
static inline int spu__ff_enabled(void) {
    int on = __atomic_load_n(&spu_floatfix_on, __ATOMIC_RELAXED);
    if (on < 0) { const char* e = getenv("SPU_FLOATFIX_STATS"); on = (e && e[0] == '1');
                  __atomic_store_n(&spu_floatfix_on, on, __ATOMIC_RELAXED); }
    return on;
}
static inline uint32_t spu__ff_be(uint32_t v) { return (v >> 23) & 0xFFu; }
static inline int spu__ff_why_fa(uint32_t x, uint32_t y);
static inline int spu__ff_why_fm(uint32_t x, uint32_t y);
static inline int spu__ff_why_fma(uint32_t x, uint32_t y, uint32_t z, uint32_t negp, uint32_t negc);
static inline __attribute__((cold, noinline)) void spu__ff_tally(int op, uint32x4_t ok, uint32x4_t a, uint32x4_t b,
                                                                  uint32x4_t c, uint32_t negp, uint32_t negc) {
    SPU__NV_LANES(ok, m); SPU__NV_LANES(a, x); SPU__NV_LANES(b, y); SPU__NV_LANES(c, z);
    __atomic_fetch_add(&spu_floatfix_vectors[op], 1, __ATOMIC_RELAXED);
    for (int i = 0; i < 4; i++) if (!m[i])
        spu__ff_count(op, op == 0 ? spu__ff_why_fa(x[i], y[i]) : op == 1 ? spu__ff_why_fm(x[i], y[i])
                                  : spu__ff_why_fma(x[i], y[i], z[i], negp, negc));
}
/* Why a fa/fs lane failed (y already sign-adjusted). zero-a/zero-b: operand 1/2 is zero;
 * ext: an operand is extended range; gap: exponents differ by more than 29; range: the
 * result is not a normal single. */
static inline int spu__ff_why_fa(uint32_t x, uint32_t y) {
    const int bx = (int)spu__ff_be(x), by = (int)spu__ff_be(y);
    if (!bx) return SPU_FF_ZX;
    if (!by) return SPU_FF_ZY;
    if (bx == 255 || by == 255) return SPU_FF_EXT;
    if (bx - by > 29 || by - bx > 29) return SPU_FF_GAP;
    return SPU_FF_RANGE;
}
static inline int spu__ff_why_fm(uint32_t x, uint32_t y) {
    const int bx = (int)spu__ff_be(x), by = (int)spu__ff_be(y);
    if (!bx) return SPU_FF_ZX;
    if (!by) return SPU_FF_ZY;
    if (bx == 255 || by == 255) return SPU_FF_EXT;
    return SPU_FF_RANGE;
}
/* fma: zero-a = the product is zero (x or y), zero-b = the addend is zero; boundary = the
 * TwoSum test (rounded onto a 24-bit boundary), told apart from range by redoing the sum. */
static inline int spu__ff_why_fma(uint32_t x, uint32_t y, uint32_t z, uint32_t negp, uint32_t negc) {
    const int bx = (int)spu__ff_be(x), by = (int)spu__ff_be(y), bz = (int)spu__ff_be(z);
    if (!bx || !by) return SPU_FF_ZX;
    if (!bz) return SPU_FF_ZY;
    if (bx == 255 || by == 255 || bz == 255) return SPU_FF_EXT;
    const double s = (double)spu__sf_f(x ^ negp) * (double)spu__sf_f(y) + (double)spu__sf_f(z ^ negc);
    uint64_t u; memcpy(&u, &s, 8);
    const int e = (int)((u >> 52) & 0x7FFu) - 1023;
    return (e < -126 || e > 127) ? SPU_FF_RANGE : SPU_FF_BOUNDARY;
}
SPU__NV_SLOW uint32x4_t spu__fa_fix(uint32x4_t r, uint32x4_t ok, uint32x4_t a, uint32x4_t b, uint32_t negb) {
    if (spu__ff_enabled()) spu__ff_tally(0, ok, a, veorq_u32(b, vdupq_n_u32(negb)), b, 0, 0);
    SPU__NV_LANES(r, o); SPU__NV_LANES(ok, m); SPU__NV_LANES(a, x); SPU__NV_LANES(b, y);
    for (int i = 0; i < 4; i++) if (!m[i]) o[i] = spu__fa_any(x[i], y[i] ^ negb);
    return vld1q_u32(o);
}
SPU__NV_SLOW uint32x4_t spu__fm_fix(uint32x4_t r, uint32x4_t ok, uint32x4_t a, uint32x4_t b) {
    if (spu__ff_enabled()) spu__ff_tally(1, ok, a, b, b, 0, 0);
    SPU__NV_LANES(r, o); SPU__NV_LANES(ok, m); SPU__NV_LANES(a, x); SPU__NV_LANES(b, y);
    for (int i = 0; i < 4; i++) if (!m[i]) o[i] = spu__fm_any(x[i], y[i]);
    return vld1q_u32(o);
}
SPU__NV_SLOW uint32x4_t spu__fma_fix(uint32x4_t r, uint32x4_t ok, uint32x4_t a, uint32x4_t b, uint32x4_t c,
                                     uint32_t negp, uint32_t negc) {
    if (spu__ff_enabled()) spu__ff_tally(2, ok, a, b, c, negp, negc);
    SPU__NV_LANES(r, o); SPU__NV_LANES(ok, m); SPU__NV_LANES(a, x); SPU__NV_LANES(b, y); SPU__NV_LANES(c, z);
    for (int i = 0; i < 4; i++) if (!m[i]) o[i] = spu__fma_any(x[i], y[i], z[i], negp, negc);
    return vld1q_u32(o);
}
SPU__NV_INLINE u128 spu__nv_u128(uint32x4_t v) { u128 r; vst1q_u32(r._u32, v); return r; }
SPU__NV_INLINE u128 spu__nv_fa(u128 a, u128 b, uint32_t negb) {
    const uint32x4_t xa = vld1q_u32(a._u32), yb = vld1q_u32(b._u32);
    const uint32x4_t x = xa, y = veorq_u32(yb, vdupq_n_u32(negb));
    const uint32x4_t px = spu__nv_plain0(x), py = spu__nv_plain0(y);
    /* the exponent-gap limit applies only between two nonzero terms */
    const uint32x4_t pre = vandq_u32(vandq_u32(px, py),
                                     vorrq_u32(vcleq_u32(vabdq_u32(spu__nv_bexp(x), spu__nv_bexp(y)), vdupq_n_u32(29)),
                                               vorrq_u32(vceqzq_u32(vshlq_n_u32(x, 1)), vceqzq_u32(vshlq_n_u32(y, 1)))));
    uint32x2_t oklo, okhi, zlo, zhi;
    const float32x2_t lo = spu__nv_trunc0(vaddq_f64(spu__nv_lo(x), spu__nv_lo(y)), &oklo, &zlo, NULL);
    const float32x2_t hi = spu__nv_trunc0(vaddq_f64(spu__nv_hi(x), spu__nv_hi(y)), &okhi, &zhi, NULL);
    const uint32x4_t ok = vandq_u32(pre, vcombine_u32(oklo, okhi));
    uint32x4_t r = spu__nv_pos0(spu__nv_vec(lo, hi), zlo, zhi);
    if (!spu__nv_all4(ok)) r = spu__fa_fix(r, ok, xa, yb, negb);
    return spu__nv_u128(r);
}
SPU__NV_INLINE u128 spu_fa(u128 a, u128 b) { return spu__nv_fa(a, b, 0); }
SPU__NV_INLINE u128 spu_fs(u128 a, u128 b) { return spu__nv_fa(a, b, 0x80000000u); }
SPU__NV_INLINE u128 spu_fm(u128 a, u128 b) {
    const uint32x4_t x = vld1q_u32(a._u32), y = vld1q_u32(b._u32);
    uint32x2_t oklo, okhi, zlo, zhi;
    const float32x2_t lo = spu__nv_trunc0(vmulq_f64(spu__nv_lo(x), spu__nv_lo(y)), &oklo, &zlo, NULL);
    const float32x2_t hi = spu__nv_trunc0(vmulq_f64(spu__nv_hi(x), spu__nv_hi(y)), &okhi, &zhi, NULL);
    const uint32x4_t ok = vandq_u32(vandq_u32(spu__nv_plain0(x), spu__nv_plain0(y)), vcombine_u32(oklo, okhi));
    uint32x4_t r = spu__nv_pos0(spu__nv_vec(lo, hi), zlo, zhi);
    if (!spu__nv_all4(ok)) r = spu__fm_fix(r, ok, x, y);
    return spu__nv_u128(r);
}
/* p + z in double with its exact rounding error (Knuth's TwoSum: round to nearest, no overflow
 * -- the operands here are at most 2^257). *err == 0 means the sum was exact. */
SPU__NV_INLINE float64x2_t spu__nv_twosum(float64x2_t p, float64x2_t z, float64x2_t* err) {
    const float64x2_t s = vaddq_f64(p, z), bb = vsubq_f64(s, p);
    *err = vaddq_f64(vsubq_f64(p, vsubq_f64(s, bb)), vsubq_f64(z, bb));
    return s;
}
/* (+-a*b) + (+-c): the product is exact in double; one rounding in the add, and TwoSum says
 * whether it rounded. (The old test sent every result with 29 zero low bits to the exact path --
 * every exactly representable result, 1.5*1 + 0.5 -- so fma's common case was slow.) */
SPU__NV_INLINE u128 spu__nv_fma(u128 a, u128 b, u128 c, uint32_t negp, uint32_t negc) {
    const uint32x4_t xa = vld1q_u32(a._u32), y = vld1q_u32(b._u32), zc = vld1q_u32(c._u32);
    const uint32x4_t x = veorq_u32(xa, vdupq_n_u32(negp)), z = veorq_u32(zc, vdupq_n_u32(negc));
    uint32x2_t oklo, okhi, zlo, zhi;
    float64x2_t elo, ehi;
    const float64x2_t slo = spu__nv_twosum(vmulq_f64(spu__nv_lo(x), spu__nv_lo(y)), spu__nv_lo(z), &elo);
    const float64x2_t shi = spu__nv_twosum(vmulq_f64(spu__nv_hi(x), spu__nv_hi(y)), spu__nv_hi(z), &ehi);
    const float32x2_t lo = spu__nv_trunc0(slo, &oklo, &zlo, &elo);
    const float32x2_t hi = spu__nv_trunc0(shi, &okhi, &zhi, &ehi);
    const uint32x4_t ok = vandq_u32(vandq_u32(vandq_u32(spu__nv_plain0(x), spu__nv_plain0(y)), spu__nv_plain0(z)),
                                    vcombine_u32(oklo, okhi));
    uint32x4_t r = spu__nv_pos0(spu__nv_vec(lo, hi), zlo, zhi);
    if (!spu__nv_all4(ok)) r = spu__fma_fix(r, ok, xa, y, zc, negp, negc);
    return spu__nv_u128(r);
}
SPU__NV_INLINE u128 spu_fma(u128 a, u128 b, u128 c)  { return spu__nv_fma(a, b, c, 0, 0); }            /* a*b + c */
SPU__NV_INLINE u128 spu_fms(u128 a, u128 b, u128 c)  { return spu__nv_fma(a, b, c, 0, 0x80000000u); }  /* a*b - c */
SPU__NV_INLINE u128 spu_fnms(u128 a, u128 b, u128 c) { return spu__nv_fma(a, b, c, 0x80000000u, 0); }  /* c - a*b */
#elif defined(__SSE4_1__) && !defined(SPU_SCALAR_HELPERS)
/* x86 SSE4.1: the NEON forms above, lane for lane -- operate in two f64x2 halves, accept
 * ordinary singles and true zeros (plain0), truncate by clearing the 29 low significand bits
 * (the value then fits a single, so the narrowing cvt is exact), clear the sign of zero results,
 * and for fma use TwoSum to know whether the add rounded. Lanes that do not qualify are redone
 * by the scalar spu__*_any (out of line), so results are bit-identical to the exact path
 * (God of War Ascension spent ~12% of every SPU thread in the per-lane scalar helpers). */
#include <smmintrin.h>
#define SPU__SX_INLINE static inline __attribute__((always_inline))
#define SPU__SX_SLOW   static inline __attribute__((noinline, cold))
SPU__SX_INLINE __m128i spu__sx_ld(u128 v) { __m128i r; memcpy(&r, &v, 16); return r; }
SPU__SX_INLINE u128 spu__sx_st(__m128i v) { u128 r; memcpy(&r, &v, 16); return r; }
SPU__SX_INLINE __m128i spu__sx_bexp(__m128i x) { return _mm_and_si128(_mm_srli_epi32(x, 23), _mm_set1_epi32(0xFF)); }
SPU__SX_INLINE __m128i spu__sx_iszero(__m128i x) { return _mm_cmpeq_epi32(_mm_slli_epi32(x, 1), _mm_setzero_si128()); }
SPU__SX_INLINE __m128i spu__sx_plain0(__m128i x) {         /* biased exponent 1..254, or +-0.0 */
    const __m128i e = spu__sx_bexp(x);
    const __m128i pl = _mm_and_si128(_mm_cmpgt_epi32(e, _mm_setzero_si128()), _mm_cmplt_epi32(e, _mm_set1_epi32(255)));
    return _mm_or_si128(pl, spu__sx_iszero(x));
}
/* Two doubles -> two single bit patterns (low half of the result). *ok (64-bit lanes): the
 * double is +-0 or has a normal single's exponent and, when err is given, either did not round
 * (*err == 0) or does not sit on a 24-bit boundary. *zm: the double is +-0. */
SPU__SX_INLINE __m128i spu__sx_trunc0(__m128d d, __m128i* ok, __m128i* zm, const __m128d* err) {
    const __m128i u = _mm_castpd_si128(d);
    const __m128i az = _mm_cmpeq_epi64(_mm_slli_epi64(u, 1), _mm_setzero_si128());
    const __m128i e = _mm_and_si128(_mm_srli_epi64(u, 52), _mm_set1_epi64x(0x7FF));
    const __m128i e_ok = _mm_and_si128(_mm_cmpgt_epi32(e, _mm_set1_epi32(896)), _mm_cmplt_epi32(e, _mm_set1_epi32(1151)));
    __m128i good = _mm_shuffle_epi32(e_ok, _MM_SHUFFLE(2, 2, 0, 0));  /* each lane's low-dword verdict */
    if (err) {
        const __m128i on_b = _mm_cmpeq_epi64(_mm_and_si128(u, _mm_set1_epi64x(0x1FFFFFFFll)), _mm_setzero_si128());
        const __m128i exact = _mm_castpd_si128(_mm_cmpeq_pd(*err, _mm_setzero_pd()));
        good = _mm_and_si128(good, _mm_or_si128(_mm_xor_si128(on_b, _mm_set1_epi32(-1)), exact));
    }
    *ok = _mm_or_si128(good, az);
    *zm = az;
    const __m128i t = _mm_and_si128(u, _mm_set1_epi64x(~0x1FFFFFFFll));
    return _mm_castps_si128(_mm_cvtpd_ps(_mm_castsi128_pd(t)));
}
SPU__SX_INLINE __m128d spu__sx_lo(__m128i x) { return _mm_cvtps_pd(_mm_castsi128_ps(x)); }
SPU__SX_INLINE __m128d spu__sx_hi(__m128i x) { return _mm_cvtps_pd(_mm_castsi128_ps(_mm_unpackhi_epi64(x, x))); }
SPU__SX_INLINE __m128i spu__sx_join(__m128i lo, __m128i hi) { return _mm_unpacklo_epi64(lo, hi); }
SPU__SX_INLINE __m128i spu__sx_joinm(__m128i lo, __m128i hi) {   /* two 64-bit-lane masks -> 4 dword lanes */
    return _mm_castps_si128(_mm_shuffle_ps(_mm_castsi128_ps(lo), _mm_castsi128_ps(hi), _MM_SHUFFLE(2, 0, 2, 0)));
}
/* r with the sign cleared in the lanes whose result is zero */
SPU__SX_INLINE __m128i spu__sx_pos0(__m128i r, __m128i zlo, __m128i zhi) {
    return _mm_andnot_si128(_mm_and_si128(spu__sx_joinm(zlo, zhi), _mm_set1_epi32((int)0x80000000u)), r);
}
SPU__SX_INLINE __m128d spu__sx_twosum(__m128d p, __m128d z, __m128d* err) {
    const __m128d s = _mm_add_pd(p, z), bb = _mm_sub_pd(s, p);
    *err = _mm_add_pd(_mm_sub_pd(p, _mm_sub_pd(s, bb)), _mm_sub_pd(z, bb));
    return s;
}
#if defined(__AVX512F__) && defined(__AVX512VL__) && !defined(SPU_SX_FIX_LANE)
/* GOWA (21/10): o conserto exato das 4 palavras de uma vez, em double, sem o __int128 por
 * palavra (spu__sx_fma_fix era ~1,5% de cada thread do EDGE em God of War Ascension: a D92C
 * usa resultados de cflts como operando de float). Todo float da SPU -- expoente 255 incluso
 * -- é exato em double; produto de mantissas de 24 bits exato em 53; a conta em double com
 * {rz-sae} e o corte a 24 bits dão o exato truncado (truncar a 53 e depois a 24 = truncar a
 * 24). Satura acima de Smax, +0 abaixo de Smin. A mesma conta do spu_vk_exato.h, conferida
 * contra spu__*_any (out/spudiff/teste_vk_exato.c, teste_sx_fix.c). SPU_SX_FIX_LANE volta
 * ao laço por palavra. */
SPU__SX_INLINE __m512d spu__xf_d(__m128i w) {      /* 4 palavras SPU -> 4 doubles exatos (+4 zeros) */
    const __m512i u = _mm512_cvtepu32_epi64(_mm256_zextsi128_si256(w));
    const __m512i e = _mm512_and_si512(_mm512_srli_epi64(u, 23), _mm512_set1_epi64(0xFF));
    const __m512i s = _mm512_slli_epi64(_mm512_srli_epi64(u, 31), 63);
    const __m512i f = _mm512_slli_epi64(_mm512_and_si512(u, _mm512_set1_epi64(0x7FFFFF)), 29);
    const __m512i de = _mm512_slli_epi64(_mm512_add_epi64(e, _mm512_set1_epi64(1023 - 127)), 52);
    return _mm512_castsi512_pd(_mm512_maskz_mov_epi64(_mm512_test_epi64_mask(e, e), _mm512_or_si512(_mm512_or_si512(s, de), f)));
}
SPU__SX_INLINE __m128i spu__xf_s(__m512d x) {      /* doubles -> palavras SPU (as 4 de baixo) */
    const __m512i u = _mm512_castpd_si512(x);
    const __m512i be = _mm512_sub_epi64(_mm512_and_si512(_mm512_srli_epi64(u, 52), _mm512_set1_epi64(0x7FF)), _mm512_set1_epi64(1023 - 127));
    const __m512i s = _mm512_and_si512(_mm512_srli_epi64(u, 32), _mm512_set1_epi64(0x80000000));
    const __m512i f = _mm512_and_si512(_mm512_srli_epi64(u, 29), _mm512_set1_epi64(0x7FFFFF));
    __m512i r = _mm512_or_si512(_mm512_or_si512(s, _mm512_slli_epi64(be, 23)), f);
    r = _mm512_mask_mov_epi64(r, _mm512_cmpgt_epi64_mask(be, _mm512_set1_epi64(255)), _mm512_or_si512(s, _mm512_set1_epi64(0x7FFFFFFF)));
    r = _mm512_maskz_mov_epi64(_mm512_cmpgt_epi64_mask(be, _mm512_setzero_si512()), r);
    return _mm256_castsi256_si128(_mm512_cvtepi64_epi32(r));
}
#define SPU__XF_RZ (_MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC)
#if defined(_WIN64) && defined(__clang__) && !defined(SPU_SX_FIX_NO_VC)
/* GOWA (23/10): no Windows x64 um u128 (e um __m128i) vai por REFERÊNCIA a uma cópia
 * na pilha, e o retorno por ponteiro oculto: cada conserto frio dentro de um laço
 * quente era uma chamada que lê e grava memória, e o clang não podia manter os
 * registradores SPU (ctx->gpr) em registradores do x86 no laço -- cada operação
 * relia e regravava o contexto. Com __vectorcall os vetores vão e voltam em XMM e o
 * conserto não toca memória nenhuma. SPU_SX_FIX_NO_VC volta à forma antiga. */
#define SPU__SX_SLOW_VC static __attribute__((noinline, cold)) __m128i __vectorcall
SPU__SX_SLOW_VC spu__sx_fa_fix_v(__m128i r, __m128i x, __m128i y, int m, uint32_t negb) {
    const __m128i e = spu__xf_s(_mm512_add_round_pd(spu__xf_d(x), spu__xf_d(_mm_xor_si128(y, _mm_set1_epi32((int)negb))), SPU__XF_RZ));
    return _mm_mask_mov_epi32(r, (__mmask8)(~m & 0xF), e);
}
SPU__SX_SLOW_VC spu__sx_fm_fix_v(__m128i r, __m128i x, __m128i y, int m) {
    const __m128i e = spu__xf_s(_mm512_mul_round_pd(spu__xf_d(x), spu__xf_d(y), SPU__XF_RZ));
    return _mm_mask_mov_epi32(r, (__mmask8)(~m & 0xF), e);
}
SPU__SX_SLOW_VC spu__sx_fma_fix_v(__m128i r, __m128i x, __m128i y, __m128i z, int m, uint32_t negp, uint32_t negc) {
    const __m128i e = spu__xf_s(_mm512_fmadd_round_pd(spu__xf_d(_mm_xor_si128(x, _mm_set1_epi32((int)negp))),
                                                      spu__xf_d(y),
                                                      spu__xf_d(_mm_xor_si128(z, _mm_set1_epi32((int)negc))), SPU__XF_RZ));
    return _mm_mask_mov_epi32(r, (__mmask8)(~m & 0xF), e);
}
SPU__SX_INLINE u128 spu__sx_fa_fix(u128 r, int m, u128 x, u128 y, uint32_t negb) {
    return spu__sx_st(spu__sx_fa_fix_v(spu__sx_ld(r), spu__sx_ld(x), spu__sx_ld(y), m, negb));
}
SPU__SX_INLINE u128 spu__sx_fm_fix(u128 r, int m, u128 x, u128 y) {
    return spu__sx_st(spu__sx_fm_fix_v(spu__sx_ld(r), spu__sx_ld(x), spu__sx_ld(y), m));
}
SPU__SX_INLINE u128 spu__sx_fma_fix(u128 r, int m, u128 x, u128 y, u128 z, uint32_t negp, uint32_t negc) {
    return spu__sx_st(spu__sx_fma_fix_v(spu__sx_ld(r), spu__sx_ld(x), spu__sx_ld(y), spu__sx_ld(z), m, negp, negc));
}
#else
SPU__SX_SLOW u128 spu__sx_fa_fix(u128 r, int m, u128 x, u128 y, uint32_t negb) {
    const __m128i e = spu__xf_s(_mm512_add_round_pd(spu__xf_d(spu__sx_ld(x)),
                                                    spu__xf_d(_mm_xor_si128(spu__sx_ld(y), _mm_set1_epi32((int)negb))), SPU__XF_RZ));
    return spu__sx_st(_mm_mask_mov_epi32(spu__sx_ld(r), (__mmask8)(~m & 0xF), e));
}
SPU__SX_SLOW u128 spu__sx_fm_fix(u128 r, int m, u128 x, u128 y) {
    const __m128i e = spu__xf_s(_mm512_mul_round_pd(spu__xf_d(spu__sx_ld(x)), spu__xf_d(spu__sx_ld(y)), SPU__XF_RZ));
    return spu__sx_st(_mm_mask_mov_epi32(spu__sx_ld(r), (__mmask8)(~m & 0xF), e));
}
SPU__SX_SLOW u128 spu__sx_fma_fix(u128 r, int m, u128 x, u128 y, u128 z, uint32_t negp, uint32_t negc) {
    const __m128i e = spu__xf_s(_mm512_fmadd_round_pd(spu__xf_d(_mm_xor_si128(spu__sx_ld(x), _mm_set1_epi32((int)negp))),
                                                      spu__xf_d(spu__sx_ld(y)),
                                                      spu__xf_d(_mm_xor_si128(spu__sx_ld(z), _mm_set1_epi32((int)negc))), SPU__XF_RZ));
    return spu__sx_st(_mm_mask_mov_epi32(spu__sx_ld(r), (__mmask8)(~m & 0xF), e));
}
#endif
#else
SPU__SX_SLOW u128 spu__sx_fa_fix(u128 r, int m, u128 x, u128 y, uint32_t negb) {
    for (int i = 0; i < 4; i++) if (!(m & (1 << i))) r._u32[i] = spu__fa_any(x._u32[i], y._u32[i] ^ negb);
    return r;
}
SPU__SX_SLOW u128 spu__sx_fm_fix(u128 r, int m, u128 x, u128 y) {
    for (int i = 0; i < 4; i++) if (!(m & (1 << i))) r._u32[i] = spu__fm_any(x._u32[i], y._u32[i]);
    return r;
}
SPU__SX_SLOW u128 spu__sx_fma_fix(u128 r, int m, u128 x, u128 y, u128 z, uint32_t negp, uint32_t negc) {
    for (int i = 0; i < 4; i++) if (!(m & (1 << i))) r._u32[i] = spu__fma_any(x._u32[i], y._u32[i], z._u32[i], negp, negc);
    return r;
}
#endif
SPU__SX_INLINE u128 spu__sx_fa(u128 a, u128 b, uint32_t negb) {
    const __m128i x = spu__sx_ld(a), y = _mm_xor_si128(spu__sx_ld(b), _mm_set1_epi32((int)negb));
    /* the exponent-gap limit applies only between two nonzero terms */
    const __m128i gap = _mm_cmplt_epi32(_mm_abs_epi32(_mm_sub_epi32(spu__sx_bexp(x), spu__sx_bexp(y))), _mm_set1_epi32(30));
    const __m128i pre = _mm_and_si128(_mm_and_si128(spu__sx_plain0(x), spu__sx_plain0(y)),
                                      _mm_or_si128(gap, _mm_or_si128(spu__sx_iszero(x), spu__sx_iszero(y))));
    __m128i ol, oh, zl, zh;
    const __m128i rl = spu__sx_trunc0(_mm_add_pd(spu__sx_lo(x), spu__sx_lo(y)), &ol, &zl, NULL);
    const __m128i rh = spu__sx_trunc0(_mm_add_pd(spu__sx_hi(x), spu__sx_hi(y)), &oh, &zh, NULL);
    const int m = _mm_movemask_ps(_mm_castsi128_ps(_mm_and_si128(pre, spu__sx_joinm(ol, oh))));
    u128 r = spu__sx_st(spu__sx_pos0(spu__sx_join(rl, rh), zl, zh));
    if (m != 0xF) r = spu__sx_fa_fix(r, m, a, b, negb);
    return r;
}
/* GOWA: AVX-512 (Zen 4/5, Ice Lake and later): fa/fs/fm/fma/fms/fnms as ONE
 * native single-precision instruction with the rounding mode embedded in it
 * ({rz-sae}: toward zero, the SPU's truncation). Only the 512-bit form takes
 * an embedded rounding mode, so the four lanes ride in the low quarter of a
 * zmm whose upper lanes are zero. The SPU reads a denormal operand as zero
 * (zeroed first) and flushes a result below Smin to +0, any zero included
 * (exponent-0 lanes are zeroed after). Truncation never takes a finite result
 * up to exponent 255, so a result there means an operand at 255 -- Inf/NaN to
 * the host, the SPU's extended range -- and a result of +-0x7F7FFFFF may be
 * the host saturating where the SPU would go on: those lanes take the exact
 * path. Everything else is the IEEE result rounded toward zero, which is the
 * SPU's. Bit-identical to spu_*_ref (out/spudiff/teste_float_sse.c, built for
 * znver5). SPU_SX_NO_AVX512 keeps the SSE/AVX2 forms below. */
#if defined(__AVX512F__) && defined(__AVX512VL__) && !defined(SPU_SX_FA_DOUBLE) && !defined(SPU_SX_NO_AVX512)
#define SPU__ZX 1
#include <immintrin.h>
#define SPU__ZX_RZ (_MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC)
#if defined(SPU_SX_DAZ)
/* GOWA: SPU_SX_DAZ -- the host thread runs with MXCSR.DAZ set (spu_daz_enter,
 * at every SPU host thread start), so the hardware reads a denormal operand
 * as a zero of the same sign. The sign of a zero operand cannot reach the
 * result: every zero result is forced to +0 on the way out (spu__zx_out). */
SPU__SX_INLINE __m512 spu__zx_in(__m128i x) { return _mm512_zextps128_ps512(_mm_castsi128_ps(x)); }
#else
SPU__SX_INLINE __m512 spu__zx_in(__m128i x) {            /* denormal -> 0; zmm, upper lanes zero */
    const __m128i z = _mm_maskz_mov_epi32(_mm_test_epi32_mask(x, _mm_set1_epi32(0x7F800000)), x);
    return _mm512_zextps128_ps512(_mm_castsi128_ps(z));
}
#endif
SPU__SX_INLINE __m128i spu__zx_out(__m512 r, __mmask8* bad) {
    const __m128i ri = _mm_castps_si128(_mm512_castps512_ps128(r));
#if defined(SPU_SX_APPROX)
    /* GOWA: SPU_SX_APPROX -- the exact path only for a result the host made
     * Inf or NaN (one vfpclassps), which is what any operand at exponent 255
     * -- the SPU's extended range -- turns into. Dropping the check entirely
     * (RPCS3's "approximate xfloat") let such NaNs through and blacked out the
     * lighting. Left approximate: a finite result saturated to +-FLT_MAX,
     * where the SPU would go on into its extended range. */
    *bad = _mm_fpclass_ps_mask(_mm_castsi128_ps(ri), 0x99) & 0xF;   /* QNaN|SNaN|+Inf|-Inf */
#else
    *bad = _mm_cmpge_epu32_mask(_mm_and_si128(ri, _mm_set1_epi32(0x7FFFFFFF)), _mm_set1_epi32(0x7F7FFFFF)) & 0xF;
#endif
#if defined(SPU_SX_NOFLUSH)
    /* GOWA: SPU_SX_NOFLUSH -- MEDIÇÃO APENAS (não exato): sem o +0 na saída,
     * para medir quanto custa achatar cada resultado (teto de um achatamento
     * só onde o valor sai do float). */
    return ri;
#endif
    return _mm_maskz_mov_epi32(_mm_test_epi32_mask(ri, _mm_set1_epi32(0x7F800000)), ri);  /* below Smin, or zero: +0 */
}
/* GOWA: forma "crua" da saída (cru = 1): o mesmo resultado sem o +0 dos
 * lanes de expoente 0. Só para valores que vão SÓ para outra conta de float
 * (scripts/flush_preguicoso.py decide): com DAZ o hardware lê o denormal como
 * zero, o sinal de um zero de entrada não chega a nenhum resultado (todo
 * resultado zero sai +0 de novo) e fcgt/fceq tratam expoente 0 como zero --
 * então o valor final de cada cadeia é o mesmo bit a bit. */
SPU__SX_INLINE __m128i spu__zx_outc(__m512 r, __mmask8* bad, int cru) {
    if (!cru) return spu__zx_out(r, bad);
#if defined(SPU_SX_CRU_CHECK)
    /* modo de checagem: o resultado normal, com o veneno 0x007BAD5A (denormal)
     * em todo lane zero -- equivalente para as contas, detectável na gravação */
    { const __m128i ri0 = spu__zx_out(r, bad);
      return _mm_mask_mov_epi32(ri0, _mm_cmpeq_epi32_mask(ri0, _mm_setzero_si128()), _mm_set1_epi32(0x007BAD5A)); }
#endif
    const __m128i ri = _mm_castps_si128(_mm512_castps512_ps128(r));
#if defined(SPU_SX_APPROX)
    *bad = _mm_fpclass_ps_mask(_mm_castsi128_ps(ri), 0x99) & 0xF;
#else
    *bad = _mm_cmpge_epu32_mask(_mm_and_si128(ri, _mm_set1_epi32(0x7FFFFFFF)), _mm_set1_epi32(0x7F7FFFFF)) & 0xF;
#endif
    return ri;
}
SPU__SX_INLINE u128 spu__zx_fa(u128 a, u128 b, uint32_t negb, int cru) {
    const __m128i x = spu__sx_ld(a), y = _mm_xor_si128(spu__sx_ld(b), _mm_set1_epi32((int)negb));
    __mmask8 bad;
    u128 r = spu__sx_st(spu__zx_outc(_mm512_add_round_ps(spu__zx_in(x), spu__zx_in(y), SPU__ZX_RZ), &bad, cru));
    if (bad) r = spu__sx_fa_fix(r, ~bad & 0xF, a, b, negb);
    return r;
}
SPU__SX_INLINE u128 spu_fa(u128 a, u128 b) { return spu__zx_fa(a, b, 0, 0); }
SPU__SX_INLINE u128 spu_fs(u128 a, u128 b) { return spu__zx_fa(a, b, 0x80000000u, 0); }
SPU__SX_INLINE u128 spu_fa_r(u128 a, u128 b) { return spu__zx_fa(a, b, 0, 1); }
SPU__SX_INLINE u128 spu_fs_r(u128 a, u128 b) { return spu__zx_fa(a, b, 0x80000000u, 1); }
SPU__SX_INLINE u128 spu__zx_fm(u128 a, u128 b, int cru) {
    __mmask8 bad;
    u128 r = spu__sx_st(spu__zx_outc(_mm512_mul_round_ps(spu__zx_in(spu__sx_ld(a)), spu__zx_in(spu__sx_ld(b)), SPU__ZX_RZ), &bad, cru));
    if (bad) r = spu__sx_fm_fix(r, ~bad & 0xF, a, b);
    return r;
}
SPU__SX_INLINE u128 spu_fm(u128 a, u128 b) { return spu__zx_fm(a, b, 0); }
SPU__SX_INLINE u128 spu_fm_r(u128 a, u128 b) { return spu__zx_fm(a, b, 1); }
SPU__SX_INLINE u128 spu__zx_fma(u128 a, u128 b, u128 c, uint32_t negp, uint32_t negc, int cru) {
    const __m128i x = _mm_xor_si128(spu__sx_ld(a), _mm_set1_epi32((int)negp));
    const __m128i z = _mm_xor_si128(spu__sx_ld(c), _mm_set1_epi32((int)negc));
    __mmask8 bad;
    u128 r = spu__sx_st(spu__zx_outc(_mm512_fmadd_round_ps(spu__zx_in(x), spu__zx_in(spu__sx_ld(b)), spu__zx_in(z), SPU__ZX_RZ), &bad, cru));
    if (bad) r = spu__sx_fma_fix(r, ~bad & 0xF, a, b, c, negp, negc);
    return r;
}
SPU__SX_INLINE u128 spu_fma(u128 a, u128 b, u128 c)  { return spu__zx_fma(a, b, c, 0, 0, 0); }            /* a*b + c */
SPU__SX_INLINE u128 spu_fms(u128 a, u128 b, u128 c)  { return spu__zx_fma(a, b, c, 0, 0x80000000u, 0); }  /* a*b - c */
SPU__SX_INLINE u128 spu_fnms(u128 a, u128 b, u128 c) { return spu__zx_fma(a, b, c, 0x80000000u, 0, 0); }  /* c - a*b */
SPU__SX_INLINE u128 spu_fma_r(u128 a, u128 b, u128 c)  { return spu__zx_fma(a, b, c, 0, 0, 1); }
SPU__SX_INLINE u128 spu_fms_r(u128 a, u128 b, u128 c)  { return spu__zx_fma(a, b, c, 0, 0x80000000u, 1); }
SPU__SX_INLINE u128 spu_fnms_r(u128 a, u128 b, u128 c) { return spu__zx_fma(a, b, c, 0x80000000u, 0, 1); }
#else
/* sem AVX-512: as formas cruas são as normais */
#define spu_fa_r spu_fa
#define spu_fs_r spu_fs
#define spu_fm_r spu_fm
#define spu_fma_r spu_fma
#define spu_fms_r spu_fms
#define spu_fnms_r spu_fnms
#endif
#if !defined(SPU__ZX) && defined(SPU_SX_RZ) && defined(__FMA__)
/* GOWA (24/10): SPU_SX_RZ -- builds without AVX-512 (x86-64-v3, for machines like
 * an i5-10300H). Every SPU host thread runs with MXCSR.RC = round toward zero
 * (spu_daz_enter), so a plain single-precision add / mul / FMA already rounds the
 * way the SPU truncates; what is left is the AVX-512 path's own work (spu__zx_*):
 * denormal operands read as zero (MXCSR.DAZ with SPU_SX_DAZ, zeroed here
 * otherwise), a result at |r| >= 0x7F7FFFFF (an exponent-255 operand -- the SPU's
 * extended range -- or the host saturating where the SPU goes on) takes the exact
 * path, and a result below Smin, or any zero, leaves as +0. The SSE/AVX2 forms
 * below (TwoSum, double FMA) cost ~20-30 instructions an op and assume
 * round-to-nearest, so this mode needs the whole process built with it (the
 * runtime's interpreter runs on the same threads): CMake GOWA_SPU_RZ. The exact
 * paths (spu__*_any) are right in either mode (exact in double, or truncation
 * after a toward-zero double rounding). The operands go through an empty asm so
 * that the compiler cannot fold a constant operation with its own round-to-nearest.
 * Bit-identical to spu_*_ref (out/spudiff/teste_float_rz.c). */
#define SPU__RZ 1
#include <immintrin.h>
#if defined(__AVX2__) && defined(__BMI2__)
#define GOWA_VK_AVX2 1   /* os núcleos vk em 2 x 256 bits (spu_vk_avx2.h) */
#endif
#if defined(SPU_SX_DAZ)
SPU__SX_INLINE __m128i spu__rz_in(__m128i x) { __asm__("" : "+x"(x)); return x; }
#else
SPU__SX_INLINE __m128i spu__rz_in(__m128i x) {
    x = _mm_andnot_si128(_mm_cmpeq_epi32(_mm_and_si128(x, _mm_set1_epi32(0x7F800000)), _mm_setzero_si128()), x);
    __asm__("" : "+x"(x)); return x;
}
#endif
SPU__SX_INLINE __m128i spu__rz_out(__m128 r, int* ok) {
    const __m128i ri = _mm_castps_si128(r);
    const __m128i bad = _mm_cmpgt_epi32(_mm_and_si128(ri, _mm_set1_epi32(0x7FFFFFFF)), _mm_set1_epi32(0x7F7FFFFE));
    *ok = ~_mm_movemask_ps(_mm_castsi128_ps(bad)) & 0xF;
    return _mm_andnot_si128(_mm_cmpeq_epi32(_mm_and_si128(ri, _mm_set1_epi32(0x7F800000)), _mm_setzero_si128()), ri);
}
SPU__SX_INLINE u128 spu__rz_fa(u128 a, u128 b, uint32_t negb) {
    const __m128i x = spu__rz_in(spu__sx_ld(a)), y = spu__rz_in(_mm_xor_si128(spu__sx_ld(b), _mm_set1_epi32((int)negb)));
    int ok;
    u128 r = spu__sx_st(spu__rz_out(_mm_add_ps(_mm_castsi128_ps(x), _mm_castsi128_ps(y)), &ok));
    if (ok != 0xF) r = spu__sx_fa_fix(r, ok, a, b, negb);
    return r;
}
SPU__SX_INLINE u128 spu_fa(u128 a, u128 b) { return spu__rz_fa(a, b, 0); }
SPU__SX_INLINE u128 spu_fs(u128 a, u128 b) { return spu__rz_fa(a, b, 0x80000000u); }
SPU__SX_INLINE u128 spu_fm(u128 a, u128 b) {
    const __m128i x = spu__rz_in(spu__sx_ld(a)), y = spu__rz_in(spu__sx_ld(b));
    int ok;
    u128 r = spu__sx_st(spu__rz_out(_mm_mul_ps(_mm_castsi128_ps(x), _mm_castsi128_ps(y)), &ok));
    if (ok != 0xF) r = spu__sx_fm_fix(r, ok, a, b);
    return r;
}
SPU__SX_INLINE u128 spu__rz_fma(u128 a, u128 b, u128 c, uint32_t negp, uint32_t negc) {
    const __m128i x = spu__rz_in(_mm_xor_si128(spu__sx_ld(a), _mm_set1_epi32((int)negp)));
    const __m128i y = spu__rz_in(spu__sx_ld(b));
    const __m128i z = spu__rz_in(_mm_xor_si128(spu__sx_ld(c), _mm_set1_epi32((int)negc)));
    int ok;
    u128 r = spu__sx_st(spu__rz_out(_mm_fmadd_ps(_mm_castsi128_ps(x), _mm_castsi128_ps(y), _mm_castsi128_ps(z)), &ok));
    if (ok != 0xF) r = spu__sx_fma_fix(r, ok, a, b, c, negp, negc);
    return r;
}
SPU__SX_INLINE u128 spu_fma(u128 a, u128 b, u128 c)  { return spu__rz_fma(a, b, c, 0, 0); }            /* a*b + c */
SPU__SX_INLINE u128 spu_fms(u128 a, u128 b, u128 c)  { return spu__rz_fma(a, b, c, 0, 0x80000000u); }  /* a*b - c */
SPU__SX_INLINE u128 spu_fnms(u128 a, u128 b, u128 c) { return spu__rz_fma(a, b, c, 0x80000000u, 0); }  /* c - a*b */
#elif !defined(SPU__ZX)
/* GOWA: fa/fs/fm in single precision on the round-to-nearest hardware, no
 * double detour. r = x op y (RN); the exact error e of that rounding comes
 * from TwoSum (add; exact for finite operands, gradual underflow included) or
 * from an FMA (mul: x*y - r). When the rounding went AWAY from zero (e != 0
 * and opposite in sign to r), truncation is the next single toward zero:
 * bits(r) - 1. Then the SPU's flush: a result below Smin (exponent field 0
 * after the step) is +0, as is any zero. Lanes with an operand that is not an
 * ordinary single or zero (denormal, exponent 255), with a result at exponent
 * 255 (saturation/extended range), or -- for mul -- with a product so small its
 * error term may underflow (biased result exponent <= 24), take the exact path.
 * Bit-identical to spu_*_ref (out/spudiff/teste_float_sse.c). SPU_SX_FA_DOUBLE
 * restores the double-precision forms. */
#ifndef SPU_SX_FA_DOUBLE
SPU__SX_INLINE __m128i spu__sx_trunc_step(__m128i ri, __m128i ei) {
    const __m128i enz = _mm_xor_si128(_mm_cmpeq_epi32(_mm_slli_epi32(ei, 1), _mm_setzero_si128()), _mm_set1_epi32(-1));
    const __m128i opp = _mm_srai_epi32(_mm_xor_si128(ei, ri), 31);
    ri = _mm_add_epi32(ri, _mm_and_si128(enz, opp));                      /* away from zero: one ulp back */
    const __m128i e0 = _mm_cmpeq_epi32(_mm_and_si128(ri, _mm_set1_epi32(0x7F800000)), _mm_setzero_si128());
    return _mm_andnot_si128(e0, ri);                                      /* below Smin, or zero: +0 */
}
SPU__SX_INLINE u128 spu__sx_fa1(u128 a, u128 b, uint32_t negb) {
    const __m128i xi = spu__sx_ld(a), yi = _mm_xor_si128(spu__sx_ld(b), _mm_set1_epi32((int)negb));
    const __m128 x = _mm_castsi128_ps(xi), y = _mm_castsi128_ps(yi);
    const __m128 s = _mm_add_ps(x, y), bb = _mm_sub_ps(s, x);
    const __m128 e = _mm_add_ps(_mm_sub_ps(x, _mm_sub_ps(s, bb)), _mm_sub_ps(y, bb));
    const __m128i si = _mm_castps_si128(s);
    const __m128i ok = _mm_and_si128(_mm_and_si128(spu__sx_plain0(xi), spu__sx_plain0(yi)),
                                     _mm_xor_si128(_mm_cmpeq_epi32(spu__sx_bexp(si), _mm_set1_epi32(255)), _mm_set1_epi32(-1)));
    u128 r = spu__sx_st(spu__sx_trunc_step(si, _mm_castps_si128(e)));
    const int m = _mm_movemask_ps(_mm_castsi128_ps(ok));
    if (m != 0xF) r = spu__sx_fa_fix(r, m, a, b, negb);
    return r;
}
SPU__SX_INLINE u128 spu_fa(u128 a, u128 b) { return spu__sx_fa1(a, b, 0); }
SPU__SX_INLINE u128 spu_fs(u128 a, u128 b) { return spu__sx_fa1(a, b, 0x80000000u); }
#else
SPU__SX_INLINE u128 spu_fa(u128 a, u128 b) { return spu__sx_fa(a, b, 0); }
SPU__SX_INLINE u128 spu_fs(u128 a, u128 b) { return spu__sx_fa(a, b, 0x80000000u); }
#endif
#if defined(__FMA__) && !defined(SPU_SX_FA_DOUBLE)
#include <immintrin.h>
SPU__SX_INLINE u128 spu_fm(u128 a, u128 b) {
    const __m128i xi = spu__sx_ld(a), yi = spu__sx_ld(b);
    const __m128 x = _mm_castsi128_ps(xi), y = _mm_castsi128_ps(yi);
    const __m128 p = _mm_mul_ps(x, y);
    const __m128 e = _mm_fmsub_ps(x, y, p);                               /* exact: x*y - p */
    const __m128i pi = _mm_castps_si128(p), pe = spu__sx_bexp(pi);
    /* the error is a multiple of 2^(ex+ey) and exact only while that is at
     * least the smallest denormal: biased product exponent > 24 */
    const __m128i rng = _mm_and_si128(_mm_cmpgt_epi32(pe, _mm_set1_epi32(24)), _mm_cmplt_epi32(pe, _mm_set1_epi32(255)));
    const __m128i ok = _mm_and_si128(_mm_and_si128(spu__sx_plain0(xi), spu__sx_plain0(yi)),
                                     _mm_or_si128(rng, _mm_or_si128(spu__sx_iszero(xi), spu__sx_iszero(yi))));
    u128 r = spu__sx_st(spu__sx_trunc_step(pi, _mm_castps_si128(e)));
    const int m = _mm_movemask_ps(_mm_castsi128_ps(ok));
    if (m != 0xF) r = spu__sx_fm_fix(r, m, a, b);
    return r;
}
#else
SPU__SX_INLINE u128 spu_fm(u128 a, u128 b) {
    const __m128i x = spu__sx_ld(a), y = spu__sx_ld(b);
    __m128i ol, oh, zl, zh;
    const __m128i rl = spu__sx_trunc0(_mm_mul_pd(spu__sx_lo(x), spu__sx_lo(y)), &ol, &zl, NULL);
    const __m128i rh = spu__sx_trunc0(_mm_mul_pd(spu__sx_hi(x), spu__sx_hi(y)), &oh, &zh, NULL);
    const __m128i pre = _mm_and_si128(spu__sx_plain0(x), spu__sx_plain0(y));
    const int m = _mm_movemask_ps(_mm_castsi128_ps(_mm_and_si128(pre, spu__sx_joinm(ol, oh))));
    u128 r = spu__sx_st(spu__sx_pos0(spu__sx_join(rl, rh), zl, zh));
    if (m != 0xF) r = spu__sx_fm_fix(r, m, a, b);
    return r;
}
#endif
SPU__SX_INLINE u128 spu__sx_fma(u128 a, u128 b, u128 c, uint32_t negp, uint32_t negc) {
    const __m128i x = _mm_xor_si128(spu__sx_ld(a), _mm_set1_epi32((int)negp)), y = spu__sx_ld(b);
    const __m128i z = _mm_xor_si128(spu__sx_ld(c), _mm_set1_epi32((int)negc));
    __m128i ol, oh, zl, zh;
    __m128d el, eh;
    const __m128d sl = spu__sx_twosum(_mm_mul_pd(spu__sx_lo(x), spu__sx_lo(y)), spu__sx_lo(z), &el);
    const __m128d sh = spu__sx_twosum(_mm_mul_pd(spu__sx_hi(x), spu__sx_hi(y)), spu__sx_hi(z), &eh);
    const __m128i rl = spu__sx_trunc0(sl, &ol, &zl, &el);
    const __m128i rh = spu__sx_trunc0(sh, &oh, &zh, &eh);
    const __m128i pre = _mm_and_si128(_mm_and_si128(spu__sx_plain0(x), spu__sx_plain0(y)), spu__sx_plain0(z));
    const int m = _mm_movemask_ps(_mm_castsi128_ps(_mm_and_si128(pre, spu__sx_joinm(ol, oh))));
    u128 r = spu__sx_st(spu__sx_pos0(spu__sx_join(rl, rh), zl, zh));
    if (m != 0xF) r = spu__sx_fma_fix(r, m, a, b, c, negp, negc);
    return r;
}
/* Opt-in (SPU_SX_FMA2): measured SLOWER in God of War Ascension (25.6/25.9
 * against 27.3/27.5 FPS) -- every exactly representable result sits on a
 * 24-bit boundary, so nearly every call takes the extra-FMA branch; the
 * branch-free TwoSum form below wins. Bit-identical either way. */
#if defined(__FMA__) && !defined(SPU_SX_FA_DOUBLE) && defined(SPU_SX_FMA2)
/* GOWA: x*y + z with ONE double FMA (the single product is exact in double, so
 * s is the exact value rounded once at 53 bits), truncated by clearing the 29
 * low significand bits. That chop is the SPU truncation unless the 53-bit
 * rounding moved the value ONTO a 24-bit boundary (29 low bits zero) from
 * below in magnitude. Only those lanes need the error: e = x*y + (z - s),
 * one more FMA, exact while z - s is (exponents of z and s within 28 of each
 * other; otherwise the exact path). e opposite in sign to s: one single ulp
 * toward zero. Same lane qualification as spu__sx_fma. */
SPU__SX_INLINE u128 spu__sx_fma2(u128 a, u128 b, u128 c, uint32_t negp, uint32_t negc) {
    const __m128i x = _mm_xor_si128(spu__sx_ld(a), _mm_set1_epi32((int)negp)), y = spu__sx_ld(b);
    const __m128i z = _mm_xor_si128(spu__sx_ld(c), _mm_set1_epi32((int)negc));
    const __m128d xl = spu__sx_lo(x), yl = spu__sx_lo(y), zl = spu__sx_lo(z);
    const __m128d xh = spu__sx_hi(x), yh = spu__sx_hi(y), zh = spu__sx_hi(z);
    const __m128d sl = _mm_fmadd_pd(xl, yl, zl), sh = _mm_fmadd_pd(xh, yh, zh);
    __m128i ol, oh, zml, zmh;
    __m128i rl = spu__sx_trunc0(sl, &ol, &zml, NULL);
    __m128i rh = spu__sx_trunc0(sh, &oh, &zmh, NULL);
    const __m128i ul = _mm_castpd_si128(sl), uh = _mm_castpd_si128(sh);
    const __m128i low29 = _mm_set1_epi64x(0x1FFFFFFFll);
    const __m128i bl = _mm_andnot_si128(zml, _mm_cmpeq_epi64(_mm_and_si128(ul, low29), _mm_setzero_si128()));
    const __m128i bh = _mm_andnot_si128(zmh, _mm_cmpeq_epi64(_mm_and_si128(uh, low29), _mm_setzero_si128()));
    __m128i okb = _mm_set1_epi32(-1);
    if (!_mm_testz_si128(_mm_or_si128(bl, bh), _mm_or_si128(bl, bh))) {
        /* boundary lanes: sign of the exact error x*y + z - s */
        const __m128d el = _mm_fmadd_pd(xl, yl, _mm_sub_pd(zl, sl));
        const __m128d eh = _mm_fmadd_pd(xh, yh, _mm_sub_pd(zh, sh));
        /* z - s exact: |exponent(z) - exponent(s)| <= 28 (z == 0 is exact too) */
        const __m128i ezl = _mm_srli_epi64(_mm_castpd_si128(zl), 52), esl = _mm_srli_epi64(ul, 52);
        const __m128i ezh = _mm_srli_epi64(_mm_castpd_si128(zh), 52), esh = _mm_srli_epi64(uh, 52);
        const __m128i m11 = _mm_set1_epi64x(0x7FF);
        const __m128i dl = _mm_sub_epi32(_mm_and_si128(ezl, m11), _mm_and_si128(esl, m11));
        const __m128i dh = _mm_sub_epi32(_mm_and_si128(ezh, m11), _mm_and_si128(esh, m11));
        const __m128i nearl = _mm_or_si128(_mm_cmpgt_epi32(_mm_set1_epi32(29), _mm_abs_epi32(dl)),
                                           _mm_cmpeq_epi64(_mm_slli_epi64(_mm_castpd_si128(zl), 1), _mm_setzero_si128()));
        const __m128i nearh = _mm_or_si128(_mm_cmpgt_epi32(_mm_set1_epi32(29), _mm_abs_epi32(dh)),
                                           _mm_cmpeq_epi64(_mm_slli_epi64(_mm_castpd_si128(zh), 1), _mm_setzero_si128()));
        /* a boundary lane whose z - s may round: exact path */
        okb = spu__sx_joinm(_mm_or_si128(_mm_xor_si128(bl, _mm_set1_epi32(-1)), _mm_shuffle_epi32(nearl, _MM_SHUFFLE(2, 2, 0, 0))),
                            _mm_or_si128(_mm_xor_si128(bh, _mm_set1_epi32(-1)), _mm_shuffle_epi32(nearh, _MM_SHUFFLE(2, 2, 0, 0))));
        /* away from zero (e != 0, sign(e) != sign(s)) on a boundary lane: step the single back */
        const __m128i enzl = _mm_xor_si128(_mm_castpd_si128(_mm_cmpeq_pd(el, _mm_setzero_pd())), _mm_set1_epi32(-1));
        const __m128i enzh = _mm_xor_si128(_mm_castpd_si128(_mm_cmpeq_pd(eh, _mm_setzero_pd())), _mm_set1_epi32(-1));
        const __m128i oppl = _mm_cmpgt_epi64(_mm_setzero_si128(), _mm_xor_si128(_mm_castpd_si128(el), ul));
        const __m128i opph = _mm_cmpgt_epi64(_mm_setzero_si128(), _mm_xor_si128(_mm_castpd_si128(eh), uh));
        const __m128i stl = _mm_and_si128(bl, _mm_and_si128(enzl, oppl)), sth = _mm_and_si128(bh, _mm_and_si128(enzh, opph));
        /* rl/rh carry the two singles in their low 64 bits: one 32-bit step per lane */
        rl = _mm_add_epi32(rl, _mm_shuffle_epi32(stl, _MM_SHUFFLE(3, 3, 2, 0)) );
        rh = _mm_add_epi32(rh, _mm_shuffle_epi32(sth, _MM_SHUFFLE(3, 3, 2, 0)) );
    }
    const __m128i pre = _mm_and_si128(_mm_and_si128(spu__sx_plain0(x), spu__sx_plain0(y)), spu__sx_plain0(z));
    /* a step can leave the normal range (exponent field 0): flush like the exact path */
    __m128i r = spu__sx_pos0(spu__sx_join(rl, rh), zml, zmh);
    const __m128i e0 = _mm_cmpeq_epi32(_mm_and_si128(r, _mm_set1_epi32(0x7F800000)), _mm_setzero_si128());
    r = _mm_andnot_si128(e0, r);
    const int m = _mm_movemask_ps(_mm_castsi128_ps(_mm_and_si128(_mm_and_si128(pre, okb), spu__sx_joinm(ol, oh))));
    u128 o = spu__sx_st(r);
    if (m != 0xF) o = spu__sx_fma_fix(o, m, a, b, c, negp, negc);
    return o;
}
SPU__SX_INLINE u128 spu_fma(u128 a, u128 b, u128 c)  { return spu__sx_fma2(a, b, c, 0, 0); }            /* a*b + c */
SPU__SX_INLINE u128 spu_fms(u128 a, u128 b, u128 c)  { return spu__sx_fma2(a, b, c, 0, 0x80000000u); }  /* a*b - c */
SPU__SX_INLINE u128 spu_fnms(u128 a, u128 b, u128 c) { return spu__sx_fma2(a, b, c, 0x80000000u, 0); }  /* c - a*b */
#elif defined(__AVX2__) && !defined(SPU_SX_FA_DOUBLE) && !defined(SPU_SX_FMA_SSE)
/* GOWA: spu__sx_fma with the four lanes in ONE 256-bit double vector (AVX2)
 * instead of two 128-bit halves -- the hottest loop of God of War Ascension
 * (0x72DF80, matrix x vector) does 24 of these per iteration. Same algorithm:
 * exact product in double, TwoSum for the add's error, truncation by clearing
 * the 29 low bits, exact path for lanes that do not qualify. Denormal operands
 * are zeroed up front (the SPU reads them as zero) instead of being sent to the
 * exact path; an exponent-255 operand becomes Inf/NaN in double and fails the
 * result range check, so it still takes the exact path. SPU_SX_FMA_SSE keeps
 * the two-half form. */
#include <immintrin.h>
SPU__SX_INLINE __m128i spu__sx_daz(__m128i x) {
    return _mm_andnot_si128(_mm_cmpeq_epi32(_mm_and_si128(x, _mm_set1_epi32(0x7F800000)), _mm_setzero_si128()), x);
}
SPU__SX_INLINE u128 spu__sx_fma4(u128 a, u128 b, u128 c, uint32_t negp, uint32_t negc) {
    const __m128i x = spu__sx_daz(_mm_xor_si128(spu__sx_ld(a), _mm_set1_epi32((int)negp)));
    const __m128i y = spu__sx_daz(spu__sx_ld(b));
    const __m128i z = spu__sx_daz(_mm_xor_si128(spu__sx_ld(c), _mm_set1_epi32((int)negc)));
    const __m256d xd = _mm256_cvtps_pd(_mm_castsi128_ps(x)), yd = _mm256_cvtps_pd(_mm_castsi128_ps(y));
    const __m256d zd = _mm256_cvtps_pd(_mm_castsi128_ps(z));
    const __m256d p = _mm256_mul_pd(xd, yd);                              /* exact */
    const __m256d s = _mm256_add_pd(p, zd), bb = _mm256_sub_pd(s, p);
    const __m256d err = _mm256_add_pd(_mm256_sub_pd(p, _mm256_sub_pd(s, bb)), _mm256_sub_pd(zd, bb));
    const __m256i u = _mm256_castpd_si256(s), zero = _mm256_setzero_si256();
    const __m256i az = _mm256_cmpeq_epi64(_mm256_slli_epi64(u, 1), zero);                 /* +-0 */
    const __m256i e = _mm256_and_si256(_mm256_srli_epi64(u, 52), _mm256_set1_epi64x(0x7FF));
    const __m256i e_ok = _mm256_and_si256(_mm256_cmpgt_epi64(e, _mm256_set1_epi64x(896)),
                                          _mm256_cmpgt_epi64(_mm256_set1_epi64x(1151), e));
    const __m256i on_b = _mm256_cmpeq_epi64(_mm256_and_si256(u, _mm256_set1_epi64x(0x1FFFFFFFll)), zero);
    const __m256i exact = _mm256_castpd_si256(_mm256_cmp_pd(err, _mm256_setzero_pd(), _CMP_EQ_OQ));
    const __m256i good = _mm256_and_si256(e_ok, _mm256_or_si256(_mm256_xor_si256(on_b, _mm256_set1_epi32(-1)), exact));
    const __m256i ok = _mm256_or_si256(good, az);
    const __m256i t = _mm256_and_si256(u, _mm256_set1_epi64x(~0x1FFFFFFFll));
    __m128i r = _mm_castps_si128(_mm256_cvtpd_ps(_mm256_castsi256_pd(t)));
    /* 64-bit lane masks -> 32-bit lanes (the low dword of each) */
    const __m256i sel = _mm256_setr_epi32(0, 2, 4, 6, 0, 2, 4, 6);
    const __m128i az4 = _mm256_castsi256_si128(_mm256_permutevar8x32_epi32(az, sel));
    const __m128i ok4 = _mm256_castsi256_si128(_mm256_permutevar8x32_epi32(ok, sel));
    r = _mm_andnot_si128(_mm_and_si128(az4, _mm_set1_epi32((int)0x80000000u)), r);  /* zero results are +0 */
    const int m = _mm_movemask_ps(_mm_castsi128_ps(ok4));
    u128 o = spu__sx_st(r);
    if (m != 0xF) o = spu__sx_fma_fix(o, m, a, b, c, negp, negc);
    return o;
}
SPU__SX_INLINE u128 spu_fma(u128 a, u128 b, u128 c)  { return spu__sx_fma4(a, b, c, 0, 0); }            /* a*b + c */
SPU__SX_INLINE u128 spu_fms(u128 a, u128 b, u128 c)  { return spu__sx_fma4(a, b, c, 0, 0x80000000u); }  /* a*b - c */
SPU__SX_INLINE u128 spu_fnms(u128 a, u128 b, u128 c) { return spu__sx_fma4(a, b, c, 0x80000000u, 0); }  /* c - a*b */
#else
SPU__SX_INLINE u128 spu_fma(u128 a, u128 b, u128 c)  { return spu__sx_fma(a, b, c, 0, 0); }            /* a*b + c */
SPU__SX_INLINE u128 spu_fms(u128 a, u128 b, u128 c)  { return spu__sx_fma(a, b, c, 0, 0x80000000u); }  /* a*b - c */
SPU__SX_INLINE u128 spu_fnms(u128 a, u128 b, u128 c) { return spu__sx_fma(a, b, c, 0x80000000u, 0); }  /* c - a*b */
#endif
#endif /* !SPU__ZX */
#else
static inline u128 spu_fa(u128 a, u128 b) { return spu__fa_s(a, b); }
static inline u128 spu_fs(u128 a, u128 b) { return spu__fs_s(a, b); }
static inline u128 spu_fm(u128 a, u128 b) { return spu__fm_s(a, b); }
static inline u128 spu_fma(u128 a, u128 b, u128 c)  { return spu__fma_s(a, b, c); }
static inline u128 spu_fms(u128 a, u128 b, u128 c)  { return spu__fms_s(a, b, c); }
static inline u128 spu_fnms(u128 a, u128 b, u128 c) { return spu__fnms_s(a, b, c); }
#endif
/* The exact path alone, for tests that check the fast path against it. */
SPU__SF_V2(spu_fa_ref, spu__fa_lane(x, y))
SPU__SF_V2(spu_fs_ref, spu__fa_lane(x, y ^ 0x80000000u))
SPU__SF_V2(spu_fm_ref, spu__fm_lane(x, y))
SPU__SF_V3(spu_fma_ref,  spu__fma_lane(x, y, z, 0, 0))
SPU__SF_V3(spu_fms_ref,  spu__fma_lane(x, y, z, 0, 0x80000000u))
SPU__SF_V3(spu_fnms_ref, spu__fma_lane(x, y, z, 0x80000000u, 0))
/* Compares (always extended range): any two zeros are equal and never greater. Nonzero
 * magnitudes order like their bit patterns. */
static inline int64_t spu__sf_key(uint32_t x) {
    if (!(x & 0x7F800000u)) return 0;
    const int64_t m = x & 0x7FFFFFFFu;
    return (x & 0x80000000u) ? -m : m;
}
static inline int64_t spu__sf_mag(uint32_t x) { return (x & 0x7F800000u) ? (int64_t)(x & 0x7FFFFFFFu) : 0; }
SPU__SF_V2(spu_fceq_ref,  spu__sf_key(x) == spu__sf_key(y) ? 0xFFFFFFFFu : 0u)
SPU__SF_V2(spu_fcgt_ref,  spu__sf_key(x) >  spu__sf_key(y) ? 0xFFFFFFFFu : 0u)
SPU__SF_V2(spu_fcmeq_ref, spu__sf_mag(x) == spu__sf_mag(y) ? 0xFFFFFFFFu : 0u)
SPU__SF_V2(spu_fcmgt_ref, spu__sf_mag(x) >  spu__sf_mag(y) ? 0xFFFFFFFFu : 0u)
#if (defined(__SSE2__) || defined(_M_X64)) && !defined(SPU_SCALAR_HELPERS) && !defined(SPU_NO_VEC_FCMP)
#include <emmintrin.h>
/* GOWA: the four compares four lanes at a time. The per-lane 64-bit keys
 * above compiled to a lane-by-lane mess of sete/sets/kshift (God of War
 * Ascension's per-draw light job, 0x72DF80 func_73E0, spent much of its time
 * there). The key fits in 32 bits: a magnitude is at most 0x7FFFFFFF, so its
 * negation does not overflow and a signed 32-bit compare orders the keys
 * exactly as the 64-bit one. key = 0 for exponent 0 (zero, denormal), else
 * +-(x & 0x7FFFFFFF). SPU_NO_VEC_FCMP keeps the per-lane forms. */
static inline __m128i spu__fcmp_key(__m128i x) {
    const __m128i z   = _mm_cmpeq_epi32(_mm_and_si128(x, _mm_set1_epi32(0x7F800000)), _mm_setzero_si128());
    const __m128i mag = _mm_and_si128(x, _mm_set1_epi32(0x7FFFFFFF));
    const __m128i neg = _mm_srai_epi32(x, 31);
    return _mm_andnot_si128(z, _mm_sub_epi32(_mm_xor_si128(mag, neg), neg));
}
static inline __m128i spu__fcmp_mag(__m128i x) {
    const __m128i z = _mm_cmpeq_epi32(_mm_and_si128(x, _mm_set1_epi32(0x7F800000)), _mm_setzero_si128());
    return _mm_andnot_si128(z, _mm_and_si128(x, _mm_set1_epi32(0x7FFFFFFF)));
}
#define SPU__FCMP(name, keyf, op) static inline u128 name(u128 a, u128 b) { \
    __m128i x, y; memcpy(&x, &a, 16); memcpy(&y, &b, 16); \
    const __m128i r = op(keyf(x), keyf(y)); u128 o; memcpy(&o, &r, 16); return o; }
SPU__FCMP(spu_fceq,  spu__fcmp_key, _mm_cmpeq_epi32)
SPU__FCMP(spu_fcgt,  spu__fcmp_key, _mm_cmpgt_epi32)
SPU__FCMP(spu_fcmeq, spu__fcmp_mag, _mm_cmpeq_epi32)
SPU__FCMP(spu_fcmgt, spu__fcmp_mag, _mm_cmpgt_epi32)
#undef SPU__FCMP
#else
static inline u128 spu_fceq(u128 a, u128 b)  { return spu_fceq_ref(a, b); }
static inline u128 spu_fcgt(u128 a, u128 b)  { return spu_fcgt_ref(a, b); }
static inline u128 spu_fcmeq(u128 a, u128 b) { return spu_fcmeq_ref(a, b); }
static inline u128 spu_fcmgt(u128 a, u128 b) { return spu_fcmgt_ref(a, b); }
#endif
#undef SPU__SF_V2
#undef SPU__SF_V3

/* cflts / cfltu: value * 2^(173 - i8), truncated toward zero, saturating to the integer range.
 * csflt / cuflt: integer / 2^(155 - i8) as an extended-range single (truncated). The ISA leaves
 * scales outside 0..127 undefined; these compute the same formula for them. */
static inline uint32_t spu__cfl_lane(uint32_t x, int scale, int is_unsigned) {
    uint32_t m; int e;
    if (!spu__sf_split(x, &m, &e)) return 0;
    const int sh = e + scale, neg = (x >> 31) != 0;
    if (is_unsigned) {
        if (neg) return 0;
        if (sh >= 9) return 0xFFFFFFFFu;                   /* >= 2^32 */
        return sh >= 0 ? m << sh : (sh <= -24 ? 0 : m >> -sh);
    }
    if (sh >= 8) return neg ? 0x80000000u : 0x7FFFFFFFu;   /* magnitude >= 2^31 */
    const uint32_t v = sh >= 0 ? m << sh : (sh <= -24 ? 0 : m >> -sh);
    return neg ? 0u - v : v;
}
static inline u128 spu_cflts_ref(u128 a, int i8) { u128 r; for (int i = 0; i < 4; i++) r._u32[i] = spu__cfl_lane(a._u32[i], 173 - (i8 & 0xFF), 0); return r; }
static inline u128 spu_cfltu_ref(u128 a, int i8) { u128 r; for (int i = 0; i < 4; i++) r._u32[i] = spu__cfl_lane(a._u32[i], 173 - (i8 & 0xFF), 1); return r; }
#if defined(__AVX2__) && !defined(SPU_SX_NO_AVX2_CVT)
/* GOWA: cflts/cfltu as straight AVX2 integer code, lane for lane the same as
 * spu__cfl_lane (out/spudiff/teste_cvt_fi.c): variable shifts give 0 for
 * counts of 32 and more, which covers the sh <= -24 case. The scalar loop
 * was an out-of-line call (~1% of the SPU threads). */
static inline __attribute__((always_inline)) u128 spu__cfl_v(u128 a, int i8, int is_unsigned) {
    __m128i x; memcpy(&x, &a, 16);
    const __m128i be  = _mm_and_si128(_mm_srli_epi32(x, 23), _mm_set1_epi32(0xFF));
    const __m128i m   = _mm_or_si128(_mm_and_si128(x, _mm_set1_epi32(0x7FFFFF)), _mm_set1_epi32(0x800000));
    const __m128i sh  = _mm_add_epi32(be, _mm_set1_epi32(173 - (i8 & 0xFF) - 150));
    const __m128i zero = _mm_setzero_si128();
    const __m128i v   = _mm_blendv_epi8(_mm_srlv_epi32(m, _mm_sub_epi32(zero, sh)),
                                        _mm_sllv_epi32(m, sh), _mm_cmpgt_epi32(sh, _mm_set1_epi32(-1)));
    const __m128i neg = _mm_srai_epi32(x, 31);
    const __m128i isz = _mm_cmpeq_epi32(be, zero);
    __m128i r;
    if (is_unsigned) {
        r = _mm_blendv_epi8(v, _mm_set1_epi32(-1), _mm_cmpgt_epi32(sh, _mm_set1_epi32(8)));
        r = _mm_andnot_si128(_mm_or_si128(neg, isz), r);
    } else {
        const __m128i sat = _mm_xor_si128(_mm_set1_epi32(0x7FFFFFFF), neg);   /* 7FFFFFFF / 80000000 */
        r = _mm_sub_epi32(_mm_xor_si128(v, neg), neg);                         /* neg ? -v : v */
        r = _mm_blendv_epi8(r, sat, _mm_cmpgt_epi32(sh, _mm_set1_epi32(7)));
        r = _mm_andnot_si128(isz, r);
    }
    u128 o; memcpy(&o, &r, 16); return o;
}
static inline u128 spu_cflts(u128 a, int i8) { return spu__cfl_v(a, i8, 0); }
static inline u128 spu_cfltu(u128 a, int i8) { return spu__cfl_v(a, i8, 1); }
#else
static inline u128 spu_cflts(u128 a, int i8) { return spu_cflts_ref(a, i8); }
static inline u128 spu_cfltu(u128 a, int i8) { return spu_cfltu_ref(a, i8); }
#endif
static inline u128 spu_csflt_ref(u128 a, int i8) { u128 r; for (int i = 0; i < 4; i++) { const int32_t v = a._s32[i];
    r._u32[i] = v ? spu__sf_pack(v < 0 ? 0x80000000u : 0, v < 0 ? 0u - (uint32_t)v : (uint32_t)v, -(155 - (i8 & 0xFF))) : 0; } return r; }
static inline u128 spu_cuflt_ref(u128 a, int i8) { u128 r; for (int i = 0; i < 4; i++) { const uint32_t v = a._u32[i];
    r._u32[i] = v ? spu__sf_pack(0, v, -(155 - (i8 & 0xFF))) : 0; } return r; }
#if defined(SPU__XI) && !defined(SPU_NO_XM) && \
    (defined(__AVX512F__) || (defined(SPU_SX_RZ) && defined(__AVX2__) && defined(__FMA__)))
/* GOWA (23/10): the integer -> float conversions as one truncating conversion
 * (embedded round-toward-zero, 512-bit form; only the low four lanes are used)
 * and the scale 2^-(155 - i8) applied on the exponent field, with the same
 * flush (biased exponent < 1 -> +0) and saturation (> 255 -> sign | 0x7FFFFFFF)
 * as spu__sf_pack. The _ref loops were ~110-130 host instructions (61 + 52
 * sites in the hot modules). Bit-identical to the _ref forms (t_xm.c). */
static inline u128 spu__cvf_fix(__m128i fb, int i8) {
    const int sc = 155 - (i8 & 0xFF);
    const __m128i e = _mm_and_si128(_mm_srli_epi32(fb, 23), _mm_set1_epi32(0xFF));   /* 0 only for 0 */
    const __m128i ep = _mm_sub_epi32(e, _mm_set1_epi32(sc));
    __m128i r = _mm_sub_epi32(fb, _mm_set1_epi32((int)((uint32_t)sc << 23)));
    const __m128i sat = _mm_or_si128(_mm_and_si128(fb, _mm_set1_epi32((int)0x80000000u)), _mm_set1_epi32(0x7FFFFFFF));
    r = _mm_blendv_epi8(r, sat, _mm_cmpgt_epi32(ep, _mm_set1_epi32(255)));
    const __m128i zero = _mm_or_si128(_mm_cmpeq_epi32(e, _mm_setzero_si128()), _mm_cmplt_epi32(ep, _mm_set1_epi32(1)));
    return spu__xi_st(_mm_andnot_si128(zero, r));
}
#if defined(__AVX512F__)
static inline u128 spu_csflt(u128 a, int i8) {
    const __m512 f = _mm512_cvt_roundepi32_ps(_mm512_castsi128_si512(spu__xi_ld(a)), _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC);
    return spu__cvf_fix(_mm_castps_si128(_mm512_castps512_ps128(f)), i8);
}
static inline u128 spu_cuflt(u128 a, int i8) {
    const __m512 f = _mm512_cvt_roundepu32_ps(_mm512_castsi128_si512(spu__xi_ld(a)), _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC);
    return spu__cvf_fix(_mm_castps_si128(_mm512_castps512_ps128(f)), i8);
}
#else
/* GOWA (24/10): without AVX-512, in the SPU_SX_RZ mode (round toward zero in the SPU
 * threads' MXCSR) the plain conversion already truncates; the unsigned one is
 * hi16 * 65536 + lo16 in one FMA (both halves exact, one rounding), as vk__cuflt1.
 * The empty asm keeps the compiler from folding a constant input with
 * round-to-nearest. Bit-identical to the _ref forms (scratchpad t_cvf_rz.c). */
static inline u128 spu_csflt(u128 a, int i8) {
    __m128i x = spu__xi_ld(a); __asm__("" : "+x"(x));
    return spu__cvf_fix(_mm_castps_si128(_mm_cvtepi32_ps(x)), i8);
}
static inline u128 spu_cuflt(u128 a, int i8) {
    __m128i x = spu__xi_ld(a); __asm__("" : "+x"(x));
    const __m128 hi = _mm_cvtepi32_ps(_mm_srli_epi32(x, 16));
    const __m128 lo = _mm_cvtepi32_ps(_mm_and_si128(x, _mm_set1_epi32(0xFFFF)));
    return spu__cvf_fix(_mm_castps_si128(_mm_fmadd_ps(hi, _mm_set1_ps(65536.0f), lo)), i8);
}
#endif
#else
static inline u128 spu_csflt(u128 a, int i8) { return spu_csflt_ref(a, i8); }
static inline u128 spu_cuflt(u128 a, int i8) { return spu_cuflt_ref(a, i8); }
#endif

/* fi -- Floating Interpolate (SPU ISA v1.2, p. 219; frest/frsqest output
 * format p. 215; single-precision rules section 9.1).
 *
 * rb holds an estimate as produced by frest/frsqest: sign S, biased exponent E,
 * a 13-bit BaseFraction and a 10-bit StepFraction. ra supplies Y = 0.ra[13:31]
 * (its low 19 bits, binary point above them). The result is
 *
 *     (-1)^S * (1.BaseFraction - 0.000StepFraction * Y) * 2^(E - 127)
 *
 * 1.BaseFraction is (2^13 + base) * 2^-13 and 0.000StepFraction is
 * step * 2^-13, so with Y = y * 2^-19 every term is an integer multiple of
 * 2^-32: the subtraction is exact in 64-bit fixed point. The magnitude lies in
 * (7/8, 2), so it needs at most one left shift to normalise; the fraction is
 * then truncated to 23 bits (truncation is the only SPU rounding mode). An
 * exponent-0 estimate is a zero operand, and a result whose exponent would
 * drop below 1 underflows -- both give +0, the only zero the SPU produces.
 * Exponent 255 is ordinary extended range, so 1/0 stays at the top of it. */
static inline u128 spu_fi_ref(u128 a, u128 b) {
    u128 r;
    for (int i = 0; i < 4; i++) {
        const uint32_t est = b._u32[i];
        const uint32_t sign = est & 0x80000000u;
        int exp = (int)((est >> 23) & 0xFFu);
        const uint64_t base = (est >> 10) & 0x1FFFu;          /* BaseFraction, 13 bits */
        const uint64_t step = est & 0x3FFu;                   /* StepFraction, 10 bits */
        const uint64_t y    = a._u32[i] & 0x7FFFFu;           /* Y numerator, 19 bits */
        if (exp == 0) { r._u32[i] = 0; continue; }
        /* magnitude * 2^32: (2^13 + base) * 2^19 - step * y */
        uint64_t m = ((((uint64_t)1 << 13) | base) << 19) - step * y;
        if (m < ((uint64_t)1 << 32)) {                        /* below 1.0: normalise */
            m <<= 1;
            if (--exp == 0) { r._u32[i] = 0; continue; }      /* underflow -> +0 */
        }
        const uint32_t frac = (uint32_t)((m >> 9) & 0x7FFFFFu); /* drop the hidden 1, truncate */
        r._u32[i] = sign | ((uint32_t)exp << 23) | frac;
    }
    return r;
}
#if defined(__AVX2__) && !defined(SPU_SX_NO_AVX2_CVT)
/* GOWA: fi in 32-bit lanes, lane for lane the same as spu_fi_ref
 * (out/spudiff/teste_cvt_fi.c). (2^13 + base) << 19 is a multiple of 256, so
 * m >> 8 = ((2^13 + base) << 11) - ceil(step*y / 256) fits in 25 bits;
 * m < 2^32 <=> m >> 8 < 2^24, and m >> 9 = (m >> 8) >> 1. */
static inline u128 spu_fi(u128 a, u128 b) {
    __m128i ea, eb; memcpy(&ea, &a, 16); memcpy(&eb, &b, 16);
    const __m128i exp  = _mm_and_si128(_mm_srli_epi32(eb, 23), _mm_set1_epi32(0xFF));
    const __m128i base = _mm_or_si128(_mm_and_si128(_mm_srli_epi32(eb, 10), _mm_set1_epi32(0x1FFF)), _mm_set1_epi32(0x2000));
    const __m128i prod = _mm_mullo_epi32(_mm_and_si128(eb, _mm_set1_epi32(0x3FF)), _mm_and_si128(ea, _mm_set1_epi32(0x7FFFF)));
    const __m128i m8   = _mm_sub_epi32(_mm_slli_epi32(base, 11), _mm_srli_epi32(_mm_add_epi32(prod, _mm_set1_epi32(255)), 8));
    const __m128i lo   = _mm_cmplt_epi32(m8, _mm_set1_epi32(1 << 24));                 /* m < 2^32: normalise */
    const __m128i frac = _mm_and_si128(_mm_blendv_epi8(_mm_srli_epi32(m8, 1), m8, lo), _mm_set1_epi32(0x7FFFFF));
    const __m128i e2   = _mm_add_epi32(exp, lo);                        /* exp - 1 where normalised */
    __m128i r = _mm_or_si128(_mm_and_si128(eb, _mm_set1_epi32((int)0x80000000u)),
                             _mm_or_si128(_mm_slli_epi32(e2, 23), frac));
    r = _mm_andnot_si128(_mm_or_si128(_mm_cmpeq_epi32(exp, _mm_setzero_si128()),
                                      _mm_cmpeq_epi32(e2, _mm_setzero_si128())), r);
    u128 o; memcpy(&o, &r, 16); return o;
}
#else
static inline u128 spu_fi(u128 a, u128 b) { return spu_fi_ref(a, b); }
#endif

/* ---- double precision: IEEE, round to nearest (see the header of this section) ---- */
/* GOWA (24/10): with SPU_SX_RZ the SPU host thread rounds toward zero (see above): the
 * double-precision ops and frds switch MXCSR to round-to-nearest around the host
 * operation only. The empty asm statements keep the operation between the two
 * MXCSR writes (ldmxcsr has side effects, the asm ties the operands and the result). */
#if defined(SPU_SX_RZ)
#include <xmmintrin.h>
static inline unsigned spu__rn_on(void) { const unsigned c = _mm_getcsr(); _mm_setcsr(c & ~0x6000u); return c; }
static inline void spu__rn_off(unsigned c) { _mm_setcsr(c); }
#define SPU__RN_PIN(v) __asm__ volatile("" : "+x"(v))
#else
static inline unsigned spu__rn_on(void) { return 0; }
static inline void spu__rn_off(unsigned c) { (void)c; }
#define SPU__RN_PIN(v) ((void)0)
#endif
static inline uint64_t spu__dw_get(u128 r, int i) { return ((uint64_t)r._u32[2*i] << 32) | r._u32[2*i+1]; }
static inline void spu__dw_put(u128* r, int i, uint64_t u) { r->_u32[2*i] = (uint32_t)(u >> 32); r->_u32[2*i+1] = (uint32_t)u; }
static inline int spu__d_isnan(uint64_t u) { return (u & 0x7FFFFFFFFFFFFFFFull) > 0x7FF0000000000000ull; }
/* operand: a denormal reads as a zero of the same sign */
static inline double spu__d_in(uint64_t u) {
    if (!(u & 0x7FF0000000000000ull)) u &= 0x8000000000000000ull;
    double d; memcpy(&d, &u, 8); return d;
}
static inline uint64_t spu__d_out(double d) {
    uint64_t u; memcpy(&u, &d, 8);
    return spu__d_isnan(u) ? 0x7FF8000000000000ull : u;
}
#define SPU__D_V2(name, op) \
    static inline uint64_t spu__d_rn_##name(double dx, double dy) { \
        const unsigned c = spu__rn_on(); SPU__RN_PIN(dx); SPU__RN_PIN(dy); \
        double v = dx op dy; SPU__RN_PIN(v); spu__rn_off(c); return spu__d_out(v); } \
    static inline u128 name(u128 a, u128 b) { u128 r; for (int i = 0; i < 2; i++) { \
    const uint64_t x = spu__dw_get(a, i), y = spu__dw_get(b, i); \
    spu__dw_put(&r, i, (spu__d_isnan(x) || spu__d_isnan(y)) ? 0x7FF8000000000000ull \
                       : spu__d_rn_##name(spu__d_in(x), spu__d_in(y))); } return r; }
SPU__D_V2(spu_dfa, +)
SPU__D_V2(spu_dfs, -)
SPU__D_V2(spu_dfm, *)
#undef SPU__D_V2
/* dfma = a*b + c, dfms = a*b - c, dfnms = -(a*b - c), dfnma = -(a*b + c); c is rt. Fused. */
static inline u128 spu__dfmx(u128 a, u128 b, u128 c, int negate, int subtract) { u128 r;
    for (int i = 0; i < 2; i++) {
        const uint64_t x = spu__dw_get(a, i), y = spu__dw_get(b, i), z = spu__dw_get(c, i);
        if (spu__d_isnan(x) || spu__d_isnan(y) || spu__d_isnan(z)) { spu__dw_put(&r, i, 0x7FF8000000000000ull); continue; }
        const double zz = subtract ? -spu__d_in(z) : spu__d_in(z);
        double dx = spu__d_in(x), dy = spu__d_in(y), dz = zz;
        const unsigned c = spu__rn_on(); SPU__RN_PIN(dx); SPU__RN_PIN(dy); SPU__RN_PIN(dz);
        double v = fma(dx, dy, dz); SPU__RN_PIN(v); spu__rn_off(c);
        /* Negate the RESULT by its sign bit: written as -fma(..) the compiler may fold it into a
         * negated fused multiply-add, (-x*y) - z, which differs from -(x*y + z) in the sign of an
         * exact zero. */
        const uint64_t u = spu__d_out(v);
        spu__dw_put(&r, i, (negate && !spu__d_isnan(u)) ? u ^ 0x8000000000000000ull : u);
    }
    return r; }
static inline u128 spu_dfma(u128 a, u128 b, u128 t)  { return spu__dfmx(a, b, t, 0, 0); }
static inline u128 spu_dfms(u128 a, u128 b, u128 t)  { return spu__dfmx(a, b, t, 0, 1); }
static inline u128 spu_dfnms(u128 a, u128 b, u128 t) { return spu__dfmx(a, b, t, 1, 1); }
static inline u128 spu_dfnma(u128 a, u128 b, u128 t) { return spu__dfmx(a, b, t, 1, 0); }
/* fesd: the single in word 2i (IEEE format, section 9.2.1) extended to doubleword i: exact,
 * Inf stays Inf, a NaN becomes the default QNaN, a denormal becomes +0.
 * frds: doubleword i rounded (to nearest) to an IEEE single in word 2i, word 2i+1 = 0; a NaN
 * becomes the single default QNaN 0x7FC00000. */
static inline u128 spu_fesd(u128 a) { u128 r;
    for (int i = 0; i < 2; i++) {
        const uint32_t w = a._u32[2*i], be = (w >> 23) & 0xFFu, f = w & 0x7FFFFFu;
        const uint64_t sign = (uint64_t)(w >> 31) << 63;
        uint64_t u;
        if (be == 0)        u = f ? 0 : sign;
        else if (be == 255) u = f ? 0x7FF8000000000000ull : (sign | 0x7FF0000000000000ull);
        else                u = sign | ((uint64_t)(be - 127 + 1023) << 52) | ((uint64_t)f << 29);
        spu__dw_put(&r, i, u);
    }
    return r; }
static inline u128 spu_frds(u128 a) { u128 r;
    for (int i = 0; i < 2; i++) {
        const uint64_t u = spu__dw_get(a, i);
        if (spu__d_isnan(u)) { r._u32[2*i] = 0x7FC00000u; r._u32[2*i+1] = 0; continue; }
        double dd = spu__d_in(u);
        const unsigned c = spu__rn_on(); SPU__RN_PIN(dd);
        float f = (float)dd; SPU__RN_PIN(f); spu__rn_off(c);
        uint32_t w; memcpy(&w, &f, 4);
        r._u32[2*i] = w; r._u32[2*i+1] = 0;
    }
    return r; }


/* ---- SPU double-precision (2 doubles/reg; dword i = words 2i(high),2i+1(low)).
 * Reassemble via _u32 -- our u128 is host-LE, so a naive _f64[i] would SWAP the
 * two words. Semantics from RPCS3 SPUInterpreter (DFASM / DFMA / FESD / FRDS). ---- */
static inline double spu__dget(u128 r, int i) {
    uint64_t u = ((uint64_t)r._u32[i*2] << 32) | r._u32[i*2+1];
    double d; memcpy(&d, &u, sizeof d); return d;
}
static inline void spu__dset(u128* r, int i, double d) {
    uint64_t u; memcpy(&u, &d, sizeof u);
    r->_u32[i*2] = (uint32_t)(u >> 32); r->_u32[i*2+1] = (uint32_t)u;
}
/* double FMA family: c = rt (accumulator, 3-register). The SPU DFMA family is FUSED (single rounding). */
/* double compares -> per-lane 64-bit mask (RPCS3 stubs these; sane impl here). */
static inline u128 spu_dfceq(u128 a, u128 b)  { u128 r; for(int i=0;i<2;i++){ uint64_t m=(spu__dget(a,i)==spu__dget(b,i))?~0ull:0ull; r._u32[i*2]=(uint32_t)(m>>32); r._u32[i*2+1]=(uint32_t)m; } return r; }
static inline u128 spu_dfcmeq(u128 a, u128 b) { u128 r; for(int i=0;i<2;i++){ double x=spu__dget(a,i),y=spu__dget(b,i); if(x<0)x=-x; if(y<0)y=-y; uint64_t m=(x==y)?~0ull:0ull; r._u32[i*2]=(uint32_t)(m>>32); r._u32[i*2+1]=(uint32_t)m; } return r; }
/* Singles occupy preferred words 0 and 2 when converting doubleword lanes. */
/* mpyhhu: high-16 x high-16 of each word, unsigned, full 32-bit product. */
static inline u128 spu_mpyhhu_ref(u128 a, u128 b){ u128 r; for(int i=0;i<4;i++) r._u32[i]=(uint32_t)a._u16[2*i+1]*(uint32_t)b._u16[2*i+1]; return r; }
/* cgx: extended carry-generate, carry-in = low bit of old rt (RPCS3 CGX). */
static inline u128 spu_cgx_ref(u128 a, u128 b, u128 t){ u128 r; for(int i=0;i<4;i++) r._u32[i]=(uint32_t)(((uint64_t)(t._u32[i]&1u)+a._u32[i]+b._u32[i])>>32); return r; }

/* ---- remaining SPU ISA ops (RPCS3 SPUInterpreter: EQV/ABSDB/AVGB/MPYHHA/
 * MPYHHAU/DFCGT/DFCMGT/XORBI/DFTSV). Completes the lifter's opcode coverage. ---- */
static inline u128 spu_eqv_ref(u128 a, u128 b)   { u128 r; for(int i=0;i<4;i++) r._u32[i]=~(a._u32[i]^b._u32[i]); return r; }
#if defined(SPU__XV)
static inline u128 spu_eqv(u128 a, u128 b) { const __m128i A = spu__xv_ld(a); const __m128i B = spu__xv_ld(b); return spu__xv_st(spu__xv_eqv(A, B)); }
#else
static inline u128 spu_eqv(u128 a, u128 b) { return spu_eqv_ref(a, b); }
#endif
static inline u128 spu_xorbi_ref(u128 a, int32_t imm){ u128 r; for(int i=0;i<16;i++) r._u8[i]=(uint8_t)(a._u8[i]^(uint8_t)imm); return r; }
#if defined(SPU__XV)
static inline u128 spu_xorbi(u128 a, int32_t imm) { const __m128i A = spu__xv_ld(a); return spu__xv_st(_mm_xor_si128(A, _mm_set1_epi8((char)imm))); }
#else
static inline u128 spu_xorbi(u128 a, int32_t imm) { return spu_xorbi_ref(a, imm); }
#endif
static inline u128 spu_absdb_ref(u128 a, u128 b)  { u128 r; for(int i=0;i<16;i++){ uint8_t x=a._u8[i],y=b._u8[i]; r._u8[i]=(uint8_t)(x>y?x-y:y-x); } return r; }
static inline u128 spu_avgb_ref(u128 a, u128 b)   { u128 r; for(int i=0;i<16;i++) r._u8[i]=(uint8_t)(((uint32_t)a._u8[i]+(uint32_t)b._u8[i]+1u)>>1); return r; }
/* mpyhha/mpyhhau: high-16 x high-16, ACCUMULATE into rt (3-register). */
static inline u128 spu_mpyhha_ref(u128 a, u128 b, u128 t)  { u128 r; for(int i=0;i<4;i++) r._s32[i]=t._s32[i]+(int32_t)a._s16[2*i+1]*(int32_t)b._s16[2*i+1]; return r; }
static inline u128 spu_mpyhhau_ref(u128 a, u128 b, u128 t) { u128 r; for(int i=0;i<4;i++) r._u32[i]=t._u32[i]+(uint32_t)a._u16[2*i+1]*(uint32_t)b._u16[2*i+1]; return r; }
/* double compare greater (RPCS3 stubs these; sane impl) -> per-lane mask. */
static inline u128 spu_dfcgt(u128 a, u128 b)  { u128 r; for(int i=0;i<2;i++){ uint64_t m=(spu__dget(a,i)>spu__dget(b,i))?~0ull:0ull; r._u32[i*2]=(uint32_t)(m>>32); r._u32[i*2+1]=(uint32_t)m; } return r; }
static inline u128 spu_dfcmgt(u128 a, u128 b) { u128 r; for(int i=0;i<2;i++){ double x=spu__dget(a,i),y=spu__dget(b,i); if(x<0)x=-x; if(y<0)y=-y; uint64_t m=(x>y)?~0ull:0ull; r._u32[i*2]=(uint32_t)(m>>32); r._u32[i*2+1]=(uint32_t)m; } return r; }
/* dftsv rt,ra,I7: double test special value (Cell BE SPU ISA). Each
 * doubleword becomes all ones if it is in any class I7 selects:
 * 0x40 NaN, 0x20 +inf, 0x10 -inf, 0x08 +0, 0x04 -0, 0x02 +denorm, 0x01 -denorm.
 * (RPCS3 stubs it as fatal; this is the ISA definition.) */
static inline uint64_t spu__dftsv1(uint64_t v, uint32_t i7) {
    uint64_t e = (v >> 52) & 0x7FF, f = v & 0xFFFFFFFFFFFFFull; int neg = (int)(v >> 63);
    int t = 0;
    if (e == 0x7FF && f) t |= i7 & 0x40;
    if (e == 0x7FF && !f) t |= i7 & (neg ? 0x10 : 0x20);
    if (e == 0 && !f) t |= i7 & (neg ? 0x04 : 0x08);
    if (e == 0 && f) t |= i7 & (neg ? 0x01 : 0x02);
    return t ? ~0ull : 0ull;
}
static inline u128 spu_dftsv(u128 a, int32_t imm){ u128 r;
    for (int i = 0; i < 2; i++) { uint64_t v = (uint64_t)a._u32[2*i] << 32 | a._u32[2*i+1];
        uint64_t m = spu__dftsv1(v, (uint32_t)imm & 0x7F); r._u32[2*i] = (uint32_t)(m >> 32); r._u32[2*i+1] = (uint32_t)m; }
    return r; }

/* ---- Phase 2: register-variable shifts/rotates ---- */
static inline u128 spu_shl_ref(u128 a, u128 b)   { u128 r; for(int i=0;i<4;i++){ uint32_t sh=b._u32[i]&0x3F; r._u32[i]=(sh>31)?0:(a._u32[i]<<sh); } return r; }
static inline u128 spu_shlh_ref(u128 a, u128 b)  { u128 r; for(int i=0;i<8;i++){ uint32_t sh=b._u16[i]&0x1F; r._u16[i]=(sh>15)?0:(uint16_t)(a._u16[i]<<sh); } return r; }
static inline u128 spu_rot_ref(u128 a, u128 b)   { u128 r; for(int i=0;i<4;i++){ uint32_t sh=b._u32[i]&31; r._u32[i]= sh ? ((a._u32[i]<<sh)|(a._u32[i]>>(32-sh))) : a._u32[i]; } return r; }
static inline u128 spu_roth_ref(u128 a, u128 b)  { u128 r; for(int i=0;i<8;i++){ uint32_t sh=b._u16[i]&15; r._u16[i]= sh ? (uint16_t)((a._u16[i]<<sh)|(a._u16[i]>>(16-sh))) : a._u16[i]; } return r; }
static inline u128 spu_shlqbi(u128 a, u128 b){ int sh=b._u32[0]&7; if(!sh) return a;
    uint64_t hi=((uint64_t)a._u32[0]<<32)|a._u32[1], lo=((uint64_t)a._u32[2]<<32)|a._u32[3];
    uint64_t nhi=(hi<<sh)|(lo>>(64-sh)), nlo=(lo<<sh);
    u128 r; r._u32[0]=(uint32_t)(nhi>>32); r._u32[1]=(uint32_t)nhi; r._u32[2]=(uint32_t)(nlo>>32); r._u32[3]=(uint32_t)nlo; return r; }
static inline u128 spu_rotqbi(u128 a, u128 b){ int sh=b._u32[0]&7; if(!sh) return a;
    uint64_t hi=((uint64_t)a._u32[0]<<32)|a._u32[1], lo=((uint64_t)a._u32[2]<<32)|a._u32[3];
    uint64_t nhi=(hi<<sh)|(lo>>(64-sh)), nlo=(lo<<sh)|(hi>>(64-sh));
    u128 r; r._u32[0]=(uint32_t)(nhi>>32); r._u32[1]=(uint32_t)nhi; r._u32[2]=(uint32_t)(nlo>>32); r._u32[3]=(uint32_t)nlo; return r; }
static inline u128 spu_shlqby_ref(u128 a, u128 b){ int sh=b._u32[0]&0x1F; u128 r=spu_zero(); if(sh>=16) return r; for(int i=0;i<16;i++){ int s=i+sh; if(s<16) r._u8[SPU_W(i)]=a._u8[SPU_W(s)]; } return r; }
#if defined(__ARM_NEON) && !defined(SPU_SCALAR_HELPERS) && !defined(SPU_SCALAR_INT)
static inline u128 spu_shlqby(u128 a, u128 b) {
    return spu__perm(a, vaddq_u8(spu__iota(), vdupq_n_u8((uint8_t)(b._u32[0] & 0x1F))));
}
#else
#if defined(SPU__XI) && !defined(SPU_SCALAR_HELPERS) && !defined(SPU_SCALAR_INT)
static inline u128 spu_shlqby(u128 a, u128 b) { return spu__xi_shift_bytes(a, (int)(b._u32[0] & 0x1F)); }
#else
static inline u128 spu_shlqby(u128 a, u128 b) { return spu_shlqby_ref(a, b); }
#endif
#endif
static inline u128 spu_rotqby_ref(u128 a, u128 b){ int sh=b._u32[0]&0x0F; u128 r; for(int i=0;i<16;i++) r._u8[SPU_W(i)]=a._u8[SPU_W((i+sh)&0x0F)]; return r; }
#if defined(__ARM_NEON) && !defined(SPU_SCALAR_HELPERS) && !defined(SPU_SCALAR_INT)
static inline u128 spu_rotqby(u128 a, u128 b) {
    return spu__perm(a, vandq_u8(vaddq_u8(spu__iota(), vdupq_n_u8((uint8_t)b._u32[0])), vdupq_n_u8(15)));
}
#else
#if defined(SPU__XI) && !defined(SPU_SCALAR_HELPERS) && !defined(SPU_SCALAR_INT)
static inline u128 spu_rotqby(u128 a, u128 b) { return spu__xi_rot_bytes(a, (int)(b._u32[0] & 0x0F)); }
#else
static inline u128 spu_rotqby(u128 a, u128 b) { return spu_rotqby_ref(a, b); }
#endif
#endif
static inline u128 spu_shlqbybi_ref(u128 a, u128 b){ int sh=(b._u32[0]>>3)&0x1F; u128 r=spu_zero(); if(sh>=16) return r; for(int i=0;i<16;i++){ int s=i+sh; if(s<16) r._u8[SPU_W(i)]=a._u8[SPU_W(s)]; } return r; }
#if defined(__ARM_NEON) && !defined(SPU_SCALAR_HELPERS) && !defined(SPU_SCALAR_INT)
static inline u128 spu_shlqbybi(u128 a, u128 b) {
    return spu__perm(a, vaddq_u8(spu__iota(), vdupq_n_u8((uint8_t)((b._u32[0] >> 3) & 0x1F))));
}
#else
#if defined(SPU__XI) && !defined(SPU_SCALAR_HELPERS) && !defined(SPU_SCALAR_INT)
static inline u128 spu_shlqbybi(u128 a, u128 b) { return spu__xi_shift_bytes(a, (int)((b._u32[0] >> 3) & 0x1F)); }
#else
static inline u128 spu_shlqbybi(u128 a, u128 b) { return spu_shlqbybi_ref(a, b); }
#endif
#endif
static inline u128 spu_rotqbybi_ref(u128 a, u128 b){ int sh=(b._u32[0]>>3)&0x0F; u128 r; for(int i=0;i<16;i++) r._u8[SPU_W(i)]=a._u8[SPU_W((i+sh)&0x0F)]; return r; }
#if defined(__ARM_NEON) && !defined(SPU_SCALAR_HELPERS) && !defined(SPU_SCALAR_INT)
static inline u128 spu_rotqbybi(u128 a, u128 b) {
    return spu__perm(a, vandq_u8(vaddq_u8(spu__iota(), vdupq_n_u8((uint8_t)(b._u32[0] >> 3))), vdupq_n_u8(15)));
}
#else
#if defined(SPU__XI) && !defined(SPU_SCALAR_HELPERS) && !defined(SPU_SCALAR_INT)
static inline u128 spu_rotqbybi(u128 a, u128 b) { return spu__xi_rot_bytes(a, (int)((b._u32[0] >> 3) & 0x0F)); }
#else
static inline u128 spu_rotqbybi(u128 a, u128 b) { return spu_rotqbybi_ref(a, b); }
#endif
#endif

/* ---- Phase 2: rotmahi ---- */
static inline u128 spu_rotmahi_ref(u128 a, int i7) { u128 r; int sh=(0-i7)&0x1F; for(int i=0;i<8;i++) r._s16[i]=(sh>15)?(a._s16[i]>>15):(a._s16[i]>>sh); return r; }

/* ---- Phase 2: byte/half immediate compares ---- */
static inline u128 spu_ceqbi_ref(u128 a, int32_t imm)  { u128 r; uint8_t v=(uint8_t)imm; for(int i=0;i<16;i++) r._u8[i]=(a._u8[i]==v)?0xFFu:0; return r; }
static inline u128 spu_ceqhi_ref(u128 a, int32_t imm)  { u128 r; int16_t v=(int16_t)imm; for(int i=0;i<8;i++) r._u16[i]=(a._s16[i]==v)?0xFFFFu:0; return r; }
static inline u128 spu_clgtbi_ref(u128 a, int32_t imm) { u128 r; uint8_t v=(uint8_t)imm; for(int i=0;i<16;i++) r._u8[i]=(a._u8[i]>v)?0xFFu:0; return r; }
static inline u128 spu_clgthi_ref(u128 a, int32_t imm) { u128 r; uint16_t v=(uint16_t)imm; for(int i=0;i<8;i++) r._u16[i]=(a._u16[i]>v)?0xFFFFu:0; return r; }
static inline u128 spu_cgthi_ref(u128 a, int32_t imm)  { u128 r; int16_t v=(int16_t)imm; for(int i=0;i<8;i++) r._u16[i]=(a._s16[i]>v)?0xFFFFu:0; return r; }
static inline u128 spu_cgtbi_ref(u128 a, int32_t imm)  { u128 r; int8_t v=(int8_t)imm; for(int i=0;i<16;i++) r._u8[i]=(a._s8[i]>v)?0xFFu:0; return r; }

/* ---- Phase 2: misc one-offs ---- */
static inline u128 spu_fscrrd(u128 a) { (void)a; return spu_zero(); }
static inline u128 spu_gb_ref(u128 a) {
    uint32_t v = ((a._u32[0]&1)<<3)|((a._u32[1]&1)<<2)|((a._u32[2]&1)<<1)|(a._u32[3]&1);
    u128 r = spu_zero(); r._u32[0]=v; return r;
}
static inline u128 spu_gbh_ref(u128 a) {
    /* gather LSB of each SPU halfword H into bit (7-H). SPU halfword H maps to
     * our _u16[H^1] (the within-word halfword swap of the value layout). */
    uint32_t v=0; for(int i=0;i<8;i++) v |= ((uint32_t)(a._u16[i^1]&1) << (7-i));
    u128 r = spu_zero(); r._u32[0]=v; return r;
}
/* gather LSB of each SPU byte i into bit (15-i); exact inverse of spu_fsmb.
 * SPU byte i lives at our _u8[SPU_W(i)] (byte-reversed within each word). */
static inline u128 spu_gbb_ref(u128 a) {
    uint32_t v=0; for(int i=0;i<16;i++) v |= ((uint32_t)(a._u8[SPU_W(i)]&1) << (15-i));
    u128 r = spu_zero(); r._u32[0]=v; return r;
}
static inline u128 spu_cg_ref(u128 a, u128 b)   { u128 r; for(int i=0;i<4;i++) r._u32[i]=(uint32_t)(((uint64_t)a._u32[i]+(uint64_t)b._u32[i])>>32); return r; }
/* sumb: sum the 4 bytes of each word. Per CBEA, RT halfword 2i (the HIGH half of
 * word i) = sum of RB's 4 bytes of word i; halfword 2i+1 (LOW half) = sum of RA's
 * 4 bytes of word i. Computed on word VALUES so byte order is irrelevant. */
static inline u128 spu_sumb_ref(u128 a, u128 b) {
    u128 r;
    for(int i=0;i<4;i++) {
        uint32_t wa=a._u32[i], wb=b._u32[i];
        uint32_t sa=((wa>>24)&0xFF)+((wa>>16)&0xFF)+((wa>>8)&0xFF)+(wa&0xFF);
        uint32_t sb=((wb>>24)&0xFF)+((wb>>16)&0xFF)+((wb>>8)&0xFF)+(wb&0xFF);
        r._u32[i]=(sb<<16)|sa;
    }
    return r;
}
/* Borrow generate: carry-out of (b + ~a + 1) == (b >= a unsigned ? 1 : 0).
 * The subtract-side sibling of cg; pairs with sf/sfx for extended subtraction. */
static inline u128 spu_bg_ref(u128 a, u128 b)   { u128 r; for(int i=0;i<4;i++) r._u32[i]=(uint32_t)(((uint64_t)b._u32[i]+(uint64_t)(~a._u32[i])+1u)>>32); return r; }
static inline u128 spu_addx_ref(u128 a, u128 b, u128 t) { u128 r; for(int i=0;i<4;i++) r._u32[i]=a._u32[i]+b._u32[i]+(t._u32[i]&1); return r; }
/* LE host: high half of word i = _s16[2i+1], low half = _s16[2i]. */
static inline u128 spu_mpyh_ref(u128 a, u128 b) { u128 r; for(int i=0;i<4;i++) r._s32[i]=((int32_t)a._s16[2*i+1] * (int32_t)b._s16[2*i]) << 16; return r; }
static inline u128 spu_mpyhh_ref(u128 a, u128 b){ u128 r; for(int i=0;i<4;i++) r._s32[i]=(int32_t)a._s16[2*i+1] * (int32_t)b._s16[2*i+1]; return r; }
static inline u128 spu_mpys_ref(u128 a, u128 b) { u128 r; for(int i=0;i<4;i++){ int32_t p=(int32_t)a._s16[2*i]*(int32_t)b._s16[2*i]; r._s32[i]=(int16_t)(p>>16); } return r; }
static inline u128 spu_mpyui_ref(u128 a, int32_t imm) { u128 r; for(int i=0;i<4;i++) r._u32[i]=(uint32_t)a._u16[2*i]*(uint32_t)(uint16_t)imm; return r; }
static const uint32_t spu_frest_fraction_lut[32] = {
    0x7FFBE0, 0x7F87A6, 0x70EF72, 0x708B40, 0x638B12, 0x633AEA, 0x5792C4, 0x574AA0,
    0x4CCA7E, 0x4C9262, 0x430A44, 0x42D62A, 0x3A2E12, 0x39FDFA, 0x3215E4, 0x31F1D2,
    0x2AA9BE, 0x2A85AC, 0x23D59A, 0x23BD8E, 0x1D8576, 0x1D8576, 0x17AD5A, 0x17AD5A,
    0x124543, 0x124543, 0x0D392D, 0x0D392D, 0x08851A, 0x08851A, 0x041D07, 0x041D07
};

static const uint32_t spu_frsqest_fraction_lut[64] = {
    0x350160, 0x34E954, 0x2F993D, 0x2F993D, 0x2AA523, 0x2AA523, 0x26190D, 0x26190D,
    0x21E4F9, 0x21E4F9, 0x1E00E9, 0x1E00E9, 0x1A5CD9, 0x1A5CD9, 0x16F8CB, 0x16F8CB,
    0x13CCC0, 0x13CCC0, 0x10CCB3, 0x10CCB3, 0x0E00AA, 0x0E00AA, 0x0B58A1, 0x0B58A1,
    0x08D498, 0x08D498, 0x067491, 0x067491, 0x043089, 0x043089, 0x020C83, 0x020C83,
    0x7FFDF4, 0x7FD1DE, 0x7859C8, 0x783DBA, 0x71559C, 0x71559C, 0x6AE57C, 0x6AE57C,
    0x64F561, 0x64F561, 0x5F7149, 0x5F7149, 0x5A4D33, 0x5A4D33, 0x55811F, 0x55811F,
    0x51050F, 0x51050F, 0x4CC8FE, 0x4CC8FE, 0x48D0F0, 0x48D0F0, 0x4510E4, 0x4510E4,
    0x4180D7, 0x4180D7, 0x3E24CC, 0x3E24CC, 0x3AF4C3, 0x3AF4C3, 0x37E8BA, 0x37E8BA
};

static inline u128 spu_frsqest_ref(u128 a) {
    u128 r;
    for (int i = 0; i < 4; i++) {
        uint32_t bits = a._u32[i];
        uint32_t exp  = (bits >> 23) & 0xFFu;
        uint32_t frac_idx = (bits >> 18) & 0x3Fu;
        uint32_t rexp = (exp == 0u) ? 0xFFu : (190u - (exp + 1u) / 2u);
        uint32_t fraction = spu_frsqest_fraction_lut[frac_idx];
        uint32_t out = (rexp << 23) | (fraction & 0x7FFFFFu);
        memcpy(&r._f32[i], &out, sizeof out);
    }
    return r;
}
/* frest: reciprocal estimate (refined by the following spu_fi Newton step).
 * Full-precision 1/x is exact after fi and >= HW-estimate accuracy. */
static inline u128 spu_frest_ref(u128 a) {
    u128 r;
    for (int i = 0; i < 4; i++) {
        uint32_t bits = a._u32[i];
        uint32_t sign = bits & 0x80000000u;
        uint32_t exp  = (bits >> 23) & 0xFFu;
        uint32_t frac_idx = (bits >> 18) & 0x1Fu;
        /* result biased exponent = 253 - exp (0 for exp >= 253); exp 0 (zero/denormal) gives 255. Fraction from the
         * hardware table. Matches RPCS3's measured spu_frest_{fraction,exponent}_lut for every input. */
        uint32_t rexp = (exp == 0u) ? 255u : (exp >= 253u) ? 0u : (253u - exp);
        uint32_t out = sign | (rexp << 23) | (spu_frest_fraction_lut[frac_idx] & 0x7FFFFFu);
        memcpy(&r._f32[i], &out, sizeof out);
    }
    return r;
}

#if defined(__AVX512F__) && defined(__AVX512VL__) && !defined(SPU_SCALAR_HELPERS) && !defined(SPU_NO_VEC_FREST)
/* GOWA: frsqest / frest four lanes at a time: the hardware tables live in
 * zmm registers and the fraction index picks the entry with vpermi2d (32
 * entries per pair of registers; frsqest's 64 use two lookups and bit 5 of
 * the index). Same bits as the _ref forms above (out/spudiff/teste_vec_fcmp_frest.c).
 * SPU_NO_VEC_FREST keeps the per-lane forms. */
#include <immintrin.h>
static inline u128 spu_frsqest(u128 a) {
    __m128i x; memcpy(&x, &a, 16);
    const __m512i t0 = _mm512_loadu_si512((const void*)&spu_frsqest_fraction_lut[0]);
    const __m512i t1 = _mm512_loadu_si512((const void*)&spu_frsqest_fraction_lut[16]);
    const __m512i t2 = _mm512_loadu_si512((const void*)&spu_frsqest_fraction_lut[32]);
    const __m512i t3 = _mm512_loadu_si512((const void*)&spu_frsqest_fraction_lut[48]);
    const __m128i idx = _mm_and_si128(_mm_srli_epi32(x, 18), _mm_set1_epi32(0x3F));
    const __m512i zi = _mm512_zextsi128_si512(idx);
    const __m128i lo = _mm512_castsi512_si128(_mm512_permutex2var_epi32(t0, zi, t1));   /* index bits 0-4 */
    const __m128i hi = _mm512_castsi512_si128(_mm512_permutex2var_epi32(t2, zi, t3));
    const __m128i frac = _mm_and_si128(_mm_mask_mov_epi32(lo, _mm_test_epi32_mask(idx, _mm_set1_epi32(0x20)), hi),
                                       _mm_set1_epi32(0x7FFFFF));
    const __m128i exp = _mm_and_si128(_mm_srli_epi32(x, 23), _mm_set1_epi32(0xFF));
    __m128i rexp = _mm_sub_epi32(_mm_set1_epi32(190), _mm_srli_epi32(_mm_add_epi32(exp, _mm_set1_epi32(1)), 1));
    rexp = _mm_mask_mov_epi32(rexp, _mm_cmpeq_epi32_mask(exp, _mm_setzero_si128()), _mm_set1_epi32(0xFF));
    const __m128i r = _mm_or_si128(_mm_slli_epi32(rexp, 23), frac);
    u128 o; memcpy(&o, &r, 16); return o;
}
static inline u128 spu_frest(u128 a) {
    __m128i x; memcpy(&x, &a, 16);
    const __m512i t0 = _mm512_loadu_si512((const void*)&spu_frest_fraction_lut[0]);
    const __m512i t1 = _mm512_loadu_si512((const void*)&spu_frest_fraction_lut[16]);
    const __m128i idx = _mm_and_si128(_mm_srli_epi32(x, 18), _mm_set1_epi32(0x1F));
    const __m128i frac = _mm_and_si128(_mm512_castsi512_si128(_mm512_permutex2var_epi32(t0, _mm512_zextsi128_si512(idx), t1)),
                                       _mm_set1_epi32(0x7FFFFF));
    const __m128i exp = _mm_and_si128(_mm_srli_epi32(x, 23), _mm_set1_epi32(0xFF));
    __m128i rexp = _mm_sub_epi32(_mm_set1_epi32(253), exp);
    rexp = _mm_mask_mov_epi32(rexp, _mm_cmpge_epu32_mask(exp, _mm_set1_epi32(253)), _mm_setzero_si128());
    rexp = _mm_mask_mov_epi32(rexp, _mm_cmpeq_epi32_mask(exp, _mm_setzero_si128()), _mm_set1_epi32(255));
    const __m128i r = _mm_or_si128(_mm_and_si128(x, _mm_set1_epi32((int)0x80000000u)),
                                   _mm_or_si128(_mm_slli_epi32(rexp, 23), frac));
    u128 o; memcpy(&o, &r, 16); return o;
}
#elif defined(__AVX2__) && !defined(SPU_SCALAR_HELPERS) && !defined(SPU_NO_VEC_FREST)
/* GOWA (24/10): without AVX-512 the table entries come by vpgatherdd (the same as
 * vk__frsqest1 / vk__frest1 in spu_vk_avx2.h); same bits as the _ref forms
 * (scratchpad t_frest.c). */
#include <immintrin.h>
static inline u128 spu_frsqest(u128 a) {
    __m128i x; memcpy(&x, &a, 16);
    const __m128i idx = _mm_and_si128(_mm_srli_epi32(x, 18), _mm_set1_epi32(0x3F));
    const __m128i frac = _mm_and_si128(_mm_i32gather_epi32((const int*)spu_frsqest_fraction_lut, idx, 4), _mm_set1_epi32(0x7FFFFF));
    const __m128i exp = _mm_and_si128(_mm_srli_epi32(x, 23), _mm_set1_epi32(0xFF));
    __m128i rexp = _mm_sub_epi32(_mm_set1_epi32(190), _mm_srli_epi32(_mm_add_epi32(exp, _mm_set1_epi32(1)), 1));
    rexp = _mm_blendv_epi8(rexp, _mm_set1_epi32(0xFF), _mm_cmpeq_epi32(exp, _mm_setzero_si128()));
    const __m128i r = _mm_or_si128(_mm_slli_epi32(rexp, 23), frac);
    u128 o; memcpy(&o, &r, 16); return o;
}
static inline u128 spu_frest(u128 a) {
    __m128i x; memcpy(&x, &a, 16);
    const __m128i idx = _mm_and_si128(_mm_srli_epi32(x, 18), _mm_set1_epi32(0x1F));
    const __m128i frac = _mm_and_si128(_mm_i32gather_epi32((const int*)spu_frest_fraction_lut, idx, 4), _mm_set1_epi32(0x7FFFFF));
    const __m128i exp = _mm_and_si128(_mm_srli_epi32(x, 23), _mm_set1_epi32(0xFF));
    __m128i rexp = _mm_sub_epi32(_mm_set1_epi32(253), exp);
    rexp = _mm_blendv_epi8(rexp, _mm_setzero_si128(), _mm_cmpgt_epi32(exp, _mm_set1_epi32(252)));   /* exp in 0..255 */
    rexp = _mm_blendv_epi8(rexp, _mm_set1_epi32(255), _mm_cmpeq_epi32(exp, _mm_setzero_si128()));
    const __m128i r = _mm_or_si128(_mm_and_si128(x, _mm_set1_epi32((int)0x80000000u)),
                                   _mm_or_si128(_mm_slli_epi32(rexp, 23), frac));
    u128 o; memcpy(&o, &r, 16); return o;
}
#else
static inline u128 spu_frsqest(u128 a) { return spu_frsqest_ref(a); }
static inline u128 spu_frest(u128 a)   { return spu_frest_ref(a); }
#endif

/* ---- Phase 3: sign extension ----
 * LE host: low sub-lane = _u8[2i] / _s16[2i] / _s32[2i] (the byte/half/word
 * at the *lower* storage address). Same caveat as the mpy family. */
static inline u128 spu_xsbh_ref(u128 a) { u128 r; for(int i=0;i<8;i++) r._s16[i] = (int8_t)a._u8[2*i]; return r; }
static inline u128 spu_xshw_ref(u128 a) { u128 r; for(int i=0;i<4;i++) r._s32[i] = (int16_t)a._s16[2*i]; return r; }
/* xswd: rt.doubleword[d] = sign_extend_64(ra.word[2d+1]) -- the ODD word of
 * each doubleword, because that is the low half in big-endian order.
 *
 * This read the EVEN words (2d) and wrote the result through _s64, which is a
 * host little-endian lane: writing it puts the value in _u32[2d] (SPU word 2d,
 * the HIGH half) and the sign fill in _u32[2d+1]. Both halves were therefore
 * wrong, and the common case -- a small positive value -- came out as ZERO.
 *
 * You Don't Know Jack's FMOD mixer hit exactly that: a compiler-generated
 * 64-bit multiply helper received `1 * 0` instead of `1 * 16`, so the mixer's
 * block count was zero and it stopped on its own assert. Unlike its siblings
 * xsbh/xshw, whose source and destination swaps cancel, nothing cancels here.
 * Write the two words explicitly. */
static inline u128 spu_xswd_ref(u128 a) {
    u128 r;
    for (int d = 0; d < 2; d++) {
        int32_t v = (int32_t)a._u32[2*d + 1];
        r._u32[2*d]     = (uint32_t)(v >> 31);   /* sign fill (high word) */
        r._u32[2*d + 1] = (uint32_t)v;           /* value      (low word) */
    }
    return r;
}

/* ---- Phase 3: OR across ---- */
static inline u128 spu_orx_ref(u128 a) {
    u128 r = spu_zero();
    r._u32[0] = a._u32[0] | a._u32[1] | a._u32[2] | a._u32[3];
    return r;
}

/* ---- Phase 3: form-select mask from bits ---- */
static inline u128 spu_fsm_ref(u128 a) {
    u128 r; uint32_t v = a._u32[0] & 0xF;
    for(int i=0;i<4;i++) r._u32[i] = ((v>>(3-i))&1) ? 0xFFFFFFFFu : 0;
    return r;
}
/* fsmh: per-halfword mask from 8 bits. SPU halfword H <- bit (7-H); store at
 * our _u16[H^1] (within-word halfword swap of the value layout). Per-halfword
 * both bytes are identical so byte order within a halfword is irrelevant, but
 * the halfword ORDER within each word must be swapped. */
static inline u128 spu_fsmh_ref(u128 a) {
    u128 r; uint32_t v = a._u32[0] & 0xFF;
    for(int i=0;i<8;i++) r._u16[i^1] = ((v>>(7-i))&1) ? 0xFFFFu : 0;
    return r;
}
/* fsmb/fsmbi: per-byte mask from 16 bits. SPU byte position P <- bit (15-P);
 * store at our _u8[SPU_W(P)] (byte-reversed within each word). A bit set at SPU
 * byte P must land at our _u8[SPU_W(P)] so that downstream word arithmetic (e.g.
 * andbi(fsmbi(0x101),-128) building a +0x80 EA offset in the low byte of words
 * 1,3) places the value in the correct byte lane. Raw _u8[P] would put 0x80 in
 * the high byte (bit 31) instead -> wrong DMA EA. */
static inline u128 spu_fsmb_ref(u128 a) {
    u128 r; uint32_t v = a._u32[0] & 0xFFFF;
    for(int i=0;i<16;i++) r._u8[SPU_W(i)] = ((v>>(15-i))&1) ? 0xFFu : 0;
    return r;
}
static inline u128 spu_fsmbi(int32_t imm) {
    u128 r; uint32_t v = imm & 0xFFFF;
    for(int i=0;i<16;i++) r._u8[SPU_W(i)] = ((v>>(15-i))&1) ? 0xFFu : 0;
    return r;
}

/* ---- Phase 3: constant generators (insertion shuffle patterns) ---- */
/* Generate-controls (cbd/chd/cwd/cdd) — insertion selectors for the (now
 * SPU-byte-correct) shufb. Each builds a control in TRUE SPU byte order: the
 * base selects b's SPU byte t at result SPU byte t (selector 0x10+t), and the
 * insert positions select a's preferred scalar bytes -- SPU byte 3 for a byte
 * (selector 0x03), SPU bytes 2,3 for a halfword (0x02,0x03), SPU bytes 0..3 for
 * a word, 0..7 for a doubleword. Every store is mapped through SPU_W to land at
 * the right host index. Verified to compose correctly with the fixed shufb for
 * both cbd-generated and immediate/LS-constant controls. */
static inline u128 spu_cbd_pos(int pos) {
    u128 r; for(int t=0;t<16;t++) r._u8[SPU_W(t)] = (uint8_t)(0x10 + t);
    r._u8[SPU_W(pos & 0xF)] = 0x03;                 /* a's preferred byte = SPU byte 3 */
    return r;
}
static inline u128 spu_chd_pos(int pos) {
    u128 r; for(int t=0;t<16;t++) r._u8[SPU_W(t)] = (uint8_t)(0x10 + t);
    int p = (pos & 0xF) & ~1;                       /* SPU halfword byte positions p, p+1 */
    r._u8[SPU_W(p)]   = 0x02;                        /* a SPU byte 2 (hi byte of preferred hw) */
    r._u8[SPU_W(p+1)] = 0x03;                        /* a SPU byte 3 (lo byte) */
    return r;
}
static inline u128 spu_cwd_pos(int pos) {
    u128 r; for(int t=0;t<16;t++) r._u8[SPU_W(t)] = (uint8_t)(0x10 + t);
    int p = (pos & 0xF) & ~3;
    for(int k=0;k<4;k++) r._u8[SPU_W(p+k)] = (uint8_t)k;   /* a SPU bytes 0..3 (preferred word) */
    return r;
}
static inline u128 spu_cdd_pos(int pos) {
    u128 r; for(int t=0;t<16;t++) r._u8[SPU_W(t)] = (uint8_t)(0x10 + t);
    int p = (pos & 0xF) & ~7;
    for(int k=0;k<8;k++) r._u8[SPU_W(p+k)] = (uint8_t)k;   /* a SPU bytes 0..7 (preferred dword) */
    return r;
}
#if defined(SPU__XI) && !defined(SPU_SCALAR_HELPERS) && !defined(SPU_SCALAR_INT) && !defined(SPU_NO_XI_INSMASK)
/* GOWA: the insertion controls by compare + blend. The _pos forms above store
 * 16 bytes, then 1-8 bytes at a variable index, then reload all 16 -- a failed
 * store-to-load forward (~15 cycles) in front of every SPU scalar store
 * (cwd/cbd + shufb + stqd). Host byte h is SPU byte t = h^3: base selector
 * 0x10+t; inside the sz-byte block holding pos, (t & (sz-1)) + off picks a's
 * preferred bytes (off = 3 byte, 2 halfword, 0 word/doubleword). Bit-identical
 * to the _pos forms (out/spudiff/teste_cwd.c). */
static inline u128 spu__xi_insmask(int pos, int sz, int off) {
    const __m128i t = spu__xi_spuidx();
    const __m128i base = _mm_add_epi8(t, _mm_set1_epi8(0x10));
    const __m128i m = _mm_cmpeq_epi8(_mm_and_si128(t, _mm_set1_epi8((char)~(sz - 1))),
                                     _mm_set1_epi8((char)((pos & 0xF) & ~(sz - 1))));
    const __m128i ins = _mm_add_epi8(_mm_and_si128(t, _mm_set1_epi8((char)(sz - 1))), _mm_set1_epi8((char)off));
    return spu__xi_st(_mm_blendv_epi8(base, ins, m));
}
static inline u128 spu_cbd(u128 a, int i7){ return spu__xi_insmask((int)a._u32[0] + i7, 1, 3); }
static inline u128 spu_chd(u128 a, int i7){ return spu__xi_insmask((int)a._u32[0] + i7, 2, 2); }
static inline u128 spu_cwd_ref(u128 a, int i7){ return spu_cwd_pos((int)a._u32[0]+i7); }
static inline u128 spu_cwd(u128 a, int i7) { return spu__xi_insmask((int)a._u32[0] + i7, 4, 0); }
static inline u128 spu_cdd(u128 a, int i7){ return spu__xi_insmask((int)a._u32[0] + i7, 8, 0); }
static inline u128 spu_cbx(u128 a, u128 b){ return spu__xi_insmask((int)(a._u32[0]+b._u32[0]), 1, 3); }
static inline u128 spu_chx(u128 a, u128 b){ return spu__xi_insmask((int)(a._u32[0]+b._u32[0]), 2, 2); }
static inline u128 spu_cwx(u128 a, u128 b){ return spu__xi_insmask((int)(a._u32[0]+b._u32[0]), 4, 0); }
static inline u128 spu_cdx(u128 a, u128 b){ return spu__xi_insmask((int)(a._u32[0]+b._u32[0]), 8, 0); }
#define SPU__XI_INSMASK 1
#else
static inline u128 spu_cbd(u128 a, int i7){ return spu_cbd_pos((int)a._u32[0]+i7); }
static inline u128 spu_chd(u128 a, int i7){ return spu_chd_pos((int)a._u32[0]+i7); }
static inline u128 spu_cwd_ref(u128 a, int i7){ return spu_cwd_pos((int)a._u32[0]+i7); }
#endif
#ifdef SPU__XI_INSMASK
/* defined above */
#elif defined(__ARM_NEON) && !defined(SPU_SCALAR_HELPERS) && !defined(SPU_SCALAR_INT)
static inline u128 spu_cwd(u128 a, int i7) {
    static const uint32_t base[4] = {0x10111213u, 0x14151617u, 0x18191A1Bu, 0x1C1D1E1Fu}, lane[4] = {0, 1, 2, 3};
    uint32x4_t m = vceqq_u32(vld1q_u32(lane), vdupq_n_u32((((uint32_t)a._u32[0] + (uint32_t)i7) >> 2) & 3));
    return spu__st(vbslq_u32(m, vdupq_n_u32(0x00010203u), vld1q_u32(base)));
}
#else
static inline u128 spu_cwd(u128 a, int i7) { return spu_cwd_ref(a, i7); }
#endif
#ifndef SPU__XI_INSMASK
static inline u128 spu_cdd(u128 a, int i7){ return spu_cdd_pos((int)a._u32[0]+i7); }
static inline u128 spu_cbx(u128 a, u128 b){ return spu_cbd_pos((int)(a._u32[0]+b._u32[0])); }
static inline u128 spu_chx(u128 a, u128 b){ return spu_chd_pos((int)(a._u32[0]+b._u32[0])); }
static inline u128 spu_cwx(u128 a, u128 b){ return spu_cwd_pos((int)(a._u32[0]+b._u32[0])); }
static inline u128 spu_cdx(u128 a, u128 b){ return spu_cdd_pos((int)(a._u32[0]+b._u32[0])); }
#endif

/* ---- Phase 3: rotate-and-mask family ---- */
static inline u128 spu_rotm_ref(u128 a, u128 b)   { u128 r; for(int i=0;i<4;i++){ uint32_t sh=(0-b._u32[i])&0x3F; r._u32[i]=(sh>31)?0:(a._u32[i]>>sh); } return r; }
static inline u128 spu_rotma_ref(u128 a, u128 b)  { u128 r; for(int i=0;i<4;i++){ uint32_t sh=(0-b._u32[i])&0x3F; r._s32[i]=(sh>31)?(a._s32[i]>>31):(a._s32[i]>>sh); } return r; }
static inline u128 spu_rothm_ref(u128 a, u128 b)  { u128 r; for(int i=0;i<8;i++){ uint32_t sh=(0-b._u16[i])&0x1F; r._u16[i]=(sh>15)?0:(uint16_t)(a._u16[i]>>sh); } return r; }
static inline u128 spu_rothma_ref(u128 a, u128 b) { u128 r; for(int i=0;i<8;i++){ uint32_t sh=(0-b._u16[i])&0x1F; r._s16[i]=(sh>15)?(a._s16[i]>>15):(a._s16[i]>>sh); } return r; }
static inline u128 spu_rothmi_ref(u128 a, int i7) { u128 r; int sh=(0-i7)&0x1F; for(int i=0;i<8;i++) r._u16[i]=(sh>15)?0:(uint16_t)(a._u16[i]>>sh); return r; }
#if defined(SPU__XV)
static inline u128 spu_rothmi(u128 a, int i7) { const __m128i A = spu__xv_ld(a); return spu__xv_st(_mm_srl_epi16(A, _mm_cvtsi32_si128((0 - i7) & 0x1F))); }
#else
static inline u128 spu_rothmi(u128 a, int i7) { return spu_rothmi_ref(a, i7); }
#endif
static inline u128 spu_rotqmbi_ref(u128 a, u128 b)   { int sh=(0-(int)b._u32[0])&7; if(!sh) return a;
    uint64_t hi=((uint64_t)a._u32[0]<<32)|a._u32[1], lo=((uint64_t)a._u32[2]<<32)|a._u32[3];
    uint64_t nlo=(lo>>sh)|(hi<<(64-sh)), nhi=(hi>>sh);
    u128 r; r._u32[0]=(uint32_t)(nhi>>32); r._u32[1]=(uint32_t)nhi; r._u32[2]=(uint32_t)(nlo>>32); r._u32[3]=(uint32_t)nlo; return r; }
#ifdef SPU__XQ
/* word i takes the bits leaving word i-1 (the next more significant); word 0 gets zeros */
static inline u128 spu_rotqmbi(u128 a, u128 b) {
    const __m128i v = spu__xi_ld(a);
    return spu__xi_st(_mm_shrdv_epi32(v, _mm_alignr_epi8(v, _mm_setzero_si128(), 12), _mm_set1_epi32((int)((0u - b._u32[0]) & 7u))));
}
#elif defined(SPU__XQ2)
static inline u128 spu_rotqmbi(u128 a, u128 b) {
    const int sh = (int)((0u - b._u32[0]) & 7u);
    const __m128i v = spu__xi_ld(a);
    return spu__xi_st(_mm_or_si128(_mm_srl_epi32(v, _mm_cvtsi32_si128(sh)),
                                   _mm_sll_epi32(_mm_slli_si128(v, 4), _mm_cvtsi32_si128(32 - sh))));
}
#else
static inline u128 spu_rotqmbi(u128 a, u128 b) { return spu_rotqmbi_ref(a, b); }
#endif
static inline u128 spu_rotqmby_ref(u128 a, u128 b)   { int sh=(0-(int)b._u32[0])&0x1F; u128 r=spu_zero(); if(sh>=16) return r; for(int i=0;i<16;i++){ int s=i-sh; if(s>=0) r._u8[SPU_W(i)]=a._u8[SPU_W(s)]; } return r; }
#if defined(__ARM_NEON) && !defined(SPU_SCALAR_HELPERS) && !defined(SPU_SCALAR_INT)
static inline u128 spu_rotqmby(u128 a, u128 b) {
    return spu__perm(a, vsubq_u8(spu__iota(), vdupq_n_u8((uint8_t)((0 - (int)b._u32[0]) & 0x1F))));
}
#else
#if defined(SPU__XI) && !defined(SPU_SCALAR_HELPERS) && !defined(SPU_SCALAR_INT)
static inline u128 spu_rotqmby(u128 a, u128 b) { return spu__xi_shift_bytes(a, -((0 - (int)b._u32[0]) & 0x1F)); }
#else
static inline u128 spu_rotqmby(u128 a, u128 b) { return spu_rotqmby_ref(a, b); }
#endif
#endif
static inline u128 spu_rotqmbybi_ref(u128 a, u128 b) { int sh=(0-((int)b._u32[0]>>3))&0x1F; u128 r=spu_zero(); if(sh>=16) return r; for(int i=0;i<16;i++){ int s=i-sh; if(s>=0) r._u8[SPU_W(i)]=a._u8[SPU_W(s)]; } return r; }
#if defined(__ARM_NEON) && !defined(SPU_SCALAR_HELPERS) && !defined(SPU_SCALAR_INT)
static inline u128 spu_rotqmbybi(u128 a, u128 b) {
    return spu__perm(a, vsubq_u8(spu__iota(), vdupq_n_u8((uint8_t)((0 - ((int)b._u32[0] >> 3)) & 0x1F))));
}
#else
#if defined(SPU__XI) && !defined(SPU_SCALAR_HELPERS) && !defined(SPU_SCALAR_INT)
static inline u128 spu_rotqmbybi(u128 a, u128 b) { return spu__xi_shift_bytes(a, -((0 - ((int)b._u32[0] >> 3)) & 0x1F)); }
#else
static inline u128 spu_rotqmbybi(u128 a, u128 b) { return spu_rotqmbybi_ref(a, b); }
#endif
#endif
static inline u128 spu_rotqmbii_ref(u128 a, int i7)  { int sh=(0-i7)&7; if(!sh) return a;
    uint64_t hi=((uint64_t)a._u32[0]<<32)|a._u32[1], lo=((uint64_t)a._u32[2]<<32)|a._u32[3];
    uint64_t nlo=(lo>>sh)|(hi<<(64-sh)), nhi=(hi>>sh);
    u128 r; r._u32[0]=(uint32_t)(nhi>>32); r._u32[1]=(uint32_t)nhi; r._u32[2]=(uint32_t)(nlo>>32); r._u32[3]=(uint32_t)nlo; return r; }
#ifdef SPU__XQ
static inline u128 spu_rotqmbii(u128 a, int i7) {
    const __m128i v = spu__xi_ld(a);
    return spu__xi_st(_mm_shrdv_epi32(v, _mm_alignr_epi8(v, _mm_setzero_si128(), 12), _mm_set1_epi32((0 - i7) & 7)));
}
#elif defined(SPU__XQ2)
static inline u128 spu_rotqmbii(u128 a, int i7) {
    const int sh = (0 - i7) & 7;
    const __m128i v = spu__xi_ld(a);
    return spu__xi_st(_mm_or_si128(_mm_srl_epi32(v, _mm_cvtsi32_si128(sh)),
                                   _mm_sll_epi32(_mm_slli_si128(v, 4), _mm_cvtsi32_si128(32 - sh))));
}
#else
static inline u128 spu_rotqmbii(u128 a, int i7) { return spu_rotqmbii_ref(a, i7); }
#endif
static inline u128 spu_rotqmbyi_ref(u128 a, int i7)  { int sh=(0-i7)&0x1F; u128 r=spu_zero(); if(sh>=16) return r; for(int i=0;i<16;i++){ int s=i-sh; if(s>=0) r._u8[SPU_W(i)]=a._u8[SPU_W(s)]; } return r; }
#if defined(__ARM_NEON) && !defined(SPU_SCALAR_HELPERS) && !defined(SPU_SCALAR_INT)
static inline u128 spu_rotqmbyi(u128 a, int i7) {
    return spu__perm(a, vsubq_u8(spu__iota(), vdupq_n_u8((uint8_t)((0 - i7) & 0x1F))));
}
#else
#if defined(SPU__XI) && !defined(SPU_SCALAR_HELPERS) && !defined(SPU_SCALAR_INT)
static inline u128 spu_rotqmbyi(u128 a, int i7) { return spu__xi_shift_bytes(a, -((0 - i7) & 0x1F)); }
#else
static inline u128 spu_rotqmbyi(u128 a, int i7) { return spu_rotqmbyi_ref(a, i7); }
#endif
#endif

/* ---- Phase 3: halfword/byte immediate logic ---- */
static inline u128 spu_andhi_ref(u128 a, int32_t imm) { u128 r; uint16_t v=(uint16_t)imm; for(int i=0;i<8;i++) r._u16[i]=a._u16[i]&v; return r; }
#if defined(SPU__XV)
static inline u128 spu_andhi(u128 a, int32_t imm) { const __m128i A = spu__xv_ld(a); return spu__xv_st(_mm_and_si128(A, _mm_set1_epi16((short)imm))); }
#else
static inline u128 spu_andhi(u128 a, int32_t imm) { return spu_andhi_ref(a, imm); }
#endif
static inline u128 spu_andbi_ref(u128 a, int32_t imm) { u128 r; uint8_t  v=(uint8_t)imm;  for(int i=0;i<16;i++) r._u8[i]=a._u8[i]&v;   return r; }
#if defined(SPU__XV)
static inline u128 spu_andbi(u128 a, int32_t imm) { const __m128i A = spu__xv_ld(a); return spu__xv_st(_mm_and_si128(A, _mm_set1_epi8((char)imm))); }
#else
static inline u128 spu_andbi(u128 a, int32_t imm) { return spu_andbi_ref(a, imm); }
#endif
static inline u128 spu_orhi_ref(u128 a, int32_t imm)  { u128 r; uint16_t v=(uint16_t)imm; for(int i=0;i<8;i++) r._u16[i]=a._u16[i]|v; return r; }
#if defined(SPU__XV)
static inline u128 spu_orhi(u128 a, int32_t imm) { const __m128i A = spu__xv_ld(a); return spu__xv_st(_mm_or_si128(A, _mm_set1_epi16((short)imm))); }
#else
static inline u128 spu_orhi(u128 a, int32_t imm) { return spu_orhi_ref(a, imm); }
#endif
static inline u128 spu_orbi_ref(u128 a, int32_t imm)  { u128 r; uint8_t  v=(uint8_t)imm;  for(int i=0;i<16;i++) r._u8[i]=a._u8[i]|v;   return r; }
#if defined(SPU__XV)
static inline u128 spu_orbi(u128 a, int32_t imm) { const __m128i A = spu__xv_ld(a); return spu__xv_st(_mm_or_si128(A, _mm_set1_epi8((char)imm))); }
#else
static inline u128 spu_orbi(u128 a, int32_t imm) { return spu_orbi_ref(a, imm); }
#endif
static inline u128 spu_xorhi_ref(u128 a, int32_t imm) { u128 r; uint16_t v=(uint16_t)imm; for(int i=0;i<8;i++) r._u16[i]=a._u16[i]^v; return r; }
#if defined(SPU__XV)
static inline u128 spu_xorhi(u128 a, int32_t imm) { const __m128i A = spu__xv_ld(a); return spu__xv_st(_mm_xor_si128(A, _mm_set1_epi16((short)imm))); }
#else
static inline u128 spu_xorhi(u128 a, int32_t imm) { return spu_xorhi_ref(a, imm); }
#endif

/* ---- Phase 3: borrow-generate extended ---- */
static inline u128 spu_bgx_ref(u128 a, u128 b, u128 t) {
    u128 r;
    for(int i=0;i<4;i++) {
        uint64_t s = (uint64_t)b._u32[i] + (uint64_t)(~a._u32[i]) + (uint64_t)(t._u32[i]&1);
        r._u32[i] = (uint32_t)(s >> 32);
    }
    return r;
}

/* ---- Phase 3: mfspr stub ---- */
static inline u128 spu_mfspr(u128 a) { (void)a; return spu_zero(); }

/* ---- immediate loaders ---- */
static inline u128 spu_il(int32_t imm)   { return spu_splat_u32((uint32_t)imm); }
static inline u128 spu_ila(uint32_t i18) { return spu_splat_u32(i18 & 0x3FFFF); }
static inline u128 spu_ilh(uint16_t imm) { return spu_splat_u16(imm); }
static inline u128 spu_ilhu(uint16_t imm){ return spu_splat_u32((uint32_t)imm << 16); }
static inline u128 spu_iohl(u128 a, uint16_t imm){ u128 r; for(int i=0;i<4;i++) r._u32[i]=a._u32[i]|(uint32_t)imm; return r; }

#ifdef __cplusplus
}
#endif

/* ---- NEON forms of the per-lane integer ops ------------------------------------------
 * Each was a scalar loop over 4/8/16 lanes (tests/bench/spu: 3-20 ns per op lifted); here it
 * is one to five vector instructions. Bit-identical to spu_X_ref (tests/test_spu_int_neon.c). */
#if defined(__ARM_NEON) && !defined(SPU_SCALAR_HELPERS) && !defined(SPU_SCALAR_INT)
static inline uint16x8_t vnegq_u16_compat(uint16x8_t x) { return vsubq_u16(vdupq_n_u16(0), x); }
static inline u128 spu_absdb(u128 a, u128 b) {
    return spu__st(vreinterpretq_u32_u8(vabdq_u8(vreinterpretq_u8_u32(spu__ld(a)), vreinterpretq_u8_u32(spu__ld(b)))));
}
static inline u128 spu_avgb(u128 a, u128 b) {
    return spu__st(vreinterpretq_u32_u8(vrhaddq_u8(vreinterpretq_u8_u32(spu__ld(a)), vreinterpretq_u8_u32(spu__ld(b)))));
}
static inline u128 spu_ceqb(u128 a, u128 b) {
    return spu__st(vreinterpretq_u32_u8(vceqq_u8(vreinterpretq_u8_u32(spu__ld(a)), vreinterpretq_u8_u32(spu__ld(b)))));
}
static inline u128 spu_cgtb(u128 a, u128 b) {
    return spu__st(vreinterpretq_u32_u8(vcgtq_s8(vreinterpretq_s8_u32(spu__ld(a)), vreinterpretq_s8_u32(spu__ld(b)))));
}
static inline u128 spu_clgtb(u128 a, u128 b) {
    return spu__st(vreinterpretq_u32_u8(vcgtq_u8(vreinterpretq_u8_u32(spu__ld(a)), vreinterpretq_u8_u32(spu__ld(b)))));
}
static inline u128 spu_ceqh(u128 a, u128 b) {
    return spu__st(vreinterpretq_u32_u16(vceqq_u16(vreinterpretq_u16_u32(spu__ld(a)), vreinterpretq_u16_u32(spu__ld(b)))));
}
static inline u128 spu_cgth(u128 a, u128 b) {
    return spu__st(vreinterpretq_u32_u16(vcgtq_s16(vreinterpretq_s16_u32(spu__ld(a)), vreinterpretq_s16_u32(spu__ld(b)))));
}
static inline u128 spu_clgth(u128 a, u128 b) {
    return spu__st(vreinterpretq_u32_u16(vcgtq_u16(vreinterpretq_u16_u32(spu__ld(a)), vreinterpretq_u16_u32(spu__ld(b)))));
}
static inline u128 spu_ah(u128 a, u128 b) {
    return spu__st(vreinterpretq_u32_u16(vaddq_u16(vreinterpretq_u16_u32(spu__ld(a)), vreinterpretq_u16_u32(spu__ld(b)))));
}
static inline u128 spu_sfh(u128 a, u128 b) {
    return spu__st(vreinterpretq_u32_u16(vsubq_u16(vreinterpretq_u16_u32(spu__ld(b)), vreinterpretq_u16_u32(spu__ld(a)))));
}
static inline u128 spu_shlh(u128 a, u128 b) {
    return spu__st(vreinterpretq_u32_u16(vshlq_u16(vreinterpretq_u16_u32(spu__ld(a)), vreinterpretq_s16_u32(vandq_u32(spu__ld(b), vdupq_n_u32(0x001F001Fu))))));
}
static inline u128 spu_roth(u128 a, u128 b) {
    const uint16x8_t x = vreinterpretq_u16_u32(spu__ld(a)); const int16x8_t n = vreinterpretq_s16_u32(vandq_u32(spu__ld(b), vdupq_n_u32(0x000F000Fu)));
    return spu__st(vreinterpretq_u32_u16(vorrq_u16(vshlq_u16(x, n), vshlq_u16(x, vsubq_s16(n, vdupq_n_s16(16))))));
}
static inline u128 spu_rothm(u128 a, u128 b) {
    const int16x8_t m = vnegq_s16(vreinterpretq_s16_u16(vandq_u16(vnegq_u16_compat(vreinterpretq_u16_u32(spu__ld(b))), vdupq_n_u16(0x1F))));
    return spu__st(vreinterpretq_u32_u16(vshlq_u16(vreinterpretq_u16_u32(spu__ld(a)), m)));
}
static inline u128 spu_rothma(u128 a, u128 b) {
    const int16x8_t m = vnegq_s16(vreinterpretq_s16_u16(vandq_u16(vnegq_u16_compat(vreinterpretq_u16_u32(spu__ld(b))), vdupq_n_u16(0x1F))));
    return spu__st(vreinterpretq_u32_s16(vshlq_s16(vreinterpretq_s16_u32(spu__ld(a)), m)));
}
static inline u128 spu_shl(u128 a, u128 b) {
    return spu__st(vshlq_u32(spu__ld(a), vreinterpretq_s32_u32(vandq_u32(spu__ld(b), vdupq_n_u32(0x3F)))));
}
static inline u128 spu_rot(u128 a, u128 b) {
    const uint32x4_t x = spu__ld(a); const int32x4_t n = vreinterpretq_s32_u32(vandq_u32(spu__ld(b), vdupq_n_u32(31)));
    return spu__st(vorrq_u32(vshlq_u32(x, n), vshlq_u32(x, vsubq_s32(n, vdupq_n_s32(32)))));
}
static inline u128 spu_rotm(u128 a, u128 b) {
    const int32x4_t m = vnegq_s32(vreinterpretq_s32_u32(vandq_u32(vsubq_u32(vdupq_n_u32(0), spu__ld(b)), vdupq_n_u32(0x3F))));
    return spu__st(vshlq_u32(spu__ld(a), m));
}
static inline u128 spu_rotma(u128 a, u128 b) {
    const int32x4_t m = vnegq_s32(vreinterpretq_s32_u32(vandq_u32(vsubq_u32(vdupq_n_u32(0), spu__ld(b)), vdupq_n_u32(0x3F))));
    return spu__st(vreinterpretq_u32_s32(vshlq_s32(vreinterpretq_s32_u32(spu__ld(a)), m)));
}
static inline u128 spu_cg(u128 a, u128 b) {
    const uint32x4_t x = spu__ld(a);
    return spu__st(vshrq_n_u32(vcltq_u32(vaddq_u32(x, spu__ld(b)), x), 31));
}
static inline u128 spu_bg(u128 a, u128 b) {
    return spu__st(vshrq_n_u32(vcgeq_u32(spu__ld(b), spu__ld(a)), 31));
}
static inline u128 spu_mpy(u128 a, u128 b) {
    return spu__st(vreinterpretq_u32_s32(vmulq_s32(vshrq_n_s32(vshlq_n_s32(vreinterpretq_s32_u32(spu__ld(a)), 16), 16), vshrq_n_s32(vshlq_n_s32(vreinterpretq_s32_u32(spu__ld(b)), 16), 16))));
}
static inline u128 spu_mpyu(u128 a, u128 b) {
    const uint32x4_t m = vdupq_n_u32(0xFFFFu);
    return spu__st(vmulq_u32(vandq_u32(spu__ld(a), m), vandq_u32(spu__ld(b), m)));
}
static inline u128 spu_mpyh(u128 a, u128 b) {
    return spu__st(vshlq_n_u32(vmulq_u32(vshrq_n_u32(spu__ld(a), 16), spu__ld(b)), 16));
}
static inline u128 spu_mpyhh(u128 a, u128 b) {
    return spu__st(vreinterpretq_u32_s32(vmulq_s32(vshrq_n_s32(vreinterpretq_s32_u32(spu__ld(a)), 16), vshrq_n_s32(vreinterpretq_s32_u32(spu__ld(b)), 16))));
}
static inline u128 spu_mpyhhu(u128 a, u128 b) {
    return spu__st(vmulq_u32(vshrq_n_u32(spu__ld(a), 16), vshrq_n_u32(spu__ld(b), 16)));
}
static inline u128 spu_mpys(u128 a, u128 b) {
    const int32x4_t p = vmulq_s32(vshrq_n_s32(vshlq_n_s32(vreinterpretq_s32_u32(spu__ld(a)), 16), 16), vshrq_n_s32(vshlq_n_s32(vreinterpretq_s32_u32(spu__ld(b)), 16), 16));
    return spu__st(vreinterpretq_u32_s32(vshrq_n_s32(p, 16)));
}
static inline u128 spu_sumb(u128 a, u128 b) {
    const uint32x4_t sa = vpaddlq_u16(vpaddlq_u8(vreinterpretq_u8_u32(spu__ld(a)))), sb = vpaddlq_u16(vpaddlq_u8(vreinterpretq_u8_u32(spu__ld(b))));
    return spu__st(vorrq_u32(vshlq_n_u32(sb, 16), sa));
}
static inline u128 spu_ceqbi(u128 a, int32_t imm) {
    return spu__st(vreinterpretq_u32_u8(vceqq_u8(vreinterpretq_u8_u32(spu__ld(a)), vdupq_n_u8((uint8_t)imm))));
}
static inline u128 spu_cgtbi(u128 a, int32_t imm) {
    return spu__st(vreinterpretq_u32_u8(vcgtq_s8(vreinterpretq_s8_u32(spu__ld(a)), vdupq_n_s8((int8_t)imm))));
}
static inline u128 spu_clgtbi(u128 a, int32_t imm) {
    return spu__st(vreinterpretq_u32_u8(vcgtq_u8(vreinterpretq_u8_u32(spu__ld(a)), vdupq_n_u8((uint8_t)imm))));
}
static inline u128 spu_ceqhi(u128 a, int32_t imm) {
    return spu__st(vreinterpretq_u32_u16(vceqq_s16(vreinterpretq_s16_u32(spu__ld(a)), vdupq_n_s16((int16_t)imm))));
}
static inline u128 spu_cgthi(u128 a, int32_t imm) {
    return spu__st(vreinterpretq_u32_u16(vcgtq_s16(vreinterpretq_s16_u32(spu__ld(a)), vdupq_n_s16((int16_t)imm))));
}
static inline u128 spu_clgthi(u128 a, int32_t imm) {
    return spu__st(vreinterpretq_u32_u16(vcgtq_u16(vreinterpretq_u16_u32(spu__ld(a)), vdupq_n_u16((uint16_t)imm))));
}
static inline u128 spu_ahi(u128 a, int32_t imm) {
    return spu__st(vreinterpretq_u32_u16(vaddq_u16(vreinterpretq_u16_u32(spu__ld(a)), vdupq_n_u16((uint16_t)imm))));
}
static inline u128 spu_sfhi(u128 a, int32_t imm) {
    return spu__st(vreinterpretq_u32_u16(vsubq_u16(vdupq_n_u16((uint16_t)imm), vreinterpretq_u16_u32(spu__ld(a)))));
}
static inline u128 spu_mpyi(u128 a, int32_t imm) {
    return spu__st(vreinterpretq_u32_s32(vmulq_s32(vshrq_n_s32(vshlq_n_s32(vreinterpretq_s32_u32(spu__ld(a)), 16), 16), vdupq_n_s32((int16_t)imm))));
}
static inline u128 spu_mpyui(u128 a, int32_t imm) {
    return spu__st(vmulq_u32(vandq_u32(spu__ld(a), vdupq_n_u32(0xFFFFu)), vdupq_n_u32((uint16_t)imm)));
}
static inline u128 spu_rothi(u128 a, int sh) {
    const uint16x8_t x = vreinterpretq_u16_u32(spu__ld(a)); sh &= 15;
    return spu__st(vreinterpretq_u32_u16(vorrq_u16(vshlq_u16(x, vdupq_n_s16((int16_t)sh)), vshlq_u16(x, vdupq_n_s16((int16_t)(sh - 16))))));
}
static inline u128 spu_roti(u128 a, int sh) {
    const uint32x4_t x = spu__ld(a); sh &= 31;
    return spu__st(vorrq_u32(vshlq_u32(x, vdupq_n_s32(sh)), vshlq_u32(x, vdupq_n_s32(sh - 32))));
}
static inline u128 spu_rotmahi(u128 a, int i7) {
    return spu__st(vreinterpretq_u32_s16(vshlq_s16(vreinterpretq_s16_u32(spu__ld(a)), vdupq_n_s16((int16_t)-((0 - i7) & 0x1F)))));
}
static inline u128 spu_rotmai(u128 a, int i7) {
    return spu__st(vreinterpretq_u32_s32(vshlq_s32(vreinterpretq_s32_u32(spu__ld(a)), vdupq_n_s32(-((0 - i7) & 0x3F)))));
}
static inline u128 spu_mpya(u128 a, u128 b, u128 c) {
    return spu__st(vaddq_u32(vreinterpretq_u32_s32(vmulq_s32(vshrq_n_s32(vshlq_n_s32(vreinterpretq_s32_u32(spu__ld(a)), 16), 16), vshrq_n_s32(vshlq_n_s32(vreinterpretq_s32_u32(spu__ld(b)), 16), 16))), spu__ld(c)));
}
static inline u128 spu_mpyhha(u128 a, u128 b, u128 t) {
    return spu__st(vaddq_u32(vreinterpretq_u32_s32(vmulq_s32(vshrq_n_s32(vreinterpretq_s32_u32(spu__ld(a)), 16), vshrq_n_s32(vreinterpretq_s32_u32(spu__ld(b)), 16))), spu__ld(t)));
}
static inline u128 spu_mpyhhau(u128 a, u128 b, u128 t) {
    return spu__st(vmlaq_u32(spu__ld(t), vshrq_n_u32(spu__ld(a), 16), vshrq_n_u32(spu__ld(b), 16)));
}
static inline u128 spu_addx(u128 a, u128 b, u128 t) {
    return spu__st(vaddq_u32(vaddq_u32(spu__ld(a), spu__ld(b)), vandq_u32(spu__ld(t), vdupq_n_u32(1))));
}
static inline u128 spu_sfx(u128 a, u128 b, u128 t) {
    return spu__st(vaddq_u32(vaddq_u32(spu__ld(b), vmvnq_u32(spu__ld(a))), vandq_u32(spu__ld(t), vdupq_n_u32(1))));
}
static inline u128 spu_cgx(u128 a, u128 b, u128 t) {
    const uint32x4_t x = spu__ld(a), s = vaddq_u32(x, spu__ld(b)), s2 = vaddq_u32(s, vandq_u32(spu__ld(t), vdupq_n_u32(1)));
    return spu__st(vshrq_n_u32(vorrq_u32(vcltq_u32(s, x), vcltq_u32(s2, s)), 31));
}
static inline u128 spu_bgx(u128 a, u128 b, u128 t) {
    const uint32x4_t y = spu__ld(b), s = vaddq_u32(y, vmvnq_u32(spu__ld(a))), s2 = vaddq_u32(s, vandq_u32(spu__ld(t), vdupq_n_u32(1)));
    return spu__st(vshrq_n_u32(vorrq_u32(vcltq_u32(s, y), vcltq_u32(s2, s)), 31));
}
static inline u128 spu_cntb(u128 a) {
    return spu__st(vreinterpretq_u32_u8(vcntq_u8(vreinterpretq_u8_u32(spu__ld(a)))));
}
static inline u128 spu_xsbh(u128 a) {
    return spu__st(vreinterpretq_u32_s16(vshrq_n_s16(vshlq_n_s16(vreinterpretq_s16_u32(spu__ld(a)), 8), 8)));
}
static inline u128 spu_xshw(u128 a) {
    return spu__st(vreinterpretq_u32_s32(vshrq_n_s32(vshlq_n_s32(vreinterpretq_s32_u32(spu__ld(a)), 16), 16)));
}
static inline u128 spu_clz(u128 a) {
    return spu__st(vclzq_u32(spu__ld(a)));
}
static inline u128 spu_xswd(u128 a) {
    const uint32x4_t x = spu__ld(a);
    return spu__st(vtrn2q_u32(vreinterpretq_u32_s32(vshrq_n_s32(vreinterpretq_s32_u32(x), 31)), x));
}
static inline u128 spu_orx(u128 a) {
    /* fold the four words onto word 0 without leaving the vector registers */
    uint32x4_t x = spu__ld(a);
    x = vorrq_u32(x, vextq_u32(x, x, 2));
    x = vorrq_u32(x, vextq_u32(x, x, 1));
    return spu__st(vandq_u32(x, vreinterpretq_u32_u64(vsetq_lane_u64(0xFFFFFFFFull, vdupq_n_u64(0), 0))));
}
static inline u128 spu_gb(u128 a) {
    static const uint32_t w[4] = {8, 4, 2, 1};
    return spu_pref_u32(vaddvq_u32(vmulq_u32(vandq_u32(spu__ld(a), vdupq_n_u32(1)), vld1q_u32(w))));
}
static inline u128 spu_gbh(u128 a) {
    static const uint16_t w[8] = {64, 128, 16, 32, 4, 8, 1, 2};
    return spu_pref_u32(vaddvq_u16(vmulq_u16(vandq_u16(vreinterpretq_u16_u32(spu__ld(a)), vdupq_n_u16(1)), vld1q_u16(w))));
}
static inline u128 spu_gbb(u128 a) {
    /* host bytes 0-7 are SPU bytes 0-7 (result bits 15-8), bytes 8-15 bits 7-0: after three
     * pairwise adds byte 0 holds the high result byte and byte 1 the low one */
    static const uint8_t w[16] = {16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8};
    uint8x16_t m = vandq_u8(vtstq_u8(vreinterpretq_u8_u32(spu__ld(a)), vdupq_n_u8(1)), vld1q_u8(w));
    m = vpaddq_u8(m, m); m = vpaddq_u8(m, m); m = vpaddq_u8(m, m);
    return spu__st(vandq_u32(vreinterpretq_u32_u8(vrev16q_u8(m)), vdupq_n_u32(0xFFFFu) & vreinterpretq_u32_u64(vsetq_lane_u64(0xFFFFFFFFull, vdupq_n_u64(0), 0))));
}
static inline u128 spu_fsm(u128 a) {
    static const uint32_t w[4] = {8, 4, 2, 1};
    return spu__st(vtstq_u32(vdupq_n_u32(a._u32[0]), vld1q_u32(w)));
}
static inline u128 spu_fsmh(u128 a) {
    static const uint16_t w[8] = {64, 128, 16, 32, 4, 8, 1, 2};
    return spu__st(vreinterpretq_u32_u16(vtstq_u16(vdupq_n_u16((uint16_t)a._u32[0]), vld1q_u16(w))));
}
static inline u128 spu_fsmb(u128 a) {
    static const uint8_t w[16] = {16, 32, 64, 128, 1, 2, 4, 8, 16, 32, 64, 128, 1, 2, 4, 8};
    const uint8x16_t v = vcombine_u8(vdup_n_u8((uint8_t)(a._u32[0] >> 8)), vdup_n_u8((uint8_t)a._u32[0]));
    return spu__st(vreinterpretq_u32_u8(vtstq_u8(v, vld1q_u8(w))));
}
#else
#if defined(SPU__XV)
static inline u128 spu_absdb(u128 a, u128 b) { const __m128i A = spu__xv_ld(a); const __m128i B = spu__xv_ld(b); return spu__xv_st(_mm_sub_epi8(_mm_max_epu8(A, B), _mm_min_epu8(A, B))); }
#else
static inline u128 spu_absdb(u128 a, u128 b) { return spu_absdb_ref(a, b); }
#endif
#if defined(SPU__XV)
static inline u128 spu_avgb(u128 a, u128 b) { const __m128i A = spu__xv_ld(a); const __m128i B = spu__xv_ld(b); return spu__xv_st(_mm_avg_epu8(A, B)); }
#else
static inline u128 spu_avgb(u128 a, u128 b) { return spu_avgb_ref(a, b); }
#endif
#if defined(SPU__XV)
static inline u128 spu_ceqb(u128 a, u128 b) { const __m128i A = spu__xv_ld(a); const __m128i B = spu__xv_ld(b); return spu__xv_st(_mm_cmpeq_epi8(A, B)); }
#else
static inline u128 spu_ceqb(u128 a, u128 b) { return spu_ceqb_ref(a, b); }
#endif
#if defined(SPU__XV)
static inline u128 spu_cgtb(u128 a, u128 b) { const __m128i A = spu__xv_ld(a); const __m128i B = spu__xv_ld(b); return spu__xv_st(_mm_cmpgt_epi8(A, B)); }
#else
static inline u128 spu_cgtb(u128 a, u128 b) { return spu_cgtb_ref(a, b); }
#endif
#if defined(SPU__XV)
static inline u128 spu_clgtb(u128 a, u128 b) { const __m128i A = spu__xv_ld(a); const __m128i B = spu__xv_ld(b); return spu__xv_st(spu__xv_movm8(spu__xv_cmpgtu8(A, B))); }
#else
static inline u128 spu_clgtb(u128 a, u128 b) { return spu_clgtb_ref(a, b); }
#endif
#if defined(SPU__XV)
static inline u128 spu_ceqh(u128 a, u128 b) { const __m128i A = spu__xv_ld(a); const __m128i B = spu__xv_ld(b); return spu__xv_st(_mm_cmpeq_epi16(A, B)); }
#else
static inline u128 spu_ceqh(u128 a, u128 b) { return spu_ceqh_ref(a, b); }
#endif
#if defined(SPU__XV)
static inline u128 spu_cgth(u128 a, u128 b) { const __m128i A = spu__xv_ld(a); const __m128i B = spu__xv_ld(b); return spu__xv_st(_mm_cmpgt_epi16(A, B)); }
#else
static inline u128 spu_cgth(u128 a, u128 b) { return spu_cgth_ref(a, b); }
#endif
#if defined(SPU__XV)
static inline u128 spu_clgth(u128 a, u128 b) { const __m128i A = spu__xv_ld(a); const __m128i B = spu__xv_ld(b); return spu__xv_st(spu__xv_movm16(spu__xv_cmpgtu16(A, B))); }
#else
static inline u128 spu_clgth(u128 a, u128 b) { return spu_clgth_ref(a, b); }
#endif
#if defined(SPU__XV)
static inline u128 spu_ah(u128 a, u128 b) { const __m128i A = spu__xv_ld(a); const __m128i B = spu__xv_ld(b); return spu__xv_st(_mm_add_epi16(A, B)); }
#else
static inline u128 spu_ah(u128 a, u128 b) { return spu_ah_ref(a, b); }
#endif
#if defined(SPU__XV)
static inline u128 spu_sfh(u128 a, u128 b) { const __m128i A = spu__xv_ld(a); const __m128i B = spu__xv_ld(b); return spu__xv_st(_mm_sub_epi16(B, A)); }
#else
static inline u128 spu_sfh(u128 a, u128 b) { return spu_sfh_ref(a, b); }
#endif
#if defined(SPU__XV)
static inline u128 spu_shlh(u128 a, u128 b) { const __m128i A = spu__xv_ld(a); const __m128i B = spu__xv_ld(b); return spu__xv_st(spu__xv_sllv16(A, _mm_and_si128(B, _mm_set1_epi16(0x1F)))); }
#else
static inline u128 spu_shlh(u128 a, u128 b) { return spu_shlh_ref(a, b); }
#endif
#if defined(SPU__XV)
static inline u128 spu_roth(u128 a, u128 b) { const __m128i A = spu__xv_ld(a); const __m128i B = spu__xv_ld(b); return spu__xv_st(_mm_or_si128(spu__xv_sllv16(A, _mm_and_si128(B, _mm_set1_epi16(15))), spu__xv_srlv16(A, _mm_sub_epi16(_mm_set1_epi16(16), _mm_and_si128(B, _mm_set1_epi16(15)))))); }
#else
static inline u128 spu_roth(u128 a, u128 b) { return spu_roth_ref(a, b); }
#endif
#if defined(SPU__XV)
static inline u128 spu_rothm(u128 a, u128 b) { const __m128i A = spu__xv_ld(a); const __m128i B = spu__xv_ld(b); return spu__xv_st(spu__xv_srlv16(A, _mm_and_si128(_mm_sub_epi16(_mm_setzero_si128(), B), _mm_set1_epi16(0x1F)))); }
#else
static inline u128 spu_rothm(u128 a, u128 b) { return spu_rothm_ref(a, b); }
#endif
#if defined(SPU__XV)
static inline u128 spu_rothma(u128 a, u128 b) { const __m128i A = spu__xv_ld(a); const __m128i B = spu__xv_ld(b); return spu__xv_st(spu__xv_srav16(A, _mm_and_si128(_mm_sub_epi16(_mm_setzero_si128(), B), _mm_set1_epi16(0x1F)))); }
#else
static inline u128 spu_rothma(u128 a, u128 b) { return spu_rothma_ref(a, b); }
#endif
#if defined(SPU__XV)
static inline u128 spu_shl(u128 a, u128 b) { const __m128i A = spu__xv_ld(a); const __m128i B = spu__xv_ld(b); return spu__xv_st(_mm_sllv_epi32(A, _mm_and_si128(B, _mm_set1_epi32(0x3F)))); }
#else
static inline u128 spu_shl(u128 a, u128 b) { return spu_shl_ref(a, b); }
#endif
#if defined(SPU__XV)
static inline u128 spu_rot(u128 a, u128 b) { const __m128i A = spu__xv_ld(a); const __m128i B = spu__xv_ld(b); return spu__xv_st(spu__xv_rolv32(A, B)); }
#else
static inline u128 spu_rot(u128 a, u128 b) { return spu_rot_ref(a, b); }
#endif
#if defined(SPU__XV)
static inline u128 spu_rotm(u128 a, u128 b) { const __m128i A = spu__xv_ld(a); const __m128i B = spu__xv_ld(b); return spu__xv_st(_mm_srlv_epi32(A, _mm_and_si128(_mm_sub_epi32(_mm_setzero_si128(), B), _mm_set1_epi32(0x3F)))); }
#else
static inline u128 spu_rotm(u128 a, u128 b) { return spu_rotm_ref(a, b); }
#endif
#if defined(SPU__XV)
static inline u128 spu_rotma(u128 a, u128 b) { const __m128i A = spu__xv_ld(a); const __m128i B = spu__xv_ld(b); return spu__xv_st(_mm_srav_epi32(A, _mm_and_si128(_mm_sub_epi32(_mm_setzero_si128(), B), _mm_set1_epi32(0x3F)))); }
#else
static inline u128 spu_rotma(u128 a, u128 b) { return spu_rotma_ref(a, b); }
#endif
#if defined(SPU__XV)
static inline u128 spu_cg(u128 a, u128 b) { const __m128i A = spu__xv_ld(a); const __m128i B = spu__xv_ld(b); return spu__xv_st(spu__xv_cg(A, B)); }
#else
static inline u128 spu_cg(u128 a, u128 b) { return spu_cg_ref(a, b); }
#endif
#if defined(SPU__XV)
static inline u128 spu_bg(u128 a, u128 b) { const __m128i A = spu__xv_ld(a); const __m128i B = spu__xv_ld(b); return spu__xv_st(spu__xv_bg(A, B)); }
#else
static inline u128 spu_bg(u128 a, u128 b) { return spu_bg_ref(a, b); }
#endif
#if defined(SPU__XI) && !defined(SPU_NO_XM)
/* GOWA (23/10): the 16x16 multiplies and the sign extensions as SSE (the _ref
 * loops went element by element through the integer registers: ~18 host
 * instructions each and a vpextrd/vpinsrd round trip inline -- the 32-bit
 * multiply idiom mpyh+mpyh+mpyu of every address computation, and xsbh, 275
 * sites in the hot modules). Values: word i in _u32[i], host order, so _s16[2i]
 * is the LOW halfword of word i. Bit-identical to the _ref forms (scratchpad
 * test t_xm.c, 23/10). SPU_NO_XM turns them off. */
#define SPU__XM 1
static inline __m128i spu__xm_lo16(__m128i v) { return _mm_and_si128(v, _mm_set1_epi32(0xFFFF)); }
/* pmaddwd with one side's high halfword zero: lo*lo signed, + hi*0 */
static inline u128 spu_mpy(u128 a, u128 b)    { return spu__xi_st(_mm_madd_epi16(spu__xi_ld(a), spu__xm_lo16(spu__xi_ld(b)))); }
static inline u128 spu_mpyu(u128 a, u128 b)   { return spu__xi_st(_mm_mullo_epi32(spu__xm_lo16(spu__xi_ld(a)), spu__xm_lo16(spu__xi_ld(b)))); }
/* (a.hi * b.lo) << 16 keeps only the low 16 bits of the product */
static inline u128 spu_mpyh(u128 a, u128 b)   { return spu__xi_st(_mm_slli_epi32(_mm_mullo_epi16(_mm_srli_epi32(spu__xi_ld(a), 16), spu__xi_ld(b)), 16)); }
static inline u128 spu_mpyhh(u128 a, u128 b)  { return spu__xi_st(_mm_madd_epi16(_mm_srli_epi32(spu__xi_ld(a), 16), _mm_srli_epi32(spu__xi_ld(b), 16))); }
static inline u128 spu_mpyhhu(u128 a, u128 b) { return spu__xi_st(_mm_mullo_epi32(_mm_srli_epi32(spu__xi_ld(a), 16), _mm_srli_epi32(spu__xi_ld(b), 16))); }
static inline u128 spu_mpys(u128 a, u128 b)   { return spu__xi_st(_mm_srai_epi32(_mm_madd_epi16(spu__xi_ld(a), spu__xm_lo16(spu__xi_ld(b))), 16)); }
#else
static inline u128 spu_mpy(u128 a, u128 b) { return spu_mpy_ref(a, b); }
static inline u128 spu_mpyu(u128 a, u128 b) { return spu_mpyu_ref(a, b); }
static inline u128 spu_mpyh(u128 a, u128 b) { return spu_mpyh_ref(a, b); }
static inline u128 spu_mpyhh(u128 a, u128 b) { return spu_mpyhh_ref(a, b); }
static inline u128 spu_mpyhhu(u128 a, u128 b) { return spu_mpyhhu_ref(a, b); }
static inline u128 spu_mpys(u128 a, u128 b) { return spu_mpys_ref(a, b); }
#endif
static inline u128 spu_sumb(u128 a, u128 b) { return spu_sumb_ref(a, b); }
#if defined(SPU__XV)
static inline u128 spu_ceqbi(u128 a, int32_t imm) { const __m128i A = spu__xv_ld(a); return spu__xv_st(_mm_cmpeq_epi8(A, _mm_set1_epi8((char)imm))); }
#else
static inline u128 spu_ceqbi(u128 a, int32_t imm) { return spu_ceqbi_ref(a, imm); }
#endif
#if defined(SPU__XV)
static inline u128 spu_cgtbi(u128 a, int32_t imm) { const __m128i A = spu__xv_ld(a); return spu__xv_st(_mm_cmpgt_epi8(A, _mm_set1_epi8((char)imm))); }
#else
static inline u128 spu_cgtbi(u128 a, int32_t imm) { return spu_cgtbi_ref(a, imm); }
#endif
#if defined(SPU__XV)
static inline u128 spu_clgtbi(u128 a, int32_t imm) { const __m128i A = spu__xv_ld(a); return spu__xv_st(spu__xv_movm8(spu__xv_cmpgtu8(A, _mm_set1_epi8((char)imm)))); }
#else
static inline u128 spu_clgtbi(u128 a, int32_t imm) { return spu_clgtbi_ref(a, imm); }
#endif
#if defined(SPU__XV)
static inline u128 spu_ceqhi(u128 a, int32_t imm) { const __m128i A = spu__xv_ld(a); return spu__xv_st(_mm_cmpeq_epi16(A, _mm_set1_epi16((short)imm))); }
#else
static inline u128 spu_ceqhi(u128 a, int32_t imm) { return spu_ceqhi_ref(a, imm); }
#endif
#if defined(SPU__XV)
static inline u128 spu_cgthi(u128 a, int32_t imm) { const __m128i A = spu__xv_ld(a); return spu__xv_st(_mm_cmpgt_epi16(A, _mm_set1_epi16((short)imm))); }
#else
static inline u128 spu_cgthi(u128 a, int32_t imm) { return spu_cgthi_ref(a, imm); }
#endif
#if defined(SPU__XV)
static inline u128 spu_clgthi(u128 a, int32_t imm) { const __m128i A = spu__xv_ld(a); return spu__xv_st(spu__xv_movm16(spu__xv_cmpgtu16(A, _mm_set1_epi16((short)imm)))); }
#else
static inline u128 spu_clgthi(u128 a, int32_t imm) { return spu_clgthi_ref(a, imm); }
#endif
#if defined(SPU__XV)
static inline u128 spu_ahi(u128 a, int32_t imm) { const __m128i A = spu__xv_ld(a); return spu__xv_st(_mm_add_epi16(A, _mm_set1_epi16((short)imm))); }
#else
static inline u128 spu_ahi(u128 a, int32_t imm) { return spu_ahi_ref(a, imm); }
#endif
#if defined(SPU__XV)
static inline u128 spu_sfhi(u128 a, int32_t imm) { const __m128i A = spu__xv_ld(a); return spu__xv_st(_mm_sub_epi16(_mm_set1_epi16((short)imm), A)); }
#else
static inline u128 spu_sfhi(u128 a, int32_t imm) { return spu_sfhi_ref(a, imm); }
#endif
#ifdef SPU__XM
static inline u128 spu_mpyi(u128 a, int32_t imm)  { return spu__xi_st(_mm_madd_epi16(spu__xi_ld(a), _mm_set1_epi32((int32_t)(uint16_t)imm))); }
static inline u128 spu_mpyui(u128 a, int32_t imm) { return spu__xi_st(_mm_mullo_epi32(spu__xm_lo16(spu__xi_ld(a)), _mm_set1_epi32((int32_t)(uint16_t)imm))); }
#else
static inline u128 spu_mpyi(u128 a, int32_t imm) { return spu_mpyi_ref(a, imm); }
static inline u128 spu_mpyui(u128 a, int32_t imm) { return spu_mpyui_ref(a, imm); }
#endif
#if defined(SPU__XV)
static inline u128 spu_rothi(u128 a, int sh) { const __m128i A = spu__xv_ld(a); return spu__xv_st(_mm_or_si128(_mm_sll_epi16(A, _mm_cvtsi32_si128(sh & 15)), _mm_srl_epi16(A, _mm_cvtsi32_si128(16 - (sh & 15))))); }
#else
static inline u128 spu_rothi(u128 a, int sh) { return spu_rothi_ref(a, sh); }
#endif
#if defined(SPU__XV)
static inline u128 spu_roti(u128 a, int sh) { const __m128i A = spu__xv_ld(a); return spu__xv_st(spu__xv_rolv32(A, _mm_set1_epi32(sh & 31))); }
#else
static inline u128 spu_roti(u128 a, int sh) { return spu_roti_ref(a, sh); }
#endif
#if defined(SPU__XV)
static inline u128 spu_rotmahi(u128 a, int i7) { const __m128i A = spu__xv_ld(a); return spu__xv_st(_mm_sra_epi16(A, _mm_cvtsi32_si128((0 - i7) & 0x1F))); }
#else
static inline u128 spu_rotmahi(u128 a, int i7) { return spu_rotmahi_ref(a, i7); }
#endif
#if defined(SPU__XV)
static inline u128 spu_rotmai(u128 a, int i7) { const __m128i A = spu__xv_ld(a); return spu__xv_st(_mm_sra_epi32(A, _mm_cvtsi32_si128((0 - i7) & 0x3F))); }
#else
static inline u128 spu_rotmai(u128 a, int i7) { return spu_rotmai_ref(a, i7); }
#endif
#ifdef SPU__XM
static inline u128 spu_mpya(u128 a, u128 b, u128 c) { return spu__xi_st(_mm_add_epi32(_mm_madd_epi16(spu__xi_ld(a), spu__xm_lo16(spu__xi_ld(b))), spu__xi_ld(c))); }
#else
static inline u128 spu_mpya(u128 a, u128 b, u128 c) { return spu_mpya_ref(a, b, c); }
#endif
static inline u128 spu_mpyhha(u128 a, u128 b, u128 t) { return spu_mpyhha_ref(a, b, t); }
static inline u128 spu_mpyhhau(u128 a, u128 b, u128 t) { return spu_mpyhhau_ref(a, b, t); }
#if defined(SPU__XV)
static inline u128 spu_addx(u128 a, u128 b, u128 t) { const __m128i A = spu__xv_ld(a); const __m128i B = spu__xv_ld(b); const __m128i T = spu__xv_ld(t); return spu__xv_st(_mm_add_epi32(_mm_add_epi32(A, B), _mm_and_si128(T, _mm_set1_epi32(1)))); }
#else
static inline u128 spu_addx(u128 a, u128 b, u128 t) { return spu_addx_ref(a, b, t); }
#endif
#if defined(SPU__XV)
static inline u128 spu_sfx(u128 a, u128 b, u128 t) { const __m128i A = spu__xv_ld(a); const __m128i B = spu__xv_ld(b); const __m128i T = spu__xv_ld(t); return spu__xv_st(_mm_add_epi32(_mm_add_epi32(B, _mm_xor_si128(A, _mm_set1_epi32(-1))), _mm_and_si128(T, _mm_set1_epi32(1)))); }
#else
static inline u128 spu_sfx(u128 a, u128 b, u128 t) { return spu_sfx_ref(a, b, t); }
#endif
static inline u128 spu_cgx(u128 a, u128 b, u128 t) { return spu_cgx_ref(a, b, t); }
static inline u128 spu_bgx(u128 a, u128 b, u128 t) { return spu_bgx_ref(a, b, t); }
#if defined(SPU__XI) && !defined(SPU_SCALAR_HELPERS) && !defined(SPU_SCALAR_INT)
static inline u128 spu_cntb(u128 a) { return spu__xi_cntb(a); }
#else
static inline u128 spu_cntb(u128 a) { return spu_cntb_ref(a); }
#endif
#ifdef SPU__XM
static inline u128 spu_xsbh(u128 a) { return spu__xi_st(_mm_srai_epi16(_mm_slli_epi16(spu__xi_ld(a), 8), 8)); }
static inline u128 spu_xshw(u128 a) { return spu__xi_st(_mm_srai_epi32(_mm_slli_epi32(spu__xi_ld(a), 16), 16)); }
#else
static inline u128 spu_xsbh(u128 a) { return spu_xsbh_ref(a); }
static inline u128 spu_xshw(u128 a) { return spu_xshw_ref(a); }
#endif
static inline u128 spu_clz(u128 a) { return spu_clz_ref(a); }
#ifdef SPU__XM
/* the odd (low, big-endian) word of each doubleword, its sign fill in the even one */
static inline u128 spu_xswd(u128 a) {
    const __m128i v = spu__xi_ld(a);
    return spu__xi_st(_mm_blend_epi16(_mm_shuffle_epi32(_mm_srai_epi32(v, 31), _MM_SHUFFLE(3, 3, 1, 1)), v, 0xCC));
}
#else
static inline u128 spu_xswd(u128 a) { return spu_xswd_ref(a); }
#endif
static inline u128 spu_orx(u128 a) { return spu_orx_ref(a); }
#ifdef SPU__XM
/* bit 0 of word 0..3 -> bits 3..0 of the preferred word */
static inline u128 spu_gb(u128 a) {
    const uint32_t m = (uint32_t)_mm_movemask_ps(_mm_castsi128_ps(_mm_slli_epi32(spu__xi_ld(a), 31)));
    const uint32_t v = ((m & 1u) << 3) | ((m & 2u) << 1) | ((m & 4u) >> 1) | ((m & 8u) >> 3);
    return spu__xi_st(_mm_cvtsi32_si128((int)v));
}
#else
static inline u128 spu_gb(u128 a) { return spu_gb_ref(a); }
#endif
static inline u128 spu_gbh(u128 a) { return spu_gbh_ref(a); }
#if defined(SPU__XI) && !defined(SPU_SCALAR_HELPERS) && !defined(SPU_SCALAR_INT)
static inline u128 spu_gbb(u128 a) { return spu__xi_gbb(a); }
#else
static inline u128 spu_gbb(u128 a) { return spu_gbb_ref(a); }
#endif
#ifdef SPU__XM
/* bit 3-i of the preferred word -> word i all ones */
static inline u128 spu_fsm(u128 a) {
    const __m128i m = _mm_setr_epi32(8, 4, 2, 1);
    return spu__xi_st(_mm_cmpeq_epi32(_mm_and_si128(_mm_shuffle_epi32(spu__xi_ld(a), 0), m), m));
}
/* bit 7-h of the preferred word -> SPU halfword h, which is host halfword h^1 */
static inline u128 spu_fsmh(u128 a) {
    const __m128i m = _mm_setr_epi16(0x40, 0x80, 0x10, 0x20, 0x04, 0x08, 0x01, 0x02);
    return spu__xi_st(_mm_cmpeq_epi16(_mm_and_si128(_mm_set1_epi16((short)a._u32[0]), m), m));
}
#else
static inline u128 spu_fsm(u128 a) { return spu_fsm_ref(a); }
static inline u128 spu_fsmh(u128 a) { return spu_fsmh_ref(a); }
#endif
static inline u128 spu_fsmb(u128 a) { return spu_fsmb_ref(a); }
#endif

/* GOWA: formas cruas (scripts/flush_preguicoso.py) fora do caminho AVX-512 */
#if !defined(SPU__ZX) && !defined(spu_fa_r)
#define spu_fa_r spu_fa
#define spu_fs_r spu_fs
#define spu_fm_r spu_fm
#define spu_fma_r spu_fma
#define spu_fms_r spu_fms
#define spu_fnms_r spu_fnms
#endif

#endif /* SPU_HELPERS_H */
