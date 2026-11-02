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
    if (((po | vo | co | tab) & 15u) || inp + 38u * P + 64u > SPU_LS_SIZE || po + 32u * P > SPU_LS_SIZE || vo + 32u * P > SPU_LS_SIZE ||
        co + 32u * P > SPU_LS_SIZE || tab + 256u > SPU_LS_SIZE)
        return 0;
#define PLA_ENC(a0, n0, b0, n1) ((a0) < (b0) + (n1) && (b0) < (a0) + (n0))
    const uint32_t ni = 38u * P + 64u, no = 32u * P;
    if (PLA_ENC(po, no, inp, ni) || PLA_ENC(vo, no, inp, ni) || PLA_ENC(co, no, inp, ni) || PLA_ENC(po, no, tab, 256u) ||
        PLA_ENC(vo, no, tab, 256u) || PLA_ENC(co, no, tab, 256u) || PLA_ENC(po, no, vo, no) || PLA_ENC(po, no, co, no) || PLA_ENC(vo, no, co, no))
        return 0;
#undef PLA_ENC
    uint8_t* ls = SPU_LS_BASE(ctx);
    const u128 esc = g[40], vies = g[39];
    const uint32_t wbe = __builtin_bswap32(g[46]._u32[3]);
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
#endif
