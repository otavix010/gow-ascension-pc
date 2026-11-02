/* GOWA (29/10, EDGE nativo, marco E3): o plugin 0x74C580 do EDGE escrito para o PC.
 *
 * O plugin empacota os vértices de um segmento no formato que vai para a RSX: passo de 54 bytes,
 * a partir dos fluxos float4 (16 B por vértice) de cada atributo. O contexto do segmento está em
 * r3 (lista de atributos em +0x160, ponteiros dos fluxos em +0x10), r4 = vértices, r5 = destino.
 * Por vértice:
 *    0..11  atributo 1 (posição): float3 copiado
 *   12..19  atributo 3: 4 x int16 normalizado, (x * 32767,5 - 0,5) truncado (fma + cflts 157)
 *   20..23  atributo 4: 11:11:10 com sinal, ((x * s + b) * 2^31) truncado, os bits altos de cada
 *   24..35  atributo 27: float3 copiado
 *   36..53  atributos 28, 29 e 30: meia precisão truncada (satura em 65504, zera abaixo de 2^-15)
 * No código da SPU isso é um laço em software pipeline de 2 vértices por volta; aqui é um vértice
 * por vez com as mesmas primitivas de float (spu_fma/spu_cflts deste arquivo traduzido), então o
 * resultado é igual bit a bit. Também refaz o que a conferência compara na LS: os registradores
 * salvos na pilha e o bloco de 16 bytes que o original escreve além do fim (4 zeros + os últimos
 * 12 bytes). Os registradores temporários (r2-r79) ficam diferentes: o EDGE não os lê depois.
 *
 * Só cobre o caso medido em todas as chamadas: destino alinhado em 16, vértices múltiplos de 8 e
 * os 7 atributos presentes; o resto volta 0 e o código traduzido segue.
 * GOWA_EDGE_NATIVO=0 desliga (ligado por padrão desde 30/10). Modelo e conferência em
 * scripts_edge/pl_modelo.py; gancho aplicado por scripts_edge/edge_nativo.py. */
#ifndef GOWA_PL_0074C580_H
#define GOWA_PL_0074C580_H
#include <stdlib.h>
#include <string.h>

static int gowa_edge_nativo_on(void)
{
    static int on = -1;
    if (on < 0) { const char* e = getenv("GOWA_EDGE_NATIVO"); on = !(e && *e == '0'); }   /* 30/10 (E7): ligado por padrão; =0 desliga */
    return on;
}

static inline uint32_t pl_be32(const uint8_t* p) { uint32_t w; memcpy(&w, p, 4); return __builtin_bswap32(w); }
static inline void pl_put16(uint8_t* p, uint32_t v) { const uint16_t h = __builtin_bswap16((uint16_t)v); memcpy(p, &h, 2); }
static inline void pl_put32(uint8_t* p, uint32_t v) { const uint32_t w = __builtin_bswap32(v); memcpy(p, &w, 4); }
/* float -> meia precisão como o plugin faz com inteiros (sf 0x38000000, rotqbii 3, clgth, selb) */
static inline uint32_t pl_half(uint32_t w)
{
    const uint32_t a = (w >> 16) & 0x7FFFu, sinal = (w >> 16) & 0x8000u;
    if (a < 0x3800u) return 0;
    if (a >= 0x4780u) return 0x7BFFu | sinal;
    return (((w - 0x38000000u) >> 13) & 0x7FFFu) | sinal;
}

/* GOWA_EDGE_NATIVO_LOG=1: conta as chamadas feitas aqui e as recusadas (vão ao traduzido) */
static volatile long g_pl74_feitas, g_pl74_recusadas;
static int pl74_recusa(void)
{
    static int log = -1;
    if (log < 0) log = getenv("GOWA_EDGE_NATIVO_LOG") != NULL;
    if (log) {
        const long k = _InterlockedIncrement(&g_pl74_recusadas);
        if ((k & (k - 1)) == 0)
            fprintf(stderr, "[edge-nativo] pl 0x74C580: %ld recusadas de %ld entradas\n", k, g_pl74_feitas);
    }
    return 0;
}

/* As contas de float sem as primitivas genéricas: y = x * m + a com arredondamento para zero, e
 * depois cflts (multiplica por 2^k, trunca, satura). O produto x * m é exato em double (24 + 16 bits);
 * a soma arredondada para zero em double e depois para float dá o mesmo que arredondar para zero
 * direto (truncar duas vezes é truncar uma). Entradas: denormal = 0 (DAZ da SPU); |x| >= 2^16
 * vira +-2^16 (cobre o expoente 255, que na SPU é número: todos esses saturam no cflts). */
#if defined(__AVX512F__) || (defined(SPU_SX_RZ) && defined(__AVX2__))
#include <immintrin.h>
#define PL_RAPIDO 1
/* 8 faixas = 2 vértices x 4 componentes. Palavras da LS (big-endian) para o host. */
static inline __m256i pl_bswap32x8(__m256i v)
{
    return _mm256_shuffle_epi8(v, _mm256_setr_epi8(3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12,
                                                   3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12));
}
static inline __m256i pl_carrega2(const uint8_t* p) { return pl_bswap32x8(_mm256_loadu_si256((const __m256i*)p)); }
/* entrada do fma: denormal -> 0 (DAZ da SPU); |x| >= 2^16 -> +-2^16 (inclui o expoente 255) */
static inline __m256 pl_entrada8(__m256i w)
{
    const __m256i sinal = _mm256_and_si256(w, _mm256_set1_epi32((int)0x80000000u));
    const __m256i e = _mm256_and_si256(_mm256_srli_epi32(w, 23), _mm256_set1_epi32(0xFF));
    w = _mm256_blendv_epi8(w, sinal, _mm256_cmpeq_epi32(e, _mm256_setzero_si256()));
    w = _mm256_blendv_epi8(w, _mm256_or_si256(sinal, _mm256_set1_epi32(0x47800000)),
                           _mm256_cmpgt_epi32(e, _mm256_set1_epi32(127 + 15)));
    return _mm256_castsi256_ps(w);
}
/* cflts(RZ(x * m + a), k): o produto é exato em double; soma e conversão para float arredondam para
 * zero (embutido no AVX-512; no AVX2 é o MXCSR da thread da SPU, SPU_SX_RZ); depois trunca e satura. */
static inline __m256i pl_fma_cflts8(__m256 x, const double* m8, const double* a8, double escala)
{
#if defined(__AVX512F__)
    const __m512d s = _mm512_add_round_pd(_mm512_mul_pd(_mm512_cvtps_pd(x), _mm512_loadu_pd(m8)),
                                          _mm512_loadu_pd(a8), _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC);
    const __m256 y = _mm512_cvt_roundpd_ps(s, _MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC);
    __m512d v = _mm512_mul_pd(_mm512_cvtps_pd(y), _mm512_set1_pd(escala));
    v = _mm512_min_pd(_mm512_max_pd(v, _mm512_set1_pd(-2147483648.0)), _mm512_set1_pd(2147483647.0));
    return _mm512_cvttpd_epi32(v);
#else
    const __m256d sl = _mm256_add_pd(_mm256_mul_pd(_mm256_cvtps_pd(_mm256_castps256_ps128(x)), _mm256_loadu_pd(m8)), _mm256_loadu_pd(a8));
    const __m256d sh = _mm256_add_pd(_mm256_mul_pd(_mm256_cvtps_pd(_mm256_extractf128_ps(x, 1)), _mm256_loadu_pd(m8 + 4)), _mm256_loadu_pd(a8 + 4));
    const __m256d e = _mm256_set1_pd(escala), lo = _mm256_set1_pd(-2147483648.0), hi = _mm256_set1_pd(2147483647.0);
    const __m256d vl = _mm256_min_pd(_mm256_max_pd(_mm256_mul_pd(_mm256_cvtps_pd(_mm256_cvtpd_ps(sl)), e), lo), hi);
    const __m256d vh = _mm256_min_pd(_mm256_max_pd(_mm256_mul_pd(_mm256_cvtps_pd(_mm256_cvtpd_ps(sh)), e), lo), hi);
    return _mm256_set_m128i(_mm256_cvttpd_epi32(vh), _mm256_cvttpd_epi32(vl));
#endif
}
/* meia precisão como o plugin (inteiros: sf 0x38000000, >> 13, satura e zera pelos 16 bits altos) */
static inline __m256i pl_half8(__m256i w)
{
    const __m256i alto = _mm256_srli_epi32(w, 16);
    const __m256i a = _mm256_and_si256(alto, _mm256_set1_epi32(0x7FFF));
    const __m256i sinal = _mm256_and_si256(alto, _mm256_set1_epi32(0x8000));
    __m256i h = _mm256_or_si256(_mm256_and_si256(_mm256_srli_epi32(_mm256_sub_epi32(w, _mm256_set1_epi32(0x38000000)), 13),
                                                 _mm256_set1_epi32(0x7FFF)), sinal);
    h = _mm256_blendv_epi8(h, _mm256_or_si256(sinal, _mm256_set1_epi32(0x7BFF)), _mm256_cmpgt_epi32(a, _mm256_set1_epi32(0x477F)));
    return _mm256_andnot_si256(_mm256_cmpgt_epi32(_mm256_set1_epi32(0x3800), a), h);
}
/* os 16 bits baixos (b = 0) ou altos (b = 2) de cada faixa, big-endian: 8 bytes por vértice no começo
 * de cada metade de 128 bits */
static inline __m256i pl_meias(__m256i v, int altos)
{
    return altos ? _mm256_shuffle_epi8(v, _mm256_setr_epi8(3, 2, 7, 6, 11, 10, 15, 14, -1, -1, -1, -1, -1, -1, -1, -1,
                                                           3, 2, 7, 6, 11, 10, 15, 14, -1, -1, -1, -1, -1, -1, -1, -1))
                 : _mm256_shuffle_epi8(v, _mm256_setr_epi8(1, 0, 5, 4, 9, 8, 13, 12, -1, -1, -1, -1, -1, -1, -1, -1,
                                                           1, 0, 5, 4, 9, 8, 13, 12, -1, -1, -1, -1, -1, -1, -1, -1));
}
#endif

static int pl_0074C580_nativo(spu_context* ctx)
{
    if (!gowa_edge_nativo_on()) return 0;
    { static int log = -1;
      if (log < 0) log = getenv("GOWA_EDGE_NATIVO_LOG") != NULL;
      if (log) {
          const long k = _InterlockedIncrement(&g_pl74_feitas);
          if ((k & (k - 1)) == 0)
              fprintf(stderr, "[edge-nativo] pl 0x74C580: %ld entradas, %ld recusadas\n", k, g_pl74_recusadas);
      } }
    uint8_t* ls = SPU_LS_BASE(ctx);
    const uint32_t r1 = ctx->gpr[1]._u32[0] & SPU_LS_MASK;
    const uint32_t r3 = ctx->gpr[3]._u32[0] & SPU_LS_MASK;
    const uint32_t n = ctx->gpr[4]._u32[0];
    const uint32_t r5 = ctx->gpr[5]._u32[0] & SPU_LS_MASK;
    if ((r5 & 15u) || (n & 7u) || !n || n > 4096 || r5 + 54u * n + 16u > SPU_LS_SIZE || (r3 & 15u) ||
        r3 + 0x170u > SPU_LS_SIZE || r1 < 0x300u)
        return pl74_recusa();
    /* os fluxos: a primeira posição de cada id na lista (ceqbi + gbb + clz) */
    static const uint8_t ids[7] = {1, 3, 4, 27, 28, 29, 30};
    uint32_t p[7];
    for (int i = 0; i < 7; i++) {
        int k = 0;
        while (k < 16 && ls[r3 + 0x160 + k] != ids[i]) k++;
        if (k == 16) return pl74_recusa();
        p[i] = pl_be32(&ls[r3 + 0x10 + 4 * k]) & SPU_LS_MASK & ~15u;
        if (p[i] + 16u * n > SPU_LS_SIZE) return pl74_recusa();
    }
    /* a pilha: o que o prólogo do original salva (r80-r117, r126 e o r1 antigo) */
    static const struct { int8_t r; uint16_t off; } salvos[] = {
        {80, 0x10}, {81, 0x20}, {82, 0x30}, {83, 0x40}, {84, 0x50}, {85, 0x60}, {86, 0x70}, {87, 0x80},
        {88, 0x90}, {89, 0xA0}, {90, 0xB0}, {91, 0xC0}, {92, 0xD0}, {93, 0xE0}, {94, 0xF0}, {95, 0x100},
        {96, 0x110}, {97, 0x120}, {98, 0x130}, {99, 0x140}, {100, 0x150}, {101, 0x160}, {102, 0x170},
        {103, 0x180}, {104, 0x190}, {105, 0x1A0}, {106, 0x1B0}, {107, 0x1C0}, {108, 0x1D0}, {109, 0x1E0},
        {110, 0x1F0}, {111, 0x200}, {112, 0x210}, {113, 0x220}, {114, 0x230}, {115, 0x240}, {116, 0x250},
        {117, 0x260}, {126, 0x270}, {1, 0x290}};
    for (size_t i = 0; i < sizeof salvos / sizeof salvos[0]; i++)
        spu_ls_write128(ctx, (r1 - salvos[i].off) & ~15u, ctx->gpr[salvos[i].r]);

    uint8_t* o = &ls[r5];
#if PL_RAPIDO
    /* dois vértices por volta; cada campo é gravado num bloco cheio em ordem crescente e o campo
     * seguinte corrige o que passou (o último registro passa 2 bytes do fim: a cauda cobre) */
    static const double M3[8] = {32767.5, 32767.5, 32767.5, 32767.5, 32767.5, 32767.5, 32767.5, 32767.5};
    static const double A3[8] = {-0.5, -0.5, -0.5, -0.5, -0.5, -0.5, -0.5, -0.5};
    static const double M4[8] = {0.99951171875, 0.99951171875, 0.9990234375, 0, 0.99951171875, 0.99951171875, 0.9990234375, 0};
    static const double A4[8] = {-0.00048828125, -0.00048828125, -0.0009765625, 0, -0.00048828125, -0.00048828125, -0.0009765625, 0};
    const __m256i desl4 = _mm256_setr_epi32(21, 21, 22, 32, 21, 21, 22, 32);
    const __m256i mask4 = _mm256_setr_epi32(0x7FF, 0x7FF, 0x3FF, 0, 0x7FF, 0x7FF, 0x3FF, 0);
    const __m256i pos4 = _mm256_setr_epi32(0, 11, 22, 0, 0, 11, 22, 0);
    for (uint32_t v = 0; v < n; v += 2, o += 108) {
        const uint32_t d = 16u * v;
        const __m256i t3 = pl_meias(pl_fma_cflts8(pl_entrada8(pl_carrega2(&ls[p[1] + d])), M3, A3, 65536.0), 1);
        __m256i f4 = pl_fma_cflts8(pl_entrada8(pl_carrega2(&ls[p[2] + d])), M4, A4, 2147483648.0);
        f4 = _mm256_sllv_epi32(_mm256_and_si256(_mm256_srlv_epi32(f4, desl4), mask4), pos4);
        f4 = _mm256_or_si256(f4, _mm256_shuffle_epi32(f4, 0xF5));   /* faixa 0 |= 1, 2 |= 3 */
        f4 = _mm256_or_si256(f4, _mm256_shuffle_epi32(f4, 0xAA));   /* faixa 0 |= 2 */
        const __m256i h28 = pl_meias(pl_half8(pl_carrega2(&ls[p[4] + d])), 0);
        const __m256i h29 = pl_meias(pl_half8(pl_carrega2(&ls[p[5] + d])), 0);
        const __m256i h30 = pl_meias(pl_half8(pl_carrega2(&ls[p[6] + d])), 0);
        for (int k = 0; k < 2; k++) {
            uint8_t* r = o + 54 * k;
            const uint32_t dk = d + 16u * (uint32_t)k;
            _mm_storeu_si128((__m128i*)r, _mm_loadu_si128((const __m128i*)&ls[p[0] + dk]));   /* posição */
            _mm_storel_epi64((__m128i*)(r + 12), k ? _mm256_extracti128_si256(t3, 1) : _mm256_castsi256_si128(t3));
            pl_put32(r + 20, (uint32_t)(k ? _mm256_extract_epi32(f4, 4) : _mm256_extract_epi32(f4, 0)));
            _mm_storeu_si128((__m128i*)(r + 24), _mm_loadu_si128((const __m128i*)&ls[p[3] + dk]));   /* atributo 27 */
            _mm_storel_epi64((__m128i*)(r + 36), k ? _mm256_extracti128_si256(h28, 1) : _mm256_castsi256_si128(h28));
            _mm_storel_epi64((__m128i*)(r + 42), k ? _mm256_extracti128_si256(h29, 1) : _mm256_castsi256_si128(h29));
            _mm_storel_epi64((__m128i*)(r + 48), k ? _mm256_extracti128_si256(h30, 1) : _mm256_castsi256_si128(h30));
        }
    }
#else
    u128 m3, a3, s4, b4;
    for (int c = 0; c < 4; c++) { m3._u32[c] = 0x46FFFF00u; a3._u32[c] = 0xBF000000u; }
    s4._u32[0] = 0x3F7FE000u; s4._u32[1] = 0x3F7FE000u; s4._u32[2] = 0x3F7FC000u; s4._u32[3] = 0;
    b4._u32[0] = 0xBA000000u; b4._u32[1] = 0xBA000000u; b4._u32[2] = 0xBA800000u; b4._u32[3] = 0;
    for (uint32_t v = 0; v < n; v++, o += 54) {
        const uint32_t d = 16u * v;
        memcpy(o, &ls[p[0] + d], 12);                                   /* posição */
        const u128 t3 = spu_cflts(spu_fma(spu_ls_read128(ctx, p[1] + d), m3, a3), 157);
        for (int c = 0; c < 4; c++) pl_put16(o + 12 + 2 * c, t3._u32[c] >> 16);
        const u128 t4 = spu_cflts(spu_fma(spu_ls_read128(ctx, p[2] + d), s4, b4), 142);
        pl_put32(o + 20, ((t4._u32[0] >> 21) & 0x7FFu) | (((t4._u32[1] >> 21) & 0x7FFu) << 11) |
                         (((t4._u32[2] >> 22) & 0x3FFu) << 22));
        memcpy(o + 24, &ls[p[3] + d], 12);                              /* atributo 27 */
        for (int i = 0; i < 3; i++) {
            const uint8_t* q = &ls[p[4 + i] + d];
            for (int c = 0; c < 3; c++) pl_put16(o + 36 + 6 * i + 2 * c, pl_half(pl_be32(q + 4 * c)));
        }
    }
#endif
    /* o bloco que o original escreve depois do fim: 4 zeros + os últimos 12 bytes */
    uint8_t cauda[16] = {0};
    memcpy(cauda + 4, o - 12, 12);
    memcpy(o, cauda, 16);
    ctx->gpr[3] = spu_il(0);
    ctx->gpr[64] = spu_il(656);
    return 1;
}
#endif
