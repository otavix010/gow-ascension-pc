/* GOWA (02/11, EDGE nativo, marco E4): o laço 0x303D8 do plugin 0x751450 do EDGE, da entrada do laço ao fim (a
 * preparação 0x30000/0x30014 continua a traduzida).
 *
 * Vértice de 9 bytes (72 bits, do bit mais alto): x, y e z quantizados em campos de w_c bits seguidos (w_c = bits
 * inteiros + bits fracionários do descritor em r6 + 16; na preparação viram a máscara r49, os deslocamentos r30 e
 * s_c = 32 - frac em r50), depois os 24 bits de um vetor unitário "smallest three" (os do 0x757930). Posição:
 *     v = (campo & máscara) + viés (r43, inteiro com sinal);  sai v * 2^(s - 32)   (o fa(csflt, cuflt) do original é
 *     exato para 8 <= s <= 31 e |v| < 2^24), w = a palavra 3 de r54 (1,0)
 * Vetor: as contas e a seleção do 0x752710 (pl_00752710.h; máscaras do plugin conferidas iguais), tabela em r53.
 * 2 vértices por volta: posições em r57 / r52 + r27, vetores com uma volta de atraso em r48 / r51 + o r27 anterior; a
 * 1a volta grava os vetores do par -1 (zeros: a preparação deixa r34, r35, r38 e r39 em 0) depois do fim do fluxo.
 * No fim: r1 += 224, r3 = 0. Temporários ficam diferentes (GOWA_CONF_REGS_ABI=1). GOWA_EDGE_NATIVO=0 desliga.
 * Gancho 'entrada' na função do laço. */
#ifndef GOWA_PL_00751450_H
#define GOWA_PL_00751450_H
#include <math.h>
#include "pl_00752710.h"

/* os P pares: posições (campos de bits + viés, vezes 2^(s - 32), w = wbits) e os dois vetores de cada par (contas e
 * seleção do 0x752710, tabela em tab); antes, os vetores "do par -1" (zeros) depois do fim do fluxo de vetores */
#if PL752_VETOR   /* 11/11: sem AVX2 (build compatível) não há o caminho vetorial nem as contas dele */
static void pl751_escreve(spu_context* ctx, uint32_t inp, uint32_t po, uint32_t vo, uint32_t P, uint32_t tab,
                          const uint32_t* w, const uint32_t* s, const uint32_t* off, const int32_t* vies, uint32_t wbits)
{
    uint8_t* ls = SPU_LS_BASE(ctx);
    memset(ls + vo + 32u * P, 0, 32);   /* 1a volta: os vetores do par -1 (zeros), depois do fim do fluxo */
    const uint32_t wbe = __builtin_bswap32(wbits);
    for (uint32_t p0 = 0; p0 < P; p0 += PL752_PARES) {
        uint32_t f[4 * PL752_PARES], ab[4 * PL752_PARES], cc[4 * PL752_PARES], sel[2 * PL752_PARES];
        for (uint32_t q = 0; q < PL752_PARES; q++)
            for (uint32_t k = 0; k < 2; k++) {
                const uint8_t* r = &ls[(inp + 18u * (p0 + q) + 9u * k) & SPU_LS_MASK];
                const uint32_t bits = ((uint32_t)r[6] << 16) | ((uint32_t)r[7] << 8) | r[8];
                f[4 * q + 2 * k] = (bits >> 14) & 0x3FFu; f[4 * q + 2 * k + 1] = (bits >> 4) & 0x3FFu;
                sel[2 * q + k] = bits & 0xFu;
            }
        pl752_contas(f, ab, cc);
        for (uint32_t q = 0; q < PL752_PARES && p0 + q < P; q++) {
            const uint32_t p = p0 + q;
            for (uint32_t k = 0; k < 2; k++) {
                const uint8_t* r = &ls[(inp + 18u * p + 9u * k) & SPU_LS_MASK];
                uint64_t x = 0;
                for (int i = 0; i < 6; i++) x = (x << 8) | r[i];
                uint32_t o[4];
                for (int c = 0; c < 3; c++) {
                    const uint32_t fld = (uint32_t)(x >> (48u - off[c] - w[c])) & ((1u << w[c]) - 1u);
                    const float val = (float)((double)((int32_t)fld + vies[c]) * ldexp(1.0, (int)s[c] - 32));
                    uint32_t bv; memcpy(&bv, &val, 4);
                    if (!(bv & 0x7F800000u)) bv = 0;   /* zero (ou abaixo do mínimo) sai +0, como a SPU */
                    o[c] = __builtin_bswap32(bv);
                }
                o[3] = wbe;
                memcpy(ls + po + 32u * p + 16u * k, o, 16);
            }
            const u128 A = pl752_u128(&ab[4 * q]), C = pl752_u128(&cc[4 * q]);
            spu_ls_write128(ctx, vo + 32u * p, pl752_sel(C, A, spu_ls_read128(ctx, tab + 16u * sel[2 * q]), 0));
            spu_ls_write128(ctx, vo + 32u * p + 16u, pl752_sel(C, A, spu_ls_read128(ctx, tab + 16u * sel[2 * q + 1]), 1));
        }
    }
}
#endif

static int pl_00751450_corpo(spu_context* ctx)
{
#if PL752_VETOR
    if (!gowa_edge_nativo_on()) return 0;
    u128* g = ctx->gpr;
    static const uint32_t m32[4] = { 0x80000000u, 0x80000000u, 0x80000000u, 0xFFFFFFFFu },
                          m30[4] = { 0x7FFFFFFFu, 0x7FFFFFFFu, 0x7FFFFFFFu, 0u },
                          k31[4] = { 0u, 0u, 0u, 0x0C0D0E0Fu }, k52[4] = { 0x08080808u, 0x08080808u, 0x08080808u, 0u };
    if (memcmp(g[42]._u32, m32, 16) || memcmp(g[44]._u32, m30, 16) || memcmp(g[45]._u32, k31, 16) || memcmp(g[58]._u32, k52, 16)) return 0;
    for (int i = 0; i < 4; i++) if (g[34]._u32[i] | g[35]._u32[i] | g[38]._u32[i] | g[39]._u32[i]) return 0;
    if (g[54]._u32[3] != 0x3F800000u) return 0;
    /* os campos da posição: larguras pelo descritor, conferidas com a máscara, os deslocamentos e o s da preparação */
    uint8_t* ls = SPU_LS_BASE(ctx);
    uint32_t w[3], s[3], off[3];
    int32_t vies[3];
    uint32_t acc = 0;
    for (int c = 0; c < 3; c++) {
        s[c] = g[50]._u32[c];
        const uint32_t m = g[49]._u32[c];
        if (s[c] < 8u || s[c] > 31u || (m & (m + 1u)) || !m) return 0;   /* máscara 2^w - 1 */
        w[c] = (uint32_t)__builtin_popcount(m);
        off[c] = acc; acc += w[c];
        if (g[30]._u32[c] != (uint32_t)-(int32_t)acc) return 0;
        vies[c] = (int32_t)g[43]._u32[c];
        if (vies[c] > (1 << 22) || vies[c] < -(1 << 22) || w[c] > 22u) return 0;
    }
    if (acc != 48u) return 0;   /* + 24 bits do vetor = os 9 bytes do vértice (o laço anda 18 por par) */
    const int32_t r27 = (int32_t)g[27]._u32[0];
    if (r27 >= 0 || (r27 & 31) || r27 < -0x10000) return 0;
    const uint32_t P = (uint32_t)(-r27) / 32u;
    const uint32_t inp = (g[33]._u32[0] - 36u) & SPU_LS_MASK, tab = g[53]._u32[0] & SPU_LS_MASK;
    const uint32_t po = (g[57]._u32[0] + (uint32_t)r27) & SPU_LS_MASK, vo = (g[48]._u32[0] + (uint32_t)r27) & SPU_LS_MASK;
    if (g[52]._u32[0] != g[57]._u32[0] + 16u || g[51]._u32[0] != g[48]._u32[0] + 16u || g[53]._u32[0] != g[1]._u32[0] + 32u) return 0;
    if (((po | vo | tab) & 15u) || inp + 18u * P + 64u > SPU_LS_SIZE || po + 32u * P > SPU_LS_SIZE || vo + 32u * P + 32u > SPU_LS_SIZE ||
        tab + 256u > SPU_LS_SIZE)
        return 0;
#define PL1_ENC(a0, a1, b0, b1) ((a0) < (b1) && (b0) < (a1))
    if (PL1_ENC(po, po + 32u * P, inp, inp + 18u * P + 64u) || PL1_ENC(vo, vo + 32u * P + 32u, inp, inp + 18u * P + 64u) ||
        PL1_ENC(po, po + 32u * P, tab, tab + 256u) || PL1_ENC(vo, vo + 32u * P + 32u, tab, tab + 256u) ||
        PL1_ENC(po, po + 32u * P, vo, vo + 32u * P + 32u))
        return 0;
#undef PL1_ENC
    pl751_escreve(ctx, inp, po, vo, P, tab, w, s, off, vies, g[54]._u32[3]);
    g[1] = spu_ai(g[1], 224);
    g[3] = spu_il(0);
    return 1;
#else
    (void)ctx;
    return 0;
#endif
}

static __attribute__((noinline)) int pl_00751450_nativo(spu_context* ctx)
{
    static volatile long n[2];
    const int r = pl_00751450_corpo(ctx);
    gowa_edge_conta("pl751450", n, r);
    return r;
}
/* 02/11: o plugin inteiro, da entrada 0x30014 (o job chama direto nela; quem chama já deixou r28 = 17, r74 = 19,
 * r29 = 18, r52 = a lista de atributos e o r1 em r1 - 0xE0). A preparação: o descritor (r6, sem alinhamento) dá por
 * componente a largura (bytes 16 + 2c e 17 + 2c, inteiros + fracionários) e s = 32 - fracionários, e o viés nas
 * palavras 0, 4 e 8; o vértice de 9 bytes vem de r4, n = r5. Acrescenta os atributos 1 (posição) e 2 (vetor) nos dois
 * primeiros zeros da lista (r3 + 0x160), guarda os ponteiros daquelas vagas em r3 + 0x58 + 4 * id e copia a tabela
 * de constantes do plugin (0x30570..0x30620, a da seleção dos vetores) para o quadro (r1 - 224 + 0x20). Sai com r3 = 0. */
static inline uint32_t pl751_palavra(const uint8_t* ls, uint32_t a)   /* lqd + rotqby: a palavra dá a volta no quadword */
{
    const uint32_t q = a & SPU_LS_MASK & ~15u;
    uint32_t v = 0;
    for (uint32_t i = 0; i < 4; i++) v = (v << 8) | ls[q + ((a + i) & 15u)];
    return v;
}
static int pl_00751450_inteiro_corpo(spu_context* ctx)
{
#if PL752_VETOR
    if (!gowa_edge_nativo_on()) return 0;
    u128* g = ctx->gpr;
    uint8_t* ls = SPU_LS_BASE(ctx);
    const uint32_t r1 = g[1]._u32[0] & SPU_LS_MASK, r3 = g[3]._u32[0] & SPU_LS_MASK, r4 = g[4]._u32[0] & SPU_LS_MASK,
                   n = g[5]._u32[0], r6 = g[6]._u32[0];
    if (g[28]._u32[0] != 17u || g[74]._u32[0] != 19u || g[29]._u32[0] != 18u || (r3 & 15u) || r3 + 0x170u > SPU_LS_SIZE ||
        r1 < 0x200u || !n || n > 4096u)
        return 0;
    { const u128 q = spu_ls_read128(ctx, r3 + 0x160u); if (memcmp(&q, &g[52], 16)) return 0; }
    uint32_t w[3], s[3], off[3], acc = 0;
    int32_t vies[3];
    for (uint32_t c = 0; c < 3; c++) {
        const uint32_t fr = ls[(r6 + 17u + 2u * c) & SPU_LS_MASK];
        w[c] = ls[(r6 + 16u + 2u * c) & SPU_LS_MASK] + fr;
        s[c] = 32u - fr;
        if (fr > 24u || s[c] < 8u || s[c] > 31u || !w[c] || w[c] > 22u) return 0;
        off[c] = acc; acc += w[c];
        vies[c] = (int32_t)pl751_palavra(ls, r6 + 4u * c);
        if (vies[c] > (1 << 22) || vies[c] < -(1 << 22)) return 0;
    }
    if (acc != 48u) return 0;
    int k1 = 0;
    while (k1 < 16 && ls[r3 + 0x160u + (uint32_t)k1]) k1++;
    int k2 = k1 + 1;
    while (k2 < 16 && ls[r3 + 0x160u + (uint32_t)k2]) k2++;
    if (k2 >= 16) return 0;
    const uint32_t p1 = pl751_palavra(ls, r3 + 16u + 4u * (uint32_t)k1), p2 = pl751_palavra(ls, r3 + 16u + 4u * (uint32_t)k2);
    const uint32_t po = p1 & SPU_LS_MASK, vo = p2 & SPU_LS_MASK, P = (n + 1u) / 2u, fq = (r1 - 224u) & ~15u, tab = fq + 32u;
    const uint32_t ni = 18u * P + 64u, no = 32u * P, nv = 32u * P + 32u;
#define PL1I_ENC(a0, n0, b0, n1) ((a0) < (b0) + (n1) && (b0) < (a0) + (n0))
    if (((po | vo) & 15u) || r4 + ni > SPU_LS_SIZE || po + no > SPU_LS_SIZE || vo + nv > SPU_LS_SIZE ||
        PL1I_ENC(po, no, r4, ni) || PL1I_ENC(vo, nv, r4, ni) || PL1I_ENC(po, no, vo, nv) || PL1I_ENC(po, no, fq, 224u) ||
        PL1I_ENC(vo, nv, fq, 224u) || PL1I_ENC(po, no, r3, 0x170u) || PL1I_ENC(vo, nv, r3, 0x170u) || PL1I_ENC(r4, ni, fq, 256u))
        return 0;
#undef PL1I_ENC
    for (uint32_t i = 0; i < 12; i++)
        spu_ls_write128(ctx, fq + 0x20u + 16u * i, spu_ls_read128(ctx, (uint32_t)(0x30570 + 16 * (int)i + g_spu_hot_reloc)));
    ls[r3 + 0x160u + (uint32_t)k1] = 1;
    ls[r3 + 0x160u + (uint32_t)k2] = 2;
    for (int b = 0; b < 4; b++) { ls[r3 + 0x5Cu + (uint32_t)b] = (uint8_t)(p1 >> (24 - 8 * b)); ls[r3 + 0x60u + (uint32_t)b] = (uint8_t)(p2 >> (24 - 8 * b)); }
    pl751_escreve(ctx, r4, po, vo, P, tab, w, s, off, vies, 0x3F800000u);
    g[3] = spu_il(0);
    return 1;
#else
    (void)ctx;
    return 0;
#endif
}

static __attribute__((noinline)) int pl_00751450_inteiro(spu_context* ctx)
{
    static volatile long n[2];
    const int r = pl_00751450_inteiro_corpo(ctx);
    gowa_edge_conta("pl751450i", n, r);
    return r;
}
#endif
