/* GOWA (30/10, EDGE nativo, marco E3): o laço 0x304A0 do plugin 0x74EF90 (posição + 2 vetores comprimidos).
 *
 * Cada vértice ocupa 12 bytes no fluxo (a partir de r44 - 48): a posição em 3 x u16 (como o plugin 0x755540:
 * v = (u16 & r68) + r63, saída v * 2^(r67 - 32), w = 1,0) e, nos 6 bytes seguintes, os dois vetores unitários como no
 * plugin 0x752710 (campos de 10 bits + seletor de 4 bits, contas pl752_contas/pl752_sel; tabela de seleção em r49).
 * Saídas em 3 fluxos de 16 bytes por vértice: posições em r78 + r35, vetor 1 logo depois (r78), vetor 2 depois dele
 * (r65); N = -r35/32 voltas de 2 vértices; os vetores do último par saem no epílogo do original, que também grava
 * 32 bytes "do par -1" (os registradores de entrada, zero) no fim de cada fluxo de vetores, restaura r80-r88 e r126
 * da pilha, soma 384 ao r1 e zera o r3 -- tudo refeito aqui. Conferido pelo oráculo (scripts_edge/pl74e_modelo.py
 * --ls: a LS inteira das 12 fotos). Só o formato das fotos (constantes conferidas abaixo). GOWA_EDGE_NATIVO=0
 * desliga. Gancho na entrada da função do laço (edge_nativo.py). */
#ifndef GOWA_PL_0074EF90_H
#define GOWA_PL_0074EF90_H
#include <stdlib.h>
#include <string.h>
#include <immintrin.h>
#include "pl_00752710.h"   /* pl752_contas, pl752_sel, pl752_u128 */
#include "edge_comum.h"

static inline int pl74e_igual(u128 a, uint32_t w0, uint32_t w1, uint32_t w2, uint32_t w3)
{
    return a._u32[0] == w0 && a._u32[1] == w1 && a._u32[2] == w2 && a._u32[3] == w3;
}

#if PL752_VETOR && (defined(__SSE4_1__) || defined(__AVX2__))
/* os n pares: o lixo "do par -1" (zeros) no fim dos dois fluxos de vetores, as posições (3 x u16 + viés, vezes a
 * potência de 2, w = 1,0) e os dois vetores de cada vértice (as contas do 0x752710; tabela da seleção em tab) */
static void pl74e_escreve(spu_context* ctx, uint32_t ent, uint32_t pb, uint32_t v1, uint32_t v2, uint32_t n, uint32_t tab,
                          const int32_t* vies, const double* esc)
{
    uint8_t* ls = SPU_LS_BASE(ctx);
    /* o lixo "do par -1" no fim de cada fluxo de vetores (o do vetor 1 é coberto pelo par 0 do vetor 2 depois) */
    memset(ls + v1 + 32u * n, 0, 32);
    memset(ls + v2 + 32u * n, 0, 32);
    /* posições */
    {
        const __m128i sel = _mm_setr_epi8(1, 0, -1, -1, 3, 2, -1, -1, 5, 4, -1, -1, -1, -1, -1, -1);
        const __m128i bs = _mm_setr_epi8(3, 2, 1, 0, 7, 6, 5, 4, 11, 10, 9, 8, 15, 14, 13, 12);
        const __m128i vi = _mm_setr_epi32(vies[0], vies[1], vies[2], 0);
        const __m128 es = _mm_setr_ps((float)esc[0], (float)esc[1], (float)esc[2], 0.0f);
        const __m128 w1 = _mm_castsi128_ps(_mm_setr_epi32(0, 0, 0, 0x3F800000));
        for (uint32_t i = 0; i < 2u * n; i++) {
            const __m128i u = _mm_shuffle_epi8(_mm_loadu_si128((const __m128i*)(ls + ent + 12u * i)), sel);
            __m128 f = _mm_mul_ps(_mm_cvtepi32_ps(_mm_add_epi32(u, vi)), es);
            f = _mm_blend_ps(f, w1, 8);
            _mm_storeu_si128((__m128i*)(ls + pb + 16u * i), _mm_shuffle_epi8(_mm_castps_si128(f), bs));
        }
    }
    /* os 2 vetores de cada vértice, PL752_PARES pares por vez (as contas do plugin 0x752710) */
    for (uint32_t p0 = 0; p0 < n; p0 += PL752_PARES) {
        uint32_t f1[4 * PL752_PARES], f2[4 * PL752_PARES], ab1[4 * PL752_PARES], ab2[4 * PL752_PARES], c1[4 * PL752_PARES], c2[4 * PL752_PARES];
        uint32_t s1[2 * PL752_PARES], s2[2 * PL752_PARES];
        for (uint32_t q = 0; q < PL752_PARES; q++) {
            const uint32_t p = p0 + q < n ? p0 + q : n - 1u;   /* além do fim: repete o último (não é gravado) */
            for (uint32_t k = 0; k < 2; k++) {
                const uint8_t* r = ls + ent + 24u * p + 12u * k + 6u;
                const uint64_t bits = ((uint64_t)r[0] << 40) | ((uint64_t)r[1] << 32) | ((uint64_t)r[2] << 24) |
                                      ((uint64_t)r[3] << 16) | ((uint64_t)r[4] << 8) | r[5];
                f1[4 * q + 2 * k] = (uint32_t)(bits >> 38) & 0x3FFu; f1[4 * q + 2 * k + 1] = (uint32_t)(bits >> 28) & 0x3FFu;
                f2[4 * q + 2 * k] = (uint32_t)(bits >> 14) & 0x3FFu; f2[4 * q + 2 * k + 1] = (uint32_t)(bits >> 4) & 0x3FFu;
                s1[2 * q + k] = (uint32_t)(bits >> 24) & 0xFu; s2[2 * q + k] = (uint32_t)bits & 0xFu;
            }
        }
        pl752_contas(f1, ab1, c1);
        pl752_contas(f2, ab2, c2);
        for (uint32_t q = 0; q < PL752_PARES && p0 + q < n; q++) {
            const uint32_t p = p0 + q;
            const u128 A1 = pl752_u128(&ab1[4 * q]), C1 = pl752_u128(&c1[4 * q]), A2 = pl752_u128(&ab2[4 * q]), C2 = pl752_u128(&c2[4 * q]);
            spu_ls_write128(ctx, v1 + 32u * p, pl752_sel(C1, A1, spu_ls_read128(ctx, tab + 16u * s1[2 * q]), 0));
            spu_ls_write128(ctx, v1 + 32u * p + 16u, pl752_sel(C1, A1, spu_ls_read128(ctx, tab + 16u * s1[2 * q + 1]), 1));
            spu_ls_write128(ctx, v2 + 32u * p, pl752_sel(C2, A2, spu_ls_read128(ctx, tab + 16u * s2[2 * q]), 0));
            spu_ls_write128(ctx, v2 + 32u * p + 16u, pl752_sel(C2, A2, spu_ls_read128(ctx, tab + 16u * s2[2 * q + 1]), 1));
        }
    }
}
#endif

static int pl_0074EF90_corpo(spu_context* ctx);
static __attribute__((noinline)) int pl_0074EF90_nativo(spu_context* ctx)
{
    static volatile long n[2];
    const int r = pl_0074EF90_corpo(ctx);
    gowa_edge_conta("pl74EF90", n, r);
    return r;
}
static int pl_0074EF90_corpo(spu_context* ctx)
{
#if PL752_VETOR && (defined(__SSE4_1__) || defined(__AVX2__))
    if (!gowa_edge_nativo_on()) return 0;
    u128* g = ctx->gpr;
    const uint32_t off = g[35]._u32[0];
    if (!off || (off & 31u) || (0u - off) > 0x10000u) return 0;
    if (!pl74e_igual(g[33], 0xFFFFFFD0u, 0xFFFFFFB7u, 0, 0) || !pl74e_igual(g[36], 0xFFFFFFF0u, 0xFFFFFFE0u, 0xFFFFFFD0u, 0xFFFFFFB7u) ||
        !pl74e_igual(g[37], 0xFFFFFFE0u, 0xFFFFFFD0u, 0xFFFFFFB7u, 0) || !pl74e_igual(g[60], 0xFFFFFFB7u, 0, 0, 0) ||
        !pl74e_igual(g[55], 0xFFFFFFFCu, 0xFFFFFFFAu, 0xFFFFFFFCu, 0) || !pl74e_igual(g[64], 0, 0xFFFFFFFFu, 0, 0xFFFFFFFFu) ||
        !pl74e_igual(g[84], 0x700u, 0x506u, 0x607u, 0) || !pl74e_igual(g[85], 0xB00u, 0x90Au, 0xA0Bu, 0) ||
        !pl74e_igual(g[86], 0x400u, 0x203u, 0x304u, 0) || !pl74e_igual(g[87], 0x800u, 0x607u, 0x708u, 0) ||
        !pl74e_igual(g[82], 0x04050607u, 0x08090A0Bu, 0x14151617u, 0x18191A1Bu) ||
        !pl74e_igual(g[83], 0x0C0D0E0Fu, 0x10111213u, 0x14151617u, 0xC0C0C0C0u) ||
        !pl74e_igual(g[34], 0x00010203u, 0x04050607u, 0x08090A0Bu, 0xC0C0C0C0u) ||
        !pl74e_igual(g[70], 0x08080808u, 0x08080808u, 0x08080808u, 0) || !pl74e_igual(g[41], 0x7FFFFFFFu, 0x7FFFFFFFu, 0x7FFFFFFFu, 0) ||
        !pl74e_igual(g[43], 0x80000000u, 0x80000000u, 0x80000000u, 0xFFFFFFFFu) ||
        !pl74e_igual(g[72], 0x46352E7Cu, 0x46352E7Cu, 0x46352E7Cu, 0x46352E7Cu) ||
        !pl74e_igual(g[71], 0xC6353150u, 0xC6353150u, 0xC6353150u, 0xC6353150u) ||
        !pl74e_igual(g[68], 0xFFFFu, 0xFFFFu, 0xFFFFu, 0) || !pl74e_igual(g[42], 0, 0, 0, 0x0C0D0E0Fu) ||
        !pl74e_igual(g[73], 0x3F800000u, 0x3F800000u, 0x3F800000u, 0x3F800000u) ||
        !pl74e_igual(g[77], 0, 0, 0, 0x3F800000u) || !pl74e_igual(g[81], 0, 0, 0, 0xFFFFFFFFu))
        return 0;
    /* os registradores que formam o lixo da 1a volta: zero (como em todas as fotos) */
    static const int zeros[] = { 39, 56, 50, 51, 52, 57, 58, 59 };
    for (unsigned i = 0; i < sizeof zeros / sizeof zeros[0]; i++) if (!pl74e_igual(g[zeros[i]], 0, 0, 0, 0)) return 0;
    double esc[3];
    int32_t vies[3];
    for (int c = 0; c < 3; c++) {
        const uint32_t s = g[67]._u32[c];
        if (s < 8u || s > 31u || g[66]._u32[c] != s - 32u) return 0;
        vies[c] = (int32_t)g[63]._u32[c];
        if (vies[c] >= (1 << 23) || vies[c] <= -(1 << 23)) return 0;
        esc[c] = 1.0;
        for (uint32_t k = s; k < 32u; k++) esc[c] *= 0.5;
    }
    const uint32_t n = (0u - off) / 32u, ent = g[44]._u32[0] - 48u, pb = g[78]._u32[0] + off;
    const uint32_t v1 = pb + 32u * n, v2 = v1 + 32u * n, tab = g[49]._u32[0];
    if (g[78]._u32[0] != v1 || g[65]._u32[0] != v2 || g[69]._u32[0] != v2 + 32u * n ||
        g[74]._u32[0] != v1 + 16u || g[75]._u32[0] != v2 + 16u) return 0;
    if ((pb & 15u) || ent > SPU_LS_SIZE || ent + 24u * n + 64u > SPU_LS_SIZE || v2 + 32u * n + 32u > SPU_LS_SIZE ||
        (tab & 15u) || tab + 256u > SPU_LS_SIZE) return 0;
    if (pb < ent + 24u * n + 64u && ent < v2 + 32u * n + 32u) return 0;   /* entrada e saídas separadas */
    pl74e_escreve(ctx, ent, pb, v1, v2, n, tab, vies, esc);
    /* o epílogo do original */
    g[3] = spu_fsmbi(0x0);
    g[1] = spu_ai(g[1], 384);
    for (int r = 80; r <= 88; r++) g[r] = spu_ls_read128(ctx, g[1]._u32[0] - 0x10u * (uint32_t)(r - 79));
    g[126] = spu_ls_read128(ctx, g[1]._u32[0] - 0xA0u);
    return 1;
#else
    (void)ctx;
    return 0;
#endif
}
/* 02/11: o plugin inteiro, da entrada 0x30090 (o job chama direto nela: quem chama já fez o que o 0x30000 faz --
 * r80-r88 e r126 na pilha, o r1 antigo em r1 - 0x180, r1 -= 384 -- e deixou as constantes nos registradores, conferidas
 * aqui). A preparação, no formato das gravações: o descritor (r6, sem alinhamento) dá por componente a largura (bytes
 * 16 + 2c e 17 + 2c; só 16 = os 3 u16) e s = 32 - fracionários, e o viés nas palavras 0, 4 e 8; o vértice de 12 bytes
 * vem de r4, n = r5 (aos pares). Acrescenta os atributos 1, 3 e 4 nos três primeiros zeros da lista (r3 + 0x160) com
 * os ponteiros daquelas vagas em r3 + 0x58 + 4 * id (posição no do 1, vetores nos do 3 e do 4) e copia a tabela de
 * constantes 0x30700..0x307B0 (a da seleção) para o quadro (r1 + 0x20). Sai como o epílogo do original: r80-r88 e
 * r126 da pilha, r1 += 384, r3 = 0. */
static inline uint32_t pl74e_palavra(const uint8_t* ls, uint32_t a)   /* lqd + rotqby: a palavra dá a volta no quadword */
{
    const uint32_t q = a & SPU_LS_MASK & ~15u;
    uint32_t v = 0;
    for (uint32_t i = 0; i < 4; i++) v = (v << 8) | ls[q + ((a + i) & 15u)];
    return v;
}
static int pl_0074EF90_n90_corpo(spu_context* ctx)
{
#if PL752_VETOR && (defined(__SSE4_1__) || defined(__AVX2__))
    if (!gowa_edge_nativo_on()) return 0;
    u128* g = ctx->gpr;
    uint8_t* ls = SPU_LS_BASE(ctx);
    const uint32_t sp = g[1]._u32[0] & SPU_LS_MASK, r3 = g[3]._u32[0] & SPU_LS_MASK, r4 = g[4]._u32[0] & SPU_LS_MASK,
                   n = g[5]._u32[0], r6 = g[6]._u32[0];
    if ((sp & 15u) || sp + 0x180u > SPU_LS_SIZE || sp < 0x100u || (r3 & 15u) || r3 + 0x170u > SPU_LS_SIZE || !n || n > 4096u)
        return 0;
    /* o que o 0x30000 deixa */
    if (spu_ls_read32(ctx, sp) != sp + 384u) return 0;
    static const struct { uint8_t r; int8_t k; } cte[] = { {20, 1}, {73, 19}, {81, 17}, {63, 18}, {19, 20}, {50, 21}, {77, 4},
        {55, 8}, {41, 12}, {44, 0}, {7, 3} };
    for (unsigned i = 0; i < sizeof cte / sizeof cte[0]; i++)
        for (int w = 0; w < 4; w++) if (g[cte[i].r]._u32[w] != (uint32_t)cte[i].k) return 0;
    static const struct { uint8_t r; uint8_t k; } soma6[] = { {85, 30}, {51, 13}, {16, 32}, {43, 31}, {13, 34}, {45, 33} };
    for (unsigned i = 0; i < sizeof soma6 / sizeof soma6[0]; i++)
        for (int w = 0; w < 4; w++) if (g[soma6[i].r]._u32[w] != g[6]._u32[w] + soma6[i].k) return 0;
    {
        const u128 l = spu_ls_read128(ctx, r3 + 0x160u);
        if (memcmp(&l, &g[33], 16)) return 0;
        static const struct { uint8_t r; uint16_t a; } cq[] = { {62, 0x700}, {61, 0x710}, {68, 0x720}, {66, 0x730} };
        for (unsigned i = 0; i < 4; i++) {
            const u128 q = spu_ls_read128(ctx, (uint32_t)(0x30000 + cq[i].a + g_spu_hot_reloc));
            if (memcmp(&q, &g[cq[i].r], 16)) return 0;
        }
    }
    int32_t vies[3];
    double esc[3];
    for (uint32_t c = 0; c < 3; c++) {
        const uint32_t fr = ls[(r6 + 17u + 2u * c) & SPU_LS_MASK];
        if (ls[(r6 + 16u + 2u * c) & SPU_LS_MASK] + fr != 16u || fr < 1u || fr > 24u) return 0;
        vies[c] = (int32_t)pl74e_palavra(ls, r6 + 4u * c);
        if (vies[c] >= (1 << 23) || vies[c] <= -(1 << 23)) return 0;
        esc[c] = 1.0;
        for (uint32_t k = 0; k < fr; k++) esc[c] *= 0.5;
    }
    static const uint8_t ids[3] = { 1, 3, 4 };
    int k[3], j = 0;
    for (int i = 0; i < 3; i++) {
        while (j < 16 && ls[r3 + 0x160u + (uint32_t)j]) j++;
        if (j >= 16) return 0;
        k[i] = j++;
    }
    uint32_t pt[3];
    for (int i = 0; i < 3; i++) pt[i] = pl74e_palavra(ls, r3 + 16u + 4u * (uint32_t)k[i]);
    const uint32_t pb = pt[0] & SPU_LS_MASK, v1 = pt[1] & SPU_LS_MASK, v2 = pt[2] & SPU_LS_MASK, P = (n + 1u) / 2u, tab = sp + 32u;
    const uint32_t ni = 24u * (P + PL752_PARES) + 64u, no = 32u * P, nv = 32u * P + 32u;
#define PL74EI_ENC(a0, n0, c0, n1) ((a0) < (c0) + (n1) && (c0) < (a0) + (n0))
    if (((pb | v1 | v2) & 15u) || r4 + ni > SPU_LS_SIZE || pb + no > SPU_LS_SIZE || v1 + nv > SPU_LS_SIZE || v2 + nv > SPU_LS_SIZE ||
        PL74EI_ENC(pb, no, v1, no) || PL74EI_ENC(pb, no, v2, no) || PL74EI_ENC(v1, no, v2, no) ||
        PL74EI_ENC(pb, no, r4, ni) || PL74EI_ENC(v1, nv, r4, ni) || PL74EI_ENC(v2, nv, r4, ni) || PL74EI_ENC(pb, no, sp, 0x180u) ||
        PL74EI_ENC(v1, nv, sp, 0x180u) || PL74EI_ENC(v2, nv, sp, 0x180u) || PL74EI_ENC(pb, no, r3, 0x170u) ||
        PL74EI_ENC(v1, nv, r3, 0x170u) || PL74EI_ENC(v2, nv, r3, 0x170u) || PL74EI_ENC(r4, ni, sp, 0x180u))
        return 0;
#undef PL74EI_ENC
    for (uint32_t i = 0; i < 12; i++)
        spu_ls_write128(ctx, sp + 0x20u + 16u * i, spu_ls_read128(ctx, (uint32_t)(0x30700 + 16 * (int)i + g_spu_hot_reloc)));
    for (int i = 0; i < 3; i++) {
        ls[r3 + 0x160u + (uint32_t)k[i]] = ids[i];
        const uint32_t a = r3 + 0x58u + 4u * ids[i];
        ls[a] = (uint8_t)(pt[i] >> 24); ls[a + 1] = (uint8_t)(pt[i] >> 16); ls[a + 2] = (uint8_t)(pt[i] >> 8); ls[a + 3] = (uint8_t)pt[i];
    }
    pl74e_escreve(ctx, r4, pb, v1, v2, P, tab, vies, esc);
    g[3] = spu_fsmbi(0x0);
    g[1] = spu_ai(g[1], 384);
    for (int r = 80; r <= 88; r++) g[r] = spu_ls_read128(ctx, g[1]._u32[0] - 0x10u * (uint32_t)(r - 79));
    g[126] = spu_ls_read128(ctx, g[1]._u32[0] - 0xA0u);
    return 1;
#else
    (void)ctx;
    return 0;
#endif
}

static __attribute__((noinline)) int pl_0074EF90_n90(spu_context* ctx)
{
    static volatile long c[2];
    const int r = pl_0074EF90_n90_corpo(ctx);
    gowa_edge_conta("pl74EF90/90", c, r);
    return r;
}
#endif
