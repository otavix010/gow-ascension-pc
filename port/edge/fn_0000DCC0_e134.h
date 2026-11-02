/* GOWA (01/11, EDGE nativo, marco E4): o laço E134 da 0xDCC0 do job 0x72DF80 -- decodificação delta, no lugar, em 8
 * faixas de 16 bits: a partir de r17 (= r28), quadword a quadword, acc += Q - viés (r11, o mesmo halfword nas 8
 * faixas) e Q = acc. O laço original processa 4 quadwords (64 bytes) por volta, com a gravação uma volta atrás e uma
 * volta a mais no fim (r12 = blocos + 1): na última volta ele lê os 64 bytes logo depois dos dados e não grava.
 * Aqui: as 4n somas em SSE (halfwords big-endian trocados de ordem, sub/add de 16 bits) e depois os registradores
 * como o laço os deixa (r3, r9, r10, r12, r13, r18-r21, r25, r30, r32, r37, r38, r41, r44, r56, r57). O caso
 * coberto é o da entrada do laço (r3 = r13 = r9 = 0 e acumuladores zerados); o resto volta 0 (fica o traduzido).
 * GOWA_EDGE_NATIVO=0 desliga. Gancho 'antes' do rótulo loc_0000E134 (edge_nativo.py). */
#ifndef GOWA_FN_0000DCC0_E134_H
#define GOWA_FN_0000DCC0_E134_H
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

static inline int e134_zero(u128 a) { return !(a._u32[0] | a._u32[1] | a._u32[2] | a._u32[3]); }
static inline u128 e134_splat(uint32_t v) { u128 r; r._u32[0] = r._u32[1] = r._u32[2] = r._u32[3] = v; return r; }

static int fn_0000DCC0_e134_corpo(spu_context* ctx)
{
    if (!gowa_edge_nativo_on()) return 0;
    u128* g = ctx->gpr;
    const uint32_t voltas = g[12]._u32[0], base = g[17]._u32[0];
    if (voltas < 2u || voltas > 4100u || (base & 15u)) return 0;
    if (!e134_zero(g[3]) || !e134_zero(g[13]) || !e134_zero(g[9]) || !e134_zero(g[10]) || !e134_zero(g[18]) ||
        !e134_zero(g[19]) || !e134_zero(g[20]) || !e134_zero(g[21])) return 0;
    if (g[16]._u32[0] != base + 16u || g[15]._u32[0] != base + 32u || g[14]._u32[0] != base + 48u) return 0;
    const uint32_t w11 = g[11]._u32[0];
    if ((w11 >> 16) != (w11 & 0xFFFFu) || g[11]._u32[1] != w11 || g[11]._u32[2] != w11 || g[11]._u32[3] != w11) return 0;
    const uint32_t n = voltas - 1u;   /* blocos de 64 bytes com dados */
    if (base + 64u * (n + 1u) > SPU_LS_SIZE) return 0;
    uint8_t* ls = SPU_LS_BASE(ctx);
    {
        const __m128i troca = _mm_setr_epi8(1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14);
        const __m128i vies = _mm_set1_epi16((short)(w11 & 0xFFFFu));
        __m128i acc = _mm_setzero_si128();
        for (uint32_t q = 0; q < 4u * n; q++) {
            uint8_t* p = ls + base + 16u * q;
            const __m128i v = _mm_shuffle_epi8(_mm_loadu_si128((const __m128i*)p), troca);
            acc = _mm_add_epi16(acc, _mm_sub_epi16(v, vies));
            _mm_storeu_si128((__m128i*)p, _mm_shuffle_epi8(acc, troca));
        }
    }
    /* os registradores como o laço os deixa na saída (última volta: lê o bloco n, grava o n-1) */
    const uint32_t ult = base + 64u * (n - 1u), prox = base + 64u * n;
    g[30] = spu_ls_read128(ctx, prox);
    g[44] = spu_ls_read128(ctx, prox + 16u);
    g[41] = spu_ls_read128(ctx, prox + 32u);
    g[37] = spu_ls_read128(ctx, prox + 48u);
    g[19] = spu_sfh(g[11], g[30]);
    g[18] = spu_sfh(g[11], g[44]);
    g[20] = spu_sfh(g[11], g[41]);
    g[21] = spu_sfh(g[11], g[37]);
    g[32] = spu_ls_read128(ctx, ult);
    g[25] = spu_ls_read128(ctx, ult + 16u);
    g[38] = spu_ls_read128(ctx, ult + 32u);
    g[10] = spu_ls_read128(ctx, ult + 48u);
    g[56] = e134_splat(64u * (n - 1u));
    g[57] = e134_splat(64u * n);
    g[13] = g[57];
    g[9] = e134_splat(64u);
    g[3] = e134_splat(64u * (n + 1u));
    g[12] = spu_ai(g[12], -(int32_t)voltas);   /* cada faixa perde 1 por volta (só a 0 decide o laço) */
    return 1;
}

static __attribute__((noinline)) int fn_0000DCC0_e134_nativo(spu_context* ctx)
{
    static volatile long n[2];
    const int r = fn_0000DCC0_e134_corpo(ctx);
    gowa_edge_conta("DCC0/E134", n, r);
    return r;
}
#endif
