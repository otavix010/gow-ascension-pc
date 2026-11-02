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
    memset(ls + vo + 32u * P, 0, 32);   /* 1a volta: os vetores do par -1 (zeros), depois do fim do fluxo */
    const uint32_t wbe = __builtin_bswap32(g[54]._u32[3]);
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
#endif
