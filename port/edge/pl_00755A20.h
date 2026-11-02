/* GOWA (02/11, EDGE nativo, marco E4): o laço 0x302B8 do plugin 0x755A20 do EDGE, da entrada do laço ao fim.
 *
 * Vértice de 19 bytes: posição float3 (sai com w = a palavra 3 de r46, 1,0) + 24 bits de um vetor unitário "smallest
 * three" (contas e seleção do 0x752710: pl_00752710.h, máscaras do plugin conferidas) + cor RGBA em 4 bytes, cada um
 * -> fma(cuflt(byte << 16, 139), r40, r39) (r40 = 1/255, r39 = 0: as próprias primitivas do runtime). Três fluxos de
 * 16 bytes por vértice: posição (r47), vetor (r38), cor (r48). Na entrada do laço a preparação já gravou a posição e a
 * cor do par 0; o laço grava posição e cor dos pares 1..n-1 e os vetores dos pares 0..n-2, e o último vetor depois do
 * laço. Aqui todos os pares nos três fluxos (o par 0 sai igual ao que a preparação gravou: as mesmas contas sobre os
 * mesmos bytes). No fim: r1 += 224, r3 = 0. Temporários ficam diferentes (GOWA_CONF_REGS_ABI=1). Conferido pelas
 * gravações. GOWA_EDGE_NATIVO=0 desliga. Gancho 'entrada' na função do laço. */
#ifndef GOWA_PL_00755A20_H
#define GOWA_PL_00755A20_H
#include "pl_00752710.h"

#if PL752_VETOR
/* os P pares: posição (w = wbits), cor (fma(cuflt(byte << 16, 139), esc, vies)) e o vetor de cada vértice (contas e
 * seleção do 0x752710, tabela em tab) */
static void pl75a_escreve(spu_context* ctx, uint32_t inp, uint32_t po, uint32_t vo, uint32_t co, uint32_t P, uint32_t tab,
                          u128 esc, u128 vies, uint32_t wbits)
{
    /* a 1a volta do original grava os vetores "do par -1" (zeros) nos 32 bytes depois do fim do fluxo de vetores; com
     * os fluxos colados isso cai no começo do seguinte e é coberto depois (por isso antes de tudo) */
    memset(SPU_LS_BASE(ctx) + vo + 32u * P, 0, 32);
    uint8_t* ls = SPU_LS_BASE(ctx);
    const uint32_t wbe = __builtin_bswap32(wbits);
    for (uint32_t p0 = 0; p0 < P; p0 += PL752_PARES) {
        uint32_t f[4 * PL752_PARES], ab[4 * PL752_PARES], cc[4 * PL752_PARES], sel[2 * PL752_PARES];
        for (uint32_t q = 0; q < PL752_PARES; q++)
            for (uint32_t k = 0; k < 2; k++) {
                const uint8_t* r = &ls[(inp + 38u * (p0 + q) + 19u * k) & SPU_LS_MASK];
                const uint32_t bits = ((uint32_t)r[12] << 16) | ((uint32_t)r[13] << 8) | r[14];
                f[4 * q + 2 * k] = (bits >> 14) & 0x3FFu; f[4 * q + 2 * k + 1] = (bits >> 4) & 0x3FFu;
                sel[2 * q + k] = bits & 0xFu;
            }
        pl752_contas(f, ab, cc);
        for (uint32_t q = 0; q < PL752_PARES && p0 + q < P; q++) {
            const uint32_t p = p0 + q;
            for (uint32_t k = 0; k < 2; k++) {
                const uint8_t* r = &ls[(inp + 38u * p + 19u * k) & SPU_LS_MASK];
                uint8_t* d = ls + po + 32u * p + 16u * k;
                memcpy(d, r, 12);
                memcpy(d + 12, &wbe, 4);
                u128 x;
                for (int c = 0; c < 4; c++) x._u32[c] = (uint32_t)r[15 + c] << 16;
                spu_ls_write128(ctx, co + 32u * p + 16u * k, spu_fma(spu_cuflt(x, 139), esc, vies));
            }
            const u128 A = pl752_u128(&ab[4 * q]), C = pl752_u128(&cc[4 * q]);
            spu_ls_write128(ctx, vo + 32u * p, pl752_sel(C, A, spu_ls_read128(ctx, tab + 16u * sel[2 * q]), 0));
            spu_ls_write128(ctx, vo + 32u * p + 16u, pl752_sel(C, A, spu_ls_read128(ctx, tab + 16u * sel[2 * q + 1]), 1));
        }
    }
}
#endif

static int pl_00755A20_corpo(spu_context* ctx)
{
#if PL752_VETOR
    if (!gowa_edge_nativo_on()) return 0;
    u128* g = ctx->gpr;
    static const uint32_t m32[4] = { 0x80000000u, 0x80000000u, 0x80000000u, 0xFFFFFFFFu },
                          m30[4] = { 0x7FFFFFFFu, 0x7FFFFFFFu, 0x7FFFFFFFu, 0u },
                          k31[4] = { 0u, 0u, 0u, 0x0C0D0E0Fu }, k52[4] = { 0x08080808u, 0x08080808u, 0x08080808u, 0u };
    if (memcmp(g[33]._u32, m32, 16) || memcmp(g[34]._u32, m30, 16) || memcmp(g[35]._u32, k31, 16) || memcmp(g[49]._u32, k52, 16)) return 0;
    if (g[52]._u32[0] != 0xC6353150u || g[53]._u32[0] != 0x46352E7Cu || g[55]._u32[0] != 0x3F800000u || g[46]._u32[3] != 0x3F800000u)
        return 0;
    static const uint32_t ca[4] = { 0x800F8080u, 0x80108080u, 0x80118080u, 0x80128080u }, cb[4] = { 0x80028080u, 0x80038080u, 0x80048080u, 0x80058080u };
    if (memcmp(g[56]._u32, ca, 16) || memcmp(g[54]._u32, cb, 16)) return 0;   /* a cor: os 4 bytes na ordem, << 16 */
    const int32_t r23 = (int32_t)g[23]._u32[0];
    if (r23 >= 0 || (r23 & 31) || r23 < -0x10000) return 0;
    const uint32_t P = (uint32_t)(-r23) / 32u + 1u;   /* + o par 0, que a preparação já fez */
    const uint32_t inp = (g[27]._u32[0] - 114u) & SPU_LS_MASK, tab = g[45]._u32[0] & SPU_LS_MASK;
    const uint32_t po = (g[47]._u32[0] + (uint32_t)r23 - 32u) & SPU_LS_MASK, vo = (g[38]._u32[0] + (uint32_t)r23 - 32u) & SPU_LS_MASK,
                   co = (g[48]._u32[0] + (uint32_t)r23 - 32u) & SPU_LS_MASK;
    if (g[43]._u32[0] != g[47]._u32[0] + 16u || g[44]._u32[0] != g[38]._u32[0] + 16u || g[42]._u32[0] != g[48]._u32[0] + 16u ||
        g[3]._u32[0] != g[23]._u32[0] - 32u || g[45]._u32[0] != g[1]._u32[0] + 32u)
        return 0;
    if (((po | vo | co | tab) & 15u) || inp + 38u * P + 64u > SPU_LS_SIZE || po + 32u * P > SPU_LS_SIZE || vo + 32u * P + 32u > SPU_LS_SIZE ||
        co + 32u * P > SPU_LS_SIZE || tab + 256u > SPU_LS_SIZE)
        return 0;
#define PLA_ENC(a0, n0, b0, n1) ((a0) < (b0) + (n1) && (b0) < (a0) + (n0))
    const uint32_t ni = 38u * P + 64u, no = 32u * P, nv = 32u * P + 32u;
    if (PLA_ENC(po, no, inp, ni) || PLA_ENC(vo, nv, inp, ni) || PLA_ENC(co, no, inp, ni) || PLA_ENC(po, no, tab, 256u) ||
        PLA_ENC(vo, nv, tab, 256u) || PLA_ENC(co, no, tab, 256u) || PLA_ENC(po, no, vo, no) || PLA_ENC(po, no, co, no) || PLA_ENC(vo, no, co, no))
        return 0;
#undef PLA_ENC
    pl75a_escreve(ctx, inp, po, vo, co, P, tab, g[40], g[39], g[46]._u32[3]);
    g[1] = spu_ai(g[1], 224);
    g[3] = spu_il(0);
    return 1;
#else
    (void)ctx;
    return 0;
#endif
}

static __attribute__((noinline)) int pl_00755A20_nativo(spu_context* ctx)
{
    static volatile long n[2];
    const int r = pl_00755A20_corpo(ctx);
    gowa_edge_conta("pl755A20", n, r);
    return r;
}
/* 02/11: o plugin inteiro, da 0x30000. Vértice de 19 bytes em r4, n = r5 (aos pares). Refaz a preparação: o r1 em
 * r1 - 0xE0, a tabela de constantes do plugin (a da seleção, 0x303F0..0x304A0 na ordem do original) no quadro
 * r1 - 224 + 0x20, os atributos 1 (posição), 2 (vetor) e 20 (cor) nos três primeiros zeros da lista, com os ponteiros
 * das vagas do 1 e do 2 em r3 + 0x58 + 4 * id; a cor com 1/255 e 0 (imediatos do original), o w da constante
 * 0x30520. Sai com r3 = 0 e o r1 de volta. */
static int pl_00755A20_inteiro_corpo(spu_context* ctx)
{
#if PL752_VETOR
    if (!gowa_edge_nativo_on()) return 0;
    u128* g = ctx->gpr;
    uint8_t* ls = SPU_LS_BASE(ctx);
    const uint32_t r1 = g[1]._u32[0] & SPU_LS_MASK, r3 = g[3]._u32[0] & SPU_LS_MASK, r4 = g[4]._u32[0] & SPU_LS_MASK, n = g[5]._u32[0];
    if ((r3 & 15u) || r3 + 0x170u > SPU_LS_SIZE || r1 < 0x200u || !n || n > 4096u) return 0;
    static const uint8_t ids[3] = { 1, 2, 20 };
    int k[3], j = 0;
    for (int i = 0; i < 3; i++) {
        while (j < 16 && ls[r3 + 0x160u + (uint32_t)j]) j++;
        if (j >= 16) return 0;
        k[i] = j++;
    }
    uint32_t pt[3];
    for (int i = 0; i < 3; i++) {
        const uint32_t a = r3 + 16u + 4u * (uint32_t)k[i];
        pt[i] = ((uint32_t)ls[a] << 24) | ((uint32_t)ls[a + 1] << 16) | ((uint32_t)ls[a + 2] << 8) | ls[a + 3];
    }
    const uint32_t po = pt[0] & SPU_LS_MASK, vo = pt[1] & SPU_LS_MASK, co = pt[2] & SPU_LS_MASK, P = (n + 1u) / 2u;
    const uint32_t fq = (r1 - 224u) & ~15u, tab = fq + 32u, ni = 38u * (P + PL752_PARES) + 64u, no = 32u * P, nv = no + 32u;
#define PL7I_ENC(a0, n0, c0, n1) ((a0) < (c0) + (n1) && (c0) < (a0) + (n0))
    if (((po | vo | co) & 15u) || r4 + ni > SPU_LS_SIZE || po + no > SPU_LS_SIZE || vo + nv > SPU_LS_SIZE || co + no > SPU_LS_SIZE ||
        PL7I_ENC(po, no, vo, no) || PL7I_ENC(po, no, co, no) || PL7I_ENC(vo, no, co, no) || PL7I_ENC(po, no, r4, ni) ||
        PL7I_ENC(vo, nv, r4, ni) || PL7I_ENC(co, no, r4, ni) || PL7I_ENC(po, no, fq, 240u) || PL7I_ENC(vo, nv, fq, 240u) ||
        PL7I_ENC(co, no, fq, 240u) || PL7I_ENC(po, no, r3, 0x170u) || PL7I_ENC(vo, nv, r3, 0x170u) || PL7I_ENC(co, no, r3, 0x170u) ||
        PL7I_ENC(r4, ni, fq, 256u))
        return 0;
#undef PL7I_ENC
    spu_ls_write128(ctx, fq, g[1]);
    static const uint16_t de[12] = { 0x3F0, 0x400, 0x430, 0x440, 0x410, 0x450, 0x460, 0x470, 0x480, 0x490, 0x4A0, 0x420 };
    for (uint32_t i = 0; i < 12; i++)
        spu_ls_write128(ctx, fq + 0x20u + 16u * i, spu_ls_read128(ctx, (uint32_t)(0x30000 + de[i] + g_spu_hot_reloc)));
    for (int i = 0; i < 3; i++) ls[r3 + 0x160u + (uint32_t)k[i]] = ids[i];
    for (int i = 0; i < 2; i++) {
        const uint32_t a = r3 + 0x58u + 4u * ids[i];
        ls[a] = (uint8_t)(pt[i] >> 24); ls[a + 1] = (uint8_t)(pt[i] >> 16); ls[a + 2] = (uint8_t)(pt[i] >> 8); ls[a + 3] = (uint8_t)pt[i];
    }
    const uint32_t wbits = spu_ls_read128(ctx, (uint32_t)(0x30520 + g_spu_hot_reloc))._u32[3];
    pl75a_escreve(ctx, r4, po, vo, co, P, tab, spu_iohl(spu_ilhu(0x3B80), 0x8080), spu_il(0), wbits);
    g[3] = spu_il(0);
    return 1;
#else
    (void)ctx;
    return 0;
#endif
}

static __attribute__((noinline)) int pl_00755A20_inteiro(spu_context* ctx)
{
    static volatile long n[2];
    const int r = pl_00755A20_inteiro_corpo(ctx);
    gowa_edge_conta("pl755A20i", n, r);
    return r;
}
#endif
