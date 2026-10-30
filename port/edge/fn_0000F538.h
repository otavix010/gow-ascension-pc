/* GOWA (30/10, EDGE nativo, marco E3): o laço F620 da função 0xF538 do job 0x72DF80 escrito para o PC.
 *
 * A F538 transforma vértices float4 por uma matriz 4x4 guardada em diagonais rotacionadas: para cada componente i,
 *     out[i] = v[i]*A[i] + v[i+1]*B[i] + v[i+2]*C[i] + v[i+3]*D[i]   (índices mod 4)
 * com A = r35, B = r36, C = r33, D = r34 (montados no prólogo a partir de r6) e as contas na ordem do original:
 * fm, depois fma, fma, fma (cada uma arredonda para zero; entradas com expoente 0 valem zero; a saída de uma conta
 * normal com expoente 0 vira +0). O original faz 8 vértices (128 bytes) por volta em software pipeline: a volta k
 * lê o grupo k e grava o grupo k-1, e roda G+1 vezes (a 1a grava zeros por cima do grupo 0, a última só lê). Aqui
 * é um grupo por vez (lê, conta, grava), com o mesmo resultado na LS.
 * Contas: AVX-512 (RZ embutido, 4 vértices por registro) ou, na build portátil, AVX2 com o MXCSR em RZ (SPU_SX_RZ).
 * Um grupo com algum valor fora do alcance do float do PC (|r| >= 0x7F7FFFFF: o host saturou ou um operando tinha o
 * expoente 255, que na SPU é número) é refeito com as primitivas do runtime (as mesmas do código traduzido).
 * Os temporários r2-r79 ficam diferentes do original (quem chama não os lê; conferência GOWA_CONF_REGS_ABI=1).
 * GOWA_EDGE_NATIVO=1 liga. Gancho (scripts_edge/edge_nativo.py): antes do vk do laço, saltando para depois dele. */
#ifndef GOWA_FN_0000F538_H
#define GOWA_FN_0000F538_H
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

/* um grupo de 8 vértices pelas primitivas do runtime, como o original (fm_r nos vértices 0, 1 e 5) */
static inline void f538_exato(spu_context* ctx, uint32_t in, uint32_t out, int v0, int n, u128 a, u128 b, u128 c, u128 d)
{
    for (int v = v0; v < v0 + n; v++) {
        const u128 x = spu_ls_read128(ctx, in + 16u * (uint32_t)v);
        u128 m = (v == 0 || v == 1 || v == 5) ? spu_fm_r(x, a) : spu_fm(x, a);
        m = spu_fma(spu_rotqbyi(x, 4), b, m);
        m = spu_fma(spu_rotqbyi(x, 8), c, m);
        m = spu_fma(spu_rotqbyi(x, 12), d, m);
        spu_ls_write128(ctx, out + 16u * (uint32_t)v, m);
    }
}

#if defined(__AVX512F__) && defined(__AVX512BW__) && defined(__AVX512VL__)
#define F538_VETOR 1
#define F538_RZ (_MM_FROUND_TO_ZERO | _MM_FROUND_NO_EXC)
static inline __m512 f538_z(__m512 x)   /* expoente 0 -> +0 */
{
    const __m512i i = _mm512_castps_si512(x);
    return _mm512_castsi512_ps(_mm512_maskz_mov_epi32(_mm512_test_epi32_mask(i, _mm512_set1_epi32(0x7F800000)), i));
}
static inline __mmask16 f538_ruim(__m512 r)
{
    return _mm512_cmpge_epu32_mask(_mm512_and_si512(_mm512_castps_si512(r), _mm512_set1_epi32(0x7FFFFFFF)), _mm512_set1_epi32(0x7F7FFFFF));
}
static inline __m512i f538_bswap(__m512i v)
{
    return _mm512_shuffle_epi8(v, _mm512_broadcast_i32x4(_mm_setr_epi8(3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12)));
}
static inline __m512 f538_k(u128 k) { return f538_z(_mm512_broadcast_f32x4(_mm_loadu_ps((const float*)k._u32))); }
/* 4 vértices (64 bytes) em p: devolve 0 se algum valor saiu do alcance (nada gravado) */
static inline int f538_quatro(uint8_t* ls, uint32_t in, uint32_t out, __m512 A, __m512 B, __m512 C, __m512 D)
{
    const __m512 v = f538_z(_mm512_castsi512_ps(f538_bswap(_mm512_loadu_si512(ls + in))));
    __m512 m = _mm512_mul_round_ps(v, A, F538_RZ);
    __mmask16 ruim = f538_ruim(m);
    m = _mm512_fmadd_round_ps(_mm512_permute_ps(v, _MM_SHUFFLE(0, 3, 2, 1)), B, f538_z(m), F538_RZ);
    ruim |= f538_ruim(m);
    m = _mm512_fmadd_round_ps(_mm512_permute_ps(v, _MM_SHUFFLE(1, 0, 3, 2)), C, f538_z(m), F538_RZ);
    ruim |= f538_ruim(m);
    m = _mm512_fmadd_round_ps(_mm512_permute_ps(v, _MM_SHUFFLE(2, 1, 0, 3)), D, f538_z(m), F538_RZ);
    ruim |= f538_ruim(m);
    if (ruim) return 0;
    _mm512_storeu_si512(ls + out, f538_bswap(_mm512_castps_si512(f538_z(m))));
    return 1;
}
#elif defined(SPU_SX_RZ) && defined(__AVX2__) && defined(__FMA__)
#define F538_VETOR 2
static inline __m256 f538_z8(__m256 x)
{
    const __m256i i = _mm256_castps_si256(x);
    const __m256i nz = _mm256_cmpeq_epi32(_mm256_and_si256(i, _mm256_set1_epi32(0x7F800000)), _mm256_setzero_si256());
    return _mm256_castsi256_ps(_mm256_andnot_si256(nz, i));
}
static inline __m256i f538_ruim8(__m256 r)
{
    return _mm256_cmpgt_epi32(_mm256_and_si256(_mm256_castps_si256(r), _mm256_set1_epi32(0x7FFFFFFF)), _mm256_set1_epi32(0x7F7FFFFE));
}
static inline __m256i f538_bswap8(__m256i v)
{
    return _mm256_shuffle_epi8(v, _mm256_setr_epi8(3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12,
                                                   3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12));
}
static inline __m256 f538_k8(u128 k) { return f538_z8(_mm256_broadcast_ps((const __m128*)k._u32)); }
/* 2 vértices (32 bytes); o MXCSR da thread da SPU arredonda para zero */
static inline int f538_dois(uint8_t* ls, uint32_t in, uint32_t out, __m256 A, __m256 B, __m256 C, __m256 D)
{
    const __m256 v = f538_z8(_mm256_castsi256_ps(f538_bswap8(_mm256_loadu_si256((const __m256i*)(ls + in)))));
    __m256 m = _mm256_mul_ps(v, A);
    __m256i ruim = f538_ruim8(m);
    m = _mm256_fmadd_ps(_mm256_permute_ps(v, _MM_SHUFFLE(0, 3, 2, 1)), B, f538_z8(m));
    ruim = _mm256_or_si256(ruim, f538_ruim8(m));
    m = _mm256_fmadd_ps(_mm256_permute_ps(v, _MM_SHUFFLE(1, 0, 3, 2)), C, f538_z8(m));
    ruim = _mm256_or_si256(ruim, f538_ruim8(m));
    m = _mm256_fmadd_ps(_mm256_permute_ps(v, _MM_SHUFFLE(2, 1, 0, 3)), D, f538_z8(m));
    ruim = _mm256_or_si256(ruim, f538_ruim8(m));
    if (!_mm256_testz_si256(ruim, ruim)) return 0;
    _mm256_storeu_si256((__m256i*)(ls + out), f538_bswap8(_mm256_castps_si256(f538_z8(m))));
    return 1;
}
#else
#define F538_VETOR 0
#endif

/* GOWA_F538_STATS=1: grupos feitos e refeitos pelo caminho exato */
static int fn_0000F538_nativo(spu_context* ctx)
{
    if (!gowa_edge_nativo_on()) return 0;
    u128* r = ctx->gpr;
    const uint32_t voltas = r[37]._u32[0];
    if (voltas < 2u || voltas > 0x800u || r[39]._u32[0] != 0u || r[32]._u32[0] != 0u) return 0;
    const uint32_t g = voltas - 1u, in = r[38]._u32[0], out = r[58]._u32[0], tam = 128u * g;
    if ((in | out) & 15u || in + tam > SPU_LS_SIZE || out + tam > SPU_LS_SIZE) return 0;
    if (out > in && out < in + tam) return 0;   /* a saída alcançaria entradas ainda não lidas */
    for (int k = 1; k < 8; k++) {   /* a saída em 8 quadwords seguidos, como o laço grava */
        static const int reg[8] = { 58, 54, 47, 46, 45, 44, 42, 40 };
        if (r[reg[k]]._u32[0] != out + 16u * (uint32_t)k) return 0;
    }
    const u128 a = r[35], b = r[36], c = r[33], d = r[34];
    uint8_t* ls = SPU_LS_BASE(ctx);
    long refeitos = 0;
#if F538_VETOR == 1
    const __m512 A = f538_k(a), B = f538_k(b), C = f538_k(c), D = f538_k(d);
#elif F538_VETOR == 2
    const __m256 A = f538_k8(a), B = f538_k8(b), C = f538_k8(c), D = f538_k8(d);
#endif
    for (uint32_t j = 0; j < g; j++) {
        const uint32_t pi = in + 128u * j, po = out + 128u * j;
        /* conta numa cópia do grupo e grava os 128 bytes juntos (serve também no lugar, out == in); um bloco com valor
         * fora do alcance é refeito pelo caminho exato (lido da LS antes da gravação do grupo): na prática só o
         * último grupo, com o lixo além do fim dos vértices (0xFFFFFFFF etc.), que o original também grava */
        uint8_t tmp[128];
        memcpy(tmp, ls + pi, 128);
#if F538_VETOR == 1
        const int nb = 2, vb = 4;
        int ok[2] = { f538_quatro(tmp, 0, 0, A, B, C, D), f538_quatro(tmp, 64, 64, A, B, C, D) };
#elif F538_VETOR == 2
        const int nb = 4, vb = 2;
        int ok[4] = { f538_dois(tmp, 0, 0, A, B, C, D), f538_dois(tmp, 32, 32, A, B, C, D),
                      f538_dois(tmp, 64, 64, A, B, C, D), f538_dois(tmp, 96, 96, A, B, C, D) };
#else
        const int nb = 1, vb = 8;
        int ok[1] = { 0 };
#endif
        int todos = 1;
        for (int k = 0; k < nb; k++) todos &= ok[k];
        if (todos) { memcpy(ls + po, tmp, 128); continue; }
        /* algum bloco ruim: os bons vão da cópia; os ruins, do caminho exato com a entrada original */
        uint8_t ent[128];
        memcpy(ent, ls + pi, 128);
        for (int k = 0; k < nb; k++) {
            if (ok[k]) continue;
            memcpy(ls + po + (uint32_t)(k * vb * 16), ent + k * vb * 16, (size_t)vb * 16);   /* entrada no lugar da saída */
            f538_exato(ctx, po, po, k * vb, vb, a, b, c, d);
            memcpy(tmp + k * vb * 16, ls + po + (uint32_t)(k * vb * 16), (size_t)vb * 16);
            refeitos++;
        }
        memcpy(ls + po, tmp, 128);
    }
    static int est = -1; static volatile long nch, nref;
    if (est < 0) est = getenv("GOWA_F538_STATS") ? 1 : 0;
    if (est) { const long n = ++nch; nref += refeitos; if ((n & (n - 1)) == 0) fprintf(stderr, "[f538] %ld chamadas, %ld blocos refeitos\n", n, nref); }
    return 1;   /* só temporários mudariam depois do laço */
}
#endif
