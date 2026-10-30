/* GOWA (29/10, EDGE nativo, marco E3): o laço de descarte de triângulos da D640 do job 0x72DF80
 * (loc_0000D92C até loc_0000DB70), com a decisão escrita para o PC.
 *
 * O laço da SPU trata 4 triângulos por volta num pipeline de 2 voltas: carrega índices e posições
 * (já transformadas pela D250: x, y em pixels, palavra 2 = bits de recorte), decide quais saem e
 * compacta os que ficam (6 bytes de índices cada) no próprio lugar. A decisão, por triângulo:
 *   A = E dos bits de recorte dos 3 vértices, O = OU;
 *   chave = (O & 4) == 0 && (A & 2) != 0       (nenhum atrás do plano próximo, todos com o bit 2)
 *   área zero = os 3 com o mesmo X ou o mesmo Y, X = trunc(RZ(x * 1 + 0,5))   (fma_r + cflts 173)
 *   costas = produto vetorial < 0: RZ(fms(bx-ax, cy-ay, RZ((by-ay)(cx-ax)))), tudo arredondando a zero
 *   sai = (A & 0xFC) || (chave && (área zero || costas))
 * (o fnms do original com máscaras de float saturadas é um OU; o teste de costas é fcgt(sinal ^ cruz, 0)
 * com o sinal ligado). A decisão sai aqui para todos os triângulos de uma vez, em vetor; o caminho dos
 * dados (a fila das máscaras, as tabelas de compactação, as 3 gravações de 16 bytes por volta e o avanço)
 * é o do original, instrução por instrução, então a LS fica igual byte a byte, inclusive o que sobra
 * depois do fim. O gancho fica logo antes do rótulo do laço (roda uma vez); devolve 0 fora do modo medido
 * (escala 1, +0,5, sinal ligado, descarte de costas ligado) e o laço traduzido segue.
 * Conferido pelo oráculo scripts_edge/spu_interp_c.py e pela conferência dos jobs. GOWA_EDGE_NATIVO=1. */
#ifndef GOWA_FN_0000D640_H
#define GOWA_FN_0000D640_H
#include <stdlib.h>
#include <string.h>
#include <immintrin.h>

#ifndef GOWA_EDGE_NATIVO_ON_DEF
#define GOWA_EDGE_NATIVO_ON_DEF
static int gowa_edge_nativo_on(void)
{
    static int on = -1;
    if (on < 0) { const char* e = getenv("GOWA_EDGE_NATIVO"); on = !(e && *e == '0'); }   /* 30/10 (E7): ligado por padrão; =0 desliga */
    return on;
}
#endif

static inline int d640_splat(const u128 v, uint32_t w) { return v._u32[0] == w && v._u32[1] == w && v._u32[2] == w && v._u32[3] == w; }
static inline uint32_t d640_be16(const uint8_t* p) { return ((uint32_t)p[0] << 8) | p[1]; }

/* A decisão em float32 com arredondamento para zero em cada operação (igual à SPU): x + 0,5, as
 * diferenças, o produto e o fms fundido. AVX-512: 16 triângulos por vez, arredondamento embutido; build
 * portátil (AVX2): 8 por vez, com o MXCSR da thread da SPU em para-zero (SPU_SX_RZ). DAZ/FTZ vêm do MXCSR
 * das threads da SPU. Devolve 0 (vai ao traduzido) se algum vértice tiver o expoente 255 (número na SPU,
 * infinito no x86) ou |x|, |y| >= 2^30 (o cflts satura diferente do cvttps). */
#if defined(__AVX512F__) && defined(__AVX512BW__)
#define D640_VETOR 1
#define D640_L 16
#define D640_RZ (_MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC)
static inline __m512i d640_bswap32(__m512i v)
{
    return _mm512_shuffle_epi8(v, _mm512_set4_epi32(0x0C0D0E0F, 0x08090A0B, 0x04050607, 0x00010203));
}
static inline __m512i d640_ld(const uint8_t* ls, __m512i off) { return d640_bswap32(_mm512_i32gather_epi32(off, (const int*)ls, 1)); }
#elif defined(SPU_SX_RZ) && defined(__AVX2__) && defined(__FMA__)
#define D640_VETOR 1
#define D640_L 8
static inline __m256i d640_ld(const uint8_t* ls, __m256i off)
{
    return _mm256_shuffle_epi8(_mm256_i32gather_epi32((const int*)ls, off, 1),
                               _mm256_setr_epi8(3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12,
                                                3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12));
}
#else
#define D640_VETOR 0
#endif

#if D640_VETOR
/* bits de saída de 4 triângulos (bit k = k-ésimo) -> máscara do grupo (bit 3 = 1o triângulo) */
static const uint8_t d640_inv4[16] = {0, 8, 4, 12, 2, 10, 6, 14, 1, 9, 5, 13, 3, 11, 7, 15};

static int d640_decide(const uint8_t* ls, uint32_t ib, uint32_t pb, uint32_t nt, uint32_t ng, uint8_t* mascara)
{
    const uint32_t total = 4u * ng;   /* inclui os que faltam no último grupo */
#if D640_L == 16
    static const uint16_t perm[3][32] = {
        {0, 3, 6, 9, 12, 15, 18, 21, 24, 27, 30, 33, 36, 39, 42, 45},
        {1, 4, 7, 10, 13, 16, 19, 22, 25, 28, 31, 34, 37, 40, 43, 46},
        {2, 5, 8, 11, 14, 17, 20, 23, 26, 29, 32, 35, 38, 41, 44, 47}};
    const __m512i pa = _mm512_loadu_si512(perm[0]), pbv = _mm512_loadu_si512(perm[1]), pc = _mm512_loadu_si512(perm[2]);
    const __m512i bs16 = _mm512_set4_epi32(0x0E0F0C0D, 0x0A0B0809, 0x06070405, 0x02030001);
    const __m512i base = _mm512_set1_epi32((int)pb), mls = _mm512_set1_epi32((int)(SPU_LS_MASK & ~15u));
    const __m512i quatro = _mm512_set1_epi32(4), oito = _mm512_set1_epi32(8);
    const __m512i absm = _mm512_set1_epi32(0x7FFFFFFF), lim = _mm512_set1_epi32(0x4E800000);
    const __m512 meio = _mm512_set1_ps(0.5f);
    for (uint32_t t0 = 0; t0 < total; t0 += 16) {
        const uint8_t* ix = &ls[ib + 6u * t0];                       /* 96 bytes: 48 índices u16 big-endian */
        const __m512i w0 = _mm512_shuffle_epi8(_mm512_loadu_si512(ix), bs16);
        const __m512i w1 = _mm512_shuffle_epi8(_mm512_loadu_si512(ix + 64), bs16);
        const __m512i oa = _mm512_and_si512(_mm512_add_epi32(base, _mm512_slli_epi32(_mm512_cvtepu16_epi32(_mm512_castsi512_si256(_mm512_permutex2var_epi16(w0, pa, w1))), 4)), mls);
        const __m512i ob = _mm512_and_si512(_mm512_add_epi32(base, _mm512_slli_epi32(_mm512_cvtepu16_epi32(_mm512_castsi512_si256(_mm512_permutex2var_epi16(w0, pbv, w1))), 4)), mls);
        const __m512i oc = _mm512_and_si512(_mm512_add_epi32(base, _mm512_slli_epi32(_mm512_cvtepu16_epi32(_mm512_castsi512_si256(_mm512_permutex2var_epi16(w0, pc, w1))), 4)), mls);
        const __mmask16 validos = (t0 + 16u <= nt) ? (__mmask16)0xFFFF : (t0 >= nt ? (__mmask16)0 : (__mmask16)((1u << (nt - t0)) - 1u));
        const __m512i xa = d640_ld(ls, oa), xb = d640_ld(ls, ob), xc = d640_ld(ls, oc);
        const __m512i ya = d640_ld(ls, _mm512_add_epi32(oa, quatro)), yb = d640_ld(ls, _mm512_add_epi32(ob, quatro)), yc = d640_ld(ls, _mm512_add_epi32(oc, quatro));
        const __m512i fa = d640_ld(ls, _mm512_add_epi32(oa, oito)), fb = d640_ld(ls, _mm512_add_epi32(ob, oito)), fc = d640_ld(ls, _mm512_add_epi32(oc, oito));
        const __mmask16 ruim = validos & (_mm512_cmpge_epi32_mask(_mm512_and_si512(xa, absm), lim) | _mm512_cmpge_epi32_mask(_mm512_and_si512(xb, absm), lim) |
                                          _mm512_cmpge_epi32_mask(_mm512_and_si512(xc, absm), lim) | _mm512_cmpge_epi32_mask(_mm512_and_si512(ya, absm), lim) |
                                          _mm512_cmpge_epi32_mask(_mm512_and_si512(yb, absm), lim) | _mm512_cmpge_epi32_mask(_mm512_and_si512(yc, absm), lim));
        if (ruim) return 0;
        const __m512i A = _mm512_and_si512(fa, _mm512_and_si512(fb, fc)), O = _mm512_or_si512(fa, _mm512_or_si512(fb, fc));
        const __mmask16 sai0 = _mm512_test_epi32_mask(A, _mm512_set1_epi32(0xFC));
        const __mmask16 chave = _mm512_testn_epi32_mask(O, quatro) & _mm512_test_epi32_mask(A, _mm512_set1_epi32(2));
        const __m512 Xa = _mm512_castsi512_ps(xa), Xb = _mm512_castsi512_ps(xb), Xc = _mm512_castsi512_ps(xc);
        const __m512 Ya = _mm512_castsi512_ps(ya), Yb = _mm512_castsi512_ps(yb), Yc = _mm512_castsi512_ps(yc);
        const __m512i ga = _mm512_cvttps_epi32(_mm512_add_round_ps(Xa, meio, D640_RZ)), gb = _mm512_cvttps_epi32(_mm512_add_round_ps(Xb, meio, D640_RZ)),
                      gc = _mm512_cvttps_epi32(_mm512_add_round_ps(Xc, meio, D640_RZ));
        const __m512i ha = _mm512_cvttps_epi32(_mm512_add_round_ps(Ya, meio, D640_RZ)), hb = _mm512_cvttps_epi32(_mm512_add_round_ps(Yb, meio, D640_RZ)),
                      hc = _mm512_cvttps_epi32(_mm512_add_round_ps(Yc, meio, D640_RZ));
        const __mmask16 zero = (_mm512_cmpeq_epi32_mask(ga, gb) & _mm512_cmpeq_epi32_mask(gb, gc)) |
                               (_mm512_cmpeq_epi32_mask(ha, hb) & _mm512_cmpeq_epi32_mask(hb, hc));
        const __m512 ex1 = _mm512_sub_round_ps(Xb, Xa, D640_RZ), ey1 = _mm512_sub_round_ps(Yb, Ya, D640_RZ);
        const __m512 ex2 = _mm512_sub_round_ps(Xc, Xa, D640_RZ), ey2 = _mm512_sub_round_ps(Yc, Ya, D640_RZ);
        const __m512 cr = _mm512_fmsub_round_ps(ex1, ey2, _mm512_mul_round_ps(ey1, ex2, D640_RZ), D640_RZ);
        const __mmask16 costas = _mm512_cmp_ps_mask(cr, _mm512_setzero_ps(), _CMP_LT_OQ);
        const uint32_t sai = (uint32_t)((sai0 | (chave & (zero | costas))) | (__mmask16)~validos);
        for (uint32_t q = 0; q < 4 && t0 / 4u + q < ng; q++) mascara[t0 / 4u + q] = d640_inv4[(sai >> (4 * q)) & 15u];
    }
#else
    static SPU_THREAD_LOCAL uint32_t off[3][8192 + 32];
    for (uint32_t t = 0; t < total + D640_L; t++)
        for (int v = 0; v < 3; v++)
            off[v][t] = t < nt ? ((pb + 16u * d640_be16(&ls[ib + 6u * t + 2u * v])) & SPU_LS_MASK & ~15u) : (pb & SPU_LS_MASK & ~15u);
    const __m256i quatro = _mm256_set1_epi32(4), oito = _mm256_set1_epi32(8), zi = _mm256_setzero_si256();
    const __m256i absm = _mm256_set1_epi32(0x7FFFFFFF), lim = _mm256_set1_epi32(0x4E7FFFFF);
    const __m256 meio = _mm256_set1_ps(0.5f);
    for (uint32_t t0 = 0; t0 < total; t0 += 8) {
        const __m256i oa = _mm256_loadu_si256((const __m256i*)&off[0][t0]), ob = _mm256_loadu_si256((const __m256i*)&off[1][t0]),
                      oc = _mm256_loadu_si256((const __m256i*)&off[2][t0]);
        const __m256i xa = d640_ld(ls, oa), xb = d640_ld(ls, ob), xc = d640_ld(ls, oc);
        const __m256i ya = d640_ld(ls, _mm256_add_epi32(oa, quatro)), yb = d640_ld(ls, _mm256_add_epi32(ob, quatro)), yc = d640_ld(ls, _mm256_add_epi32(oc, quatro));
        const __m256i fa = d640_ld(ls, _mm256_add_epi32(oa, oito)), fb = d640_ld(ls, _mm256_add_epi32(ob, oito)), fc = d640_ld(ls, _mm256_add_epi32(oc, oito));
        const __m256i r1 = _mm256_or_si256(_mm256_or_si256(_mm256_cmpgt_epi32(_mm256_and_si256(xa, absm), lim), _mm256_cmpgt_epi32(_mm256_and_si256(xb, absm), lim)),
                                           _mm256_or_si256(_mm256_cmpgt_epi32(_mm256_and_si256(xc, absm), lim), _mm256_cmpgt_epi32(_mm256_and_si256(ya, absm), lim)));
        const __m256i r2 = _mm256_or_si256(r1, _mm256_or_si256(_mm256_cmpgt_epi32(_mm256_and_si256(yb, absm), lim), _mm256_cmpgt_epi32(_mm256_and_si256(yc, absm), lim)));
        const int validos = t0 + 8u <= nt ? 0xFF : (t0 >= nt ? 0 : (int)((1u << (nt - t0)) - 1u));
        if (_mm256_movemask_ps(_mm256_castsi256_ps(r2)) & validos) return 0;
        const __m256i A = _mm256_and_si256(fa, _mm256_and_si256(fb, fc)), O = _mm256_or_si256(fa, _mm256_or_si256(fb, fc));
        const __m256i sai0 = _mm256_xor_si256(_mm256_cmpeq_epi32(_mm256_and_si256(A, _mm256_set1_epi32(0xFC)), zi), _mm256_set1_epi32(-1));
        const __m256i chave = _mm256_andnot_si256(_mm256_cmpeq_epi32(_mm256_and_si256(A, _mm256_set1_epi32(2)), zi),
                                                  _mm256_cmpeq_epi32(_mm256_and_si256(O, quatro), zi));
        const __m256 Xa = _mm256_castsi256_ps(xa), Xb = _mm256_castsi256_ps(xb), Xc = _mm256_castsi256_ps(xc);
        const __m256 Ya = _mm256_castsi256_ps(ya), Yb = _mm256_castsi256_ps(yb), Yc = _mm256_castsi256_ps(yc);
        /* MXCSR da thread em para-zero (SPU_SX_RZ): as operações comuns já arredondam para zero */
        const __m256i ga = _mm256_cvttps_epi32(_mm256_add_ps(Xa, meio)), gb = _mm256_cvttps_epi32(_mm256_add_ps(Xb, meio)), gc = _mm256_cvttps_epi32(_mm256_add_ps(Xc, meio));
        const __m256i ha = _mm256_cvttps_epi32(_mm256_add_ps(Ya, meio)), hb = _mm256_cvttps_epi32(_mm256_add_ps(Yb, meio)), hc = _mm256_cvttps_epi32(_mm256_add_ps(Yc, meio));
        const __m256i zero = _mm256_or_si256(_mm256_and_si256(_mm256_cmpeq_epi32(ga, gb), _mm256_cmpeq_epi32(gb, gc)),
                                             _mm256_and_si256(_mm256_cmpeq_epi32(ha, hb), _mm256_cmpeq_epi32(hb, hc)));
        const __m256 ex1 = _mm256_sub_ps(Xb, Xa), ey1 = _mm256_sub_ps(Yb, Ya), ex2 = _mm256_sub_ps(Xc, Xa), ey2 = _mm256_sub_ps(Yc, Ya);
        const __m256 cr = _mm256_fmsub_ps(ex1, ey2, _mm256_mul_ps(ey1, ex2));
        const __m256i costas = _mm256_castps_si256(_mm256_cmp_ps(cr, _mm256_setzero_ps(), _CMP_LT_OQ));
        const __m256i sv = _mm256_or_si256(sai0, _mm256_and_si256(chave, _mm256_or_si256(zero, costas)));
        const uint32_t sai = (uint32_t)(_mm256_movemask_ps(_mm256_castsi256_ps(sv)) | (~validos & 0xFF));
        for (uint32_t q = 0; q < 2 && t0 / 4u + q < ng; q++) mascara[t0 / 4u + q] = d640_inv4[(sai >> (4 * q)) & 15u];
    }
#endif
    return 1;
}
#endif

static __attribute__((noinline)) int fn_0000D640_laco_nativo(spu_context* ctx)
{
    if (!D640_VETOR || !gowa_edge_nativo_on()) return 0;
#if D640_VETOR
    u128* g = ctx->gpr;
    /* o modo medido: escala 1, +0,5, sinal ligado, descarte de costas ligado */
    if (!d640_splat(g[39], 0x3F800000u) || !d640_splat(g[38], 0x3F000000u) || !d640_splat(g[63], 0x80000000u) ||
        !d640_splat(g[55], 0xFFFFFFFFu))
        return 0;
    uint8_t* ls = SPU_LS_BASE(ctx);
    const uint32_t c = g[81]._u32[0] & SPU_LS_MASK & ~15u;
    const uint32_t nidx = d640_be16(&ls[c + 0x190]);
    const uint32_t nt = nidx / 3u, ng = (nt + 3u) / 4u;
    const uint32_t ib = g[58]._u32[0] & SPU_LS_MASK, pb = g[29]._u32[0];
    if ((ib & 15u) || g[47]._u32[0] != 0 || g[34]._u32[0] != ng + 2u || ng > 2048u || ib + 24u * ng + 160u > SPU_LS_SIZE)
        return 0;
    /* a decisão de todos os grupos (bit 3 = 1o triângulo; 1 = sai; triângulos que faltam no último = 1) */
    static SPU_THREAD_LOCAL uint8_t mascara[2048 + 4];
    if (!d640_decide(ls, ib, pb, nt, ng, mascara)) return 0;
    /* o caminho dos dados do original (mesma ordem; gb(r7) = a máscara do grupo da volta anterior) */
    u128 r2, r3 = g[3], r4, r5 = g[5], r8 = g[8], r9 = g[9], r10 = g[10], r11, r12 = g[12], r14, r18, r19, r20, r23,
         r25, r28, r30 = g[30], r31 = g[31], r32 = g[32], r33 = g[33], r34 = g[34], r41 = g[41], r46 = g[46],
         r47 = g[47], r48 = g[48], r51, r52, r54 = g[54], r64 = g[64], r72, r73, r75, r76, r77, r78, r82;
    const u128 r35 = g[35], r58 = g[58], r59 = g[59], r60 = g[60], r61 = g[61], r62 = g[62], r65 = g[65],
               r66 = g[66], r67 = g[67], r68 = g[68], r69 = g[69], r71 = g[71];
    for (uint32_t volta = 0;; volta++) {
        r51 = spu_or(r31, r5);
        r8 = spu_ls_read128(ctx, r61._u32[0] + r8._u32[0]);
        r75 = spu_shli(r51, 4);
        r51 = spu_shufb(r32, r32, r48);
        r82 = spu_a(r33, r9);
        r73 = spu_rotqmby(r71, r3);
        r52 = spu_shufb(r54, r32, r48);
        r77 = spu_ls_read128(ctx, r82._u32[0] + 0x0);
        r28 = spu_cgt(r82, r65);
        r14 = spu_ls_read128(ctx, r62._u32[0] + r75._u32[0]);
        r5 = spu_selb(r69, r8, r73);
        r20 = spu_ls_read128(ctx, r60._u32[0] + r75._u32[0]);
        r33 = spu_shlqbyi(r31, 4);
        r76 = spu_selb(r82, r67, r28);
        r32 = spu_ai(r47, 24);
        r4 = r76;
        r47 = r32;
        r78 = spu_shufb(r10, r12, r14);
        r19 = spu_shufb(r12, r12, r20);
        r31 = r33;
        r33 = r82;
        r18 = spu_shufb(r77, r78, r5);
        r25 = spu_shufb(r78, r19, r8);
        r12 = spu_shufb(r19, r19, r8);
        r23 = spu_rotqbyi(r76, 4);
        r32 = spu_ls_read128(ctx, r59._u32[0] + r32._u32[0]);
        r54 = spu_ls_read128(ctx, r58._u32[0] + r47._u32[0]);
        spu_ls_write128(ctx, r33._u32[0] + 0x0, r18);
        spu_ls_write128(ctx, r4._u32[0] + 0x10, r25);
        spu_ls_write128(ctx, r23._u32[0] + 0x20, r12);
        r11 = spu_cntb(r75);
        r48 = spu_xor(r48, r68);
        r11 = spu_shufb(r11, r35, r35);
        r73 = spu_sfi(r11, 4);
        r9 = spu_mpyui(r73, 6);
        r2 = spu_ai(r34, -1);
        r34 = r2;
        r78 = spu_a(r41, r9);
        r28 = spu_il(0);
        r28._u32[0] = (volta >= 1 && volta - 1 < ng) ? mascara[volta - 1] : 0;   /* gb(r7) */
        r75 = spu_ceqi(r2, 1);
        r41 = spu_andi(r78, 15);
        r12 = spu_and(r66, r75);
        r72 = spu_a(r46, r73);
        r8 = spu_shli(r41, 4);
        r5 = spu_or(r12, r28);
        r46 = r72;
        r12 = r30;
        r10 = r64;
        r3 = spu_sfi(r41, 0);
        if (r2._u32[0] == 0) break;
        r64 = r52;
        r30 = r51;
    }
    /* o que o resto da D640 usa depois do laço: r46 (triângulos que ficaram); r1/r81 não mudam. Os
     * temporários do caminho dos dados voltam com o último valor (os da decisão em float, não: o
     * descarte não os lê depois). */
    g[46] = r46; g[2] = r2; g[3] = r3; g[4] = r4; g[5] = r5; g[8] = r8; g[9] = r9; g[10] = r10; g[11] = r11;
    g[12] = r12; g[30] = r30; g[31] = r31; g[32] = r32; g[33] = r33; g[34] = r34; g[41] = r41; g[47] = r47;
    g[48] = r48; g[54] = r54; g[64] = r64; g[51] = r51; g[52] = r52; g[72] = r72; g[73] = r73; g[75] = r75;
    return 1;
#else
    (void)ctx;
    return 0;
#endif
}
#endif
