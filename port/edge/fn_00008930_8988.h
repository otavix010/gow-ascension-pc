/* GOWA (01/11, EDGE nativo, marco E4): o laço 8988/89F0 da 0x8930 do job 0x72DF80 (primeira compactação dos
 * registros de 8 bytes). Para cada entrada i < cnt (cnt = r29 & 0xFF), e = LS[r33 + i]; e == 0xFF pula. Senão, com
 * o registro em r27 (= r5 + 8 + 8i): byte 3 == 1 e byte 1 == 8 é o caso especial (0x8EB8: copia o estado para
 * 0x10980/0x10990 e relê o cnt); fora dele, os 8 bytes em r31 + 8i + 8 vão para r26 + 8, e r26 += 8, r32 += 1.
 * Por volta: r28 += 1, r30 += 8, r27 += 8 (em todas as faixas, como o ai do original).
 * O nativo faz as voltas comuns até antes da última e para antes de um caso especial; o laço traduzido segue do
 * rótulo com os registradores ajustados (os temporários saem como no original). Origem e destino sobrepostos
 * devolvem 0. GOWA_EDGE_NATIVO=0 desliga. Gancho 'antes' do rótulo loc_00008988 (edge_nativo.py). */
#ifndef GOWA_FN_00008930_8988_H
#define GOWA_FN_00008930_8988_H
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

static int fn_00008930_8988_corpo(spu_context* ctx)
{
    if (!gowa_edge_nativo_on()) return 0;
    u128* g = ctx->gpr;
    const uint32_t cnt = g[29]._u32[0] & 0xFFu, tab = g[33]._u32[0], orig = g[31]._u32[0];
    uint32_t i = g[28]._u32[0], o30 = g[30]._u32[0], reg = g[27]._u32[0], dst = g[26]._u32[0];
    if (i + 1u >= cnt) return 0;
    /* faixas: destino [dst + 8, dst + 8 + 8 cnt) contra a origem [orig + o30 + 8, ...), os registros e a tabela */
    const uint32_t d0 = (dst + 8u) & SPU_LS_MASK, s0 = (orig + o30 + 8u) & SPU_LS_MASK, r0 = reg & SPU_LS_MASK, t0 = (tab + i) & SPU_LS_MASK;
    const uint32_t n = cnt - i;
    if (d0 + 8u * n > SPU_LS_SIZE || s0 + 8u * n > SPU_LS_SIZE || r0 + 8u * n > SPU_LS_SIZE || t0 + n > SPU_LS_SIZE) return 0;
    if ((d0 < s0 + 8u * n && s0 < d0 + 8u * n) || (d0 < r0 + 8u * n && r0 < d0 + 8u * n) || (d0 < t0 + n && t0 < d0 + 8u * n)) return 0;
    uint8_t* ls = SPU_LS_BASE(ctx);
    uint32_t feitas = 0, copias = 0;
    for (; i + 1u < cnt; i++) {
        const uint8_t e = ls[(tab + i) & SPU_LS_MASK];
        if (e != 0xFFu) {
            const uint32_t r = reg & SPU_LS_MASK;
            if (ls[r + 3u] == 1u && ls[r + 1u] == 8u) break;   /* caso especial: o traduzido faz */
            memcpy(ls + ((dst + 8u) & SPU_LS_MASK), ls + ((orig + o30 + 8u) & SPU_LS_MASK), 8);
            dst += 8u; copias++;
        }
        o30 += 8u; reg += 8u; feitas++;
    }
    if (!feitas) return 0;
    g[28] = spu_ai(g[28], (int)feitas);
    g[30] = spu_ai(g[30], (int)(8u * feitas));
    g[27] = spu_ai(g[27], (int)(8u * feitas));
    if (copias) { g[26] = spu_ai(g[26], (int)(8u * copias)); g[32] = spu_ai(g[32], (int)copias); }
    return 1;
}

static __attribute__((noinline)) int fn_00008930_8988_nativo(spu_context* ctx)
{
    static volatile long n[2];
    const int r = fn_00008930_8988_corpo(ctx);
    gowa_edge_conta("8930/8988", n, r);
    return r;
}
#endif
