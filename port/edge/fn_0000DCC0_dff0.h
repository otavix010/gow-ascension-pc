/* GOWA (02/11, EDGE nativo, marco E4): o laço DFF0 da DCC0 do job 0x72DF80 -- o mesmo desempacotamento do DE6C
 * (fn_0000DCC0_de6c.h) para largura 9 <= w <= 16: índice j do bloco b = os 24 bits a partir do byte
 * fluxo_b + (j*w >> 3), valor = (x >> (24 - w - (j*w & 7))) & (2^w - 1), em u16 NO LUGAR, do último bloco para o
 * primeiro. O original lê duas meias janelas de 16 índices por volta (r46/r38/r37 + r23 e + r23 + r34, r34 = -2w),
 * fluxo_b = r46 + r23 - 2w - 4w(n-1-b), e grava 64 bytes em r54/r55/r57/r56 + r36 (= out + 64b) duas voltas depois
 * de ler; as voltas 0 e 1 gravam zeros no endereço do último bloco; n + 2 voltas, r23 -= 4w até ficar negativo.
 * Aqui a mesma ordem de leituras e gravações. Modelo conferido pelo oráculo nas 4 fotos com DFF0 (w = 9 e 10).
 * Na saída (loc_0000E0E4) nenhum registrador do laço é lido se a palavra alta de r29 não for 0 (o E0E4 reescreve
 * o r10 antes); com ela 0, ou estados fora do comum, volta 0 (fica o vk e o traduzido). GOWA_EDGE_NATIVO=0 desliga.
 * Gancho 'antes_vk' do laço, saltando para loc_0000E0E4. */
#ifndef GOWA_FN_0000DCC0_DFF0_H
#define GOWA_FN_0000DCC0_DFF0_H
#include <stdlib.h>
#include <string.h>
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

static inline int dff0_zero(u128 v) { return !(v._u32[0] | v._u32[1] | v._u32[2] | v._u32[3]); }

static int fn_0000DCC0_dff0_corpo(spu_context* ctx)
{
    if (!gowa_edge_nativo_on()) return 0;
    u128* g = ctx->gpr;
    if ((g[29]._u32[0] >> 16) == 0u) return 0;   /* o r10 do laço seria lido em loc_0000E194 */
    const uint32_t mask = g[27]._u32[0] & 0xFFFFu;
    for (int i = 0; i < 4; i++) if (g[27]._u32[i] != (mask | (mask << 16))) return 0;
    const uint32_t w = (uint32_t)__builtin_popcount(mask);
    /* 02/11: w = 16 fica com o traduzido -- nele o original não dá a conta acima nos índices pares (a bancada
     * scripts_edge/banc/bj_dff0.py achou; reports 710/715, cenário preto/riscado no 6ee7878) */
    if (w < 9u || w > 15u || mask != (1u << w) - 1u) return 0;
    if ((int32_t)g[34]._u32[0] != -(int32_t)(2u * w)) return 0;
    if (g[33]._u32[0] || g[33]._u32[1] || g[33]._u32[2] != 0xFFFFFFC0u || g[33]._u32[3] != 0xFFFFFFC0u) return 0;
    static const int zeros[] = { 14, 26, 22, 21, 43, 40, 42, 39, 50, 51, 49, 52 };
    for (size_t k = 0; k < sizeof zeros / sizeof zeros[0]; k++) if (!dff0_zero(g[zeros[k]])) return 0;
    const uint32_t r46 = g[46]._u32[0], out = g[54]._u32[0];
    if (g[38]._u32[0] != r46 + 16u || g[37]._u32[0] != r46 + 32u) return 0;
    if (g[55]._u32[0] != out + 16u || g[57]._u32[0] != out + 32u || g[56]._u32[0] != out + 48u || (out & 15u)) return 0;
    const int32_t r23 = (int32_t)g[23]._u32[0];
    if (r23 < 0) return 0;
    const uint32_t voltas = (uint32_t)r23 / (4u * w) + 1u;
    if (voltas < 3u || voltas > 0x400u) return 0;
    const uint32_t n = voltas - 2u;
    if (g[36]._u32[0] != 64u * (n - 1u)) return 0;
    if (out + 64u * n > SPU_LS_SIZE || r46 + (uint32_t)r23 + 64u > SPU_LS_SIZE || r46 + (uint32_t)r23 < 2u * w + 4u * w * (n - 1u)) return 0;
    uint8_t* ls = SPU_LS_BASE(ctx);
    uint16_t blocos[3][32];   /* lidos e ainda não gravados (o original grava duas voltas depois) */
    for (uint32_t i = 0; i < voltas; i++) {
        if (i < n) {
            const uint32_t f = r46 + (uint32_t)r23 - 2u * w - 4u * w * i;
            uint16_t* v = blocos[i % 3u];
            for (uint32_t j = 0; j < 32u; j++) {
                const uint32_t bo = j * w;
                const uint32_t a = (f + (bo >> 3)) & SPU_LS_MASK;
                const uint32_t x24 = ((uint32_t)ls[a] << 16) | ((uint32_t)ls[(a + 1u) & SPU_LS_MASK] << 8) | ls[(a + 2u) & SPU_LS_MASK];
                const uint32_t x = (x24 >> (24u - w - (bo & 7u))) & mask;
                v[j] = (uint16_t)((x >> 8) | (x << 8));   /* big-endian */
            }
        }
        uint8_t* d = ls + out + 64u * (i < 2u ? n - 1u : n + 1u - i);
        if (i < 2u) memset(d, 0, 64);
        else memcpy(d, blocos[(i - 2u) % 3u], 64);
    }
    return 1;
}

static __attribute__((noinline)) int fn_0000DCC0_dff0_nativo(spu_context* ctx)
{
    static volatile long nn[2];
    const int r = fn_0000DCC0_dff0_corpo(ctx);
    gowa_edge_conta("DCC0/DFF0", nn, r);
    return r;
}
#endif
