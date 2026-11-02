/* GOWA (02/11, EDGE nativo, marco E4): o laço 83E8 da 0x73E0 do job 0x72DF80 -- no fluxo B (r103, no lugar), cada
 * quadword fica com os 12 primeiros bytes dele e os 4 últimos (w) do quadword correspondente do fluxo A (selb com
 * r13 = os 4 últimos bytes). 8 quadwords (128 bytes) por volta, J voltas (r17 -= 8 enquanto > 0), lendo um bloco à
 * frente: o bloco 0 de A e de B já está nos registradores (r33.. e r24..), o bloco j de A em r84 + 128j, o de B em
 * r103 + 128j. Os registradores saem como o laço os deixa, inclusive os da faixa preservada (r81, r83, r84, r90-r98):
 * o último bloco gravado, o bloco seguinte já lido e os ponteiros. Se o que se grava em B encosta no que se lê de A,
 * volta 0 (fica o traduzido).
 * GOWA_EDGE_NATIVO=0 desliga. Gancho 'antes' do rótulo loc_000083E8, saltando para o fim do laço (edge_nativo.py). */
#ifndef GOWA_FN_000073E0_83E8_H
#define GOWA_FN_000073E0_83E8_H
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

static int fn_000073E0_83e8_corpo(spu_context* ctx)
{
    if (!gowa_edge_nativo_on()) return 0;
    u128* g = ctx->gpr;
    if (g[13]._u32[0] || g[13]._u32[1] || g[13]._u32[2] || g[13]._u32[3] != 0xFFFFFFFFu) return 0;
    const int32_t r17 = (int32_t)g[17]._u32[0];
    const uint32_t J = r17 > 8 ? (uint32_t)(r17 + 7) / 8u : 1u;
    const uint32_t B = g[14]._u32[0] & SPU_LS_MASK, A = g[84]._u32[0] & SPU_LS_MASK;   /* blocos 1.. de A: r84 + 128j */
    if (J > 0x200u || (B & 15u) || (A & 15u) || g[15]._u32[0] != g[14]._u32[0] + 128u || g[16]._u32[0] != g[84]._u32[0] + 128u) return 0;
    if (B + 128u * (J + 1u) > SPU_LS_SIZE || A + 128u * (J + 1u) > SPU_LS_SIZE) return 0;
    /* o que se grava ([B, B + 128J)) não pode encostar no que se lê de A (os blocos 1..J; o 0 está nos registradores) */
    if (A + 128u < B + 128u * J && B < A + 128u * (J + 1u)) return 0;
    const u128 m = g[13];
    /* bloco 0: dos registradores */
    static const int ra[8] = { 33, 30, 29, 28, 27, 26, 25, 32 }, rb[8] = { 24, 23, 22, 21, 20, 19, 18, 31 };
    for (int k = 0; k < 8; k++) spu_ls_write128(ctx, B + 16u * (uint32_t)k, spu_selb(g[rb[k]], g[ra[k]], m));
    /* blocos 1..J-1: o w de A sobre B, no lugar */
    uint8_t* ls = SPU_LS_BASE(ctx);
    for (uint32_t j = 1; j < J; j++)
        for (uint32_t k = 0; k < 8u; k++)
            memcpy(ls + B + 128u * j + 16u * k + 12u, ls + A + 128u * j + 16u * k + 12u, 4);
    /* registradores de saída */
    const uint32_t ult = B + 128u * (J - 1u);
    static const int rs[8] = { 97, 96, 95, 94, 92, 91, 90, 98 };
    for (int k = 0; k < 8; k++) g[rs[k]] = spu_ls_read128(ctx, ult + 16u * (uint32_t)k);
    for (int k = 0; k < 8; k++) {
        g[ra[k]] = spu_ls_read128(ctx, A + 128u * J + 16u * (uint32_t)k);
        g[rb[k]] = spu_ls_read128(ctx, B + 128u * J + 16u * (uint32_t)k);
    }
    g[17] = spu_ai(g[17], -(int32_t)(8u * J));
    g[79] = spu_cgti(g[17], 0);
    g[14] = spu_ai(g[14], (int32_t)(128u * J));
    g[81] = g[14];
    g[16] = spu_ai(g[16], (int32_t)(128u * J));
    g[83] = g[16];
    g[15] = spu_ai(g[15], (int32_t)(128u * J));
    g[84] = g[15];
    return 1;
}

static __attribute__((noinline)) int fn_000073E0_83e8_nativo(spu_context* ctx)
{
    static volatile long n[2];
    const int r = fn_000073E0_83e8_corpo(ctx);
    gowa_edge_conta("73E0/83E8", n, r);
    return r;
}
#endif
