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
    __m256i ruim = a880_ruim8(w);   /* w * 1,0 = w (com o expoente 255 o PC não representa) */
#define BBC8_R(v) ruim = _mm256_or_si256(ruim, a880_ruim8(v))
    const __m256 xx = _mm256_mul_ps(x, x);
    BBC8_R(xx);
    const __m256 t = _mm256_fmadd_ps(y, y, a880_z8(xx));
    BBC8_R(t);
    const __m256 L = a880_z8(_mm256_fmadd_ps(z, z, a880_z8(t)));
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
#if A880_VETOR == 2
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
