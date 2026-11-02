/* GOWA (02/11, EDGE nativo, marco E4): os dois laços internos do orquestrador 0x9090 do job 0x72DF80 -- palavras
 * 0xFF000000 | endereço gravadas em sequência a partir de r13 (comandos do buffer, um por bloco de 256 bytes):
 *   9620: r5 (= 2047) palavras r8 | r9 (r9 = 0xFF000000), r8 += 256, r13 += 4;
 *   96E0: enquanto r80 > 256: palavra r81 | r6 (r6 = 0xFF000000), r80 -= 256, r81 += 256, r13 += 4.
 * Os registradores saem como o laço os deixa (inclusive os temporários da última volta). Palavras desalinhadas ou
 * faixas fora da LS voltam 0 (fica o traduzido). GOWA_EDGE_NATIVO=0 desliga. Ganchos 'antes' dos rótulos
 * loc_00009620 e loc_000096E0, saltando para o fim do laço (edge_nativo.py). */
#ifndef GOWA_FN_00009090_LACOS_H
#define GOWA_FN_00009090_LACOS_H
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

/* grava n palavras v, v + 256, ... a partir de a; devolve o conteúdo do quadword da última palavra ANTES dela */
static u128 l9090_enche(spu_context* ctx, uint32_t a, uint32_t v, uint32_t n)
{
    u128 antes = spu_ls_read128(ctx, a + 4u * (n - 1u));
    uint8_t* ls = SPU_LS_BASE(ctx);
    for (uint32_t k = 0; k < n; k++) {
        const uint32_t w = __builtin_bswap32((v + 256u * k) | 0xFF000000u);
        memcpy(ls + ((a + 4u * k) & SPU_LS_MASK), &w, 4);
        if (k + 2u == n) antes = spu_ls_read128(ctx, a + 4u * (n - 1u));   /* as voltas anteriores podem ter gravado nele */
    }
    return antes;
}

static int fn_00009090_9620_corpo(spu_context* ctx)
{
    if (!gowa_edge_nativo_on()) return 0;
    u128* g = ctx->gpr;
    const uint32_t n = g[5]._u32[0], a = g[13]._u32[0] & SPU_LS_MASK;
    if (n != 2047u || g[5]._u32[1] != n || g[5]._u32[2] != n || g[5]._u32[3] != n) return 0;
    if (g[9]._u32[0] != 0xFF000000u || (a & 3u) || a + 4u * n > SPU_LS_SIZE || (g[8]._u32[0] & 0xFF000000u)) return 0;
    const u128 r8_0 = g[8], r13_0 = g[13];
    const uint32_t v_ult = g[8]._u32[0] + 256u * (n - 1u);
    if ((v_ult & 0xFF000000u) || v_ult < g[8]._u32[0]) return 0;
    const u128 antes = l9090_enche(ctx, a, g[8]._u32[0], n);
    /* temporários da última volta */
    const u128 r8_ult = spu_ai(r8_0, (int32_t)(256u * (n - 1u))), r13_ult = spu_ai(r13_0, (int32_t)(4u * (n - 1u)));
    g[77] = spu_or(r8_ult, g[9]);
    g[76] = antes;
    g[78] = spu_cwd(r13_ult, 0);
    g[75] = spu_shufb(g[77], g[76], g[78]);
    g[5] = spu_ai(g[5], -(int32_t)n);
    g[8] = spu_ai(r8_0, (int32_t)(256u * n));
    g[13] = spu_ai(r13_0, (int32_t)(4u * n));
    return 1;
}

static int fn_00009090_96e0_corpo(spu_context* ctx)
{
    if (!gowa_edge_nativo_on()) return 0;
    u128* g = ctx->gpr;
    const uint32_t r80 = g[80]._u32[0], a = g[13]._u32[0] & SPU_LS_MASK;
    if (r80 <= 256u || g[6]._u32[0] != 0xFF000000u || (a & 3u)) return 0;
    const uint32_t n = (r80 - 1u) / 256u;   /* voltas: r80 - 256k > 256 para k < n */
    if (a + 4u * n > SPU_LS_SIZE || (g[81]._u32[0] & 0xFF000000u)) return 0;
    const uint32_t v_ult = g[81]._u32[0] + 256u * (n - 1u);
    if ((v_ult & 0xFF000000u) || v_ult < g[81]._u32[0]) return 0;
    const u128 r80_0 = g[80], r81_0 = g[81], r13_0 = g[13];
    const u128 antes = l9090_enche(ctx, a, g[81]._u32[0], n);
    const u128 r81_ult = spu_ai(r81_0, (int32_t)(256u * (n - 1u))), r13_ult = spu_ai(r13_0, (int32_t)(4u * (n - 1u)));
    g[39] = spu_or(r81_ult, g[6]);
    g[38] = antes;
    g[14] = spu_cwd(r13_ult, 0);
    g[37] = spu_shufb(g[39], g[38], g[14]);
    g[80] = spu_ai(r80_0, -(int32_t)(256u * n));
    g[81] = spu_ai(r81_0, (int32_t)(256u * n));
    g[36] = spu_clgti(g[80], 256);
    g[13] = spu_ai(r13_0, (int32_t)(4u * n));
    return 1;
}

static __attribute__((noinline)) int fn_00009090_9620_nativo(spu_context* ctx)
{
    static volatile long n[2];
    const int r = fn_00009090_9620_corpo(ctx);
    gowa_edge_conta("9090/9620", n, r);
    return r;
}

static __attribute__((noinline)) int fn_00009090_96e0_nativo(spu_context* ctx)
{
    static volatile long n[2];
    const int r = fn_00009090_96e0_corpo(ctx);
    gowa_edge_conta("9090/96E0", n, r);
    return r;
}
#endif
