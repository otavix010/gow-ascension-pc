/* GOWA (01/11, EDGE nativo, marco E4): função 0xBFB8 do job 0x72DF80 inteira -- monta a estrutura de estado S (r3) a
 * partir do descritor D (16 bytes em r4) e dos argumentos (folha, sem pilha):
 *   S+0x00 = D (cópia); S+0x10 + 4k = r5 + k * passo, k = 0..(D[0] & 15), passo = ((u16 D[8..9] + 7) & ~7) << 4;
 *   S+0x50 = {0, 0, r7 + r8, 0}; S+0x60 = {0, 0, 0, P[0]}; S+0x70 = {P[4], P[8], 0, P[16]};
 *   S+0x80 = {P[20], P[24], P[28], P[32]}; S+0x90 palavra 0 = P[36] (P = r12; P = 0 zera esses campos);
 *   S+0xA0..0xCF = 48 bytes de r11 (se r11 != 0); S+0xD0..0x14F = 128 bytes de r10 (se r10 != 0);
 *   S+0x150 = {r9, r9, -, -}; S+0x160 = 0; S+0x170 = {r5, r5 + r6, r7, r7 + r8};
 *   S+0x180 = {r14, Q[0], Q[4], -} (Q = r13; 0 se Q = 0); S+0x190 bytes 0-1 = D[10..11], byte 2 &= 0x7F.
 * As palavras de P e Q são lidas como o original lê (rotqby dentro do quadword do endereço). Endereços fora do comum
 * ou sobrepostos à estrutura voltam 0 (fica o traduzido). GOWA_EDGE_NATIVO=0 desliga. Gancho 'entrada'. */
#ifndef GOWA_FN_0000BFB8_H
#define GOWA_FN_0000BFB8_H
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

static inline int bfb8_fora(uint32_t a, uint32_t n, uint32_t s) { return a < s + 0x1A0u && s < a + n; }

static int fn_0000BFB8_corpo(spu_context* ctx)
{
    if (!gowa_edge_nativo_on()) return 0;
    u128* g = ctx->gpr;
    const uint32_t S = g[3]._u32[0] & SPU_LS_MASK, D = g[4]._u32[0] & SPU_LS_MASK;
    const uint32_t m10 = g[10]._u32[0], m11 = g[11]._u32[0], P = g[12]._u32[0], Q = g[13]._u32[0];
    if ((S & 15u) || (D & 15u) || S + 0x1A0u > SPU_LS_SIZE || bfb8_fora(D, 16u, S)) return 0;
    if (m10 && (((m10 & SPU_LS_MASK) & 15u) || (m10 & SPU_LS_MASK) + 128u > SPU_LS_SIZE || bfb8_fora(m10 & SPU_LS_MASK, 128u, S))) return 0;
    if (m11 && (((m11 & SPU_LS_MASK) & 15u) || (m11 & SPU_LS_MASK) + 48u > SPU_LS_SIZE || bfb8_fora(m11 & SPU_LS_MASK, 48u, S))) return 0;
    if (P && (((P & SPU_LS_MASK) & 3u) || (P & SPU_LS_MASK) + 40u > SPU_LS_SIZE || bfb8_fora(P & SPU_LS_MASK, 40u, S))) return 0;
    if (Q && (((Q & SPU_LS_MASK) & 3u) || (Q & SPU_LS_MASK) + 8u > SPU_LS_SIZE || bfb8_fora(Q & SPU_LS_MASK, 8u, S))) return 0;
    uint8_t* ls = SPU_LS_BASE(ctx);
    uint8_t d[16];
    memcpy(d, ls + D, 16);
    const uint32_t r5 = g[5]._u32[0], r6 = g[6]._u32[0], r7 = g[7]._u32[0], r8 = g[8]._u32[0], r9 = g[9]._u32[0];
    /* a ordem das escritas segue o original (sem sobreposição, a ordem não muda o resultado) */
    spu_ls_write32(ctx, S + 0x50u, 0u); spu_ls_write32(ctx, S + 0x54u, 0u);
    spu_ls_write32(ctx, S + 0x58u, r7 + r8); spu_ls_write32(ctx, S + 0x5Cu, 0u);
    memset(ls + S + 0x160u, 0, 16);
    spu_ls_write32(ctx, S + 0x170u, r5); spu_ls_write32(ctx, S + 0x174u, r5 + r6);
    spu_ls_write32(ctx, S + 0x178u, r7); spu_ls_write32(ctx, S + 0x17Cu, r7 + r8);
    spu_ls_write32(ctx, S + 0x180u, g[14]._u32[0]);
    ls[S + 0x190u] = d[10]; ls[S + 0x191u] = d[11]; ls[S + 0x192u] &= 0x7Fu;
    spu_ls_write32(ctx, S + 0x150u, r9); spu_ls_write32(ctx, S + 0x154u, r9);
    spu_ls_write32(ctx, S + 0x60u, 0u); spu_ls_write32(ctx, S + 0x64u, 0u); spu_ls_write32(ctx, S + 0x68u, 0u);
    memcpy(ls + S, d, 16);
    const uint32_t n = (d[0] & 15u) + 1u, passo = ((((uint32_t)d[8] << 8 | d[9]) + 7u) & ~7u) << 4;
    for (uint32_t k = 0; k < n; k++) spu_ls_write32(ctx, S + 16u + 4u * k, r5 + k * passo);
    if (m10) memcpy(ls + S + 0xD0u, ls + (m10 & SPU_LS_MASK), 128);
    if (m11) memcpy(ls + S + 0xA0u, ls + (m11 & SPU_LS_MASK), 48);
    if (!P) {
        spu_ls_write32(ctx, S + 0x6Cu, 0u);
        memset(ls + S + 0x70u, 0, 32);
        spu_ls_write32(ctx, S + 0x90u, 0u);
    } else {
        const uint32_t p = P & SPU_LS_MASK;
        spu_ls_write32(ctx, S + 0x6Cu, spu_ls_read32(ctx, p));
        spu_ls_write32(ctx, S + 0x70u, spu_ls_read32(ctx, p + 4u));
        spu_ls_write32(ctx, S + 0x74u, spu_ls_read32(ctx, p + 8u));
        spu_ls_write32(ctx, S + 0x78u, 0u);
        spu_ls_write32(ctx, S + 0x7Cu, spu_ls_read32(ctx, p + 16u));
        for (uint32_t k = 0; k < 4u; k++) spu_ls_write32(ctx, S + 0x80u + 4u * k, spu_ls_read32(ctx, p + 20u + 4u * k));
        spu_ls_write32(ctx, S + 0x90u, spu_ls_read32(ctx, p + 36u));
    }
    if (!Q) { spu_ls_write32(ctx, S + 0x184u, 0u); spu_ls_write32(ctx, S + 0x188u, 0u); }
    else {
        const uint32_t q = Q & SPU_LS_MASK;
        spu_ls_write32(ctx, S + 0x184u, spu_ls_read32(ctx, q));
        spu_ls_write32(ctx, S + 0x188u, spu_ls_read32(ctx, q + 4u));
    }
    return 1;
}

static __attribute__((noinline)) int fn_0000BFB8_nativo(spu_context* ctx)
{
    static volatile long n[2];
    const int r = fn_0000BFB8_corpo(ctx);
    gowa_edge_conta("BFB8", n, r);
    return r;
}
#endif
