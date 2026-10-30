/* GOWA (30/10, EDGE nativo, marco E3): o laço D4DC da função 0xD250 do job 0x72DF80 (projeção dos vértices).
 *
 * Por vértice (x, y, z) de entrada (16 bytes, w ignorado), com a matriz composta do prólogo (linhas X, Y, Z, W):
 *     c   = fma(z, Cz, fma(y, Cy, fma_r(x, Cx, Cw)))        para c = X, Y, Z, W
 *     inv = fma(fnms_r(y0, W, 1), y0, y0), y0 = fi(W, frest(W))
 *     n   = fm(c, inv) xor sinal(W)                        para X, Y, Z
 *     sx  = fma(nx, r77, r76), sy = fma(ny, r73, r72), sz = fma(nz, r68, r67)   (viewport)
 *     código = (sx>r60)<<7 | (r62>sx)<<6 | (sy>r57)<<5 | (r61>sy)<<4 | (sz>r63)<<3 | (r64>sz)<<2 | (W>r58)<<1
 *     saída = (sx, sy, código, 0)
 * O original roda N = -r23/64 voltas de 4 vértices em software pipeline: o grupo j sai na volta j+2 (as voltas 0 e 1
 * gravam lixo no lugar do grupo 0, sobrescrito depois) e só N-2 grupos são gravados. Conferido pelo oráculo
 * (scripts_edge/d250_modelo.py: 4032/4032 vértices das fotos e 480 aleatórios para os bits). Contas em AVX-512 com
 * RZ embutido; frest/fi pela tabela e aritmética inteira do runtime. Um vértice com valor fora do alcance do float do
 * PC (|r| >= 0x7F7FFFFF) vai pelas primitivas do runtime. Temporários r2-r79 ficam diferentes (o epílogo só restaura
 * r80/r126 da pilha); GOWA_CONF_REGS_ABI=1. GOWA_EDGE_NATIVO=1 liga. Gancho: antes do vk do laço, saltando para
 * depois dele. */
#ifndef GOWA_FN_0000D250_H
#define GOWA_FN_0000D250_H
#include <stdlib.h>
#include <string.h>
#include <immintrin.h>

#ifndef GOWA_EDGE_NATIVO_ON_DEF
#define GOWA_EDGE_NATIVO_ON_DEF
static int gowa_edge_nativo_on(void)
{
    static int on = -1;
    if (on < 0) { const char* e = getenv("GOWA_EDGE_NATIVO"); on = e && *e == '1'; }
    return on;
}
#endif

typedef struct {
    uint32_t cx[4], cy[4], cz[4], cw[4];   /* por componente de saída X, Y, Z, W */
    uint32_t esc[3], des[3], max[3], min[3], wmin;
} d250_k;

static inline u128 d250_sp(uint32_t w) { u128 r; r._u32[0] = r._u32[1] = r._u32[2] = r._u32[3] = w; return r; }
/* um vértice pelas primitivas do runtime, na ordem do laço */
static inline void d250_exato(const d250_k* k, uint32_t x, uint32_t y, uint32_t z, uint32_t o[4])
{
    const u128 X = d250_sp(x), Y = d250_sp(y), Z = d250_sp(z);
    u128 c[4];
    for (int i = 0; i < 4; i++)
        c[i] = spu_fma(Z, d250_sp(k->cz[i]), spu_fma(Y, d250_sp(k->cy[i]), spu_fma_r(X, d250_sp(k->cx[i]), d250_sp(k->cw[i]))));
    const u128 w = c[3];
    const u128 y0 = spu_fi(w, spu_frest(w));
    const u128 inv = spu_fma(spu_fnms_r(y0, w, d250_sp(0x3F800000u)), y0, y0);
    const uint32_t sg = w._u32[0] & 0x80000000u;
    uint32_t s[3];
    for (int i = 0; i < 3; i++) {
        const u128 n = d250_sp(spu_fm(c[i], inv)._u32[0] ^ sg);
        s[i] = spu_fma(n, d250_sp(k->esc[i]), d250_sp(k->des[i]))._u32[0];
    }
    uint32_t cod = 0;
    for (int i = 0; i < 3; i++) {
        if (spu_fcgt(d250_sp(s[i]), d250_sp(k->max[i]))._u32[0]) cod |= 0x80u >> (2 * i);
        if (spu_fcgt(d250_sp(k->min[i]), d250_sp(s[i]))._u32[0]) cod |= 0x40u >> (2 * i);
    }
    if (spu_fcgt(w, d250_sp(k->wmin))._u32[0]) cod |= 2u;
    o[0] = s[0]; o[1] = s[1]; o[2] = cod; o[3] = 0;
}

static const uint32_t d250_frest_lut[32] = {   /* a do runtime (spu_frest_fraction_lut) */
    0x7FFBE0, 0x7F87A6, 0x70EF72, 0x708B40, 0x638B12, 0x633AEA, 0x5792C4, 0x574AA0,
    0x4CCA7E, 0x4C9262, 0x430A44, 0x42D62A, 0x3A2E12, 0x39FDFA, 0x3215E4, 0x31F1D2,
    0x2AA9BE, 0x2A85AC, 0x23D59A, 0x23BD8E, 0x1D8576, 0x1D8576, 0x17AD5A, 0x17AD5A,
    0x124543, 0x124543, 0x0D392D, 0x0D392D, 0x08851A, 0x08851A, 0x041D07, 0x041D07};

#if defined(__AVX512F__) && defined(__AVX512BW__) && defined(__AVX512VL__)
#define D250_VETOR 1
#define D250_RZ (_MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC)
static inline __m512 d250_z(__m512 x)
{
    const __m512i i = _mm512_castps_si512(x);
    return _mm512_castsi512_ps(_mm512_maskz_mov_epi32(_mm512_test_epi32_mask(i, _mm512_set1_epi32(0x7F800000)), i));
}
static inline __mmask16 d250_ruim(__m512 r)
{
    return _mm512_cmpge_epu32_mask(_mm512_and_si512(_mm512_castps_si512(r), _mm512_set1_epi32(0x7FFFFFFF)), _mm512_set1_epi32(0x7F7FFFFF));
}
static inline __m512i d250_bswap(__m512i v)
{
    return _mm512_shuffle_epi8(v, _mm512_broadcast_i32x4(_mm_setr_epi8(3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12)));
}
static inline __m512 d250_b4(const uint32_t* v) { return _mm512_castsi512_ps(_mm512_broadcast_i32x4(_mm_loadu_si128((const __m128i*)v))); }
/* frest + fi (inteiros, como o runtime) */
static inline __m512 d250_rec(__m512 w)
{
    const __m512i ui = _mm512_castps_si512(w);
    const __m512i e = _mm512_and_si512(_mm512_srli_epi32(ui, 23), _mm512_set1_epi32(0xFF));
    const __m512i idx = _mm512_and_si512(_mm512_srli_epi32(ui, 18), _mm512_set1_epi32(0x1F));
    const __m512i fr = _mm512_permutex2var_epi32(_mm512_loadu_si512(&d250_frest_lut[0]), idx, _mm512_loadu_si512(&d250_frest_lut[16]));
    __m512i rexp = _mm512_sub_epi32(_mm512_set1_epi32(253), e);
    rexp = _mm512_mask_mov_epi32(rexp, _mm512_cmpge_epi32_mask(e, _mm512_set1_epi32(253)), _mm512_setzero_si512());
    rexp = _mm512_mask_mov_epi32(rexp, _mm512_cmpeq_epi32_mask(e, _mm512_setzero_si512()), _mm512_set1_epi32(255));
    const __m512i est = _mm512_or_si512(_mm512_and_si512(ui, _mm512_set1_epi32((int)0x80000000u)),
                                        _mm512_or_si512(_mm512_slli_epi32(rexp, 23), _mm512_and_si512(fr, _mm512_set1_epi32(0x7FFFFF))));
    const __m512i fe = _mm512_and_si512(_mm512_srli_epi32(est, 23), _mm512_set1_epi32(0xFF));
    const __m512i base = _mm512_or_si512(_mm512_and_si512(_mm512_srli_epi32(est, 10), _mm512_set1_epi32(0x1FFF)), _mm512_set1_epi32(0x2000));
    const __m512i prod = _mm512_mullo_epi32(_mm512_and_si512(est, _mm512_set1_epi32(0x3FF)), _mm512_and_si512(ui, _mm512_set1_epi32(0x7FFFF)));
    const __m512i m8 = _mm512_sub_epi32(_mm512_slli_epi32(base, 11), _mm512_srli_epi32(_mm512_add_epi32(prod, _mm512_set1_epi32(255)), 8));
    const __mmask16 baixo = _mm512_cmplt_epi32_mask(m8, _mm512_set1_epi32(1 << 24));
    const __m512i frac = _mm512_and_si512(_mm512_mask_blend_epi32(baixo, _mm512_srli_epi32(m8, 1), m8), _mm512_set1_epi32(0x7FFFFF));
    const __m512i e2 = _mm512_mask_sub_epi32(fe, baixo, fe, _mm512_set1_epi32(1));
    __m512i fiv = _mm512_or_si512(_mm512_and_si512(est, _mm512_set1_epi32((int)0x80000000u)), _mm512_or_si512(_mm512_slli_epi32(e2, 23), frac));
    fiv = _mm512_maskz_mov_epi32(~(_mm512_cmpeq_epi32_mask(fe, _mm512_setzero_si512()) | _mm512_cmpeq_epi32_mask(e2, _mm512_setzero_si512())), fiv);
    return _mm512_castsi512_ps(fiv);
}
/* um grupo de 4 vértices (faixa = componente: X, Y, Z, W); devolve a máscara de vértices ruins (bits 0-3) */
static inline unsigned d250_grupo(const uint8_t* in, uint8_t* out, const d250_k* k,
                                  __m512 CX, __m512 CY, __m512 CZ, __m512 CW, __m512 ESC, __m512 DES, __m512 MAX, __m512 MIN)
{
    const __m512 v = d250_z(_mm512_castsi512_ps(d250_bswap(_mm512_loadu_si512(in))));
    const __m512 X = _mm512_permute_ps(v, _MM_SHUFFLE(0, 0, 0, 0)), Y = _mm512_permute_ps(v, _MM_SHUFFLE(1, 1, 1, 1)),
                 Z = _mm512_permute_ps(v, _MM_SHUFFLE(2, 2, 2, 2));
    __m512 c = _mm512_fmadd_round_ps(X, CX, CW, D250_RZ);
    __mmask16 ruim = d250_ruim(c);
    c = d250_z(_mm512_fmadd_round_ps(Y, CY, d250_z(c), D250_RZ)); ruim |= d250_ruim(c);
    c = d250_z(_mm512_fmadd_round_ps(Z, CZ, c, D250_RZ)); ruim |= d250_ruim(c);
    const __m512 W = _mm512_permute_ps(c, _MM_SHUFFLE(3, 3, 3, 3));
    const __m512 y0 = d250_z(d250_rec(W));
    ruim |= d250_ruim(y0);
    const __m512 t = d250_z(_mm512_fnmadd_round_ps(y0, W, _mm512_set1_ps(1.0f), D250_RZ));
    ruim |= d250_ruim(t);
    const __m512 inv = d250_z(_mm512_fmadd_round_ps(t, y0, y0, D250_RZ));
    ruim |= d250_ruim(inv);
    __m512 n = d250_z(_mm512_mul_round_ps(c, inv, D250_RZ));
    ruim |= d250_ruim(n) & 0x7777;
    n = _mm512_castsi512_ps(_mm512_xor_si512(_mm512_castps_si512(n), _mm512_and_si512(_mm512_castps_si512(W), _mm512_set1_epi32((int)0x80000000u))));
    const __m512 s = d250_z(_mm512_fmadd_round_ps(n, ESC, DES, D250_RZ));
    ruim |= d250_ruim(s) & 0x7777;
    /* SPU fcgt sobre valores no alcance e já sem denormais = a comparação comum */
    const __mmask16 gmax = _mm512_cmp_ps_mask(s, MAX, _CMP_GT_OQ), gmin = _mm512_cmp_ps_mask(MIN, s, _CMP_GT_OQ);
    const __mmask16 wpos = _mm512_cmp_ps_mask(W, _mm512_set1_ps(0.0f), _CMP_GT_OQ);   /* r58 = 0 (conferido no chamador) */
    unsigned maus = 0;
    for (int q = 0; q < 4; q++) if ((ruim >> (4 * q)) & 0xF) maus |= 1u << q;
    uint32_t r[16];
    _mm512_storeu_si512(r, _mm512_castps_si512(s));
    for (int q = 0; q < 4; q++) {
        const unsigned gx = (gmax >> (4 * q)) & 7u, nx = (gmin >> (4 * q)) & 7u;
        uint32_t cod = (wpos >> (4 * q + 3)) & 1u ? 2u : 0u;
        cod |= (gx & 1u) << 7 | (nx & 1u) << 6 | ((gx >> 1) & 1u) << 5 | ((nx >> 1) & 1u) << 4 | ((gx >> 2) & 1u) << 3 | ((nx >> 2) & 1u) << 2;
        const uint32_t o[4] = { __builtin_bswap32(r[4 * q]), __builtin_bswap32(r[4 * q + 1]), __builtin_bswap32(cod), 0 };
        if (!((maus >> q) & 1u)) memcpy(out + 16 * q, o, 16);
    }
    (void)k;
    return maus;
}
#elif defined(SPU_SX_RZ) && defined(__AVX2__) && defined(__FMA__)
#define D250_VETOR 2
/* build portátil: o MXCSR da thread da SPU arredonda para zero (SPU_SX_RZ); 2 vértices por registro */
static inline __m256 d250_z8(__m256 x)
{
    const __m256i i = _mm256_castps_si256(x);
    return _mm256_castsi256_ps(_mm256_andnot_si256(_mm256_cmpeq_epi32(_mm256_and_si256(i, _mm256_set1_epi32(0x7F800000)), _mm256_setzero_si256()), i));
}
static inline __m256i d250_ruim8(__m256 r)
{
    return _mm256_cmpgt_epi32(_mm256_and_si256(_mm256_castps_si256(r), _mm256_set1_epi32(0x7FFFFFFF)), _mm256_set1_epi32(0x7F7FFFFE));
}
static inline __m256i d250_bswap8(__m256i v)
{
    return _mm256_shuffle_epi8(v, _mm256_setr_epi8(3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12,
                                                   3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12));
}
static inline __m256 d250_b4_8(const uint32_t* v) { return _mm256_castsi256_ps(_mm256_broadcastsi128_si256(_mm_loadu_si128((const __m128i*)v))); }
static inline __m256 d250_rec8(__m256 w)
{
    const __m256i ui = _mm256_castps_si256(w);
    const __m256i e = _mm256_and_si256(_mm256_srli_epi32(ui, 23), _mm256_set1_epi32(0xFF));
    const __m256i idx = _mm256_and_si256(_mm256_srli_epi32(ui, 18), _mm256_set1_epi32(0x1F));
    /* a tabela de 32 em 4 registros (o gather é lento nos Ryzen): bit 3 e bit 4 do índice escolhem o registro */
    const __m256i t0 = _mm256_permutevar8x32_epi32(_mm256_loadu_si256((const __m256i*)&d250_frest_lut[0]), idx);
    const __m256i t1 = _mm256_permutevar8x32_epi32(_mm256_loadu_si256((const __m256i*)&d250_frest_lut[8]), idx);
    const __m256i t2 = _mm256_permutevar8x32_epi32(_mm256_loadu_si256((const __m256i*)&d250_frest_lut[16]), idx);
    const __m256i t3 = _mm256_permutevar8x32_epi32(_mm256_loadu_si256((const __m256i*)&d250_frest_lut[24]), idx);
    const __m256i b3 = _mm256_slli_epi32(idx, 28), b4 = _mm256_slli_epi32(idx, 27);   /* bit 3 e bit 4 no bit de sinal */
    const __m256i lo = _mm256_castps_si256(_mm256_blendv_ps(_mm256_castsi256_ps(t0), _mm256_castsi256_ps(t1), _mm256_castsi256_ps(b3)));
    const __m256i hi = _mm256_castps_si256(_mm256_blendv_ps(_mm256_castsi256_ps(t2), _mm256_castsi256_ps(t3), _mm256_castsi256_ps(b3)));
    const __m256i fr = _mm256_castps_si256(_mm256_blendv_ps(_mm256_castsi256_ps(lo), _mm256_castsi256_ps(hi), _mm256_castsi256_ps(b4)));
    __m256i rexp = _mm256_sub_epi32(_mm256_set1_epi32(253), e);
    rexp = _mm256_andnot_si256(_mm256_cmpgt_epi32(e, _mm256_set1_epi32(252)), rexp);
    rexp = _mm256_blendv_epi8(rexp, _mm256_set1_epi32(255), _mm256_cmpeq_epi32(e, _mm256_setzero_si256()));
    const __m256i est = _mm256_or_si256(_mm256_and_si256(ui, _mm256_set1_epi32((int)0x80000000u)),
                                        _mm256_or_si256(_mm256_slli_epi32(rexp, 23), _mm256_and_si256(fr, _mm256_set1_epi32(0x7FFFFF))));
    const __m256i fe = _mm256_and_si256(_mm256_srli_epi32(est, 23), _mm256_set1_epi32(0xFF));
    const __m256i base = _mm256_or_si256(_mm256_and_si256(_mm256_srli_epi32(est, 10), _mm256_set1_epi32(0x1FFF)), _mm256_set1_epi32(0x2000));
    const __m256i prod = _mm256_mullo_epi32(_mm256_and_si256(est, _mm256_set1_epi32(0x3FF)), _mm256_and_si256(ui, _mm256_set1_epi32(0x7FFFF)));
    const __m256i m8 = _mm256_sub_epi32(_mm256_slli_epi32(base, 11), _mm256_srli_epi32(_mm256_add_epi32(prod, _mm256_set1_epi32(255)), 8));
    const __m256i baixo = _mm256_cmpgt_epi32(_mm256_set1_epi32(1 << 24), m8);
    const __m256i frac = _mm256_and_si256(_mm256_blendv_epi8(_mm256_srli_epi32(m8, 1), m8, baixo), _mm256_set1_epi32(0x7FFFFF));
    const __m256i e2 = _mm256_add_epi32(fe, baixo);
    __m256i fiv = _mm256_or_si256(_mm256_and_si256(est, _mm256_set1_epi32((int)0x80000000u)), _mm256_or_si256(_mm256_slli_epi32(e2, 23), frac));
    fiv = _mm256_andnot_si256(_mm256_or_si256(_mm256_cmpeq_epi32(fe, _mm256_setzero_si256()), _mm256_cmpeq_epi32(e2, _mm256_setzero_si256())), fiv);
    return _mm256_castsi256_ps(fiv);
}
/* 2 vértices; devolve a máscara de vértices ruins (bits 0-1) e grava os bons */
static inline unsigned d250_dois(const uint8_t* in, uint8_t* out, __m256 CX, __m256 CY, __m256 CZ, __m256 CW,
                                 __m256 ESC, __m256 DES, __m256 MAX, __m256 MIN)
{
    const __m256 v = d250_z8(_mm256_castsi256_ps(d250_bswap8(_mm256_loadu_si256((const __m256i*)in))));
    const __m256 X = _mm256_permute_ps(v, _MM_SHUFFLE(0, 0, 0, 0)), Y = _mm256_permute_ps(v, _MM_SHUFFLE(1, 1, 1, 1)),
                 Z = _mm256_permute_ps(v, _MM_SHUFFLE(2, 2, 2, 2));
    __m256 c = _mm256_fmadd_ps(X, CX, CW);
    __m256i ruim = d250_ruim8(c);
    c = d250_z8(_mm256_fmadd_ps(Y, CY, d250_z8(c))); ruim = _mm256_or_si256(ruim, d250_ruim8(c));
    c = d250_z8(_mm256_fmadd_ps(Z, CZ, c)); ruim = _mm256_or_si256(ruim, d250_ruim8(c));
    const __m256 W = _mm256_permute_ps(c, _MM_SHUFFLE(3, 3, 3, 3));
    const __m256 y0 = d250_z8(d250_rec8(W));
    ruim = _mm256_or_si256(ruim, d250_ruim8(y0));
    const __m256 t = d250_z8(_mm256_fnmadd_ps(y0, W, _mm256_set1_ps(1.0f)));
    ruim = _mm256_or_si256(ruim, d250_ruim8(t));
    const __m256 inv = d250_z8(_mm256_fmadd_ps(t, y0, y0));
    ruim = _mm256_or_si256(ruim, d250_ruim8(inv));
    const __m256i m3 = _mm256_setr_epi32(-1, -1, -1, 0, -1, -1, -1, 0);
    __m256 n = d250_z8(_mm256_mul_ps(c, inv));
    ruim = _mm256_or_si256(ruim, _mm256_and_si256(d250_ruim8(n), m3));
    n = _mm256_castsi256_ps(_mm256_xor_si256(_mm256_castps_si256(n), _mm256_and_si256(_mm256_castps_si256(W), _mm256_set1_epi32((int)0x80000000u))));
    const __m256 s = d250_z8(_mm256_fmadd_ps(n, ESC, DES));
    ruim = _mm256_or_si256(ruim, _mm256_and_si256(d250_ruim8(s), m3));
    const unsigned gmax = (unsigned)_mm256_movemask_ps(_mm256_cmp_ps(s, MAX, _CMP_GT_OQ));
    const unsigned gmin = (unsigned)_mm256_movemask_ps(_mm256_cmp_ps(MIN, s, _CMP_GT_OQ));
    const unsigned wpos = (unsigned)_mm256_movemask_ps(_mm256_cmp_ps(W, _mm256_setzero_ps(), _CMP_GT_OQ));
    const unsigned rm = (unsigned)_mm256_movemask_ps(_mm256_castsi256_ps(ruim));
    const unsigned maus = ((rm & 0x0Fu) ? 1u : 0u) | ((rm & 0xF0u) ? 2u : 0u);
    uint32_t r[8];
    _mm256_storeu_si256((__m256i*)r, _mm256_castps_si256(s));
    for (int q = 0; q < 2; q++) {
        if ((maus >> q) & 1u) continue;
        const unsigned gx = (gmax >> (4 * q)) & 7u, nx = (gmin >> (4 * q)) & 7u;
        uint32_t cod = ((wpos >> (4 * q + 3)) & 1u) ? 2u : 0u;
        cod |= (gx & 1u) << 7 | (nx & 1u) << 6 | ((gx >> 1) & 1u) << 5 | ((nx >> 1) & 1u) << 4 | ((gx >> 2) & 1u) << 3 | ((nx >> 2) & 1u) << 2;
        const uint32_t o[4] = { __builtin_bswap32(r[4 * q]), __builtin_bswap32(r[4 * q + 1]), __builtin_bswap32(cod), 0 };
        memcpy(out + 16 * q, o, 16);
    }
    return maus;
}
/* 8 vértices (2 grupos) em SoA, como o original faz com 4: cada faixa é um vértice, na ordem v0 v2 v4 v6 | v1 v3 v5 v7
 * (a da transposição). Grava os 8 e devolve a máscara (bit = vértice) dos que precisam do caminho exato. */
static inline unsigned d250_oito(const uint8_t* in, uint8_t* out, const d250_k* k)
{
    const __m256 A = d250_z8(_mm256_castsi256_ps(d250_bswap8(_mm256_loadu_si256((const __m256i*)(in)))));
    const __m256 B = d250_z8(_mm256_castsi256_ps(d250_bswap8(_mm256_loadu_si256((const __m256i*)(in + 32)))));
    const __m256 C = d250_z8(_mm256_castsi256_ps(d250_bswap8(_mm256_loadu_si256((const __m256i*)(in + 64)))));
    const __m256 D = d250_z8(_mm256_castsi256_ps(d250_bswap8(_mm256_loadu_si256((const __m256i*)(in + 96)))));
    const __m256 t0 = _mm256_unpacklo_ps(A, B), t1 = _mm256_unpackhi_ps(A, B), t2 = _mm256_unpacklo_ps(C, D), t3 = _mm256_unpackhi_ps(C, D);
    const __m256 X = _mm256_shuffle_ps(t0, t2, _MM_SHUFFLE(1, 0, 1, 0)), Y = _mm256_shuffle_ps(t0, t2, _MM_SHUFFLE(3, 2, 3, 2));
    const __m256 Z = _mm256_shuffle_ps(t1, t3, _MM_SHUFFLE(1, 0, 1, 0));
#define D250_S(w) d250_z8(_mm256_castsi256_ps(_mm256_set1_epi32((int)(w))))
    __m256i ruim = _mm256_setzero_si256();
    __m256 c[4];
    for (int i = 0; i < 4; i++) {
        __m256 v = _mm256_fmadd_ps(X, D250_S(k->cx[i]), D250_S(k->cw[i]));
        ruim = _mm256_or_si256(ruim, d250_ruim8(v));
        v = d250_z8(_mm256_fmadd_ps(Y, D250_S(k->cy[i]), d250_z8(v))); ruim = _mm256_or_si256(ruim, d250_ruim8(v));
        v = d250_z8(_mm256_fmadd_ps(Z, D250_S(k->cz[i]), v)); ruim = _mm256_or_si256(ruim, d250_ruim8(v));
        c[i] = v;
    }
    const __m256 W = c[3];
    const __m256 y0 = d250_z8(d250_rec8(W));
    ruim = _mm256_or_si256(ruim, d250_ruim8(y0));
    const __m256 t = d250_z8(_mm256_fnmadd_ps(y0, W, _mm256_set1_ps(1.0f)));
    ruim = _mm256_or_si256(ruim, d250_ruim8(t));
    const __m256 inv = d250_z8(_mm256_fmadd_ps(t, y0, y0));
    ruim = _mm256_or_si256(ruim, d250_ruim8(inv));
    const __m256 sg = _mm256_castsi256_ps(_mm256_and_si256(_mm256_castps_si256(W), _mm256_set1_epi32((int)0x80000000u)));
    __m256 s[3];
    __m256i cod = _mm256_and_si256(_mm256_castps_si256(_mm256_cmp_ps(W, _mm256_setzero_ps(), _CMP_GT_OQ)), _mm256_set1_epi32(2));
    for (int i = 0; i < 3; i++) {
        __m256 n = d250_z8(_mm256_mul_ps(c[i], inv));
        ruim = _mm256_or_si256(ruim, d250_ruim8(n));
        n = _mm256_xor_ps(n, sg);
        s[i] = d250_z8(_mm256_fmadd_ps(n, D250_S(k->esc[i]), D250_S(k->des[i])));
        ruim = _mm256_or_si256(ruim, d250_ruim8(s[i]));
        const __m256i gx = _mm256_castps_si256(_mm256_cmp_ps(s[i], D250_S(k->max[i]), _CMP_GT_OQ));
        const __m256i nx = _mm256_castps_si256(_mm256_cmp_ps(D250_S(k->min[i]), s[i], _CMP_GT_OQ));
        cod = _mm256_or_si256(cod, _mm256_and_si256(gx, _mm256_set1_epi32((int)(0x80u >> (2 * i)))));
        cod = _mm256_or_si256(cod, _mm256_and_si256(nx, _mm256_set1_epi32((int)(0x40u >> (2 * i)))));
    }
#undef D250_S
    /* de volta a um vértice por 16 bytes: (sx, sy, código, 0) */
    const __m256 C4 = _mm256_castsi256_ps(cod), Z0 = _mm256_setzero_ps();
    const __m256 u0 = _mm256_unpacklo_ps(s[0], s[1]), u1 = _mm256_unpackhi_ps(s[0], s[1]);
    const __m256 u2 = _mm256_unpacklo_ps(C4, Z0), u3 = _mm256_unpackhi_ps(C4, Z0);
    _mm256_storeu_si256((__m256i*)(out), d250_bswap8(_mm256_castps_si256(_mm256_shuffle_ps(u0, u2, _MM_SHUFFLE(1, 0, 1, 0)))));
    _mm256_storeu_si256((__m256i*)(out + 32), d250_bswap8(_mm256_castps_si256(_mm256_shuffle_ps(u0, u2, _MM_SHUFFLE(3, 2, 3, 2)))));
    _mm256_storeu_si256((__m256i*)(out + 64), d250_bswap8(_mm256_castps_si256(_mm256_shuffle_ps(u1, u3, _MM_SHUFFLE(1, 0, 1, 0)))));
    _mm256_storeu_si256((__m256i*)(out + 96), d250_bswap8(_mm256_castps_si256(_mm256_shuffle_ps(u1, u3, _MM_SHUFFLE(3, 2, 3, 2)))));
    const unsigned rm = (unsigned)_mm256_movemask_ps(_mm256_castsi256_ps(ruim));
    /* faixa -> vértice: v0 v2 v4 v6 | v1 v3 v5 v7 */
    static const unsigned char vert[8] = { 0, 2, 4, 6, 1, 3, 5, 7 };
    unsigned maus = 0;
    for (int f = 0; f < 8; f++) if ((rm >> f) & 1u) maus |= 1u << vert[f];
    return maus;
}
#else
#define D250_VETOR 0
#endif

/* GOWA_D250_STATS=1: chamadas e vértices pelo caminho exato */
static __attribute__((noinline)) int fn_0000D250_nativo(spu_context* ctx)
{
    if (!D250_VETOR || !gowa_edge_nativo_on()) return 0;   /* sem AVX-512 fica o vk */
    u128* r = ctx->gpr;
    const uint32_t off = r[23]._u32[0];
    if (off == 0u || (off & 63u) || (0u - off) > 0x20000u || r[21]._u32[0] != 0u || r[58]._u32[0] != 0u) return 0;
    const uint32_t n = (0u - off) / 64u;
    if (n < 3u) return 0;
    const uint32_t in = r[39]._u32[0] + off, out = r[66]._u32[0], g = n - 2u;
    if (r[37]._u32[0] != out + 16u || r[35]._u32[0] != out + 32u || r[34]._u32[0] != out + 48u) return 0;
    if (r[33]._u32[0] != r[39]._u32[0] + 16u || r[32]._u32[0] != r[39]._u32[0] + 32u || r[29]._u32[0] != r[39]._u32[0] + 48u) return 0;
    if ((in | out) & 15u || in + 64u * n > SPU_LS_SIZE || out + 64u * g > SPU_LS_SIZE) return 0;
    if (out < in + 64u * n && in < out + 64u * g) return 0;   /* o original lê 2 grupos à frente do que grava */
    d250_k k;
    const int lx[4] = { 48, 47, 46, 55 }, ly[4] = { 45, 44, 43, 54 }, lz[4] = { 42, 41, 40, 53 }, lw[4] = { 52, 51, 49, 56 };
    for (int i = 0; i < 4; i++) { k.cx[i] = r[lx[i]]._u32[0]; k.cy[i] = r[ly[i]]._u32[0]; k.cz[i] = r[lz[i]]._u32[0]; k.cw[i] = r[lw[i]]._u32[0]; }
    k.esc[0] = r[77]._u32[0]; k.des[0] = r[76]._u32[0]; k.esc[1] = r[73]._u32[0]; k.des[1] = r[72]._u32[0];
    k.esc[2] = r[68]._u32[0]; k.des[2] = r[67]._u32[0];
    k.max[0] = r[60]._u32[0]; k.min[0] = r[62]._u32[0]; k.max[1] = r[57]._u32[0]; k.min[1] = r[61]._u32[0];
    k.max[2] = r[63]._u32[0]; k.min[2] = r[64]._u32[0]; k.wmin = r[58]._u32[0];
    /* constantes no alcance (as contas e comparações vetoriais supõem isso) */
    const uint32_t* todas = &k.cx[0];
    for (int i = 0; i < 16 + 12; i++) if (((todas[i] >> 23) & 0xFFu) == 0xFFu) return 0;
    uint8_t* ls = SPU_LS_BASE(ctx);
    long refeitos = 0;
#if D250_VETOR
    uint32_t e4[4], d4[4], mx[4], mn[4];
    for (int i = 0; i < 3; i++) { e4[i] = k.esc[i]; d4[i] = k.des[i]; mx[i] = k.max[i]; mn[i] = k.min[i]; }
    e4[3] = d4[3] = 0; mx[3] = 0x7F7FFFFFu; mn[3] = 0xFF7FFFFFu;   /* faixa W: sem comparação útil */
#if D250_VETOR == 1
    const __m512 CX = d250_z(d250_b4(k.cx)), CY = d250_z(d250_b4(k.cy)), CZ = d250_z(d250_b4(k.cz)), CW = d250_z(d250_b4(k.cw));
    const __m512 ESC = d250_z(d250_b4(e4)), DES = d250_z(d250_b4(d4)), MAX = d250_z(d250_b4(mx)), MIN = d250_z(d250_b4(mn));
#else
    const __m256 CX = d250_z8(d250_b4_8(k.cx)), CY = d250_z8(d250_b4_8(k.cy)), CZ = d250_z8(d250_b4_8(k.cz)), CW = d250_z8(d250_b4_8(k.cw));
    const __m256 ESC = d250_z8(d250_b4_8(e4)), DES = d250_z8(d250_b4_8(d4)), MAX = d250_z8(d250_b4_8(mx)), MIN = d250_z8(d250_b4_8(mn));
#endif
    for (uint32_t j = 0; j < g; j++) {
#if D250_VETOR == 1
        const unsigned maus = d250_grupo(ls + in + 64u * j, ls + out + 64u * j, &k, CX, CY, CZ, CW, ESC, DES, MAX, MIN);
        const int nq = 4;
#else
        unsigned maus;
        int nq = 4;
        if (j + 1u < g) { maus = d250_oito(ls + in + 64u * j, ls + out + 64u * j, &k); nq = 8; }
        else maus = d250_dois(ls + in + 64u * j, ls + out + 64u * j, CX, CY, CZ, CW, ESC, DES, MAX, MIN) |
                    d250_dois(ls + in + 64u * j + 32u, ls + out + 64u * j + 32u, CX, CY, CZ, CW, ESC, DES, MAX, MIN) << 2;
#endif
        for (int q = 0; q < nq; q++) {
            if (!((maus >> q) & 1u)) continue;
            const uint8_t* p = ls + in + 64u * j + 16u * (uint32_t)q;
            uint32_t w3[3], o[4];
            for (int c = 0; c < 3; c++) { memcpy(&w3[c], p + 4 * c, 4); w3[c] = __builtin_bswap32(w3[c]); }
            d250_exato(&k, w3[0], w3[1], w3[2], o);
            for (int c = 0; c < 4; c++) o[c] = __builtin_bswap32(o[c]);
            memcpy(ls + out + 64u * j + 16u * (uint32_t)q, o, 16);
            refeitos++;
        }
        if (nq == 8) j++;   /* o SoA fez dois grupos */
    }
#endif
    static int est = -1; static volatile long nch, nref;
    if (est < 0) est = getenv("GOWA_D250_STATS") ? 1 : 0;
    if (est) { const long c = ++nch; nref += refeitos; if ((c & (c - 1)) == 0) fprintf(stderr, "[d250] %ld chamadas, %ld vértices refeitos\n", c, nref); }
    return 1;
}
#endif
