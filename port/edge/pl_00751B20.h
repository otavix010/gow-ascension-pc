/* GOWA (02/11, EDGE nativo, marco E4): o laço 0x30358 do plugin 0x751B20 do EDGE, da entrada do laço ao fim (a
 * preparação 0x30000, que acrescenta os atributos 1, 3, 4 e 20 na lista do contexto e copia a tabela de seleção para
 * a pilha, continua a traduzida).
 *
 * Vértice de 22 bytes: posição float3 (sai com w = a palavra 3 de r67, 1,0) + os 48 bits dos dois vetores unitários
 * "smallest three" do 0x752710 (mesmos campos, contas e seleção: pl_00752710.h; tabela em r42) + cor RGBA em 4 bytes,
 * cada um -> fma(cuflt(byte << 16, 139), r56, r57) (r56 = 1/255, r57 = 0: as primitivas do runtime, como no 0x755A20).
 * Quatro fluxos de saída de 16 bytes por vértice, todos a partir do fim (r28 = -32 * pares): posição em r69, cor em
 * r70, vetores em r54 e r55 (com uma volta de atraso: a 1a volta grava os vetores "do par -1", feitos dos
 * registradores que a preparação deixou, nos 32 bytes depois do fim de cada fluxo de vetores). Entrada em r35 - 88.
 * No fim: r1 += 256, r80 e r126 relidos da pilha, r3 = 0. Temporários ficam diferentes (GOWA_CONF_REGS_ABI=1). Não
 * roda nas gravações: conferido na bancada sintética (preparação traduzida, o laço traduzido x este sobre as mesmas
 * LS, as duas builds). GOWA_EDGE_NATIVO=0 desliga. Gancho 'entrada' na função do laço. */
#ifndef GOWA_PL_00751B20_H
#define GOWA_PL_00751B20_H
#include "pl_00752710.h"

static inline int pl751b_igual(u128 a, uint32_t w0, uint32_t w1, uint32_t w2, uint32_t w3)
{
    return a._u32[0] == w0 && a._u32[1] == w1 && a._u32[2] == w2 && a._u32[3] == w3;
}

static int pl_00751B20_corpo(spu_context* ctx)
{
#if PL752_VETOR
    if (!gowa_edge_nativo_on()) return 0;
    u128* g = ctx->gpr;
    /* as máscaras da seleção e as constantes das contas (as do pl752_sel / pl752_contas), a cor e o w */
    if (!pl751b_igual(g[32], 0x80000000u, 0x80000000u, 0x80000000u, 0xFFFFFFFFu) ||
        !pl751b_igual(g[30], 0x7FFFFFFFu, 0x7FFFFFFFu, 0x7FFFFFFFu, 0) || !pl751b_igual(g[31], 0, 0, 0, 0x0C0D0E0Fu) ||
        !pl751b_igual(g[58], 0x08080808u, 0x08080808u, 0x08080808u, 0) ||
        g[61]._u32[0] != 0x46352E7Cu || g[60]._u32[0] != 0xC6353150u || g[62]._u32[0] != 0x3F800000u ||
        !pl751b_igual(g[73], 0x80028080u, 0x80038080u, 0x80048080u, 0x80058080u) ||
        !pl751b_igual(g[74], 0x80088080u, 0x80098080u, 0x800A8080u, 0x800B8080u) ||
        !pl751b_igual(g[80], 0, 0, 0, 0xFFFFFFFFu) || g[67]._u32[3] != 0x3F800000u || g[19]._u32[0] != 0)
        return 0;
    const int32_t r28 = (int32_t)g[28]._u32[0];
    if (r28 >= 0 || (r28 & 31) || r28 < -0x10000) return 0;
    const uint32_t P = (uint32_t)(-r28) / 32u;
    if (g[64]._u32[0] != g[69]._u32[0] + 16u || g[63]._u32[0] != g[70]._u32[0] + 16u || g[65]._u32[0] != g[54]._u32[0] + 16u ||
        g[66]._u32[0] != g[55]._u32[0] + 16u || g[42]._u32[0] != g[1]._u32[0] + 32u)
        return 0;
    const uint32_t inp = (g[35]._u32[0] - 88u) & SPU_LS_MASK, tab = g[42]._u32[0] & SPU_LS_MASK;
    const uint32_t po = (g[69]._u32[0] + (uint32_t)r28) & SPU_LS_MASK, co = (g[70]._u32[0] + (uint32_t)r28) & SPU_LS_MASK,
                   v1 = (g[54]._u32[0] + (uint32_t)r28) & SPU_LS_MASK, v2 = (g[55]._u32[0] + (uint32_t)r28) & SPU_LS_MASK;
    const uint32_t ni = 44u * P + 128u, no = 32u * P, nv = 32u * P + 32u;
    if (((po | co | v1 | v2 | tab) & 15u) || inp + ni > SPU_LS_SIZE || po + no > SPU_LS_SIZE || co + no > SPU_LS_SIZE ||
        v1 + nv > SPU_LS_SIZE || v2 + nv > SPU_LS_SIZE || tab + 256u > SPU_LS_SIZE)
        return 0;
#define PLB_ENC(a0, n0, b0, n1) ((a0) < (b0) + (n1) && (b0) < (a0) + (n0))
    if (PLB_ENC(po, no, inp, ni) || PLB_ENC(co, no, inp, ni) || PLB_ENC(v1, nv, inp, ni) || PLB_ENC(v2, nv, inp, ni) ||
        PLB_ENC(po, no, tab, 256u) || PLB_ENC(co, no, tab, 256u) || PLB_ENC(v1, nv, tab, 256u) || PLB_ENC(v2, nv, tab, 256u) ||
        PLB_ENC(po, no, co, no) || PLB_ENC(po, no, v1, nv) || PLB_ENC(po, no, v2, nv) || PLB_ENC(co, no, v1, nv) ||
        PLB_ENC(co, no, v2, nv) || PLB_ENC(v1, nv, v2, nv))
        return 0;
#undef PLB_ENC
    uint8_t* ls = SPU_LS_BASE(ctx);
    /* 1a volta do original: os vetores "do par -1" com os registradores que a preparação deixou, depois do fim */
    spu_ls_write128(ctx, v1 + no, pl752_sel(g[12], g[29], g[40], 0));
    spu_ls_write128(ctx, v1 + no + 16u, pl752_sel(g[12], g[29], g[41], 1));
    spu_ls_write128(ctx, v2 + no, pl752_sel(g[43], g[34], g[47], 0));
    spu_ls_write128(ctx, v2 + no + 16u, pl752_sel(g[43], g[34], g[49], 1));
    const u128 esc = g[56], vies = g[57];
    for (uint32_t p0 = 0; p0 < P; p0 += PL752_PARES) {
        uint32_t f1[4 * PL752_PARES], f2[4 * PL752_PARES], ab1[4 * PL752_PARES], ab2[4 * PL752_PARES], c1[4 * PL752_PARES], c2[4 * PL752_PARES];
        uint32_t s1[2 * PL752_PARES], s2[2 * PL752_PARES];
        for (uint32_t q = 0; q < PL752_PARES; q++)
            for (uint32_t k = 0; k < 2; k++) {
                const uint8_t* r = &ls[(inp + 44u * (p0 + q) + 22u * k) & SPU_LS_MASK];
                const uint64_t bits = ((uint64_t)r[12] << 40) | ((uint64_t)r[13] << 32) | ((uint64_t)r[14] << 24) |
                                      ((uint64_t)r[15] << 16) | ((uint64_t)r[16] << 8) | r[17];
                f1[4 * q + 2 * k] = (uint32_t)(bits >> 38) & 0x3FFu; f1[4 * q + 2 * k + 1] = (uint32_t)(bits >> 28) & 0x3FFu;
                f2[4 * q + 2 * k] = (uint32_t)(bits >> 14) & 0x3FFu; f2[4 * q + 2 * k + 1] = (uint32_t)(bits >> 4) & 0x3FFu;
                s1[2 * q + k] = (uint32_t)(bits >> 24) & 0xFu; s2[2 * q + k] = (uint32_t)bits & 0xFu;
            }
        pl752_contas(f1, ab1, c1);
        pl752_contas(f2, ab2, c2);
        for (uint32_t q = 0; q < PL752_PARES && p0 + q < P; q++) {
            const uint32_t p = p0 + q;
            for (uint32_t k = 0; k < 2; k++) {
                const uint8_t* r = &ls[(inp + 44u * p + 22u * k) & SPU_LS_MASK];
                pl752_pos(ls, po + 32u * p + 16u * k, r);
                u128 x;
                for (int c = 0; c < 4; c++) x._u32[c] = (uint32_t)r[18 + c] << 16;
                spu_ls_write128(ctx, co + 32u * p + 16u * k, spu_fma(spu_cuflt(x, 139), esc, vies));
            }
            const u128 A1 = pl752_u128(&ab1[4 * q]), C1 = pl752_u128(&c1[4 * q]), A2 = pl752_u128(&ab2[4 * q]), C2 = pl752_u128(&c2[4 * q]);
            spu_ls_write128(ctx, v1 + 32u * p, pl752_sel(C1, A1, spu_ls_read128(ctx, tab + 16u * s1[2 * q]), 0));
            spu_ls_write128(ctx, v1 + 32u * p + 16u, pl752_sel(C1, A1, spu_ls_read128(ctx, tab + 16u * s1[2 * q + 1]), 1));
            spu_ls_write128(ctx, v2 + 32u * p, pl752_sel(C2, A2, spu_ls_read128(ctx, tab + 16u * s2[2 * q]), 0));
            spu_ls_write128(ctx, v2 + 32u * p + 16u, pl752_sel(C2, A2, spu_ls_read128(ctx, tab + 16u * s2[2 * q + 1]), 1));
        }
    }
    g[1] = spu_ai(g[1], 256);
    g[80] = spu_ls_read128(ctx, g[1]._u32[0] - 0x10u);
    g[126] = spu_ls_read128(ctx, g[1]._u32[0] - 0x20u);
    g[3] = spu_il(0);
    return 1;
#else
    (void)ctx;
    return 0;
#endif
}

static __attribute__((noinline)) int pl_00751B20_nativo(spu_context* ctx)
{
    static volatile long n[2];
    const int r = pl_00751B20_corpo(ctx);
    gowa_edge_conta("pl751B20", n, r);
    return r;
}
#endif
