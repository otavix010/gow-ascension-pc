/* GOWA (02/11, EDGE nativo, marco E4): o laço 0x30160 do plugin 0x75AF20 do EDGE, da entrada do laço ao fim (a
 * preparação 0x30000 continua a traduzida).
 *
 * Vértice de 15 bytes no fluxo: os 12 primeiros (a posição float3) vão para um float4 com w = a palavra 3 de r13 (a
 * constante do plugin em 0x302A0; selb com r20 = fsmbi 0xF); o resto do vértice este plugin não usa. 2 vértices por
 * volta, 32 bytes, a partir de r8 + r7; N = -r9/32 voltas. Na entrada do laço a preparação já embaralhou o par 0
 * (r16, r12) e o par 1 (r10, r6); a entrada é r14 - 60. Aqui todos os pares direto da LS (os mesmos bytes). No fim,
 * como o laço: r3 = 0, r1 += 224. Temporários ficam diferentes (GOWA_CONF_REGS_ABI=1). GOWA_EDGE_NATIVO=0 desliga.
 * Gancho 'entrada' na função do laço. */
#ifndef GOWA_PL_0075AF20_H
#define GOWA_PL_0075AF20_H
#include <stdlib.h>
#include <string.h>
#include "edge_comum.h"

#ifndef GOWA_EDGE_NATIVO_ON_DEF
#define GOWA_EDGE_NATIVO_ON_DEF
static int gowa_edge_nativo_on(void)
{
    static int on = -1;
    if (on < 0) { const char* e = getenv("GOWA_EDGE_NATIVO"); on = !(e && *e == '0'); }
    return on;
}
#endif

static inline int plaf20_igual(u128 a, uint32_t w0, uint32_t w1, uint32_t w2, uint32_t w3)
{
    return a._u32[0] == w0 && a._u32[1] == w1 && a._u32[2] == w2 && a._u32[3] == w3;
}

/* o laço dos copiadores: os registradores mudam de plugin para plugin (a mesma conta). passo = bytes por vértice;
 * b = o controle do embaralhamento do vértice B: os bytes desde passo (passo < 16) ou passo - 16 da janela seguinte. */
typedef struct { uint8_t passo, off, off2, out, out16, in, w, id, b, sel, tres; } plcopia_regs;

static int plcopia_laco(spu_context* ctx, const plcopia_regs* q)
{
    if (!gowa_edge_nativo_on()) return 0;
    u128* g = ctx->gpr;
    const uint32_t passo = q->passo, off = g[q->off]._u32[0];
    if (!off || (off & 31u) || (0u - off) > 0x20000u || g[q->off2]._u32[0] != off) return 0;
    const uint32_t bi = (passo < 16u ? passo : passo - 16u) * 0x01010101u;
    if (!plaf20_igual(g[q->id], 0x00010203u, 0x04050607u, 0x08090A0Bu, 0x0C0D0E0Fu) ||
        !plaf20_igual(g[q->b], bi + 0x00010203u, bi + 0x04050607u, bi + 0x08090A0Bu, bi + 0x0C0D0E0Fu) ||
        !plaf20_igual(g[q->sel], 0, 0, 0, 0xFFFFFFFFu) || !plaf20_igual(g[q->tres], 0x03030303u, 0x03030303u, 0x03030303u, 0x03030303u))
        return 0;
    const uint32_t n = (0u - off) / 32u, ent = (g[q->in]._u32[0] - 4u * passo) & SPU_LS_MASK, out = (g[q->out]._u32[0] + off) & SPU_LS_MASK;
    const uint32_t tam = 2u * passo * n + 4u * passo + 64u;   /* o original lê duas voltas à frente */
    if (g[q->out16]._u32[0] != g[q->out]._u32[0] + 16u) return 0;
    if ((out & 15u) || ent + tam > SPU_LS_SIZE || out + 32u * n > SPU_LS_SIZE) return 0;
    if (out < ent + tam && ent < out + 32u * n) return 0;
    uint8_t* ls = SPU_LS_BASE(ctx);
    uint32_t w;
    memcpy(&w, &g[q->w]._u32[3], 4);
    w = __builtin_bswap32(w);
    for (uint32_t i = 0; i < 2u * n; i++) {
        uint8_t* d = ls + out + 16u * i;
        memcpy(d, ls + ent + passo * i, 12);
        memcpy(d + 12, &w, 4);
    }
    g[3] = spu_il(0);
    g[1] = spu_ai(g[1], 224);
    return 1;
}

static __attribute__((noinline)) int pl_0075AF20_nativo(spu_context* ctx)
{
    static const plcopia_regs q = { 15, 9, 7, 8, 11, 14, 13, 19, 17, 20, 18 };
    static volatile long c[2];
    const int r = plcopia_laco(ctx, &q);
    gowa_edge_conta("pl75AF20", c, r);
    return r;
}

/* 02/11: o 0x75B470 (vértice de 22 bytes; laço 0x30154). Não roda nas gravações: conferido na bancada sintética
 * (o laço traduzido e este sobre as mesmas LS e registradores, preparação traduzida). */
static __attribute__((noinline)) int pl_0075B470_nativo(spu_context* ctx)
{
    static const plcopia_regs q = { 22, 11, 9, 8, 10, 15, 17, 20, 18, 21, 19 };
    static volatile long c[2];
    const int r = plcopia_laco(ctx, &q);
    gowa_edge_conta("pl75B470", c, r);
    return r;
}

/* 02/11: os copiadores inteiros, da entrada. A preparação: o r1 antigo em r1 - 0xE0 e r1 -= 224, as 12 constantes do
 * plugin a partir de T no quadro (r1 + 0x20..0xD0), o atributo 1 no primeiro zero da lista (r3 + 0x160) com o ponteiro
 * daquela vaga em r3 + 0x5C; a saída é esse ponteiro: por vértice os 12 primeiros bytes da entrada (r4, passo) e
 * w = a palavra 3 da constante em T + 0xD0, aos pares (n = r5). Na 0x30018 (onde o job chama o 0x75B470 e o 0x75B710
 * direto) o r1 já desceu e o antigo já está gravado; conferido com o que o 0x30000 deixa nos registradores (r44 = 1,
 * r45 = a lista, r25 = r4 + 2 * passo, r57 = a constante em T); o 0x75AF20 é chamado em +0x10 (na carga). Sai com
 * r3 = 0 e o r1 de volta. */
static inline uint32_t plcopia_palavra(const uint8_t* ls, uint32_t a)
{
    const uint32_t q = a & SPU_LS_MASK & ~15u;
    uint32_t v = 0;
    for (uint32_t i = 0; i < 4; i++) v = (v << 8) | ls[q + ((a + i) & 15u)];
    return v;
}
static int plcopia_inteiro(spu_context* ctx, uint32_t passo, uint32_t T, int na18)   /* na18: 1 = 0x30018, 2 = 0x30010 do 0x75AF20 */
{
    if (!gowa_edge_nativo_on()) return 0;
    u128* g = ctx->gpr;
    uint8_t* ls = SPU_LS_BASE(ctx);
    const uint32_t r1 = (g[1]._u32[0] + (na18 ? 224u : 0u)) & SPU_LS_MASK, r3 = g[3]._u32[0] & SPU_LS_MASK,
                   r4 = g[4]._u32[0] & SPU_LS_MASK, n = g[5]._u32[0], fq = (r1 - 224u) & ~15u;
    if ((r3 & 15u) || r3 + 0x170u > SPU_LS_SIZE || r1 < 0x200u || r1 > SPU_LS_SIZE || !n || n > 4096u) return 0;
    if (na18 == 2) {   /* o 0x30000 do 0x75AF20 só faz r42 = 1, grava o r1, r1 -= 224 e r43 = a lista */
        const u128 l = spu_ls_read128(ctx, r3 + 0x160u);
        if (spu_ls_read32(ctx, fq) != r1 || memcmp(&l, &g[43], 16)) return 0;
        for (int i = 0; i < 4; i++) if (g[42]._u32[i] != 1u) return 0;
    } else if (na18) {
        const u128 l = spu_ls_read128(ctx, r3 + 0x160u), c = spu_ls_read128(ctx, (uint32_t)((int32_t)T + g_spu_hot_reloc));
        if (spu_ls_read32(ctx, fq) != r1 || memcmp(&l, &g[45], 16) || memcmp(&c, &g[57], 16)) return 0;
        for (int i = 0; i < 4; i++) if (g[44]._u32[i] != 1u || g[25]._u32[i] != g[4]._u32[i] + 2u * passo) return 0;
    }
    int k = 0;
    while (k < 16 && ls[r3 + 0x160u + (uint32_t)k]) k++;
    if (k == 16) return 0;
    const uint32_t ptr = plcopia_palavra(ls, r3 + 16u + 4u * (uint32_t)k), out = ptr & SPU_LS_MASK, nv = (n + 1u) & ~1u;
    const uint32_t tam = passo * nv + 4u * passo + 64u;
#define PLCI_ENC(a0, n0, c0, n1) ((a0) < (c0) + (n1) && (c0) < (a0) + (n0))
    if ((out & 15u) || out + 16u * nv > SPU_LS_SIZE || r4 + tam > SPU_LS_SIZE || PLCI_ENC(out, 16u * nv, r4, tam) ||
        PLCI_ENC(out, 16u * nv, fq, 224u) || PLCI_ENC(out, 16u * nv, r3, 0x170u) || PLCI_ENC(r4, tam, fq, 224u))
        return 0;
#undef PLCI_ENC
    if (!na18) spu_ls_write128(ctx, fq, g[1]);
    for (uint32_t i = 0; i < 12; i++)
        spu_ls_write128(ctx, fq + 0x20u + 16u * i, spu_ls_read128(ctx, (uint32_t)((int32_t)(T + 16u * i) + g_spu_hot_reloc)));
    uint32_t w;
    { const u128 c = spu_ls_read128(ctx, (uint32_t)((int32_t)(T + 0xD0u) + g_spu_hot_reloc)); w = __builtin_bswap32(c._u32[3]); }
    ls[r3 + 0x160u + (uint32_t)k] = 1;
    for (int b = 0; b < 4; b++) ls[r3 + 0x5Cu + (uint32_t)b] = (uint8_t)(ptr >> (24 - 8 * b));
    for (uint32_t i = 0; i < nv; i++) {
        uint8_t* d = ls + out + 16u * i;
        memcpy(d, ls + r4 + passo * i, 12);
        memcpy(d + 12, &w, 4);
    }
    g[3] = spu_il(0);
    if (na18) g[1] = spu_ai(g[1], 224);
    return 1;
}
#define PLCOPIA_INTEIRO(nome, rot, passo, T, na18)                  static __attribute__((noinline)) int nome(spu_context* ctx)     {                                                                   static volatile long c[2];                                      const int r = plcopia_inteiro(ctx, (passo), (T), (na18));         gowa_edge_conta(rot, c, r);                                     return r;                                                   }
PLCOPIA_INTEIRO(pl_0075AF20_inteiro, "pl75AF20i", 15u, 0x301D0u, 0)
PLCOPIA_INTEIRO(pl_0075AF20_n10, "pl75AF20i/10", 15u, 0x301D0u, 2)
PLCOPIA_INTEIRO(pl_0075B470_inteiro, "pl75B470i", 22u, 0x301C0u, 0)
PLCOPIA_INTEIRO(pl_0075B470_n18, "pl75B470i/18", 22u, 0x301C0u, 1)
PLCOPIA_INTEIRO(pl_0075B710_inteiro, "pl75B710i", 18u, 0x301C0u, 0)
PLCOPIA_INTEIRO(pl_0075B710_n18, "pl75B710i/18", 18u, 0x301C0u, 1)
#undef PLCOPIA_INTEIRO
#endif
