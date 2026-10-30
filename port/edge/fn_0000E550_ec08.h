/* GOWA (29/10): o laço EC08 do E550 do job do EDGE (0x72DF80) nativo: o skinning de 4 ossos por vértice.
 *
 * O que o laço faz, por par de vértices (A, B), lido do código traduzido e conferido pelas fotos da fase 1:
 *  - fluxo de 8 bytes por vértice em r51-16+16j: (w0, b0, w1, b1, w2, b2, w3, b3); peso = fm(float(w), r91 = 1/255),
 *    osso = deslocamento b*48 na paleta de 3 linhas (bases r37, r35, r34: +0, +16, +32);
 *  - cada linha L da matriz misturada = fma(w3, M[b3].L, fma_r(w2, M[b2].L, fma_r(w1, M[b1].L, fm_r(w0, M[b0].L))));
 *  - grava em r49 (+96 por par): A.L37 com w = 1 (selb r97/r84), A.L35 e A.L34 com w = 0, depois o mesmo de B;
 *  - transforma a posição no lugar (r86, +32 por par): T = (L37.w, L35.w, L34.w, 0),
 *    p' = fma(rot2(p), L34, fma(rot1(p), L35, fma_r(p, L37, T))), rot1 = (y,z,x,w), rot2 = (z,x,y,w).
 * O laço é em software pipelining: a volta j usa pesos, deslocamentos e posições preparados no fim da volta j-1.
 * O nativo faz os pares 0..n-2 e deixa nos registradores vivos da entrada (scripts_edge/vivos_laco.py) os valores
 * do último par, calculados com as mesmas operações do fim da volta; o laço traduzido roda a última volta e o
 * resto da função segue igual. Devolve 0 (e o núcleo vk/o laço traduzido fazem tudo) se algo não for o esperado.
 * Liga com GOWA_EDGE_NATIVO=1. */
#ifndef GOWA_FN_E550_EC08_H
#define GOWA_FN_E550_EC08_H
#include <stdint.h>
#include <stdlib.h>
#include <string.h>

#ifndef GOWA_EDGE_NATIVO_ON_DEF
#define GOWA_EDGE_NATIVO_ON_DEF
static int gowa_edge_nativo_on(void)
{
    static int on = -1;
    if (on < 0) { const char* e = getenv("GOWA_EDGE_NATIVO"); on = e && *e == '1'; }
    return on;
}
#endif

static inline int ec08_igual(u128 a, uint32_t w0, uint32_t w1, uint32_t w2, uint32_t w3)
{
    return a._u32[0] == w0 && a._u32[1] == w1 && a._u32[2] == w2 && a._u32[3] == w3;
}
static inline u128 ec08_splat(u128 v, int i) { u128 r; r._u32[0] = r._u32[1] = r._u32[2] = r._u32[3] = v._u32[i]; return r; }
static inline u128 ec08_rot(u128 p, int k)   /* rot1 = (y,z,x,w), rot2 = (z,x,y,w) */
{
    u128 r;
    r._u32[0] = p._u32[(0 + k) % 3]; r._u32[1] = p._u32[(1 + k) % 3]; r._u32[2] = p._u32[(2 + k) % 3]; r._u32[3] = p._u32[3];
    return r;
}
static inline u128 ec08_and(u128 a, u128 m) { u128 r; for (int i = 0; i < 4; i++) r._u32[i] = a._u32[i] & m._u32[i]; return r; }
static inline u128 ec08_selb(u128 a, u128 b, u128 m) { u128 r; for (int i = 0; i < 4; i++) r._u32[i] = (a._u32[i] & ~m._u32[i]) | (b._u32[i] & m._u32[i]); return r; }

/* um par (A, B): as 6 linhas gravadas em r49 e as 2 posições em r86, com as operações exatas do runtime */
static inline void ec08_par(spu_context* ctx, uint32_t s, uint32_t m_out, uint32_t p_out, const uint32_t base[3],
                            u128 k91, u128 k84, u128 k97)
{
    const uint8_t* ls = SPU_LS_BASE(ctx);
    for (int v = 0; v < 2; v++) {
        const uint8_t* q = ls + ((s + 8u * (uint32_t)v) & SPU_LS_MASK);
        u128 wf; for (int i = 0; i < 4; i++) wf._u32[i] = q[2 * i];
        wf = spu_fm(spu_cuflt(wf, 155), k91);
        u128 L[3];
        for (int l = 0; l < 3; l++) {
            u128 acc = {{0}};
            for (int b = 0; b < 4; b++) {
                const uint32_t off = (uint32_t)q[2 * b + 1] * 48u;
                const u128 m = spu_ls_read128(ctx, base[l] + off);
                const u128 w = ec08_splat(wf, b);
                acc = b == 0 ? spu_fm_r(w, m) : b < 3 ? spu_fma_r(w, m, acc) : spu_fma(w, m, acc);
            }
            L[l] = acc;
        }
        /* L[0] = linha da base r37, L[1] = r35, L[2] = r34 */
        const u128 g0 = ec08_selb(k97, L[0], k84), g1 = ec08_and(L[1], k84), g2 = ec08_and(L[2], k84);
        spu_ls_write128(ctx, m_out + 48u * (uint32_t)v + 0x00, g0);
        spu_ls_write128(ctx, m_out + 48u * (uint32_t)v + 0x10, g1);
        spu_ls_write128(ctx, m_out + 48u * (uint32_t)v + 0x20, g2);
        u128 T; T._u32[0] = L[0]._u32[3]; T._u32[1] = L[1]._u32[3]; T._u32[2] = L[2]._u32[3]; T._u32[3] = 0;
        const u128 p = spu_ls_read128(ctx, p_out + 16u * (uint32_t)v);
        u128 t = spu_fma_r(p, g0, T);
        t = spu_fma(ec08_rot(p, 1), g1, t);
        t = spu_fma(ec08_rot(p, 2), g2, t);
        spu_ls_write128(ctx, p_out + 16u * (uint32_t)v, t);
    }
}

#if defined(__AVX512F__) && defined(__AVX512BW__) && defined(__AVX512VL__)
#define EC08_VETOR 1
/* dois pares (4 vértices, um por faixa de 128 bits) com as contas do runtime em 16 faixas: entradas com expoente 0
 * lidas como zero, RZ embutido, as formas cruas (fm_r/fma_r) sem achatar a saída e as normais com o expoente 0
 * virando +0 (spu__zx_in/out/outc). Um resultado com |r| >= 0x7F7FFFFF (o host saturou ou um operando veio do
 * expoente 255) faz os dois pares irem pelo caminho exato (ec08_par). Devolve 0 sem gravar nada nesse caso. */
#define EC08_RZ (_MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC)
static inline __m512 ec08_z(__m512 x)   /* expoente 0 -> +0 (entrada denormal, ou saída achatada) */
{
    const __m512i i = _mm512_castps_si512(x);
    return _mm512_castsi512_ps(_mm512_maskz_mov_epi32(_mm512_test_epi32_mask(i, _mm512_set1_epi32(0x7F800000)), i));
}
static inline __mmask16 ec08_ruim(__m512 r)
{
    return _mm512_cmpge_epu32_mask(_mm512_and_si512(_mm512_castps_si512(r), _mm512_set1_epi32(0x7FFFFFFF)), _mm512_set1_epi32(0x7F7FFFFF));
}
static inline __m512i ec08_bswap(__m512i v)
{
    const __m512i m = _mm512_broadcast_i32x4(_mm_setr_epi8(3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12));
    return _mm512_shuffle_epi8(v, m);
}
static inline __m512 ec08_ld4(const uint8_t* ls, const uint32_t a[4])
{
    __m512i v = _mm512_castsi128_si512(_mm_loadu_si128((const __m128i*)(ls + a[0])));
    v = _mm512_inserti32x4(v, _mm_loadu_si128((const __m128i*)(ls + a[1])), 1);
    v = _mm512_inserti32x4(v, _mm_loadu_si128((const __m128i*)(ls + a[2])), 2);
    v = _mm512_inserti32x4(v, _mm_loadu_si128((const __m128i*)(ls + a[3])), 3);
    return _mm512_castsi512_ps(ec08_bswap(v));
}
static inline int ec08_dois(spu_context* ctx, uint32_t s, uint32_t m_out, uint32_t p_out, const uint32_t base[3],
                            u128 k91, u128 k84, u128 k97)
{
    uint8_t* ls = SPU_LS_BASE(ctx);
    if ((p_out & 15u) || p_out + 64u > SPU_LS_SIZE) return 0;
    uint32_t wv[16], kv[16], off[4][4];
    for (int v = 0; v < 4; v++) {
        const uint8_t* q = ls + ((s + 8u * (uint32_t)v) & SPU_LS_MASK);
        for (int b = 0; b < 4; b++) { wv[4 * v + b] = q[2 * b]; kv[4 * v + b] = k91._u32[b]; off[b][v] = (uint32_t)q[2 * b + 1] * 48u; }
    }
    __mmask16 ruim = 0;
    __m512 W = _mm512_mul_round_ps(_mm512_cvtepu32_ps(_mm512_loadu_si512(wv)), ec08_z(_mm512_castsi512_ps(_mm512_loadu_si512(kv))), EC08_RZ);
    ruim |= ec08_ruim(W); W = ec08_z(W);
    __m512 Wb[4];
    for (int b = 0; b < 4; b++)
        Wb[b] = _mm512_permutexvar_ps(_mm512_setr_epi32(b, b, b, b, 4 + b, 4 + b, 4 + b, 4 + b, 8 + b, 8 + b, 8 + b, 8 + b, 12 + b, 12 + b, 12 + b, 12 + b), W);
    __m512 L[3];
    for (int l = 0; l < 3; l++) {
        __m512 acc = _mm512_setzero_ps();
        for (int b = 0; b < 4; b++) {
            uint32_t a[4];
            for (int v = 0; v < 4; v++) a[v] = (base[l] + off[b][v]) & (SPU_LS_MASK & ~0xFu);
            const __m512 M = ec08_z(ec08_ld4(ls, a));
            if (b == 0) acc = _mm512_mul_round_ps(Wb[0], M, EC08_RZ);
            else acc = _mm512_fmadd_round_ps(Wb[b], M, ec08_z(acc), EC08_RZ);
            ruim |= ec08_ruim(acc);
        }
        L[l] = ec08_z(acc);   /* a última conta (fma) achata a saída */
    }
    const __m512i k84v = _mm512_broadcast_i32x4(_mm_loadu_si128((const __m128i*)k84._u32));
    const __m512i k97v = _mm512_broadcast_i32x4(_mm_loadu_si128((const __m128i*)k97._u32));
    const __m512 g0 = _mm512_castsi512_ps(_mm512_or_si512(_mm512_andnot_si512(k84v, k97v), _mm512_and_si512(_mm512_castps_si512(L[0]), k84v)));
    const __m512 g1 = _mm512_castsi512_ps(_mm512_and_si512(_mm512_castps_si512(L[1]), k84v));
    const __m512 g2 = _mm512_castsi512_ps(_mm512_and_si512(_mm512_castps_si512(L[2]), k84v));
    const __m512i i3 = _mm512_setr_epi32(3, 3, 3, 3, 7, 7, 7, 7, 11, 11, 11, 11, 15, 15, 15, 15);
    __m512 T = _mm512_mask_blend_ps(0x2222, _mm512_permutexvar_ps(i3, L[0]), _mm512_permutexvar_ps(i3, L[1]));
    T = _mm512_mask_blend_ps(0x4444, T, _mm512_permutexvar_ps(i3, L[2]));
    T = _mm512_maskz_mov_ps(0x7777, T);
    const __m512 P = ec08_z(_mm512_castsi512_ps(ec08_bswap(_mm512_loadu_si512(ls + p_out))));
    const __m512 P1 = _mm512_permutexvar_ps(_mm512_setr_epi32(1, 2, 0, 3, 5, 6, 4, 7, 9, 10, 8, 11, 13, 14, 12, 15), P);
    const __m512 P2 = _mm512_permutexvar_ps(_mm512_setr_epi32(2, 0, 1, 3, 6, 4, 5, 7, 10, 8, 9, 11, 14, 12, 13, 15), P);
    __m512 t = _mm512_fmadd_round_ps(P, ec08_z(g0), ec08_z(T), EC08_RZ);          /* fma_r: sem achatar */
    ruim |= ec08_ruim(t);
    t = _mm512_fmadd_round_ps(P1, ec08_z(g1), ec08_z(t), EC08_RZ);
    ruim |= ec08_ruim(t); t = ec08_z(t);
    t = _mm512_fmadd_round_ps(P2, ec08_z(g2), t, EC08_RZ);
    ruim |= ec08_ruim(t); t = ec08_z(t);
    if (ruim) return 0;
    const __m512i G0 = ec08_bswap(_mm512_castps_si512(g0)), G1 = ec08_bswap(_mm512_castps_si512(g1)), G2 = ec08_bswap(_mm512_castps_si512(g2));
    for (int v = 0; v < 4; v++) {
        const uint32_t m = m_out + 48u * (uint32_t)v;
        __m128i x0, x1, x2;
        switch (v) {
        case 0: x0 = _mm512_extracti32x4_epi32(G0, 0); x1 = _mm512_extracti32x4_epi32(G1, 0); x2 = _mm512_extracti32x4_epi32(G2, 0); break;
        case 1: x0 = _mm512_extracti32x4_epi32(G0, 1); x1 = _mm512_extracti32x4_epi32(G1, 1); x2 = _mm512_extracti32x4_epi32(G2, 1); break;
        case 2: x0 = _mm512_extracti32x4_epi32(G0, 2); x1 = _mm512_extracti32x4_epi32(G1, 2); x2 = _mm512_extracti32x4_epi32(G2, 2); break;
        default: x0 = _mm512_extracti32x4_epi32(G0, 3); x1 = _mm512_extracti32x4_epi32(G1, 3); x2 = _mm512_extracti32x4_epi32(G2, 3); break;
        }
        _mm_storeu_si128((__m128i*)(ls + ((m + 0x00) & (SPU_LS_MASK & ~0xFu))), x0);
        _mm_storeu_si128((__m128i*)(ls + ((m + 0x10) & (SPU_LS_MASK & ~0xFu))), x1);
        _mm_storeu_si128((__m128i*)(ls + ((m + 0x20) & (SPU_LS_MASK & ~0xFu))), x2);
    }
    _mm512_storeu_si512(ls + p_out, ec08_bswap(_mm512_castps_si512(t)));
    return 1;
}
#else
#define EC08_VETOR 0
#endif

/* os registradores vivos na entrada da volta do par j, como o fim da volta j-1 os deixa */
static inline void ec08_vivos(spu_context* ctx, uint32_t s, uint32_t pos)
{
    u128* r = ctx->gpr;
    const u128 q = spu_ls_read128(ctx, s);
    const u128 v33 = spu_andhi(q, 255);
    const u128 v30 = spu_shufb(q, r[81], r[77]);
    const u128 v10 = spu_shufb(q, r[81], r[102]);
    const u128 v80 = spu_cuflt(v30, 155), v78 = spu_cuflt(v10, 155);
    const u128 v16 = spu_shli(v33, 4), v13 = spu_shli(v33, 5);
    r[46] = spu_ls_read128(ctx, pos);
    r[47] = spu_ls_read128(ctx, pos + 0x10);
    r[6] = spu_a(v13, v16);
    const u128 v20 = spu_fm(v80, r[91]), v19 = spu_fm(v78, r[91]);
    r[11] = spu_shlqbyi(r[6], 4); r[7] = spu_shlqbyi(r[6], 8); r[8] = spu_shlqbyi(r[6], 12);
    r[54] = spu_shufb(r[46], r[81], r[36]); r[55] = spu_shufb(r[47], r[81], r[36]);
    r[53] = spu_shufb(r[46], r[81], r[82]); r[52] = spu_shufb(r[47], r[81], r[82]);
    r[4] = spu_roti(r[6], 16); r[12] = spu_roti(r[11], 16); r[3] = spu_roti(r[7], 16); r[9] = spu_roti(r[8], 16);
    r[45] = spu_shufb(v20, r[81], r[93]); r[42] = spu_shufb(v20, r[81], r[95]);
    r[41] = spu_shufb(v20, r[81], r[81]); r[43] = spu_shufb(v20, r[81], r[94]);
    r[33] = spu_shufb(v19, r[81], r[81]); r[44] = spu_shufb(v19, r[81], r[93]);
    r[40] = spu_shufb(v19, r[81], r[95]); r[38] = spu_shufb(v19, r[81], r[94]);
}

static int fn_0000E550_ec08_feito(spu_context* ctx);
/* GOWA_EC08_STATS=1: a cada 4096 chamadas, quantas o nativo fez */
static int fn_0000E550_ec08_nativo(spu_context* ctx)
{
    if (!gowa_edge_nativo_on()) return 0;
    const int ok = fn_0000E550_ec08_feito(ctx);
    static int est = -1; static volatile long nch, nok;
    if (est < 0) est = getenv("GOWA_EC08_STATS") ? 1 : 0;
    if (est) { const long c = ++nch; if (ok) nok++; if ((c & 4095) == 0 || c == 64) fprintf(stderr, "[ec08] %ld chamadas, %ld nativas\n", c, nok); }
    return ok;
}
static int fn_0000E550_ec08_feito(spu_context* ctx)
{
    u128* r = ctx->gpr;
    const uint32_t cont = r[48]._u32[0];
    if ((cont & 1u) || cont < 4u || cont > 0x4000u) return 0;
    const uint32_t n = cont / 2u;   /* pares */
    /* as constantes que o laço usa têm de ser as de sempre (o fluxo e as máscaras do módulo) */
    if (!ec08_igual(r[81], 0x00010203u, 0x00010203u, 0x00010203u, 0x00010203u) ||
        !ec08_igual(r[77], 0x80808000u, 0x80808002u, 0x80808004u, 0x80808006u) ||
        !ec08_igual(r[102], 0x80808008u, 0x8080800Au, 0x8080800Cu, 0x8080800Eu) ||
        !ec08_igual(r[36], 0x04050607u, 0x08090A0Bu, 0x00010203u, 0x0C0D0E0Fu) ||
        !ec08_igual(r[82], 0x08090A0Bu, 0x00010203u, 0x04050607u, 0x0C0D0E0Fu) ||
        !ec08_igual(r[93], 0x0C0D0E0Fu, 0x0C0D0E0Fu, 0x0C0D0E0Fu, 0x0C0D0E0Fu) ||
        !ec08_igual(r[94], 0x08090A0Bu, 0x08090A0Bu, 0x08090A0Bu, 0x08090A0Bu) ||
        !ec08_igual(r[95], 0x04050607u, 0x04050607u, 0x04050607u, 0x04050607u) ||
        !ec08_igual(r[69], 0x1C1D1E1Fu, 0x04050607u, 0x08090A0Bu, 0x80808080u) ||
        !ec08_igual(r[70], 0x80808080u, 0x0C0D0E0Fu, 0x1C1D1E1Fu, 0x80808080u))
        return 0;
    const uint32_t s0 = r[51]._u32[0] - 16u, p0 = r[50]._u32[0] - 32u, m0 = r[49]._u32[0];
    if (p0 != r[86]._u32[0]) return 0;   /* posições transformadas no lugar */
    const uint32_t base[3] = { r[37]._u32[0], r[35]._u32[0], r[34]._u32[0] };
    const u128 k91 = r[91], k84 = r[84], k97 = r[97];
    uint32_t j0 = 0;
#if EC08_VETOR
    static int vet = -1;
    if (vet < 0) { const char* e = getenv("GOWA_EC08_VETOR"); vet = !(e && *e == '0'); }
    if (vet)
        for (; j0 + 2 < n; j0 += 2)   /* dois pares por vez, menos o último par (fica com o laço traduzido) */
            if (!ec08_dois(ctx, s0 + 16u * j0, m0 + 96u * j0, p0 + 32u * j0, base, k91, k84, k97)) {
                ec08_par(ctx, s0 + 16u * j0, m0 + 96u * j0, p0 + 32u * j0, base, k91, k84, k97);
                ec08_par(ctx, s0 + 16u * (j0 + 1), m0 + 96u * (j0 + 1), p0 + 32u * (j0 + 1), base, k91, k84, k97);
            }
#endif
    for (uint32_t j = j0; j + 1 < n; j++)
        ec08_par(ctx, s0 + 16u * j, m0 + 96u * j, p0 + 32u * j, base, k91, k84, k97);
    /* a última volta fica com o laço traduzido */
    const uint32_t j = n - 1;
    ec08_vivos(ctx, s0 + 16u * j, p0 + 32u * j);
    /* os ponteiros e o contador andam por ai (as 4 faixas), j voltas */
    for (int i = 0; i < 4; i++) {
        r[51]._u32[i] += 16u * j; r[50]._u32[i] += 32u * j; r[49]._u32[i] += 96u * j;
        r[86]._u32[i] += 32u * j; r[48]._u32[i] -= 2u * j;
    }
    return 1;
}
#endif
