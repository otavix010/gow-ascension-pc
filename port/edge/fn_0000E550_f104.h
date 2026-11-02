/* GOWA (02/11, EDGE nativo, marco E4): o laço F104 da E550 do job 0x72DF80 -- os dois fluxos de vetores (normal e
 * tangente, r66 = 2) transformados pela matriz 3x3 de cada vértice que o EC08 acabou de montar (linhas R0, R1, R2 em
 * 48 bytes por vértice; grupo 0 em r39, os seguintes a partir de r73 = r39 + 192), 4 vértices por volta:
 *     v' = fma(v.zxyw, R2, fma(v.yzxw, R1, fm(v, R0)))      (cada conta arredondando para zero, como o original)
 * fluxo A em r62 (lido uma volta à frente, gravado no lugar), fluxo B em r65 (lido e gravado na mesma volta),
 * r21 -= 4 por volta até 0. Na saída (loc_0000F3FC) só r4, r80, r83, r62 e r65 dos registradores do laço são lidos
 * (scripts_edge/vivos.py --abi): r4 = fm(A0, R0 do vértice 0) e r83 = fma(A1.yzxw, R1, fm(A1, R0)) do vértice 1 do
 * último grupo, r80 = B3.yzxw do último grupo (valores lidos antes das gravações), r62/r65 += 64 por volta.
 * Contas na build portátil (AVX2, MXCSR em RZ e DAZ): 2 vértices por registro; um grupo com valor fora do alcance do
 * float do PC (|r| >= 0x7F7FFFFF) é refeito pelas primitivas do runtime. GOWA_EDGE_NATIVO=0 desliga. Gancho 'multi'
 * antes do vk do laço, saltando para loc_0000F3FC. */
#ifndef GOWA_FN_0000E550_F104_H
#define GOWA_FN_0000E550_F104_H
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

static inline int f104_igual(u128 a, uint32_t w0, uint32_t w1, uint32_t w2, uint32_t w3)
{
    return a._u32[0] == w0 && a._u32[1] == w1 && a._u32[2] == w2 && a._u32[3] == w3;
}
/* um vetor pelas primitivas do runtime, como o original (r36 = yzxw, r82 = zxyw, r81 = 0x00010203...) */
static inline u128 f104_exato(u128 v, u128 r0, u128 r1, u128 r2, u128 k81, u128 k36, u128 k82)
{
    return spu_fma(spu_shufb(v, k81, k82), r2, spu_fma(spu_shufb(v, k81, k36), r1, spu_fm(v, r0)));
}

#if defined(SPU_SX_RZ) && defined(SPU_SX_DAZ) && defined(__AVX2__) && defined(__FMA__)
#define F104_VETOR 1
static inline __m256i f104_bswap8(__m256i v)
{
    return _mm256_shuffle_epi8(v, _mm256_setr_epi8(3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12,
                                                   3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12));
}
static inline __m256 f104_ld2(const uint8_t* a, const uint8_t* b)
{
    return _mm256_castsi256_ps(f104_bswap8(_mm256_inserti128_si256(_mm256_castsi128_si256(_mm_loadu_si128((const __m128i*)a)),
                                                                   _mm_loadu_si128((const __m128i*)b), 1)));
}
/* 2 vértices (32 bytes em v), matrizes em m (48 bytes cada, seguidas); saída em o; 0 = fora do alcance (nada gravado) */
static inline int f104_dois(const uint8_t* v, const uint8_t* m, uint8_t* o)
{
    const __m256 x = _mm256_castsi256_ps(f104_bswap8(_mm256_loadu_si256((const __m256i*)v)));
    const __m256 R0 = f104_ld2(m, m + 48), R1 = f104_ld2(m + 16, m + 64), R2 = f104_ld2(m + 32, m + 80);
    const __m256i mod = _mm256_set1_epi32(0x7FFFFFFF);
    __m256 t = _mm256_mul_ps(x, R0);
    __m256i mx = _mm256_and_si256(_mm256_castps_si256(t), mod);
    t = _mm256_fmadd_ps(_mm256_permute_ps(x, _MM_SHUFFLE(3, 0, 2, 1)), R1, t);
    mx = _mm256_max_epu32(mx, _mm256_and_si256(_mm256_castps_si256(t), mod));
    t = _mm256_fmadd_ps(_mm256_permute_ps(x, _MM_SHUFFLE(3, 1, 0, 2)), R2, t);
    mx = _mm256_max_epu32(mx, _mm256_and_si256(_mm256_castps_si256(t), mod));
    const __m256i ruim = _mm256_cmpgt_epi32(mx, _mm256_set1_epi32(0x7F7FFFFE));
    if (!_mm256_testz_si256(ruim, ruim)) return 0;
    const __m256i ti = _mm256_castps_si256(t);   /* expoente 0 -> +0 na saída */
    const __m256i z = _mm256_andnot_si256(_mm256_cmpeq_epi32(_mm256_and_si256(ti, _mm256_set1_epi32(0x7F800000)), _mm256_setzero_si256()), ti);
    _mm256_storeu_si256((__m256i*)o, f104_bswap8(z));
    return 1;
}
#else
#define F104_VETOR 0
#endif

static int fn_0000E550_f104_corpo(spu_context* ctx)
{
#if F104_VETOR
    if (!gowa_edge_nativo_on()) return 0;
    u128* g = ctx->gpr;
    if (!f104_igual(g[81], 0x00010203u, 0x00010203u, 0x00010203u, 0x00010203u) ||
        !f104_igual(g[36], 0x04050607u, 0x08090A0Bu, 0x00010203u, 0x0C0D0E0Fu) ||
        !f104_igual(g[82], 0x08090A0Bu, 0x00010203u, 0x04050607u, 0x0C0D0E0Fu)) return 0;
    const uint32_t cont = g[21]._u32[0];
    if (!cont || (cont & 3u) || cont > 0x4000u) return 0;
    const uint32_t N = cont / 4u;
    const uint32_t a0 = g[62]._u32[0], b0 = g[65]._u32[0], m0 = g[39]._u32[0];
    if (g[40]._u32[0] != m0 + 192u || ((a0 | b0 | m0) & 15u)) return 0;
    if (a0 + 64u * N > SPU_LS_SIZE || b0 + 64u * N > SPU_LS_SIZE || m0 + 192u * N > SPU_LS_SIZE) return 0;
    if ((a0 < b0 + 64u * N && b0 < a0 + 64u * N) || (a0 < m0 + 192u * N && m0 < a0 + 64u * N) ||
        (b0 < m0 + 192u * N && m0 < b0 + 64u * N)) return 0;
    /* o grupo 0 já está nos registradores: tem de ser o que a LS tem */
    static const int ra[4] = { 37, 35, 34, 38 };
    for (int k = 0; k < 4; k++) {
        const u128 x = spu_ls_read128(ctx, a0 + 16u * (uint32_t)k);
        if (memcmp(&x, &g[ra[k]], 16)) return 0;
    }
    uint8_t* ls = SPU_LS_BASE(ctx);
    const u128 k81 = g[81], k36 = g[36], k82 = g[82];
    u128 A0 = { 0 }, A1 = { 0 }, B3 = { 0 }, M0R0 = { 0 }, M1R0 = { 0 }, M1R1 = { 0 };
    for (uint32_t gr = 0; gr < N; gr++) {
        const uint32_t pa = a0 + 64u * gr, pb = b0 + 64u * gr, pm = m0 + 192u * gr;
        if (gr + 1u == N) {   /* os vivos da saída, com os valores de antes das gravações */
            A0 = spu_ls_read128(ctx, pa); A1 = spu_ls_read128(ctx, pa + 16u); B3 = spu_ls_read128(ctx, pb + 48u);
            M0R0 = spu_ls_read128(ctx, pm); M1R0 = spu_ls_read128(ctx, pm + 48u); M1R1 = spu_ls_read128(ctx, pm + 64u);
        }
        for (int fl = 0; fl < 2; fl++) {
            const uint32_t p = fl ? pb : pa;
            for (int h = 0; h < 2; h++) {
                uint8_t o[32];
                if (f104_dois(ls + p + 32u * (uint32_t)h, ls + pm + 96u * (uint32_t)h, o)) { memcpy(ls + p + 32u * (uint32_t)h, o, 32); continue; }
                for (int k = 0; k < 2; k++) {
                    const uint32_t pv = p + 32u * (uint32_t)h + 16u * (uint32_t)k, mv = pm + 96u * (uint32_t)h + 48u * (uint32_t)k;
                    const u128 r = f104_exato(spu_ls_read128(ctx, pv), spu_ls_read128(ctx, mv), spu_ls_read128(ctx, mv + 16u),
                                              spu_ls_read128(ctx, mv + 32u), k81, k36, k82);
                    spu_ls_write128(ctx, pv, r);
                }
            }
        }
    }
    g[4] = spu_fm(A0, M0R0);
    g[83] = spu_fma(spu_shufb(A1, k81, k36), M1R1, spu_fm(A1, M1R0));
    g[80] = spu_shufb(B3, k81, k36);
    for (int i = 0; i < 4; i++) { g[62]._u32[i] += 64u * N; g[65]._u32[i] += 64u * N; }
    return 1;
#else
    (void)ctx;
    return 0;
#endif
}

static __attribute__((noinline)) int fn_0000E550_f104_nativo(spu_context* ctx)
{
    static volatile long nn[2];
    const int r = fn_0000E550_f104_corpo(ctx);
    gowa_edge_conta("E550/F104", nn, r);
    return r;
}
#endif
