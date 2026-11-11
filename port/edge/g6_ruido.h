/* GOWA (02/11, tradução do General6 0x727200): a função 0x4930 é o ruído de Perlin melhorado (Ken Perlin, 2002)
 * em 4 pontos (r3..r6 = x, y, z, _; saída r3 = o ruído de cada ponto). A 0x63B4 a chama 4 vezes seguidas com
 * entradas independentes; o núcleo vetorial gerado (vc_00004930.inc, vetorizar_chamadas.py) já junta as 4
 * chamadas, mas copiando as ~790 instruções da SPU (196 shufb). Aqui é o algoritmo: 16 pontos em 2 voltas de
 * AVX2 de 8, com as mesmas contas na mesma ordem (o traço do scripts_edge/banc/g6_traco.py):
 *   piso pelos bits: t = x - (0,99999994 se x < 0), trunc(t) pela máscara do expoente; f = x - trunc
 *   suave(f) = ((f*f)*f) * fma(f, fms(f, 6, 15), 10)
 *   grad = ((dx*gx) + (dy*gy)) + (dz*gz), d = f ou f - 1; g = tabela de 16 gradientes na LS 0x8580
 *   interpola = fma(b - a, t, a); permutação de 512 palavras na LS 0x8680 (big-endian)
 * A thread da SPU roda com MXCSR em para-zero + DAZ (sem FTZ): as contas saem iguais às da SPU; só a saída precisa
 * do +0 nos lanes de expoente 0 (o resto só vai para outras contas de float). Fora do alcance (|x| >= 2^23 em
 * algum ponto: piso e índice saem por outro caminho na SPU) devolve ao núcleo gerado.
 * Modelo e conferência: scripts_edge/banc/g6_modelo.py (800/800 nas fotos, 1600/1600 nos casos extremos).
 * GOWA_EDGE_NATIVO=0 ou GOWA_EDGE_SEM=g6_ruido desligam. */
#ifndef GOWA_G6_RUIDO_H
#define GOWA_G6_RUIDO_H
#include <immintrin.h>
#include "edge_comum.h"
#include "edge_sem.h"

int gowa_vc_gowajm_00727200_00004930(spu_context* ctx, const u128* in, u128 r1_in, u128* res, u128* fim);

#define G6_PERM 0x8680u
#define G6_GRAD 0x8580u

#if defined(__AVX2__) && defined(__FMA__)
static inline __m256i g6_be(__m256i v)   /* palavras big-endian da LS -> do host */
{
    const __m256i k = _mm256_setr_epi8(3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12,
                                       3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12);
    return _mm256_shuffle_epi8(v, k);
}
static inline __m256i g6_perm(const uint8_t* ls, __m256i i)
{
    return g6_be(_mm256_i32gather_epi32((const int*)(ls + G6_PERM), i, 4));
}
/* piso: (parte inteira & 255, fração) */
static inline __m256 g6_piso(__m256 x, __m256i* ii)
{
    const __m256i xi = _mm256_castps_si256(x);
    const __m256i neg = _mm256_and_si256(_mm256_srai_epi32(xi, 31), _mm256_set1_epi32(0x3F7FFFFF));
    const __m256 t = _mm256_sub_ps(x, _mm256_castsi256_ps(neg));
    const __m256i e = _mm256_and_si256(_mm256_srli_epi32(_mm256_castps_si256(t), 23), _mm256_set1_epi32(255));
    __m256i m = _mm256_srlv_epi32(_mm256_set1_epi32(0x007FFFFF), _mm256_sub_epi32(e, _mm256_set1_epi32(127)));
    m = _mm256_or_si256(m, _mm256_cmpgt_epi32(_mm256_set1_epi32(127), e));   /* |t| < 1: tudo */
    const __m256 tr = _mm256_castsi256_ps(_mm256_andnot_si256(m, _mm256_castps_si256(t)));
    *ii = _mm256_and_si256(_mm256_cvttps_epi32(tr), _mm256_set1_epi32(255));
    return _mm256_sub_ps(x, tr);
}
static inline __m256 g6_suave(__m256 f)
{
    const __m256 p = _mm256_fmadd_ps(f, _mm256_fmsub_ps(f, _mm256_set1_ps(6.0f), _mm256_set1_ps(15.0f)), _mm256_set1_ps(10.0f));
    return _mm256_mul_ps(_mm256_mul_ps(_mm256_mul_ps(f, f), f), p);
}
static inline __m256 g6_grad(const uint8_t* ls, __m256i h, __m256 dx, __m256 dy, __m256 dz)
{
    const __m256i o = _mm256_slli_epi32(_mm256_and_si256(g6_perm(ls, h), _mm256_set1_epi32(15)), 4);
    const int* g = (const int*)(ls + G6_GRAD);
    const __m256 gx = _mm256_castsi256_ps(g6_be(_mm256_i32gather_epi32(g, o, 1)));
    const __m256 gy = _mm256_castsi256_ps(g6_be(_mm256_i32gather_epi32(g + 1, o, 1)));
    const __m256 gz = _mm256_castsi256_ps(g6_be(_mm256_i32gather_epi32(g + 2, o, 1)));
    return _mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(dx, gx), _mm256_mul_ps(dy, gy)), _mm256_mul_ps(dz, gz));
}
static inline __m256 g6_interp(__m256 t, __m256 a, __m256 b) { return _mm256_fmadd_ps(_mm256_sub_ps(b, a), t, a); }

static inline __m256 g6_ruido8(const uint8_t* ls, __m256 x, __m256 y, __m256 z)
{
    __m256i X, Y, Z;
    const __m256 fx = g6_piso(x, &X), fy = g6_piso(y, &Y), fz = g6_piso(z, &Z);
    const __m256 u = g6_suave(fx), v = g6_suave(fy), w = g6_suave(fz);
    const __m256i um = _mm256_set1_epi32(1);
    const __m256i A = _mm256_add_epi32(g6_perm(ls, X), Y), B = _mm256_add_epi32(g6_perm(ls, _mm256_add_epi32(X, um)), Y);
    const __m256i AA = _mm256_add_epi32(g6_perm(ls, A), Z), AB = _mm256_add_epi32(g6_perm(ls, _mm256_add_epi32(A, um)), Z);
    const __m256i BA = _mm256_add_epi32(g6_perm(ls, B), Z), BB = _mm256_add_epi32(g6_perm(ls, _mm256_add_epi32(B, um)), Z);
    const __m256 m1 = _mm256_set1_ps(1.0f);
    const __m256 gx = _mm256_sub_ps(fx, m1), gy = _mm256_sub_ps(fy, m1), gz = _mm256_sub_ps(fz, m1);
    const __m256 z0 = g6_interp(v, g6_interp(u, g6_grad(ls, AA, fx, fy, fz), g6_grad(ls, BA, gx, fy, fz)),
                                   g6_interp(u, g6_grad(ls, AB, fx, gy, fz), g6_grad(ls, BB, gx, gy, fz)));
    const __m256 z1 = g6_interp(v, g6_interp(u, g6_grad(ls, _mm256_add_epi32(AA, um), fx, fy, gz), g6_grad(ls, _mm256_add_epi32(BA, um), gx, fy, gz)),
                                   g6_interp(u, g6_grad(ls, _mm256_add_epi32(AB, um), fx, gy, gz), g6_grad(ls, _mm256_add_epi32(BB, um), gx, gy, gz)));
    const __m256 r = g6_interp(w, z0, z1);
    /* +0 nos lanes de expoente 0 (denormal ou zero de qualquer sinal), como a saída da SPU */
    const __m256i ri = _mm256_castps_si256(r);
    const __m256i ex = _mm256_and_si256(ri, _mm256_set1_epi32(0x7F800000));
    return _mm256_castsi256_ps(_mm256_andnot_si256(_mm256_cmpeq_epi32(ex, _mm256_setzero_si256()), ri));
}

/* a mesma conta com as tabelas já na ordem do host (o laço 0x63B4 as prepara uma vez, g6_tabelas): a permutação em
 * palavras do host e os 16 gradientes em registradores (permutevar das 8 de baixo e das 8 de cima, escolhidas pelo
 * bit 3 do índice) no lugar dos 24 gathers dos gradientes */
typedef struct { int32_t perm[512]; __m256 g[3][2]; } g6_tab;
static inline void g6_tabelas(const uint8_t* ls, g6_tab* t)
{
    for (int i = 0; i < 512; i += 8)
        _mm256_storeu_si256((__m256i*)&t->perm[i], g6_be(_mm256_loadu_si256((const __m256i*)(ls + G6_PERM + 4 * i))));
    for (int c = 0; c < 3; c++) {
        float v[16];
        for (int i = 0; i < 16; i++) { const uint32_t b = _byteswap_ulong(*(const uint32_t*)(ls + G6_GRAD + 16 * i + 4 * c)); memcpy(&v[i], &b, 4); }
        t->g[c][0] = _mm256_loadu_ps(v); t->g[c][1] = _mm256_loadu_ps(v + 8);
    }
}
static inline __m256i g6t_perm(const g6_tab* t, __m256i i) { return _mm256_i32gather_epi32(t->perm, i, 4); }
static inline __m256 g6t_g(const __m256* g, __m256i o)
{
    return _mm256_blendv_ps(_mm256_permutevar8x32_ps(g[0], o), _mm256_permutevar8x32_ps(g[1], o), _mm256_castsi256_ps(_mm256_slli_epi32(o, 28)));
}
static inline __m256 g6t_grad(const g6_tab* t, __m256i h, __m256 dx, __m256 dy, __m256 dz)
{
    const __m256i o = _mm256_and_si256(g6t_perm(t, h), _mm256_set1_epi32(15));
    return _mm256_add_ps(_mm256_add_ps(_mm256_mul_ps(dx, g6t_g(t->g[0], o)), _mm256_mul_ps(dy, g6t_g(t->g[1], o))), _mm256_mul_ps(dz, g6t_g(t->g[2], o)));
}
static inline __m256 g6_ruido8t(const g6_tab* t, __m256 x, __m256 y, __m256 z)
{
    __m256i X, Y, Z;
    const __m256 fx = g6_piso(x, &X), fy = g6_piso(y, &Y), fz = g6_piso(z, &Z);
    const __m256 u = g6_suave(fx), v = g6_suave(fy), w = g6_suave(fz);
    const __m256i um = _mm256_set1_epi32(1);
    const __m256i A = _mm256_add_epi32(g6t_perm(t, X), Y), B = _mm256_add_epi32(g6t_perm(t, _mm256_add_epi32(X, um)), Y);
    const __m256i AA = _mm256_add_epi32(g6t_perm(t, A), Z), AB = _mm256_add_epi32(g6t_perm(t, _mm256_add_epi32(A, um)), Z);
    const __m256i BA = _mm256_add_epi32(g6t_perm(t, B), Z), BB = _mm256_add_epi32(g6t_perm(t, _mm256_add_epi32(B, um)), Z);
    const __m256 m1 = _mm256_set1_ps(1.0f);
    const __m256 gx = _mm256_sub_ps(fx, m1), gy = _mm256_sub_ps(fy, m1), gz = _mm256_sub_ps(fz, m1);
    const __m256 z0 = g6_interp(v, g6_interp(u, g6t_grad(t, AA, fx, fy, fz), g6t_grad(t, BA, gx, fy, fz)),
                                   g6_interp(u, g6t_grad(t, AB, fx, gy, fz), g6t_grad(t, BB, gx, gy, fz)));
    const __m256 z1 = g6_interp(v, g6_interp(u, g6t_grad(t, _mm256_add_epi32(AA, um), fx, fy, gz), g6t_grad(t, _mm256_add_epi32(BA, um), gx, fy, gz)),
                                   g6_interp(u, g6t_grad(t, _mm256_add_epi32(AB, um), fx, gy, gz), g6t_grad(t, _mm256_add_epi32(BB, um), gx, gy, gz)));
    const __m256 r = g6_interp(w, z0, z1);
    const __m256i ri = _mm256_castps_si256(r);
    const __m256i ex = _mm256_and_si256(ri, _mm256_set1_epi32(0x7F800000));
    return _mm256_castsi256_ps(_mm256_andnot_si256(_mm256_cmpeq_epi32(ex, _mm256_setzero_si256()), ri));
}

/* no lugar do núcleo gerado (o #define no spu_recomp.c troca a chamada): mesmas entradas (in[4*c + k] = r3+k da
 * chamada c), res[c] = r3 de cada chamada; fim = os registradores 2..79 depois da 4a chamada, que o chamador não lê
 * (só o r3, posto à parte) -- ficam os que estavam, e o prólogo vai para a pilha pelo _fim do gerado */
static int g6_vc4930(spu_context* ctx, const u128* in, u128 r1_in, u128* res, u128* fim)
{
    static int on = -1;
    if (on < 0) { const char* e = getenv("GOWA_EDGE_NATIVO"); on = !(e && *e == '0') && !gowa_edge_sem("g6_ruido"); }
    static volatile long conta[2];
    if (on) {
        /* x, y e z dos 16 pontos: p[c*4+k] = in[c*4+k]._u32[0..2] */
        __m256i lim = _mm256_setzero_si256();
        __m256 X[2], Y[2], Z[2];
        for (int b = 0; b < 2; b++) {
            const __m256i q0 = _mm256_loadu2_m128i((const __m128i*)&in[8 * b + 4], (const __m128i*)&in[8 * b]);
            const __m256i q1 = _mm256_loadu2_m128i((const __m128i*)&in[8 * b + 5], (const __m128i*)&in[8 * b + 1]);
            const __m256i q2 = _mm256_loadu2_m128i((const __m128i*)&in[8 * b + 6], (const __m128i*)&in[8 * b + 2]);
            const __m256i q3 = _mm256_loadu2_m128i((const __m128i*)&in[8 * b + 7], (const __m128i*)&in[8 * b + 3]);
            /* transpor 4x4 em cada metade: lane j da metade h = ponto 4*(2b+h) + j */
            const __m256i t0 = _mm256_unpacklo_epi32(q0, q1), t1 = _mm256_unpacklo_epi32(q2, q3);
            const __m256i t2 = _mm256_unpackhi_epi32(q0, q1), t3 = _mm256_unpackhi_epi32(q2, q3);
            X[b] = _mm256_castsi256_ps(_mm256_unpacklo_epi64(t0, t1));
            Y[b] = _mm256_castsi256_ps(_mm256_unpackhi_epi64(t0, t1));
            Z[b] = _mm256_castsi256_ps(_mm256_unpacklo_epi64(t2, t3));
            const __m256i a = _mm256_set1_epi32(0x7FFFFFFF);
            lim = _mm256_max_epu32(lim, _mm256_and_si256(_mm256_castps_si256(X[b]), a));
            lim = _mm256_max_epu32(lim, _mm256_and_si256(_mm256_castps_si256(Y[b]), a));
            lim = _mm256_max_epu32(lim, _mm256_and_si256(_mm256_castps_si256(Z[b]), a));
        }
        if (_mm256_testz_si256(_mm256_cmpgt_epi32(lim, _mm256_set1_epi32(0x4AFFFFFF)), _mm256_set1_epi32(-1))) {
            const uint8_t* ls = SPU_LS_BASE(ctx);
            for (int b = 0; b < 2; b++) {
                const __m256 r = g6_ruido8(ls, X[b], Y[b], Z[b]);
                _mm_storeu_ps((float*)&res[2 * b], _mm256_castps256_ps128(r));
                _mm_storeu_ps((float*)&res[2 * b + 1], _mm256_extractf128_ps(r, 1));
            }
            memcpy(&fim[2], &ctx->gpr[2], sizeof(u128) * 78);
            fim[3] = res[3];
            gowa_edge_conta("g6_ruido", conta, 1);
            return 1;
        }
        gowa_edge_conta("g6_ruido", conta, 0);
    }
    return gowa_vc_gowajm_00727200_00004930(ctx, in, r1_in, res, fim);
}
#else   /* 11/11: build compatível (sem AVX2/FMA): o núcleo gerado, como com GOWA_EDGE_NATIVO=0 */
static int g6_vc4930(spu_context* ctx, const u128* in, u128 r1_in, u128* res, u128* fim)
{
    return gowa_vc_gowajm_00727200_00004930(ctx, in, r1_in, res, fim);
}
#endif
#endif
