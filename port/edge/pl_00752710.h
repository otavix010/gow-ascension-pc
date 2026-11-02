/* GOWA (29/10, EDGE nativo, marco E3): o plugin 0x752710 do EDGE (descompressão de posição e de dois
 * vetores unitários por vértice) escrito para o PC, a partir da entrada do laço 0x302E8 (a preparação
 * 0x30020 continua a traduzida: roda uma vez por chamada).
 *
 * Por vértice, 18 bytes: posição float3 (sai com w = 1,0) + 48 bits = dois vetores "smallest three":
 * seletor de 4 bits + dois componentes de 10 bits (vetor 1: seletor nos bits 24-27, componentes 38-47 e
 * 28-37; vetor 2: seletor 0-3, componentes 14-23 e 4-13). Cada componente = fma(11595,62, 1,0 com os 10
 * bits na mantissa, -11596,33); o terceiro = fm(u, fi(u, frsqest(u))), u = fs(1, fa(fm(a,a), fm(b,b))).
 * O seletor escolhe uma entrada de uma tabela que a preparação copiou para a pilha (r40 + sel*16): o
 * sinal de cada palavra vai para o terceiro componente (selb com 0x80000000), o resto é o padrão da troca
 * de bytes, e o vértice B do par soma 8 a cada byte do padrão. Os seletores 12-15 (vértices de enchimento
 * no fim da lista) leem a pilha além da tabela: saem iguais porque a conta é a mesma, inclusive nas
 * faixas "de lixo" (somas cruzadas entre os dois vértices do par).
 * O original grava os vetores com uma volta de atraso: na primeira volta saem dois pares de "lixo"
 * calculados dos registradores de entrada (o do vetor 2 fica depois do fim do fluxo); aqui também.
 * Conferido pelo modelo scripts_edge/pl752_nativo_modelo.py (12/12 fotos iguais) e pela conferência dos
 * jobs. GOWA_EDGE_NATIVO=1 liga. */
#ifndef GOWA_PL_00752710_H
#define GOWA_PL_00752710_H
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

static const uint32_t pl752_lut[64] = {
    0x350160, 0x34E954, 0x2F993D, 0x2F993D, 0x2AA523, 0x2AA523, 0x26190D, 0x26190D,
    0x21E4F9, 0x21E4F9, 0x1E00E9, 0x1E00E9, 0x1A5CD9, 0x1A5CD9, 0x16F8CB, 0x16F8CB,
    0x13CCC0, 0x13CCC0, 0x10CCB3, 0x10CCB3, 0x0E00AA, 0x0E00AA, 0x0B58A1, 0x0B58A1,
    0x08D498, 0x08D498, 0x067491, 0x067491, 0x043089, 0x043089, 0x020C83, 0x020C83,
    0x7FFDF4, 0x7FD1DE, 0x7859C8, 0x783DBA, 0x71559C, 0x71559C, 0x6AE57C, 0x6AE57C,
    0x64F561, 0x64F561, 0x5F7149, 0x5F7149, 0x5A4D33, 0x5A4D33, 0x55811F, 0x55811F,
    0x51050F, 0x51050F, 0x4CC8FE, 0x4CC8FE, 0x48D0F0, 0x48D0F0, 0x4510E4, 0x4510E4,
    0x4180D7, 0x4180D7, 0x3E24CC, 0x3E24CC, 0x3AF4C3, 0x3AF4C3, 0x37E8BA, 0x37E8BA};

#if defined(__AVX512F__) && defined(__AVX512BW__)
#define PL752_VETOR 1
#define PL752_PARES 4
#define PL752_RZ (_MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC)
/* r13 = (aA, bA, aB, bB) de 4 pares -> c (as 4 faixas de cada par) */
static inline void pl752_contas(const uint32_t* f10, uint32_t* ab_out, uint32_t* c_out)
{
    const __m512 x = _mm512_castsi512_ps(_mm512_or_si512(_mm512_loadu_si512(f10), _mm512_set1_epi32(0x3F800000)));
    const __m512 ab = _mm512_fmadd_round_ps(_mm512_castsi512_ps(_mm512_set1_epi32(0x46352E7C)), x,
                                            _mm512_castsi512_ps(_mm512_set1_epi32((int)0xC6353150u)), PL752_RZ);
    const __m512 q = _mm512_mul_round_ps(ab, ab, PL752_RZ);
    const __m512 sm = _mm512_add_round_ps(q, _mm512_castsi512_ps(_mm512_bsrli_epi128(_mm512_castps_si512(q), 4)), PL752_RZ);
    const __m512 u = _mm512_sub_round_ps(_mm512_set1_ps(1.0f), sm, PL752_RZ);
    const __m512i ui = _mm512_castps_si512(u);
    /* frsqest: tabela de 64 pelo índice (bits 18-23); expoente 190 - (e+1)/2, ou 255 com e = 0; sinal 0 */
    const __m512i e = _mm512_and_si512(_mm512_srli_epi32(ui, 23), _mm512_set1_epi32(0xFF));
    const __m512i idx = _mm512_and_si512(_mm512_srli_epi32(ui, 18), _mm512_set1_epi32(0x3F));
    const __m512i lo = _mm512_permutex2var_epi32(_mm512_loadu_si512(&pl752_lut[0]), idx, _mm512_loadu_si512(&pl752_lut[16]));
    const __m512i hi = _mm512_permutex2var_epi32(_mm512_loadu_si512(&pl752_lut[32]), idx, _mm512_loadu_si512(&pl752_lut[48]));
    const __m512i fr = _mm512_mask_blend_epi32(_mm512_test_epi32_mask(idx, _mm512_set1_epi32(32)), lo, hi);
    const __m512i rexp = _mm512_mask_blend_epi32(_mm512_cmpeq_epi32_mask(e, _mm512_setzero_si512()),
                                                 _mm512_sub_epi32(_mm512_set1_epi32(190), _mm512_srli_epi32(_mm512_add_epi32(e, _mm512_set1_epi32(1)), 1)),
                                                 _mm512_set1_epi32(0xFF));
    const __m512i est = _mm512_or_si512(_mm512_slli_epi32(rexp, 23), _mm512_and_si512(fr, _mm512_set1_epi32(0x7FFFFF)));
    /* fi(u, est): a mesma aritmética inteira do spu_fi do runtime */
    const __m512i fe = _mm512_and_si512(_mm512_srli_epi32(est, 23), _mm512_set1_epi32(0xFF));
    const __m512i base = _mm512_or_si512(_mm512_and_si512(_mm512_srli_epi32(est, 10), _mm512_set1_epi32(0x1FFF)), _mm512_set1_epi32(0x2000));
    const __m512i prod = _mm512_mullo_epi32(_mm512_and_si512(est, _mm512_set1_epi32(0x3FF)), _mm512_and_si512(ui, _mm512_set1_epi32(0x7FFFF)));
    const __m512i m8 = _mm512_sub_epi32(_mm512_slli_epi32(base, 11), _mm512_srli_epi32(_mm512_add_epi32(prod, _mm512_set1_epi32(255)), 8));
    const __mmask16 baixo = _mm512_cmplt_epi32_mask(m8, _mm512_set1_epi32(1 << 24));
    const __m512i frac = _mm512_and_si512(_mm512_mask_blend_epi32(baixo, _mm512_srli_epi32(m8, 1), m8), _mm512_set1_epi32(0x7FFFFF));
    const __m512i e2 = _mm512_mask_sub_epi32(fe, baixo, fe, _mm512_set1_epi32(1));
    __m512i fiv = _mm512_or_si512(_mm512_and_si512(est, _mm512_set1_epi32((int)0x80000000u)), _mm512_or_si512(_mm512_slli_epi32(e2, 23), frac));
    fiv = _mm512_maskz_mov_epi32(~(_mm512_cmpeq_epi32_mask(fe, _mm512_setzero_si512()) | _mm512_cmpeq_epi32_mask(e2, _mm512_setzero_si512())), fiv);
    const __m512 c = _mm512_mul_round_ps(u, _mm512_castsi512_ps(fiv), PL752_RZ);
    _mm512_storeu_si512(ab_out, _mm512_castps_si512(ab));
    _mm512_storeu_si512(c_out, _mm512_castps_si512(c));
}
#elif defined(SPU_SX_RZ) && defined(__AVX2__) && defined(__FMA__)
#define PL752_VETOR 1
#define PL752_PARES 2
/* MXCSR da thread da SPU em para-zero (SPU_SX_RZ): as operações comuns arredondam para zero. 02/11: a entrada montada
 * pelos valores (setr), não por uma carga de 256 bits sobre 8 gravações escalares recentes (o repasse da gravação
 * para a carga falha e cada chamada esperava) */
static inline void pl752_contas(const uint32_t* f10, uint32_t* ab_out, uint32_t* c_out)
{
    const __m256i fi = _mm256_setr_epi32((int)f10[0], (int)f10[1], (int)f10[2], (int)f10[3], (int)f10[4], (int)f10[5], (int)f10[6], (int)f10[7]);
    const __m256 x = _mm256_castsi256_ps(_mm256_or_si256(fi, _mm256_set1_epi32(0x3F800000)));
    const __m256 ab = _mm256_fmadd_ps(_mm256_castsi256_ps(_mm256_set1_epi32(0x46352E7C)), x, _mm256_castsi256_ps(_mm256_set1_epi32((int)0xC6353150u)));
    const __m256 q = _mm256_mul_ps(ab, ab);
    const __m256 sm = _mm256_add_ps(q, _mm256_castsi256_ps(_mm256_bsrli_epi128(_mm256_castps_si256(q), 4)));
    const __m256 u = _mm256_sub_ps(_mm256_set1_ps(1.0f), sm);
    const __m256i ui = _mm256_castps_si256(u);
    const __m256i e = _mm256_and_si256(_mm256_srli_epi32(ui, 23), _mm256_set1_epi32(0xFF));
    const __m256i idx = _mm256_and_si256(_mm256_srli_epi32(ui, 18), _mm256_set1_epi32(0x3F));
    const __m256i fr = _mm256_i32gather_epi32((const int*)pl752_lut, idx, 4);
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
    const __m256 c = _mm256_mul_ps(u, _mm256_castsi256_ps(fiv));
    _mm256_storeu_si256((__m256i*)ab_out, _mm256_castps_si256(ab));
    _mm256_storeu_si256((__m256i*)c_out, _mm256_castps_si256(c));
}
#else
#define PL752_VETOR 0
#endif

#if PL752_VETOR
static inline u128 pl752_u128(const uint32_t* w) { u128 r; memcpy(&r, w, 16); return r; }
/* a seleção do original para um vértice do par: c com o sinal da entrada, padrão sem o sinal (+8 no B) */
static inline u128 pl752_sel(u128 c, u128 ab, u128 e, int b)
{
    u128 m32, m30, k31, k52;
    for (int i = 0; i < 4; i++) {
        m32._u32[i] = i < 3 ? 0x80000000u : 0xFFFFFFFFu;
        m30._u32[i] = i < 3 ? 0x7FFFFFFFu : 0u;
        k31._u32[i] = i < 3 ? 0u : 0x0C0D0E0Fu;
        k52._u32[i] = i < 3 ? 0x08080808u : 0u;
    }
    const u128 cc = spu_selb(c, e, m32);
    u128 ctl = spu_selb(k31, e, m30);
    if (b) ctl = spu_a(ctl, k52);
    return spu_shufb(cc, ab, ctl);
}
/* 02/11: a mesma seleção pré-calculada por chamada (a tabela em r40 não muda durante o laço): para cada seletor
 * s (0..15) e vértice b, o shufb do original vira dois pshufb (bytes de c e de ab, big-endian), um AND (o 0x7F do
 * primeiro byte das palavras 0..2 de c, cujo sinal vem da entrada da tabela) e um OR (esse sinal, a palavra 3 =
 * a da tabela e as constantes 0x00/0xFF/0x80 dos controles >= 0x80) */
typedef struct { __m128i mc, mab, e, o; } pl752_sh;
static inline uint8_t pl752_be8(const uint32_t* w, uint32_t j) { return (uint8_t)(w[j >> 2] >> (24u - 8u * (j & 3u))); }
static void pl752_prepara(spu_context* ctx, uint32_t r40, pl752_sh* t)
{
    for (uint32_t s = 0; s < 16u; s++) {
        const u128 e = spu_ls_read128(ctx, r40 + 16u * s);
        for (uint32_t b = 0; b < 2u; b++) {
            uint32_t ctl[4];
            for (int i = 0; i < 3; i++) ctl[i] = (e._u32[i] & 0x7FFFFFFFu) + (b ? 0x08080808u : 0u);
            ctl[3] = 0x0C0D0E0Fu;
            uint8_t mc[16], mab[16], an[16], ou[16];
            for (uint32_t j = 0; j < 16u; j++) {
                const uint32_t c = pl752_be8(ctl, j);
                mc[j] = mab[j] = 0x80; an[j] = 0; ou[j] = 0;
                if (c & 0x80u) ou[j] = (c & 0xC0u) == 0x80u ? 0x00 : (c & 0xE0u) == 0xC0u ? 0xFF : 0x80;
                else if ((c & 0x1Fu) >= 16u) { mab[j] = (uint8_t)((c & 0x1Fu) - 16u); an[j] = 0xFF; }
                else {
                    const uint32_t k = c & 0x0Fu;
                    if (k >= 12u) ou[j] = pl752_be8(e._u32, k);                        /* palavra 3 de cc = a da tabela */
                    else if (!(k & 3u)) { mc[j] = (uint8_t)k; an[j] = 0x7F; ou[j] = (uint8_t)((e._u32[k >> 2] >> 24) & 0x80u); }
                    else { mc[j] = (uint8_t)k; an[j] = 0xFF; }
                }
            }
            pl752_sh* x = &t[2u * s + b];
            x->mc = _mm_loadu_si128((const __m128i*)mc); x->mab = _mm_loadu_si128((const __m128i*)mab);
            x->e = _mm_loadu_si128((const __m128i*)an); x->o = _mm_loadu_si128((const __m128i*)ou);
        }
    }
}
static inline void pl752_sel_rapido(uint8_t* ls, uint32_t dst, const uint32_t* c, const uint32_t* ab, const pl752_sh* x)
{
    const __m128i be = _mm_setr_epi8(3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12);
    const __m128i cb = _mm_shuffle_epi8(_mm_loadu_si128((const __m128i*)c), be), abb = _mm_shuffle_epi8(_mm_loadu_si128((const __m128i*)ab), be);
    const __m128i v = _mm_or_si128(_mm_and_si128(_mm_or_si128(_mm_shuffle_epi8(cb, x->mc), _mm_shuffle_epi8(abb, x->mab)), x->e), x->o);
    _mm_storeu_si128((__m128i*)&ls[dst & SPU_LS_MASK & ~15u], v);
}
static inline void pl752_pos(uint8_t* ls, uint32_t dst, const uint8_t* src)
{
    uint8_t* d = &ls[dst & SPU_LS_MASK & ~15u];
    memcpy(d, src, 12);
    d[12] = 0x3F; d[13] = 0x80; d[14] = 0; d[15] = 0;
}
#endif

static int pl_00752710_corpo(spu_context* ctx);
static int pl_00752710_nativo(spu_context* ctx)
{
    static volatile long n[2];
    const int r = pl_00752710_corpo(ctx);
    gowa_edge_conta("pl752", n, r);
    return r;
}

#if PL752_VETOR
/* os P pares: posições (w = 1,0) e os dois vetores de cada par, as contas em lotes; tabela da seleção em r40 */
static void pl752_pares(spu_context* ctx, uint32_t inp, uint32_t b0, uint32_t b1, uint32_t b2, uint32_t P, uint32_t r40)
{
    uint8_t* ls = SPU_LS_BASE(ctx);
    /* a seleção pré-calculada, se nenhum fluxo de saída encosta na tabela (r40 .. r40 + 256: os seletores 12-15
     * leem além dela) -- senão a de cada vértice, como antes */
    pl752_sh sh[32];
    const uint32_t tb = r40 & SPU_LS_MASK & ~15u, no = 32u * P + 32u;
#define PL752_ENC(a) (((a) & SPU_LS_MASK) < tb + 256u && tb < ((a) & SPU_LS_MASK) + no)
    const int rapido = !PL752_ENC(b0) && !PL752_ENC(b1) && !PL752_ENC(b2) && tb + 256u <= SPU_LS_SIZE;
#undef PL752_ENC
    if (rapido) pl752_prepara(ctx, r40, sh);
    /* os pares em lotes: as contas em vetor, a seleção e as gravações par a par */
    for (uint32_t p0 = 0; p0 < P; p0 += PL752_PARES) {
        uint32_t f1[4 * PL752_PARES], f2[4 * PL752_PARES], ab1[4 * PL752_PARES], ab2[4 * PL752_PARES], c1[4 * PL752_PARES], c2[4 * PL752_PARES];
        uint32_t s1[2 * PL752_PARES], s2[2 * PL752_PARES];
        for (uint32_t q = 0; q < PL752_PARES; q++) {
            for (uint32_t k = 0; k < 2; k++) {
                const uint8_t* r = &ls[(inp + 36u * (p0 + q) + 18u * k) & SPU_LS_MASK];
                const uint64_t bits = ((uint64_t)r[12] << 40) | ((uint64_t)r[13] << 32) | ((uint64_t)r[14] << 24) |
                                      ((uint64_t)r[15] << 16) | ((uint64_t)r[16] << 8) | r[17];
                f1[4 * q + 2 * k] = (uint32_t)(bits >> 38) & 0x3FFu; f1[4 * q + 2 * k + 1] = (uint32_t)(bits >> 28) & 0x3FFu;
                f2[4 * q + 2 * k] = (uint32_t)(bits >> 14) & 0x3FFu; f2[4 * q + 2 * k + 1] = (uint32_t)(bits >> 4) & 0x3FFu;
                s1[2 * q + k] = (uint32_t)(bits >> 24) & 0xFu; s2[2 * q + k] = (uint32_t)bits & 0xFu;
            }
        }
        pl752_contas(f1, ab1, c1);
        pl752_contas(f2, ab2, c2);
        for (uint32_t q = 0; q < PL752_PARES && p0 + q < P; q++) {
            const uint32_t p = p0 + q;
            pl752_pos(ls, b0 + 32u * p, &ls[inp + 36u * p]);
            pl752_pos(ls, b0 + 32u * p + 16u, &ls[inp + 36u * p + 18u]);
            if (rapido) {
                pl752_sel_rapido(ls, b1 + 32u * p, &c1[4 * q], &ab1[4 * q], &sh[2u * s1[2 * q]]);
                pl752_sel_rapido(ls, b1 + 32u * p + 16u, &c1[4 * q], &ab1[4 * q], &sh[2u * s1[2 * q + 1] + 1u]);
                pl752_sel_rapido(ls, b2 + 32u * p, &c2[4 * q], &ab2[4 * q], &sh[2u * s2[2 * q]]);
                pl752_sel_rapido(ls, b2 + 32u * p + 16u, &c2[4 * q], &ab2[4 * q], &sh[2u * s2[2 * q + 1] + 1u]);
                continue;
            }
            const u128 A1 = pl752_u128(&ab1[4 * q]), C1 = pl752_u128(&c1[4 * q]), A2 = pl752_u128(&ab2[4 * q]), C2 = pl752_u128(&c2[4 * q]);
            spu_ls_write128(ctx, b1 + 32u * p, pl752_sel(C1, A1, spu_ls_read128(ctx, r40 + 16u * s1[2 * q]), 0));
            spu_ls_write128(ctx, b1 + 32u * p + 16u, pl752_sel(C1, A1, spu_ls_read128(ctx, r40 + 16u * s1[2 * q + 1]), 1));
            spu_ls_write128(ctx, b2 + 32u * p, pl752_sel(C2, A2, spu_ls_read128(ctx, r40 + 16u * s2[2 * q]), 0));
            spu_ls_write128(ctx, b2 + 32u * p + 16u, pl752_sel(C2, A2, spu_ls_read128(ctx, r40 + 16u * s2[2 * q + 1]), 1));
        }
    }
}

#endif
static int pl_00752710_corpo(spu_context* ctx)
{
#if PL752_VETOR
    if (!gowa_edge_nativo_on()) return 0;
    u128* g = ctx->gpr;
    uint8_t* ls = SPU_LS_BASE(ctx);
    const uint32_t r5 = g[5]._u32[0];
    /* só na primeira volta do laço (o que a preparação deixou): offset = -r5, atraso dos vetores = 0 */
    if (!r5 || (r5 & 31u) || r5 > 0x10000u || g[26]._u32[0] != (uint32_t)(0u - r5) || g[7]._u32[0] != 0u) {
        static volatile long d; if (getenv("GOWA_EDGE_CONTA") && _InterlockedIncrement(&d) <= 8)
            fprintf(stderr, "[pl752-recusa] r5=0x%X r26=0x%X r7=0x%X\n", r5, g[26]._u32[0], g[7]._u32[0]);
        return 0;
    }
    const uint32_t P = r5 / 32u;
    const uint32_t inp = (g[35]._u32[0] - 72u) & SPU_LS_MASK;
    const uint32_t b0 = (g[61]._u32[0] - r5) & SPU_LS_MASK, b1 = (g[50]._u32[0] - r5) & SPU_LS_MASK, b2 = (g[51]._u32[0] - r5) & SPU_LS_MASK;
    const uint32_t r40 = g[40]._u32[0];
    if (inp + 36u * (P + PL752_PARES) + 64u > SPU_LS_SIZE || b0 + r5 > SPU_LS_SIZE || b1 + r5 + 32u > SPU_LS_SIZE || b2 + r5 + 32u > SPU_LS_SIZE ||
        (r40 & SPU_LS_MASK) + 256u > SPU_LS_SIZE)
        return 0;
    /* 1a volta do original: posições do par 0 e os vetores "do par -1" com os registradores de entrada */
    const uint32_t r18 = g[7]._u32[0];
    pl752_pos(ls, b0, &ls[inp]);
    pl752_pos(ls, b0 + 16u, &ls[inp + 18u]);
    spu_ls_write128(ctx, g[50]._u32[0] + r18, pl752_sel(g[15], g[13], g[38], 0));
    spu_ls_write128(ctx, g[58]._u32[0] + r18, pl752_sel(g[15], g[13], g[39], 1));
    spu_ls_write128(ctx, g[51]._u32[0] + r18, pl752_sel(g[45], g[47], g[44], 0));
    spu_ls_write128(ctx, g[59]._u32[0] + r18, pl752_sel(g[45], g[47], g[46], 1));
    pl752_pares(ctx, inp, b0, b1, b2, P, r40);
    g[1] = spu_ai(g[1], 224);
    g[3] = spu_il(0);
    return 1;
#else
    (void)ctx;
    return 0;
#endif
}
/* 02/11: o plugin inteiro, da 0x30000. Vértice de 18 bytes em r4, n = r5 (aos pares). Refaz a preparação: o r1 em
 * r1 - 0xE0, a tabela de constantes do plugin (a da seleção) no quadro r1 - 224 + 0x20..0xD0, os atributos 1, 3 e 4
 * nos três primeiros zeros da lista (r3 + 0x160) com os ponteiros daquelas vagas em r3 + 0x58 + 4 * id; a saída é a
 * posição no fluxo do 1 e os vetores nos do 3 e do 4, e os vetores "do par -1" (zeros) depois do fim dos dois. Sai com
 * r3 = 0 e o r1 de volta. Conferido na bancada sintética e nas gravações. */
static int pl_00752710_inteiro_corpo(spu_context* ctx, int na20)
{
#if PL752_VETOR
    if (!gowa_edge_nativo_on()) return 0;
    u128* g = ctx->gpr;
    uint8_t* ls = SPU_LS_BASE(ctx);
    const uint32_t r1 = (g[1]._u32[0] + (na20 ? 224u : 0u)) & SPU_LS_MASK, r3 = g[3]._u32[0] & SPU_LS_MASK, r4 = g[4]._u32[0] & SPU_LS_MASK, n = g[5]._u32[0];
    if ((r3 & 15u) || r3 + 0x170u > SPU_LS_SIZE || r1 < 0x200u || !n || n > 4096u) return 0;
    static const uint8_t ids[3] = { 1, 3, 4 };
    int k[3], j = 0;
    for (int i = 0; i < 3; i++) {
        while (j < 16 && ls[r3 + 0x160u + (uint32_t)j]) j++;
        if (j >= 16) return 0;
        k[i] = j++;
    }
    uint32_t pt[3];
    for (int i = 0; i < 3; i++) {
        const uint32_t a = r3 + 16u + 4u * (uint32_t)k[i];
        pt[i] = ((uint32_t)ls[a] << 24) | ((uint32_t)ls[a + 1] << 16) | ((uint32_t)ls[a + 2] << 8) | ls[a + 3];
    }
    const uint32_t b0 = pt[0] & SPU_LS_MASK, b1 = pt[1] & SPU_LS_MASK, b2 = pt[2] & SPU_LS_MASK, P = (n + 1u) / 2u;
    const uint32_t fq = (r1 - 224u) & ~15u, tab = fq + 32u, ni = 36u * (P + PL752_PARES) + 64u, no = 32u * P, nv = 32u * P + 32u;
#define PL7I_ENC(a0, n0, c0, n1) ((a0) < (c0) + (n1) && (c0) < (a0) + (n0))
    /* os dados dos três fluxos não se encostam; o lixo dos vetores (32 bytes depois do fim de b1 e de b2, gravado antes
     * de tudo, como no original) pode cair no começo de outro fluxo (os fluxos vêm colados), mas não na entrada, no
     * quadro nem no contexto */
    if (((b0 | b1 | b2) & 15u) || r4 + ni > SPU_LS_SIZE || b0 + no > SPU_LS_SIZE || b1 + nv > SPU_LS_SIZE || b2 + nv > SPU_LS_SIZE ||
        PL7I_ENC(b0, no, b1, no) || PL7I_ENC(b0, no, b2, no) || PL7I_ENC(b1, no, b2, no) ||
        PL7I_ENC(b0, no, r4, ni) || PL7I_ENC(b1, nv, r4, ni) || PL7I_ENC(b2, nv, r4, ni) || PL7I_ENC(b0, no, fq, 240u) ||
        PL7I_ENC(b1, nv, fq, 240u) || PL7I_ENC(b2, nv, fq, 240u) || PL7I_ENC(b0, no, r3, 0x170u) || PL7I_ENC(b1, nv, r3, 0x170u) ||
        PL7I_ENC(b2, nv, r3, 0x170u) || PL7I_ENC(r4, ni, fq, 256u))
        return 0;
#undef PL7I_ENC
    if (!na20) spu_ls_write128(ctx, fq, g[1]);   /* na 0x30020 quem chama já gravou */
    static const uint16_t de[12] = { 0x500, 0x510, 0x520, 0x530, 0x540, 0x550, 0x4B0, 0x4C0, 0x4D0, 0x560, 0x4E0, 0x4F0 };
    for (uint32_t i = 0; i < 12; i++)
        spu_ls_write128(ctx, fq + 0x20u + 16u * i, spu_ls_read128(ctx, (uint32_t)(0x30000 + de[i] + g_spu_hot_reloc)));
    for (int i = 0; i < 3; i++) {
        ls[r3 + 0x160u + (uint32_t)k[i]] = ids[i];
        const uint32_t a = r3 + 0x58u + 4u * ids[i];
        ls[a] = (uint8_t)(pt[i] >> 24); ls[a + 1] = (uint8_t)(pt[i] >> 16); ls[a + 2] = (uint8_t)(pt[i] >> 8); ls[a + 3] = (uint8_t)pt[i];
    }
    memset(ls + b1 + no, 0, 32);   /* os vetores "do par -1" */
    memset(ls + b2 + no, 0, 32);
    pl752_pares(ctx, r4, b0, b1, b2, P, tab);
    g[3] = spu_il(0);
    if (na20) g[1] = spu_ai(g[1], 224);
    return 1;
#else
    (void)ctx;
    return 0;
#endif
}

static __attribute__((noinline)) int pl_00752710_inteiro(spu_context* ctx)
{
    static volatile long n[2];
    const int r = pl_00752710_inteiro_corpo(ctx, 0);
    gowa_edge_conta("pl752i", n, r);
    return r;
}

/* a entrada 0x30020 (o job também chama direto nela): vale se o que o 0x30000 faz já está feito -- r1 descido 224 com
 * o antigo gravado em r1, r8 = 1, r69 = 3, r23 = a lista, r44 = r3 + 16, r25 e r21 = as constantes 0x304B0/0x304C0 */
static __attribute__((noinline)) int pl_00752710_n20(spu_context* ctx)
{
    static volatile long n[2];
    const u128* g = ctx->gpr;
    const uint32_t sp = g[1]._u32[0];
    const u128 lista = spu_ls_read128(ctx, g[3]._u32[0] + 0x160u), c25 = spu_ls_read128(ctx, (uint32_t)(0x304B0 + g_spu_hot_reloc)),
               c21 = spu_ls_read128(ctx, (uint32_t)(0x304C0 + g_spu_hot_reloc));
    int ok = spu_ls_read32(ctx, sp) == sp + 224u && !memcmp(&lista, &g[23], 16) && !memcmp(&c25, &g[25], 16) && !memcmp(&c21, &g[21], 16);
    for (int i = 0; i < 4 && ok; i++)
        ok = g[8]._u32[i] == 1u && g[69]._u32[i] == 3u && g[44]._u32[i] == g[3]._u32[i] + 16u;
    const int r = ok ? pl_00752710_inteiro_corpo(ctx, 1) : 0;
    gowa_edge_conta("pl752i/20", n, r);
    return r;
}
#endif
