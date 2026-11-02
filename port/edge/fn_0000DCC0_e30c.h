/* GOWA (01/11, EDGE nativo, marco E4): o laço E30C da 0xDCC0 do job 0x72DF80 -- cópia desalinhada da origem r6 + 16
 * (= r60) para o destino alinhado r7, 16 bytes por volta, com a gravação uma volta atrás: a 1a volta grava um
 * quadword zerado no destino e a volta j >= 2 grava os bytes [origem + 16(j-2), +16) em destino + 16(j-2). São
 * m = 1 + ceil(r61 / 16) voltas (r61 = 0: só a gravação zerada); a última volta lê a janela seguinte e não grava.
 * Coberto: destino alinhado e origem pelo menos 16 bytes acima do destino ou sem sobreposição (aí a ordem do
 * original dá o mesmo que um memmove para frente); o resto volta 0. Os registradores saem como o laço os deixa,
 * calculados com as mesmas operações da SPU (spu_ai/andi/shufb/a/clgt) faixa a faixa. GOWA_EDGE_NATIVO=0 desliga.
 * Gancho 'antes' do rótulo loc_0000E30C (edge_nativo.py). */
#ifndef GOWA_FN_0000DCC0_E30C_H
#define GOWA_FN_0000DCC0_E30C_H
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

static int fn_0000DCC0_e30c_corpo(spu_context* ctx)
{
    if (!gowa_edge_nativo_on()) return 0;
    u128* g = ctx->gpr;
    /* o estado da entrada do laço: r8 = r3 = r10 = r11 = 0, r9 = -16 */
    if (g[8]._u32[0] || g[3]._u32[0] || g[3]._u32[1] || g[3]._u32[2] || g[3]._u32[3] || g[9]._u32[0] != 0xFFFFFFF0u) return 0;
    for (int k = 0; k < 4; k++) if (g[10]._u32[k] || g[11]._u32[k]) return 0;
    const uint32_t len = g[61]._u32[0];
    const uint32_t dst = g[7]._u32[0] & SPU_LS_MASK, src = (g[6]._u32[0] + 16u) & SPU_LS_MASK;
    if ((dst & 15u) || len > 0x20000u) return 0;
    const uint32_t m = len ? 1u + (len + 15u) / 16u : 1u, n = 16u * (m - 1u);
    if (dst + 16u > SPU_LS_SIZE || dst + n > SPU_LS_SIZE || ((src & ~15u) + n + 32u) > SPU_LS_SIZE) return 0;
    if (!(src >= dst + 16u || src + n + 32u <= dst)) return 0;
    uint8_t* ls = SPU_LS_BASE(ctx);
    if (m == 1u) memset(ls + dst, 0, 16);
    else memmove(ls + dst, ls + src, n);
    /* registradores de saída */
    const u128 g6 = g[6], g7 = g[7], g9 = g[9];
    g[38] = spu_ai(g6, (int32_t)(16u * m));
    g[6] = g[38];
    g[9] = spu_ai(g9, (int32_t)(16u * m));
    g[22] = spu_andi(g[38], 15);
    g[23] = spu_shufb(g[22], g[22], g[58]);
    g[3] = spu_a(g[23], g[53]);
    g[31] = spu_clgt(g[61], g[9]);
    if (m == 1u) { g[29] = spu_shufb(g[11], g[10], spu_il(0)); }   /* zeros, como a 1a volta */
    else g[29] = spu_ls_read128(ctx, dst + 16u * (m - 2u));
    g[7] = spu_ai(g7, (int32_t)n);
    g[17] = g[7];
    g[8] = spu_il(16);
    g[11] = spu_ls_read128(ctx, g[38]._u32[0]);
    g[10] = spu_ls_read128(ctx, g[38]._u32[0] + 16u);
    return 1;
}

static __attribute__((noinline)) int fn_0000DCC0_e30c_nativo(spu_context* ctx)
{
    static volatile long n[2];
    const int r = fn_0000DCC0_e30c_corpo(ctx);
    gowa_edge_conta("DCC0/E30C", n, r);
    return r;
}
#endif
