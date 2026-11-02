/* GOWA (02/11, EDGE nativo, marco E4): o laço CD1C/CCD0 da 0xCC98 do job 0x72DF80 -- para cada entrada de 32 bytes
 * da lista em r6 (r15 de 0 até r7): w = palavra em r6 + 16; se w != 0, grava em r8 (desalinhado, 16 bytes) o
 * registro {C0, w & 0xFF0, C2, palavra em r6 + 12}, com C0/C2 = palavras 0 e 2 do quadword constante em 0xF930, e
 * r8 += 16; r6 += 32 sempre. O nativo roda no topo do laço (logo depois do rótulo CD1C) e faz as voltas que faltam;
 * o código do topo então vê r15 == r7 e sai como no original (os temporários dele saem iguais; os do corpo não são
 * lidos depois). Lista e saída sobrepostas ou endereços desalinhados voltam 0. GOWA_EDGE_NATIVO=0 desliga.
 * Gancho 'depois_rotulo' em loc_0000CD1C (edge_nativo.py). */
#ifndef GOWA_FN_0000CC98_CCD0_H
#define GOWA_FN_0000CC98_CCD0_H
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

static int fn_0000CC98_ccd0_corpo(spu_context* ctx)
{
    if (!gowa_edge_nativo_on()) return 0;
    u128* g = ctx->gpr;
    const uint32_t r7 = g[7]._u32[0];
    uint32_t r15 = g[15]._u32[0];
    if (r15 == r7) return 0;
    const uint32_t n = r7 - r15;
    const uint32_t lista = g[6]._u32[0] & SPU_LS_MASK, out = g[8]._u32[0] & SPU_LS_MASK;
    if (n > 0x2000u || (lista & 3u) || lista + 32u * n > SPU_LS_SIZE || out + 16u * n > SPU_LS_SIZE) return 0;
    if (out < lista + 32u * n && lista < out + 16u * n) return 0;
    if (out < 0xF940u && 0xF930u < out + 16u * n) return 0;           /* a constante */
    uint8_t* ls = SPU_LS_BASE(ctx);
    uint8_t reg[16];
    memcpy(reg, ls + 0xF930u, 16);
    uint32_t o = out, gravados = 0;
    for (uint32_t k = 0; k < n; k++) {
        const uint8_t* e = ls + lista + 32u * k;
        uint32_t w;
        memcpy(&w, e + 16, 4);
        if (!w) continue;
        w &= __builtin_bswap32(0x0FF0u);
        memcpy(reg + 4, &w, 4);
        memcpy(reg + 12, e + 12, 4);
        memcpy(ls + o, reg, 16);
        o += 16u; gravados++;
    }
    g[15] = spu_ai(g[15], (int32_t)n);
    g[6] = spu_ai(g[6], (int32_t)(32u * n));
    if (gravados) g[8] = spu_ai(g[8], (int32_t)(16u * gravados));
    return 1;
}

static __attribute__((noinline)) int fn_0000CC98_ccd0_nativo(spu_context* ctx)
{
    static volatile long n[2];
    const int r = fn_0000CC98_ccd0_corpo(ctx);
    gowa_edge_conta("CC98/CCD0", n, r);
    return r;
}
#endif
