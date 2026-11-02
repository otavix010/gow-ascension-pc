/* GOWA (02/11, EDGE nativo, marco E4): o laço DE6C da DCC0 do job 0x72DF80 -- índices empacotados com largura w <= 8
 * bits, MSB primeiro, desempacotados em u16 NO LUGAR (a saída fica por cima do próprio fluxo, por isso o original
 * anda do último bloco para o primeiro). Bloco b = 32 índices = 64 bytes em out + 64b (out = r32), índice j do
 * bloco: hw = bytes[a], bytes[a+1] com a = fluxo_b + (j*w >> 3), valor = (hw >> (16 - w - (j*w & 7))) & (2^w - 1),
 * fluxo_b = r41 + r10 - 4w(n-1-b). O original roda n + 2 voltas em software pipeline: a volta i lê a janela do
 * bloco n-1-i (antes das gravações dela) e grava o que leu duas voltas antes; as voltas 0 e 1 gravam zeros no
 * endereço do último bloco. Aqui a mesma ordem de leituras e gravações (então a sobreposição dá o mesmo resultado).
 * Modelo: scripts_edge/de6c_modelo.py (11/11 fotos). Na saída (loc_0000E0E4) só o r10 dos registradores do laço
 * é lido depois (scripts_edge/vivos.py --abi): r10 += r17 por volta. Estados fora do comum voltam 0 (fica o
 * traduzido). GOWA_EDGE_NATIVO=0 desliga. Gancho 'antes' do rótulo, saltando para loc_0000E0E4. */
#ifndef GOWA_FN_0000DCC0_DE6C_H
#define GOWA_FN_0000DCC0_DE6C_H
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

static inline int de6c_zero(u128 v) { return !(v._u32[0] | v._u32[1] | v._u32[2] | v._u32[3]); }

static int fn_0000DCC0_de6c_corpo(spu_context* ctx)
{
    if (!gowa_edge_nativo_on()) return 0;
    u128* g = ctx->gpr;
    const uint32_t mask = g[27]._u32[0] & 0xFFFFu;
    for (int i = 0; i < 4; i++) if (g[27]._u32[i] != (mask | (mask << 16))) return 0;
    const uint32_t w = (uint32_t)__builtin_popcount(mask);
    if (w < 1u || w > 8u || mask != (1u << w) - 1u) return 0;
    if ((int32_t)g[17]._u32[0] != -(int32_t)(4u * w)) return 0;
    if (g[14]._u32[0] || g[14]._u32[1] || g[14]._u32[2] != 0xFFFFFFC0u || g[14]._u32[3] != 0xFFFFFFC0u) return 0;
    if (!de6c_zero(g[6]) || !de6c_zero(g[16]) || !de6c_zero(g[18]) || !de6c_zero(g[24]) ||
        !de6c_zero(g[15]) || !de6c_zero(g[26]) || !de6c_zero(g[30])) return 0;
    const uint32_t r41 = g[31]._u32[0], out = g[32]._u32[0];
    if (g[23]._u32[0] != r41 + 16u || g[21]._u32[0] != r41 + 32u) return 0;
    if (g[35]._u32[0] != out + 16u || g[34]._u32[0] != out + 32u || g[33]._u32[0] != out + 48u || (out & 15u)) return 0;
    const int32_t r10 = (int32_t)g[10]._u32[0];
    if (r10 < 0) return 0;
    const uint32_t voltas = (uint32_t)r10 / (4u * w) + 1u;
    if (voltas < 3u || voltas > 0x400u) return 0;
    const uint32_t n = voltas - 2u;
    if (g[36]._u32[0] != 64u * (n - 1u)) return 0;
    if (out + 64u * n > SPU_LS_SIZE || r41 + (uint32_t)r10 + 64u > SPU_LS_SIZE || r41 + (uint32_t)r10 < 4u * w * (n - 1u)) return 0;
    uint8_t* ls = SPU_LS_BASE(ctx);
    uint16_t blocos[3][32];   /* lidos e ainda não gravados (o original grava duas voltas depois) */
    for (uint32_t i = 0; i < voltas; i++) {
        if (i < n) {
            const uint32_t f = r41 + (uint32_t)r10 - 4u * w * i;
            uint16_t* v = blocos[i % 3u];
            for (uint32_t j = 0; j < 32u; j++) {
                const uint32_t bo = j * w;
                const uint32_t a = (f + (bo >> 3)) & SPU_LS_MASK;
                const uint32_t hw = ((uint32_t)ls[a] << 8) | ls[(a + 1u) & SPU_LS_MASK];
                const uint32_t x = (hw >> (16u - w - (bo & 7u))) & mask;
                v[j] = (uint16_t)((x >> 8) | (x << 8));   /* big-endian */
            }
        }
        uint8_t* d = ls + out + 64u * (i < 2u ? n - 1u : n + 1u - i);
        if (i < 2u) memset(d, 0, 64);
        else memcpy(d, blocos[(i - 2u) % 3u], 64);
    }
    for (uint32_t i = 0; i < voltas; i++) g[10] = spu_a(g[10], g[17]);
    return 1;
}

static __attribute__((noinline)) int fn_0000DCC0_de6c_nativo(spu_context* ctx)
{
    static volatile long nn[2];
    const int r = fn_0000DCC0_de6c_corpo(ctx);
    gowa_edge_conta("DCC0/DE6C", nn, r);
    return r;
}
#endif
