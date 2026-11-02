/* GOWA (02/11, EDGE nativo, marco E4): o laço 0x30230 do plugin 0x757930 do EDGE (o primo de 1 vetor do 0x752710),
 * da entrada do laço ao fim do plugin (a preparação 0x30000 continua a traduzida).
 *
 * Vértice de 15 bytes: posição float3 (sai com w = 1,0) + 24 bits de um vetor unitário "smallest three": dois
 * componentes de 10 bits (bits 14-23 e 4-13) e o seletor (bits 0-3). As contas (fma com 11595,62 e -11596,33, o
 * terceiro = fm(u, fi(u, frsqest(u))), u = 1 - a² - b²) e a seleção pela tabela que a preparação copiou para a pilha
 * (r35 + seletor*16; o vértice B soma 8 aos bytes do padrão) são as do 0x752710 (pl752_contas e pl752_sel do
 * pl_00752710.h): as máscaras r29/r30/r31/r40 do plugin são conferidas iguais às de lá. 2 vértices por volta:
 * posições em r37 + r19 / r34 + r19, vetores com uma volta de atraso em r32 / r36 + o r19 anterior; a 1a volta grava
 * os vetores "do par -1" (zeros: a preparação deixa r8, r25, r26 e r27 em 0) depois do fim do fluxo. No fim, como o
 * original: r1 += 224, r3 = 0. Temporários ficam diferentes (GOWA_CONF_REGS_ABI=1). GOWA_EDGE_NATIVO=0 desliga.
 * Gancho 'entrada' na função do laço (edge_nativo.py). */
#ifndef GOWA_PL_00757930_H
#define GOWA_PL_00757930_H
#include "pl_00752710.h"

#if PL752_VETOR
/* os P pares: o lixo "do par -1" (zeros) depois do fim do fluxo de vetores, as posições (w = 1,0) e o vetor de cada
 * vértice (contas e seleção do 0x752710, tabela em tab) */
static void pl757_escreve(spu_context* ctx, uint32_t inp, uint32_t po, uint32_t vo, uint32_t P, uint32_t tab)
{
    uint8_t* ls = SPU_LS_BASE(ctx);
    memset(ls + vo + 32u * P, 0, 32);   /* 1a volta: os vetores do par -1 (zeros), depois do fim do fluxo */
    for (uint32_t p0 = 0; p0 < P; p0 += PL752_PARES) {
        uint32_t f[4 * PL752_PARES], ab[4 * PL752_PARES], c[4 * PL752_PARES], s[2 * PL752_PARES];
        for (uint32_t q = 0; q < PL752_PARES; q++)
            for (uint32_t k = 0; k < 2; k++) {
                const uint8_t* r = &ls[(inp + 30u * (p0 + q) + 15u * k) & SPU_LS_MASK];
                const uint32_t bits = ((uint32_t)r[12] << 16) | ((uint32_t)r[13] << 8) | r[14];
                f[4 * q + 2 * k] = (bits >> 14) & 0x3FFu; f[4 * q + 2 * k + 1] = (bits >> 4) & 0x3FFu;
                s[2 * q + k] = bits & 0xFu;
            }
        pl752_contas(f, ab, c);
        for (uint32_t q = 0; q < PL752_PARES && p0 + q < P; q++) {
            const uint32_t p = p0 + q;
            pl752_pos(ls, po + 32u * p, &ls[inp + 30u * p]);
            pl752_pos(ls, po + 32u * p + 16u, &ls[inp + 30u * p + 15u]);
            const u128 A = pl752_u128(&ab[4 * q]), C = pl752_u128(&c[4 * q]);
            spu_ls_write128(ctx, vo + 32u * p, pl752_sel(C, A, spu_ls_read128(ctx, tab + 16u * s[2 * q]), 0));
            spu_ls_write128(ctx, vo + 32u * p + 16u, pl752_sel(C, A, spu_ls_read128(ctx, tab + 16u * s[2 * q + 1]), 1));
        }
    }
}
#endif

static int pl_00757930_corpo(spu_context* ctx)
{
#if PL752_VETOR
    if (!gowa_edge_nativo_on()) return 0;
    u128* g = ctx->gpr;
    static const uint32_t m32[4] = { 0x80000000u, 0x80000000u, 0x80000000u, 0xFFFFFFFFu },
                          m30[4] = { 0x7FFFFFFFu, 0x7FFFFFFFu, 0x7FFFFFFFu, 0u },
                          k31[4] = { 0u, 0u, 0u, 0x0C0D0E0Fu }, k52[4] = { 0x08080808u, 0x08080808u, 0x08080808u, 0u };
    if (memcmp(g[29]._u32, m32, 16) || memcmp(g[30]._u32, m30, 16) || memcmp(g[31]._u32, k31, 16) || memcmp(g[40]._u32, k52, 16)) return 0;
    for (int i = 0; i < 4; i++) if (g[8]._u32[i] | g[25]._u32[i] | g[26]._u32[i] | g[27]._u32[i] | g[10]._u32[i]) return 0;
    const int32_t r19 = (int32_t)g[19]._u32[0];
    if (r19 >= 0 || (r19 & 31) || r19 < -0x10000) return 0;
    const uint32_t P = (uint32_t)(-r19) / 32u;
    const uint32_t inp = (g[22]._u32[0] - 60u) & SPU_LS_MASK, tab = g[35]._u32[0] & SPU_LS_MASK;
    const uint32_t po = (g[37]._u32[0] + (uint32_t)r19) & SPU_LS_MASK, vo = (g[32]._u32[0] + (uint32_t)r19) & SPU_LS_MASK;
    if (g[34]._u32[0] != g[37]._u32[0] + 16u || g[36]._u32[0] != g[32]._u32[0] + 16u || g[35]._u32[0] != g[1]._u32[0] + 32u) return 0;
    if (((po | vo | tab) & 15u) || inp + 30u * P + 64u > SPU_LS_SIZE || po + 32u * P > SPU_LS_SIZE || vo + 32u * P + 32u > SPU_LS_SIZE ||
        tab + 256u > SPU_LS_SIZE)
        return 0;
    /* as faixas que o laço grava não encostam no que ele lê (entrada e tabela) nem uma na outra */
#define PL7_ENC(a0, a1, b0, b1) ((a0) < (b1) && (b0) < (a1))
    if (PL7_ENC(po, po + 32u * P, inp, inp + 30u * P + 64u) || PL7_ENC(vo, vo + 32u * P + 32u, inp, inp + 30u * P + 64u) ||
        PL7_ENC(po, po + 32u * P, tab, tab + 256u) || PL7_ENC(vo, vo + 32u * P + 32u, tab, tab + 256u) ||
        PL7_ENC(po, po + 32u * P, vo, vo + 32u * P + 32u))
        return 0;
#undef PL7_ENC
    pl757_escreve(ctx, inp, po, vo, P, tab);
    g[1] = spu_ai(g[1], 224);
    g[3] = spu_il(0);
    return 1;
#else
    (void)ctx;
    return 0;
#endif
}

static __attribute__((noinline)) int pl_00757930_nativo(spu_context* ctx)
{
    static volatile long n[2];
    const int r = pl_00757930_corpo(ctx);
    gowa_edge_conta("pl757930", n, r);
    return r;
}
/* 02/11: o plugin inteiro, da 0x30000. Vértice de 15 bytes em r4, n = r5 (aos pares). Refaz a preparação: o r1 em
 * r1 - 0xE0, a tabela de constantes do plugin (0x30340..0x303F0, a da seleção) no quadro r1 - 224 + 0x20, os
 * atributos 1 (posição) e 2 (vetor) nos dois primeiros zeros da lista com os ponteiros das vagas em
 * r3 + 0x58 + 4 * id. Sai com r3 = 0 e o r1 de volta. */
static int pl_00757930_inteiro_corpo(spu_context* ctx)
{
#if PL752_VETOR
    if (!gowa_edge_nativo_on()) return 0;
    u128* g = ctx->gpr;
    uint8_t* ls = SPU_LS_BASE(ctx);
    const uint32_t r1 = g[1]._u32[0] & SPU_LS_MASK, r3 = g[3]._u32[0] & SPU_LS_MASK, r4 = g[4]._u32[0] & SPU_LS_MASK, n = g[5]._u32[0];
    if ((r3 & 15u) || r3 + 0x170u > SPU_LS_SIZE || r1 < 0x200u || !n || n > 4096u) return 0;
    static const uint8_t ids[2] = { 1, 2 };
    int k[2], j = 0;
    for (int i = 0; i < 2; i++) {
        while (j < 16 && ls[r3 + 0x160u + (uint32_t)j]) j++;
        if (j >= 16) return 0;
        k[i] = j++;
    }
    uint32_t pt[2];
    for (int i = 0; i < 2; i++) {
        const uint32_t a = r3 + 16u + 4u * (uint32_t)k[i];
        pt[i] = ((uint32_t)ls[a] << 24) | ((uint32_t)ls[a + 1] << 16) | ((uint32_t)ls[a + 2] << 8) | ls[a + 3];
    }
    const uint32_t po = pt[0] & SPU_LS_MASK, vo = pt[1] & SPU_LS_MASK, P = (n + 1u) / 2u, fq = (r1 - 224u) & ~15u, tab = fq + 32u;
    const uint32_t ni = 30u * (P + PL752_PARES) + 64u, no = 32u * P, nv = 32u * P + 32u;
#define PL7I_ENC(a0, n0, c0, n1) ((a0) < (c0) + (n1) && (c0) < (a0) + (n0))
    if (((po | vo) & 15u) || r4 + ni > SPU_LS_SIZE || po + no > SPU_LS_SIZE || vo + nv > SPU_LS_SIZE || PL7I_ENC(po, no, vo, no) ||
        PL7I_ENC(po, no, r4, ni) || PL7I_ENC(vo, nv, r4, ni) || PL7I_ENC(po, no, fq, 240u) || PL7I_ENC(vo, nv, fq, 240u) ||
        PL7I_ENC(po, no, r3, 0x170u) || PL7I_ENC(vo, nv, r3, 0x170u) || PL7I_ENC(r4, ni, fq, 256u))
        return 0;
#undef PL7I_ENC
    spu_ls_write128(ctx, fq, g[1]);
    for (uint32_t i = 0; i < 12; i++)
        spu_ls_write128(ctx, fq + 0x20u + 16u * i, spu_ls_read128(ctx, (uint32_t)(0x30340 + 16 * (int)i + g_spu_hot_reloc)));
    for (int i = 0; i < 2; i++) {
        ls[r3 + 0x160u + (uint32_t)k[i]] = ids[i];
        const uint32_t a = r3 + 0x58u + 4u * ids[i];
        ls[a] = (uint8_t)(pt[i] >> 24); ls[a + 1] = (uint8_t)(pt[i] >> 16); ls[a + 2] = (uint8_t)(pt[i] >> 8); ls[a + 3] = (uint8_t)pt[i];
    }
    pl757_escreve(ctx, r4, po, vo, P, tab);
    g[3] = spu_il(0);
    return 1;
#else
    (void)ctx;
    return 0;
#endif
}

static __attribute__((noinline)) int pl_00757930_inteiro(spu_context* ctx)
{
    static volatile long n[2];
    const int r = pl_00757930_inteiro_corpo(ctx);
    gowa_edge_conta("pl757930i", n, r);
    return r;
}
#endif
