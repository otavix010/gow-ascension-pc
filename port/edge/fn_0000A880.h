/* GOWA (30/10, EDGE nativo, marco E3): o laço A950 da função 0xA880 do job 0x72DF80 escrito para o PC.
 *
 * Normais a partir de dois fluxos de vetores (a em r81, b em r82; 8 por volta, sem software pipeline entre voltas):
 *     c  = b * a.yzxx - a * b.yzxx            (fm_r, fnms)          -> n = (c.y, c.z, c.x, 0) * b.w   (fm)
 *     d  = (n.x² + n.y²) + n.z²               (fm, fa_r, fa)
 *     se d > 0x0DA2425F: y = fi(d, frsqest(d)); y += (1 - d*y*y) * (y * 0,5)   (fm_r, fnms, fm_r, fma) -> n * y (fm)
 *     senão: n                                                   (fcgt, selb); w = 0 na saída
 * Contas: AVX-512 (RZ embutido, 4 vetores por registro) ou, na build portátil, AVX2 com o MXCSR em RZ (SPU_SX_RZ);
 * frsqest/fi pela mesma tabela e aritmética inteira do runtime (como no plugin 0x752710). Um bloco com algum valor
 * fora do alcance do float do PC (|r| >= 0x7F7FFFFF; o host saturou ou havia o expoente 255, que na SPU é número)
 * é refeito pelas primitivas do runtime na ordem do original. Temporários r2-r79 (e r80, que o epílogo restaura)
 * ficam diferentes; GOWA_CONF_REGS_ABI=1. GOWA_EDGE_NATIVO=1 liga. Gancho: antes do vk do laço, saltando para depois
 * dele (scripts_edge/edge_nativo.py). */
#ifndef GOWA_FN_0000A880_H
#define GOWA_FN_0000A880_H
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

static inline u128 a880_k(uint32_t a, uint32_t b, uint32_t c, uint32_t d) { u128 r; r._u32[0] = a; r._u32[1] = b; r._u32[2] = c; r._u32[3] = d; return r; }

/* um vetor pelas primitivas do runtime, como o original; k = posição na volta (as formas cruas variam com ela) */
static inline u128 a880_exato(u128 a, u128 b, int k)
{
    const u128 c47 = a880_k(0x04050607u, 0x08090A0Bu, 0x00010203u, 0x00010203u);
    const u128 c39 = a880_k(0x00010203u, 0x00010203u, 0x00010203u, 0x00010203u);
    const u128 c67 = a880_k(0x04050607u, 0x08090A0Bu, 0x0C0D0E0Fu, 0x10111213u);
    const u128 c38 = a880_k(0x08090A0Bu, 0x0C0D0E0Fu, 0x10111213u, 0x14151617u);
    const u128 m18 = a880_k(0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0);
    const u128 eps = a880_k(0x0DA2425Fu, 0x0DA2425Fu, 0x0DA2425Fu, 0x0DA2425Fu);
    const u128 um = a880_k(0x3F800000u, 0x3F800000u, 0x3F800000u, 0x3F800000u);
    const u128 meio = a880_k(0x3F000000u, 0x3F000000u, 0x3F000000u, 0x3F000000u);
    static const int fnms_cru[8] = { 1, 0, 1, 1, 0, 1, 0, 0 }, h_cru[8] = { 1, 1, 1, 1, 1, 1, 0, 0 };
    const u128 p = spu_fm_r(b, spu_shufb(a, a, c47));
    const u128 c = spu_fnms(a, spu_shufb(b, b, c47), p);
    const u128 bw = spu_rotqbyi(b, 12);
    const u128 n = spu_fm(spu_shufb(bw, bw, c39), spu_and(spu_shufb(c, c, c47), m18));
    const u128 s = spu_fm(n, n);
    const u128 dd = spu_fa(spu_fa_r(s, spu_shufb(s, s, c67)), spu_shufb(s, s, c38));
    const u128 d = spu_shufb(dd, dd, c39);
    const u128 gt = spu_fcgt(d, eps);
    const u128 y = spu_fi(d, spu_frsqest(d));
    const u128 t = spu_fm_r(y, y);
    const u128 u = fnms_cru[k] ? spu_fnms_r(d, t, um) : spu_fnms(d, t, um);
    const u128 h = h_cru[k] ? spu_fm_r(y, meio) : spu_fm(y, meio);
    const u128 o = spu_fm(n, spu_fma(u, h, y));
    return spu_and(spu_selb(n, spu_and(gt, o), gt), m18);
}

static const uint32_t a880_lut[64] = {   /* frsqest (a mesma do runtime e do pl_00752710.h) */
    0x350160, 0x34E954, 0x2F993D, 0x2F993D, 0x2AA523, 0x2AA523, 0x26190D, 0x26190D,
    0x21E4F9, 0x21E4F9, 0x1E00E9, 0x1E00E9, 0x1A5CD9, 0x1A5CD9, 0x16F8CB, 0x16F8CB,
    0x13CCC0, 0x13CCC0, 0x10CCB3, 0x10CCB3, 0x0E00AA, 0x0E00AA, 0x0B58A1, 0x0B58A1,
    0x08D498, 0x08D498, 0x067491, 0x067491, 0x043089, 0x043089, 0x020C83, 0x020C83,
    0x7FFDF4, 0x7FD1DE, 0x7859C8, 0x783DBA, 0x71559C, 0x71559C, 0x6AE57C, 0x6AE57C,
    0x64F561, 0x64F561, 0x5F7149, 0x5F7149, 0x5A4D33, 0x5A4D33, 0x55811F, 0x55811F,
    0x51050F, 0x51050F, 0x4CC8FE, 0x4CC8FE, 0x48D0F0, 0x48D0F0, 0x4510E4, 0x4510E4,
    0x4180D7, 0x4180D7, 0x3E24CC, 0x3E24CC, 0x3AF4C3, 0x3AF4C3, 0x37E8BA, 0x37E8BA};

#if defined(__AVX512F__) && defined(__AVX512BW__) && defined(__AVX512VL__)
#define A880_VETOR 1
#define A880_RZ (_MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC)
static inline __m512 a880_z(__m512 x)
{
    const __m512i i = _mm512_castps_si512(x);
    return _mm512_castsi512_ps(_mm512_maskz_mov_epi32(_mm512_test_epi32_mask(i, _mm512_set1_epi32(0x7F800000)), i));
}
static inline __mmask16 a880_ruim(__m512 r)
{
    return _mm512_cmpge_epu32_mask(_mm512_and_si512(_mm512_castps_si512(r), _mm512_set1_epi32(0x7FFFFFFF)), _mm512_set1_epi32(0x7F7FFFFF));
}
static inline __m512i a880_bswap(__m512i v)
{
    return _mm512_shuffle_epi8(v, _mm512_broadcast_i32x4(_mm_setr_epi8(3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12)));
}
/* frsqest + fi (inteiros, como o runtime) para d positivo normal */
static inline __m512 a880_rsq(__m512 d)
{
    const __m512i ui = _mm512_castps_si512(d);
    const __m512i e = _mm512_and_si512(_mm512_srli_epi32(ui, 23), _mm512_set1_epi32(0xFF));
    const __m512i idx = _mm512_and_si512(_mm512_srli_epi32(ui, 18), _mm512_set1_epi32(0x3F));
    const __m512i lo = _mm512_permutex2var_epi32(_mm512_loadu_si512(&a880_lut[0]), idx, _mm512_loadu_si512(&a880_lut[16]));
    const __m512i hi = _mm512_permutex2var_epi32(_mm512_loadu_si512(&a880_lut[32]), idx, _mm512_loadu_si512(&a880_lut[48]));
    const __m512i fr = _mm512_mask_blend_epi32(_mm512_test_epi32_mask(idx, _mm512_set1_epi32(32)), lo, hi);
    const __m512i rexp = _mm512_mask_blend_epi32(_mm512_cmpeq_epi32_mask(e, _mm512_setzero_si512()),
                                                 _mm512_sub_epi32(_mm512_set1_epi32(190), _mm512_srli_epi32(_mm512_add_epi32(e, _mm512_set1_epi32(1)), 1)),
                                                 _mm512_set1_epi32(0xFF));
    const __m512i est = _mm512_or_si512(_mm512_slli_epi32(rexp, 23), _mm512_and_si512(fr, _mm512_set1_epi32(0x7FFFFF)));
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
/* 4 vetores (64 bytes de cada fluxo); devolve a máscara de faixas ruins (0 = tudo certo, saída gravada em o) */
static inline __mmask16 a880_quatro(const uint8_t* pa, const uint8_t* pb, uint8_t* o)
{
    const __m512 a = a880_z(_mm512_castsi512_ps(a880_bswap(_mm512_loadu_si512(pa))));
    const __m512 b = a880_z(_mm512_castsi512_ps(a880_bswap(_mm512_loadu_si512(pb))));
    const __m512 a1 = _mm512_permute_ps(a, _MM_SHUFFLE(0, 0, 2, 1)), b1 = _mm512_permute_ps(b, _MM_SHUFFLE(0, 0, 2, 1));
    __m512 p = _mm512_mul_round_ps(b, a1, A880_RZ);
    __mmask16 ruim = a880_ruim(p);
    const __m512 c = a880_z(_mm512_fnmadd_round_ps(a, b1, a880_z(p), A880_RZ));
    ruim |= a880_ruim(c);
    const __m512 c1 = _mm512_castsi512_ps(_mm512_maskz_mov_epi32(0x7777, _mm512_castps_si512(_mm512_permute_ps(c, _MM_SHUFFLE(0, 0, 2, 1)))));
    const __m512 bw = _mm512_permute_ps(b, _MM_SHUFFLE(3, 3, 3, 3));
    const __m512 n = a880_z(_mm512_mul_round_ps(bw, c1, A880_RZ));
    ruim |= a880_ruim(n);
    const __m512 s = a880_z(_mm512_mul_round_ps(n, n, A880_RZ));
    ruim |= a880_ruim(s);
    const __m512 q = _mm512_add_round_ps(s, _mm512_permute_ps(s, _MM_SHUFFLE(0, 3, 2, 1)), A880_RZ);
    ruim |= a880_ruim(q);
    const __m512 dd = a880_z(_mm512_add_round_ps(a880_z(q), _mm512_permute_ps(s, _MM_SHUFFLE(1, 0, 3, 2)), A880_RZ));
    ruim |= a880_ruim(dd);
    const __m512 d = _mm512_permute_ps(dd, _MM_SHUFFLE(0, 0, 0, 0));
    /* d > épsilon (os dois positivos e no alcance: a comparação de inteiros serve) */
    const __mmask16 gt = _mm512_cmpgt_epi32_mask(_mm512_castps_si512(d), _mm512_set1_epi32(0x0DA2425F));
    const __m512 y = a880_rsq(d);
    const __m512 t = _mm512_mul_round_ps(y, y, A880_RZ);
    __mmask16 r2 = a880_ruim(t);
    const __m512 u = a880_z(_mm512_fnmadd_round_ps(d, a880_z(t), _mm512_set1_ps(1.0f), A880_RZ));
    r2 |= a880_ruim(u);
    const __m512 h = a880_z(_mm512_mul_round_ps(a880_z(y), _mm512_set1_ps(0.5f), A880_RZ));
    const __m512 y2 = a880_z(_mm512_fmadd_round_ps(u, h, a880_z(y), A880_RZ));
    r2 |= a880_ruim(y2) | a880_ruim(y);
    const __m512 out = a880_z(_mm512_mul_round_ps(n, y2, A880_RZ));
    r2 |= a880_ruim(out);
    ruim |= r2 & gt;   /* com d <= épsilon a saída é n: o resto não conta */
    const __m512 r = _mm512_castsi512_ps(_mm512_maskz_mov_epi32(0x7777, _mm512_castps_si512(_mm512_mask_blend_ps(gt, n, out))));
    if (!ruim) _mm512_storeu_si512(o, a880_bswap(_mm512_castps_si512(r)));
    return ruim;
}
#elif defined(SPU_SX_RZ) && defined(__AVX2__) && defined(__FMA__)
#define A880_VETOR 2
/* build portátil (o MXCSR da thread da SPU arredonda para zero): os 8 vetores da volta em SoA, uma faixa por vetor
 * (ordem v0 v2 v4 v6 | v1 v3 v5 v7, a da transposição); as mesmas contas, na mesma ordem, de cada faixa do AoS */
static inline __m256 a880_z8(__m256 x)
{
    const __m256i i = _mm256_castps_si256(x);
    return _mm256_castsi256_ps(_mm256_andnot_si256(_mm256_cmpeq_epi32(_mm256_and_si256(i, _mm256_set1_epi32(0x7F800000)), _mm256_setzero_si256()), i));
}
static inline __m256i a880_ruim8(__m256 r)
{
    return _mm256_cmpgt_epi32(_mm256_and_si256(_mm256_castps_si256(r), _mm256_set1_epi32(0x7FFFFFFF)), _mm256_set1_epi32(0x7F7FFFFE));
}
static inline __m256i a880_bswap8(__m256i v)
{
    return _mm256_shuffle_epi8(v, _mm256_setr_epi8(3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12,
                                                   3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12));
}
static inline void a880_soa(const uint8_t* p, __m256* x, __m256* y, __m256* z, __m256* w)
{
    const __m256 A = a880_z8(_mm256_castsi256_ps(a880_bswap8(_mm256_loadu_si256((const __m256i*)p))));
    const __m256 B = a880_z8(_mm256_castsi256_ps(a880_bswap8(_mm256_loadu_si256((const __m256i*)(p + 32)))));
    const __m256 C = a880_z8(_mm256_castsi256_ps(a880_bswap8(_mm256_loadu_si256((const __m256i*)(p + 64)))));
    const __m256 D = a880_z8(_mm256_castsi256_ps(a880_bswap8(_mm256_loadu_si256((const __m256i*)(p + 96)))));
    const __m256 t0 = _mm256_unpacklo_ps(A, B), t1 = _mm256_unpackhi_ps(A, B), t2 = _mm256_unpacklo_ps(C, D), t3 = _mm256_unpackhi_ps(C, D);
    *x = _mm256_shuffle_ps(t0, t2, _MM_SHUFFLE(1, 0, 1, 0)); *y = _mm256_shuffle_ps(t0, t2, _MM_SHUFFLE(3, 2, 3, 2));
    *z = _mm256_shuffle_ps(t1, t3, _MM_SHUFFLE(1, 0, 1, 0)); *w = _mm256_shuffle_ps(t1, t3, _MM_SHUFFLE(3, 2, 3, 2));
}
static inline __m256 a880_rsq8(__m256 d)
{
    const __m256i ui = _mm256_castps_si256(d);
    const __m256i e = _mm256_and_si256(_mm256_srli_epi32(ui, 23), _mm256_set1_epi32(0xFF));
    const __m256i idx = _mm256_and_si256(_mm256_srli_epi32(ui, 18), _mm256_set1_epi32(0x3F));
    /* a tabela de 64 em 8 registros (sem gather): bits 3, 4 e 5 do índice escolhem */
    __m256i t[8];
    for (int i = 0; i < 8; i++) t[i] = _mm256_permutevar8x32_epi32(_mm256_loadu_si256((const __m256i*)&a880_lut[8 * i]), idx);
    const __m256 b3 = _mm256_castsi256_ps(_mm256_slli_epi32(idx, 28)), b4 = _mm256_castsi256_ps(_mm256_slli_epi32(idx, 27)),
                 b5 = _mm256_castsi256_ps(_mm256_slli_epi32(idx, 26));
#define A880_BL(a, b, m) _mm256_castps_si256(_mm256_blendv_ps(_mm256_castsi256_ps(a), _mm256_castsi256_ps(b), m))
    const __m256i l0 = A880_BL(t[0], t[1], b3), l1 = A880_BL(t[2], t[3], b3), l2 = A880_BL(t[4], t[5], b3), l3 = A880_BL(t[6], t[7], b3);
    const __m256i fr = A880_BL(A880_BL(l0, l1, b4), A880_BL(l2, l3, b4), b5);
#undef A880_BL
    const __m256i rexp = _mm256_blendv_epi8(_mm256_sub_epi32(_mm256_set1_epi32(190), _mm256_srli_epi32(_mm256_add_epi32(e, _mm256_set1_epi32(1)), 1)),
                                            _mm256_set1_epi32(0xFF), _mm256_cmpeq_epi32(e, _mm256_setzero_si256()));
    const __m256i est = _mm256_or_si256(_mm256_slli_epi32(rexp, 23), _mm256_and_si256(fr, _mm256_set1_epi32(0x7FFFFF)));
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
/* a volta inteira (8 vetores de cada fluxo -> 8 normais em o); devolve a máscara (bit = vetor) dos ruins */
static inline unsigned a880_oito(const uint8_t* pa, const uint8_t* pb, uint8_t* o)
{
    __m256 ax, ay, az, aw, bx, by, bz, bw;
    a880_soa(pa, &ax, &ay, &az, &aw); a880_soa(pb, &bx, &by, &bz, &bw);
    (void)aw;
    __m256i ruim = _mm256_setzero_si256();
#define A880_R(v) ruim = _mm256_or_si256(ruim, a880_ruim8(v))
    /* c_i = b_i * a_(i+1) - a_i * b_(i+1) (o AoS com a.yzxx, b.yzxx), i = 0, 1, 2 */
    __m256 p0 = _mm256_mul_ps(bx, ay), p1 = _mm256_mul_ps(by, az), p2 = _mm256_mul_ps(bz, ax);
    A880_R(p0); A880_R(p1); A880_R(p2);
    const __m256 c0 = a880_z8(_mm256_fnmadd_ps(ax, by, a880_z8(p0)));
    const __m256 c1 = a880_z8(_mm256_fnmadd_ps(ay, bz, a880_z8(p1)));
    const __m256 c2 = a880_z8(_mm256_fnmadd_ps(az, bx, a880_z8(p2)));
    A880_R(c0); A880_R(c1); A880_R(c2);
    /* n = (c1, c2, c0) * b.w */
    const __m256 nx = a880_z8(_mm256_mul_ps(bw, c1)), ny = a880_z8(_mm256_mul_ps(bw, c2)), nz = a880_z8(_mm256_mul_ps(bw, c0));
    A880_R(nx); A880_R(ny); A880_R(nz);
    const __m256 sx = a880_z8(_mm256_mul_ps(nx, nx)), sy = a880_z8(_mm256_mul_ps(ny, ny)), sz = a880_z8(_mm256_mul_ps(nz, nz));
    A880_R(sx); A880_R(sy); A880_R(sz);
    const __m256 q = _mm256_add_ps(sx, sy);
    A880_R(q);
    const __m256 d = a880_z8(_mm256_add_ps(a880_z8(q), sz));
    A880_R(d);
#undef A880_R
    const __m256 gt = _mm256_castsi256_ps(_mm256_cmpgt_epi32(_mm256_castps_si256(d), _mm256_set1_epi32(0x0DA2425F)));
    const __m256 y = a880_rsq8(d);
    const __m256 t = _mm256_mul_ps(y, y);
    __m256i r2 = a880_ruim8(t);
    const __m256 u = a880_z8(_mm256_fnmadd_ps(d, a880_z8(t), _mm256_set1_ps(1.0f)));
    r2 = _mm256_or_si256(r2, a880_ruim8(u));
    const __m256 h = a880_z8(_mm256_mul_ps(a880_z8(y), _mm256_set1_ps(0.5f)));
    const __m256 y2 = a880_z8(_mm256_fmadd_ps(u, h, a880_z8(y)));
    r2 = _mm256_or_si256(r2, _mm256_or_si256(a880_ruim8(y2), a880_ruim8(y)));
    const __m256 ox = a880_z8(_mm256_mul_ps(nx, y2)), oy = a880_z8(_mm256_mul_ps(ny, y2)), oz = a880_z8(_mm256_mul_ps(nz, y2));
    r2 = _mm256_or_si256(r2, _mm256_or_si256(a880_ruim8(ox), _mm256_or_si256(a880_ruim8(oy), a880_ruim8(oz))));
    ruim = _mm256_or_si256(ruim, _mm256_and_si256(r2, _mm256_castps_si256(gt)));   /* com d <= épsilon sai n */
    const __m256 rx = _mm256_blendv_ps(nx, ox, gt), ry = _mm256_blendv_ps(ny, oy, gt), rz = _mm256_blendv_ps(nz, oz, gt);
    const __m256 Z0 = _mm256_setzero_ps();
    const __m256 u0 = _mm256_unpacklo_ps(rx, ry), u1 = _mm256_unpackhi_ps(rx, ry), u2 = _mm256_unpacklo_ps(rz, Z0), u3 = _mm256_unpackhi_ps(rz, Z0);
    _mm256_storeu_si256((__m256i*)(o), a880_bswap8(_mm256_castps_si256(_mm256_shuffle_ps(u0, u2, _MM_SHUFFLE(1, 0, 1, 0)))));
    _mm256_storeu_si256((__m256i*)(o + 32), a880_bswap8(_mm256_castps_si256(_mm256_shuffle_ps(u0, u2, _MM_SHUFFLE(3, 2, 3, 2)))));
    _mm256_storeu_si256((__m256i*)(o + 64), a880_bswap8(_mm256_castps_si256(_mm256_shuffle_ps(u1, u3, _MM_SHUFFLE(1, 0, 1, 0)))));
    _mm256_storeu_si256((__m256i*)(o + 96), a880_bswap8(_mm256_castps_si256(_mm256_shuffle_ps(u1, u3, _MM_SHUFFLE(3, 2, 3, 2)))));
    const unsigned rm = (unsigned)_mm256_movemask_ps(_mm256_castsi256_ps(ruim));
    static const unsigned char vert[8] = { 0, 2, 4, 6, 1, 3, 5, 7 };
    unsigned maus = 0;
    for (int f = 0; f < 8; f++) if ((rm >> f) & 1u) maus |= 1u << vert[f];
    return maus;
}
#else
#define A880_VETOR 0
#endif

/* GOWA_A880_STATS=1: chamadas e vetores refeitos pelo caminho exato */
static __attribute__((noinline)) int fn_0000A880_nativo(spu_context* ctx)
{
    /* só na build portátil (AVX2, SoA): em AVX-512 o caminho AoS empata com o vk (30/10), que fica */
    if (A880_VETOR != 2 || !gowa_edge_nativo_on()) return 0;
    u128* r = ctx->gpr;
    const int32_t cont = (int32_t)r[83]._u32[0];
    if (cont <= 0 || cont > 0x4000 || r[52]._u32[0] != 0u || r[51]._u32[0] != 0u) return 0;
    if (r[47]._u32[0] != 0x04050607u || r[47]._u32[1] != 0x08090A0Bu || r[47]._u32[2] != 0x00010203u || r[47]._u32[3] != 0x00010203u)
        return 0;
    const uint32_t g = ((uint32_t)cont + 7u) / 8u, pa = r[49]._u32[0], pb = r[50]._u32[0], po = r[48]._u32[0], tam = 128u * g;
    if (pa != r[81]._u32[0] || pb != r[82]._u32[0] || po != r[53]._u32[0]) return 0;
    if ((pa | pb | po) & 15u || pa + tam > SPU_LS_SIZE || pb + tam > SPU_LS_SIZE || po + tam > SPU_LS_SIZE) return 0;
    uint8_t* ls = SPU_LS_BASE(ctx);
    long refeitos = 0;
    for (uint32_t j = 0; j < g; j++) {
        /* lê os dois blocos do grupo antes de gravar (a saída pode ser um dos fluxos) */
        uint8_t ea[128], eb[128], o[128];
        memcpy(ea, ls + pa + 128u * j, 128);
        memcpy(eb, ls + pb + 128u * j, 128);
        int bom[8] = { 0, 0, 0, 0, 0, 0, 0, 0 };
#if A880_VETOR == 1
        for (int q = 0; q < 2; q++) {
            const __mmask16 m = a880_quatro(ea + 64 * q, eb + 64 * q, o + 64 * q);
            for (int v = 0; v < 4; v++) bom[4 * q + v] = m == 0;
        }
#elif A880_VETOR == 2
        { const unsigned maus = a880_oito(ea, eb, o);
          for (int v = 0; v < 8; v++) bom[v] = !((maus >> v) & 1u); }
#endif
        for (int v = 0; v < 8; v++) {
            if (bom[v]) continue;
            u128 a, b;
            uint32_t w[4];
            for (int i = 0; i < 4; i++) { memcpy(&w[i], ea + 16 * v + 4 * i, 4); a._u32[i] = __builtin_bswap32(w[i]); }
            for (int i = 0; i < 4; i++) { memcpy(&w[i], eb + 16 * v + 4 * i, 4); b._u32[i] = __builtin_bswap32(w[i]); }
            const u128 x = a880_exato(a, b, v);
            for (int i = 0; i < 4; i++) { const uint32_t be = __builtin_bswap32(x._u32[i]); memcpy(o + 16 * v + 4 * i, &be, 4); }
            refeitos++;
        }
        memcpy(ls + po + 128u * j, o, 128);
    }
    static int est = -1; static volatile long nch, nref;
    if (est < 0) est = getenv("GOWA_A880_STATS") ? 1 : 0;
    if (est) { const long n = ++nch; nref += refeitos; if ((n & (n - 1)) == 0) fprintf(stderr, "[a880] %ld chamadas, %ld vetores refeitos\n", n, nref); }
    return 1;
}
#endif
