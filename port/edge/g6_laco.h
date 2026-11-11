/* GOWA (02/11, tradução do General6 0x727200): o laço 0x63B4 (16 voltas de 4 amostras), pelo traço simbólico do
 * corpo e pelo modelo scripts_edge/banc/g63_modelo.py (40/40 laços iguais às fotos de entrada e saída, LS inteira):
 *   V = os 4 vetores r88, r85, r83, r82, que a cada amostra avançam por M1 (colunas r99, r107, r98, r108):
 *       V'.c = fma(r108.c, V.w, fma(r98.c, V.z, fma(r107.c, V.y, r99.c * V.x)))
 *   W = M2 V (colunas r104, r103, r102, r101, a mesma forma); o ponto da oitava o, coordenada c:
 *       p.c = fma(W82.c, A3.c, fma(W83.c, A2.c, fma(W85.c, A1.c, W88.c * A0.c)))
 *   saída = (((entrada + N1) + N2) + N3) + N4 (N = ruído 0x4930, g6_ruido8t), gravada no lugar (r125)
 *   meia-precisão: f = fma(saída * K0[s], K1.x, K2.x) limitada a [r105.x, r106.x]; h = (E.x * f) * 2^-112;
 *       grava r127.x e r127.y | ((h & 0x80000000) | (h << 3)) >> 16 em r100 + 8s
 * Gancho 'entrada_segue' na 0x63B4: faz as voltas 0..n-2 (de 2 em 2, 8 amostras por conta AVX2), deixa em ctx->gpr
 * os vetores, r100 e r125 e na pilha o contador 0x9B0; a última volta fica no traduzido, que reescreve todos os
 * temporários do corpo (o corpo é linear: só r82, r83, r85, r88, r100, r125 e r39 passam de uma volta à outra) e o
 * quadro da pilha das chamadas do ruído.
 * A thread da SPU roda em para-zero + DAZ (sem FTZ): as contas saem iguais às da SPU, menos o sinal de um resultado
 * zero (a SPU dá sempre +0) e os denormais (a SPU zera): +0 nos lanes de expoente 0 onde os bits importam (as
 * coordenadas do ruído, a saída gravada, o h e os vetores devolvidos). Fora do alcance (|x| >= 2^40 numa entrada ou
 * num intermediário, coordenada do ruído >= 2^23, endereços que se sobrepõem) devolve ao traduzido.
 * GOWA_EDGE_NATIVO=0 ou GOWA_EDGE_SEM=g6_laco desligam. */
#ifndef GOWA_G6_LACO_H
#define GOWA_G6_LACO_H
#include "g6_ruido.h"

#if defined(__AVX2__) && defined(__FMA__)
#define G6L_LIM 0x53FFFFFFu   /* |x| < 2^40 */

static inline uint32_t g6l_be32(const uint8_t* ls, uint32_t a) { return _byteswap_ulong(*(const uint32_t*)(ls + a)); }
static inline void g6l_pbe32(uint8_t* ls, uint32_t a, uint32_t v) { *(uint32_t*)(ls + a) = _byteswap_ulong(v); }
static inline __m128 g6l_reg(spu_context* ctx, int r) { return _mm_loadu_ps((const float*)&ctx->gpr[r]); }
static inline __m256 g6l_z8(__m256 v)   /* +0 nos lanes de expoente 0 */
{
    const __m256i i = _mm256_castps_si256(v);
    const __m256i e = _mm256_and_si256(i, _mm256_set1_epi32(0x7F800000));
    return _mm256_castsi256_ps(_mm256_andnot_si256(_mm256_cmpeq_epi32(e, _mm256_setzero_si256()), i));
}
static inline __m128 g6l_z4(__m128 v)
{
    const __m128i i = _mm_castps_si128(v);
    const __m128i e = _mm_and_si128(i, _mm_set1_epi32(0x7F800000));
    return _mm_castsi128_ps(_mm_andnot_si128(_mm_cmpeq_epi32(e, _mm_setzero_si128()), i));
}
static inline __m128i g6l_abs4(__m128 v) { return _mm_and_si128(_mm_castps_si128(v), _mm_set1_epi32(0x7FFFFFFF)); }
static inline __m256i g6l_abs8(__m256 v) { return _mm256_and_si256(_mm256_castps_si256(v), _mm256_set1_epi32(0x7FFFFFFF)); }
/* a coluna c de uma matriz guardada por colunas: m[0..3] = colunas (a de V.x, V.y, V.z, V.w) */
static inline __m128 g6l_matvec(const __m128* m, __m128 v)
{
    const __m128 x = _mm_shuffle_ps(v, v, 0x00), y = _mm_shuffle_ps(v, v, 0x55);
    const __m128 z = _mm_shuffle_ps(v, v, 0xAA), w = _mm_shuffle_ps(v, v, 0xFF);
    return _mm_fmadd_ps(m[3], w, _mm_fmadd_ps(m[2], z, _mm_fmadd_ps(m[1], y, _mm_mul_ps(m[0], x))));
}

/* nv voltas (1 ou 2) a partir de V; devolve 0 (nada gravado) se algo sair do alcance */
static int g6l_bloco(uint8_t* ls, const g6_tab* tab, int nv, __m128 V[4], uint32_t r125, uint32_t r100,
                     const __m128* M1, const __m128* M2, const float A[4][4][4], const float K0[4],
                     float K1x, float K2x, float E, float lo, float hi, uint32_t w0, uint32_t w1)
{
    /* as 8 amostras: V de cada amostra, na ordem (4 vetores) */
    __m128 Vs[8][4];
    __m128i lim = _mm_setzero_si128();
    for (int s = 0; s < 4 * nv; s++) {
        for (int k = 0; k < 4; k++) { Vs[s][k] = V[k]; V[k] = g6l_matvec(M1, V[k]); lim = _mm_max_epu32(lim, g6l_abs4(V[k])); }
    }
    for (int s = 4 * nv; s < 8; s++) for (int k = 0; k < 4; k++) Vs[s][k] = Vs[s - 4][k];
    /* W = M2 V por amostra, em SoA: Wc[k][c] = lane s */
    __m256 Wc[4][3];
    for (int k = 0; k < 4; k++) {
        float t[3][8];
        for (int s = 0; s < 8; s++) {
            const __m128 w = g6l_matvec(M2, Vs[s][k]);
            lim = _mm_max_epu32(lim, g6l_abs4(w));
            float q[4]; _mm_storeu_ps(q, w);
            t[0][s] = q[0]; t[1][s] = q[1]; t[2][s] = q[2];
        }
        for (int c = 0; c < 3; c++) Wc[k][c] = _mm256_loadu_ps(t[c]);
    }
    if (!_mm_testz_si128(_mm_cmpgt_epi32(lim, _mm_set1_epi32((int)G6L_LIM)), _mm_set1_epi32(-1))) return 0;
    /* entrada: 4 * nv palavras em r125 (big-endian); as duas voltas são contíguas */
    __m256 acc = _mm256_castsi256_ps(g6_be(_mm256_loadu2_m128i((const __m128i*)(ls + r125 + 16 * (nv - 1)), (const __m128i*)(ls + r125))));
    __m256i lim8 = g6l_abs8(acc);
    /* ordem dos vetores na fórmula: W88 (k=0), W85 (1), W83 (2), W82 (3) */
    __m256 N[4];
    __m256i limp = _mm256_setzero_si256();
    __m256 P[4][3];
    for (int o = 0; o < 4; o++) {
        for (int c = 0; c < 3; c++) {
            const __m256 p = _mm256_fmadd_ps(Wc[3][c], _mm256_set1_ps(A[o][3][c]), _mm256_fmadd_ps(Wc[2][c], _mm256_set1_ps(A[o][2][c]),
                             _mm256_fmadd_ps(Wc[1][c], _mm256_set1_ps(A[o][1][c]), _mm256_mul_ps(Wc[0][c], _mm256_set1_ps(A[o][0][c])))));
            P[o][c] = g6l_z8(p);
            limp = _mm256_max_epu32(limp, g6l_abs8(P[o][c]));
        }
    }
    if (!_mm256_testz_si256(_mm256_cmpgt_epi32(limp, _mm256_set1_epi32(0x4AFFFFFF)), _mm256_set1_epi32(-1))) return 0;
    if (!_mm256_testz_si256(_mm256_cmpgt_epi32(lim8, _mm256_set1_epi32((int)G6L_LIM)), _mm256_set1_epi32(-1))) return 0;
    for (int o = 0; o < 4; o++) N[o] = g6_ruido8t(tab, P[o][0], P[o][1], P[o][2]);
    acc = g6l_z8(_mm256_add_ps(_mm256_add_ps(_mm256_add_ps(_mm256_add_ps(acc, N[0]), N[1]), N[2]), N[3]));
    /* meia-precisão */
    const __m256 k0 = _mm256_setr_ps(K0[0], K0[1], K0[2], K0[3], K0[0], K0[1], K0[2], K0[3]);
    __m256 f = _mm256_fmadd_ps(_mm256_mul_ps(acc, k0), _mm256_set1_ps(K1x), _mm256_set1_ps(K2x));
    const __m256 vhi = _mm256_set1_ps(hi), vlo = _mm256_set1_ps(lo);
    f = _mm256_blendv_ps(f, vhi, _mm256_cmp_ps(f, vhi, _CMP_GT_OQ));
    f = _mm256_blendv_ps(vlo, f, _mm256_cmp_ps(f, vlo, _CMP_GT_OQ));
    const __m256 h = g6l_z8(_mm256_mul_ps(_mm256_mul_ps(_mm256_set1_ps(E), f), _mm256_castsi256_ps(_mm256_set1_epi32(0x07800000))));
    const __m256i hi_ = _mm256_castps_si256(h);
    const __m256i half = _mm256_srli_epi32(_mm256_or_si256(_mm256_and_si256(hi_, _mm256_set1_epi32((int)0x80000000)), _mm256_slli_epi32(hi_, 3)), 16);
    /* grava: a saída no lugar e os 8 bytes de cada amostra */
    float out[8]; uint32_t hs[8];
    _mm256_storeu_ps(out, acc); _mm256_storeu_si256((__m256i*)hs, half);
    for (int s = 0; s < 4 * nv; s++) {
        uint32_t ob; memcpy(&ob, &out[s], 4);
        g6l_pbe32(ls, r125 + 4 * s, ob);
        g6l_pbe32(ls, r100 + 8 * s, w0);
        g6l_pbe32(ls, r100 + 8 * s + 4, w1 | hs[s]);
    }
    return 1;
}

static int g6_laco63_nativo(spu_context* ctx)
{
    static int on = -1;
    if (on < 0) { const char* e = getenv("GOWA_EDGE_NATIVO"); on = !(e && *e == '0') && !gowa_edge_sem("g6_laco"); }
    static volatile long conta[2];
    if (!on) return 0;
    uint8_t* ls = (uint8_t*)SPU_LS_BASE(ctx);
    const uint32_t sp = ctx->gpr[1]._u32[0] & 0x3FFF0u;
    if (sp < 0x1000u || sp + 0xC00u > 0x40000u) return 0;
    const uint32_t cnt = g6l_be32(ls, sp + 0x9B0), n = g6l_be32(ls, sp + 0x870);
    const uint32_t rem = n - cnt;
    if (rem < 2 || rem > 4096) { gowa_edge_conta("g6_laco", conta, 0); return 0; }
    const uint32_t k = rem - 1;
    uint32_t r100 = ctx->gpr[100]._u32[0], r125 = ctx->gpr[125]._u32[0];
    /* endereços: alinhados, dentro da LS e sem sobrepor a pilha do laço, as tabelas do ruído nem um ao outro */
    if ((r125 & 15) || (r100 & 7) || r125 + 16 * k > 0x40000u || r100 + 32 * k > 0x40000u) { gowa_edge_conta("g6_laco", conta, 0); return 0; }
    #define G6L_SOBREPOE(a0, a1, b0, b1) ((a0) < (b1) && (b0) < (a1))
    if (G6L_SOBREPOE(r125, r125 + 16 * k, r100, r100 + 32 * k) ||
        G6L_SOBREPOE(r125, r125 + 16 * k, sp + 0x860, sp + 0xB80) || G6L_SOBREPOE(r100, r100 + 32 * k, sp + 0x860, sp + 0xB80) ||
        G6L_SOBREPOE(r125, r125 + 16 * k, G6_GRAD, G6_PERM + 2048) || G6L_SOBREPOE(r100, r100 + 32 * k, G6_GRAD, G6_PERM + 2048)) {
        gowa_edge_conta("g6_laco", conta, 0); return 0;
    }
    #undef G6L_SOBREPOE
    const __m128 M1[4] = { g6l_reg(ctx, 99), g6l_reg(ctx, 107), g6l_reg(ctx, 98), g6l_reg(ctx, 108) };
    const __m128 M2[4] = { g6l_reg(ctx, 104), g6l_reg(ctx, 103), g6l_reg(ctx, 102), g6l_reg(ctx, 101) };
    static const int OIT[4][4] = { { 122, 123, 124, 121 }, { 120, 117, 118, 119 }, { 115, 116, 113, 114 }, { 112, 110, 111, 109 } };
    float A[4][4][4];
    __m128i lim = _mm_setzero_si128();
    for (int o = 0; o < 4; o++) for (int j = 0; j < 4; j++) {
        const __m128 a = g6l_reg(ctx, OIT[o][j]);
        _mm_storeu_ps(A[o][j], a); lim = _mm_max_epu32(lim, g6l_abs4(a));
    }
    for (int j = 0; j < 4; j++) { lim = _mm_max_epu32(lim, g6l_abs4(M1[j])); lim = _mm_max_epu32(lim, g6l_abs4(M2[j])); }
    float K0[4], K1x, K2x, E, lo, hi;
    uint32_t b;
    for (int j = 0; j < 4; j++) { b = g6l_be32(ls, sp + 0x8E0 + 4 * j); memcpy(&K0[j], &b, 4); }
    b = g6l_be32(ls, sp + 0x8D0); memcpy(&K1x, &b, 4);
    b = g6l_be32(ls, sp + 0x8C0); memcpy(&K2x, &b, 4);
    b = g6l_be32(ls, sp + 0x9C0); memcpy(&E, &b, 4);
    memcpy(&lo, &ctx->gpr[105]._u32[0], 4); memcpy(&hi, &ctx->gpr[106]._u32[0], 4);
    lim = _mm_max_epu32(lim, g6l_abs4(_mm_loadu_ps(K0)));
    lim = _mm_max_epu32(lim, g6l_abs4(_mm_setr_ps(K1x, K2x, E, lo)));
    lim = _mm_max_epu32(lim, g6l_abs4(_mm_set1_ps(hi)));
    __m128 V[4] = { g6l_reg(ctx, 88), g6l_reg(ctx, 85), g6l_reg(ctx, 83), g6l_reg(ctx, 82) };
    for (int j = 0; j < 4; j++) lim = _mm_max_epu32(lim, g6l_abs4(V[j]));
    if (!_mm_testz_si128(_mm_cmpgt_epi32(lim, _mm_set1_epi32((int)G6L_LIM)), _mm_set1_epi32(-1))) { gowa_edge_conta("g6_laco", conta, 0); return 0; }
    const uint32_t w0 = ctx->gpr[127]._u32[0], w1 = ctx->gpr[127]._u32[1];
    g6_tab tab;
    g6_tabelas(ls, &tab);
    uint32_t feitas = 0;
    while (feitas < k) {
        const int nv = (k - feitas >= 2) ? 2 : 1;
        __m128 Vn[4] = { V[0], V[1], V[2], V[3] };
        if (!g6l_bloco(ls, &tab, nv, Vn, r125, r100, M1, M2, (const float (*)[4][4])A, K0, K1x, K2x, E, lo, hi, w0, w1)) break;
        for (int j = 0; j < 4; j++) V[j] = Vn[j];
        r125 += 16u * nv; r100 += 32u * nv; feitas += nv;
    }
    if (!feitas) { gowa_edge_conta("g6_laco", conta, 0); return 0; }
    /* devolve: vetores (+0 nos zeros), ponteiros (as 4 palavras, como o ai) e o contador (as 4 palavras) */
    const int RV[4] = { 88, 85, 83, 82 };
    for (int j = 0; j < 4; j++) _mm_storeu_ps((float*)&ctx->gpr[RV[j]], g6l_z4(V[j]));
    for (int i = 0; i < 4; i++) {
        ctx->gpr[125]._u32[i] += 16u * feitas;
        ctx->gpr[100]._u32[i] += 32u * feitas;
        g6l_pbe32(ls, sp + 0x9B0 + 4 * i, g6l_be32(ls, sp + 0x9B0 + 4 * i) + feitas);
    }
    gowa_edge_conta("g6_laco", conta, 1);
    return 1;
}
#else   /* 11/11: build compatível (sem AVX2/FMA): o laço traduzido */
static int g6_laco63_nativo(spu_context* ctx) { (void)ctx; return 0; }
#endif
#endif
