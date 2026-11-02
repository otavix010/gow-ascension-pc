/* GOWA (02/11, EDGE nativo, marco E4): função 0xBBC8 do job 0x72DF80 inteira -- normaliza no lugar os vetores de um
 * fluxo (r3 = endereço, r4 = quantidade, arredondada para cima em grupos de 8; 128 bytes por volta):
 *     L = z*z + (y*y + x*x)        (fm_r, fma_r, fma)
 *     s = fi(L, frsqest(L))        (só a estimativa, sem passo de Newton)
 *     v = (x*s, y*s, z*s, w*1,0)   (fm)
 * O original faz software pipeline (lê 3 voltas adiante, grava a volta atual): como cada volta só lê o que ainda não
 * gravou, o efeito na LS é o da volta a volta no lugar. Contas na build portátil (AVX2 com o MXCSR em RZ,
 * SPU_SX_RZ): os 8 vetores da volta em SoA, frsqest/fi pela mesma tabela e aritmética inteira do runtime (do
 * fn_0000A880.h); uma volta com algum valor fora do alcance do float do PC (|r| >= 0x7F7FFFFF, ou o expoente 255
 * que na SPU é número -- L = 0 dá s nessa faixa) é refeita pelas primitivas do runtime na ordem do original.
 * Função folha sem pilha; temporários r2-r79 ficam diferentes e o r3 de saída não é usado por nenhum chamador
 * (GOWA_CONF_REGS_ABI=1). GOWA_EDGE_NATIVO=0 desliga. Gancho 'entrada' + chamada direta (edge_nativo.py). */
#ifndef GOWA_FN_0000BBC8_H
#define GOWA_FN_0000BBC8_H
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "edge_comum.h"
#include "fn_0000A880.h"

/* uma volta (8 vetores, em ordem) pelas primitivas do runtime, como o original: dois grupos de 4 */
static void bbc8_exato(u128* q)
{
    u128 um; um._u32[0] = um._u32[1] = um._u32[2] = um._u32[3] = 0x3F800000u;
    for (int gr = 0; gr < 2; gr++) {
        u128* v = q + 4 * gr;
        u128 X, Y, Z;
        for (int i = 0; i < 4; i++) { X._u32[i] = v[i]._u32[0]; Y._u32[i] = v[i]._u32[1]; Z._u32[i] = v[i]._u32[2]; }
        const u128 L = spu_fma(Z, Z, spu_fma_r(Y, Y, spu_fm_r(X, X)));
        const u128 s = spu_fi(L, spu_frsqest(L));
        for (int i = 0; i < 4; i++) {
            u128 m = um;
            m._u32[0] = m._u32[1] = m._u32[2] = s._u32[i];
            v[i] = spu_fm(v[i], m);
        }
    }
}

#if A880_VETOR == 2
/* a volta inteira no lugar (p = 128 bytes da LS); devolve 0 se algum vetor saiu do alcance (nada gravado) */
static inline int bbc8_oito(uint8_t* p)
{
    __m256 x, y, z, w;
    a880_soa(p, &x, &y, &z, &w);
    w = a880_z8(w);                 /* w * 1,0 = w, gravado: com o +0 */
    __m256i ruim = a880_ruim8(w);   /* (com o expoente 255 o PC não representa) */
#define BBC8_R(v) ruim = _mm256_or_si256(ruim, a880_ruim8(v))
    const __m256 xx = _mm256_mul_ps(x, x);
    BBC8_R(xx);
    const __m256 t = _mm256_fmadd_ps(y, y, A880_ZI(xx));
    BBC8_R(t);
    const __m256 L = a880_z8(_mm256_fmadd_ps(z, z, A880_ZI(t)));   /* L vai para o frsqest inteiro */
    BBC8_R(L);
    const __m256 s = a880_rsq8(L);
    BBC8_R(s);
    const __m256 ox = a880_z8(_mm256_mul_ps(x, s)), oy = a880_z8(_mm256_mul_ps(y, s)), oz = a880_z8(_mm256_mul_ps(z, s));
    BBC8_R(ox); BBC8_R(oy); BBC8_R(oz);
#undef BBC8_R
    if (!_mm256_testz_si256(ruim, ruim)) return 0;
    const __m256 u0 = _mm256_unpacklo_ps(ox, oy), u1 = _mm256_unpackhi_ps(ox, oy), u2 = _mm256_unpacklo_ps(oz, w), u3 = _mm256_unpackhi_ps(oz, w);
    _mm256_storeu_si256((__m256i*)(p), a880_bswap8(_mm256_castps_si256(_mm256_shuffle_ps(u0, u2, _MM_SHUFFLE(1, 0, 1, 0)))));
    _mm256_storeu_si256((__m256i*)(p + 32), a880_bswap8(_mm256_castps_si256(_mm256_shuffle_ps(u0, u2, _MM_SHUFFLE(3, 2, 3, 2)))));
    _mm256_storeu_si256((__m256i*)(p + 64), a880_bswap8(_mm256_castps_si256(_mm256_shuffle_ps(u1, u3, _MM_SHUFFLE(1, 0, 1, 0)))));
    _mm256_storeu_si256((__m256i*)(p + 96), a880_bswap8(_mm256_castps_si256(_mm256_shuffle_ps(u1, u3, _MM_SHUFFLE(3, 2, 3, 2)))));
    return 1;
}
#elif A880_VETOR == 1
/* 02/11: build com AVX-512 (sem o MXCSR em RZ): 4 vértices por zmm (AoS), arredondamento para zero embutido em cada
 * conta; x, y, z repetidos nas faixas de cada vértice dão L em todas as faixas. Mesma ordem do original. */
static inline int bbc8_quatro(uint8_t* p)
{
    const __m512 v = _mm512_castsi512_ps(a880_bswap(_mm512_loadu_si512(p)));
    const __m512 vx = _mm512_permute_ps(v, 0x00), vy = _mm512_permute_ps(v, 0x55), vz = _mm512_permute_ps(v, 0xAA);
    const __m512i mod = _mm512_set1_epi32(0x7FFFFFFF);
#define BBC8_M(x) mx = _mm512_max_epu32(mx, _mm512_and_si512(_mm512_castps_si512(x), mod))
    __m512i mx = _mm512_and_si512(_mm512_castps_si512(v), mod);   /* w * 1,0 = w */
    const __m512 xx = _mm512_mul_round_ps(vx, vx, A880_RZ);
    BBC8_M(xx);
    const __m512 t = _mm512_fmadd_round_ps(vy, vy, xx, A880_RZ);
    BBC8_M(t);
    const __m512 L = a880_z(_mm512_fmadd_round_ps(vz, vz, t, A880_RZ));   /* L vai para o frsqest inteiro */
    BBC8_M(L);
    const __m512 sc = _mm512_mask_blend_ps(0x8888, a880_rsq(L), _mm512_set1_ps(1.0f));
    BBC8_M(sc);
    const __m512 o = a880_z(_mm512_mul_round_ps(v, sc, A880_RZ));
    BBC8_M(o);
#undef BBC8_M
    if (_mm512_cmpgt_epi32_mask(mx, _mm512_set1_epi32(0x7F7FFFFE))) return 0;
    _mm512_storeu_si512(p, a880_bswap(_mm512_castps_si512(o)));
    return 1;
}
static inline int bbc8_oito(uint8_t* p)
{
    uint8_t tmp[128];
    memcpy(tmp, p, 128);
    if (!bbc8_quatro(tmp) || !bbc8_quatro(tmp + 64)) return 0;   /* nada gravado na LS: a volta inteira vai ao exato */
    memcpy(p, tmp, 128);
    return 1;
}
#endif

static int fn_0000BBC8_corpo(spu_context* ctx);
/* GOWA_BBC8_STATS=1: voltas refeitas pelo caminho exato */
static __attribute__((noinline)) int fn_0000BBC8_nativo(spu_context* ctx)
{
    static volatile long n[2];
    const int r = fn_0000BBC8_corpo(ctx);
    gowa_edge_conta("BBC8", n, r);
    return r;
}

static int fn_0000BBC8_corpo(spu_context* ctx)
{
#if A880_VETOR == 2 || A880_VETOR == 1
    if (!gowa_edge_nativo_on()) return 0;
    u128* g = ctx->gpr;
    const uint32_t p = g[3]._u32[0], cont = g[4]._u32[0];
    if (cont == 0u || cont > 0x4000u || (p & 15u)) return 0;   /* 0 = o original dá a volta na LS inteira */
    const uint32_t voltas = (cont + 7u) / 8u, tam = 128u * voltas;
    if (p + tam > SPU_LS_SIZE) return 0;
    uint8_t* ls = SPU_LS_BASE(ctx);
    long refeitas = 0;
    for (uint32_t j = 0; j < voltas; j++) {
        uint8_t* b = ls + p + 128u * j;
        if (bbc8_oito(b)) continue;
        u128 q[8];
        for (int i = 0; i < 8; i++) q[i] = spu_ls_read128(ctx, p + 128u * j + 16u * (uint32_t)i);
        bbc8_exato(q);
        for (int i = 0; i < 8; i++) spu_ls_write128(ctx, p + 128u * j + 16u * (uint32_t)i, q[i]);
        refeitas++;
    }
    static int est = -1; static volatile long nch, nref;
    if (est < 0) est = getenv("GOWA_BBC8_STATS") ? 1 : 0;
    if (est) { const long k = ++nch; nref += refeitas; if ((k & (k - 1)) == 0) fprintf(stderr, "[bbc8] %ld chamadas, %ld voltas refeitas\n", k, nref); }
    return 1;
#else
    (void)ctx;
    return 0;
#endif
}
#endif
