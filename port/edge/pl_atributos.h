/* GOWA (02/11, EDGE nativo, marco E4): os plugins do EDGE que descrevem os atributos do vértice para o RSX (0x756880,
 * 0x75CBC0, 0x757DD0, 0x7538C0, 0x756C40, 0x75B9B0, 0x75BB70, 0x75CA80, 0x75A380), inteiros. Para o atributo i (índice a = byte em r9 + i), um registro de 16 bytes no buffer de
 * comandos, a partir do cursor (palavra em r5 + 8, sem alinhamento):
 *     0x00041740 + 4a (SET_VERTEX_DATA_ARRAY_FORMAT), formato_i, 0x00041680 + 4a (..._OFFSET), (r7 + desl_i) | (r6 << 31)
 * O cursor anda 16 por registro (o original o grava antes de cada um). O original monta cada registro desalinhado por
 * ler-misturar-gravar de dois quadwords; aqui os 16n bytes direto, com o buffer, o cursor e os índices sem se encostar
 * (senão volta 0 e fica o traduzido). Funções folha sem pilha: saem com r3 = 0 (o que o original devolve); os
 * temporários ficam diferentes (só a ABI importa a quem chama). Nos divididos em 0x30000 + 0x3000C, a entrada deixa
 * r9 + 13 num registro (o deslocamento do rotqby que tira o byte de r9): conferido; gancho 'entrada' na 0x3000C. Os
 * outros (0x7538C0, 0x756C40, 0x75B9B0, 0x75BB70, 0x75CA80) são uma função só: gancho na 0x30000. As tabelas saem do
 * próprio código pelo oráculo (scripts_edge/pl_atributos_tabela.py). GOWA_EDGE_NATIVO=0 desliga. */
#ifndef GOWA_PL_ATRIBUTOS_H
#define GOWA_PL_ATRIBUTOS_H
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

/* n registros; rdesl = o registro em que a entrada 0x30000 deixou r9 + 13 (-1: plugin inteiro numa função só) */
static int pl_atributos(spu_context* ctx, int n, const uint32_t* fmt, const uint32_t* desl, int rdesl)
{
    if (!gowa_edge_nativo_on()) return 0;
    u128* g = ctx->gpr;
    const uint32_t r5 = g[5]._u32[0] & SPU_LS_MASK, r9 = g[9]._u32[0] & SPU_LS_MASK, r7 = g[7]._u32[0];
    const uint32_t tam = 16u * (uint32_t)n;
    if (rdesl >= 0 && g[rdesl]._u32[0] != g[9]._u32[0] + 13u) return 0;
    if (r5 + 16u > SPU_LS_SIZE || r9 + (uint32_t)n > SPU_LS_SIZE || ((r5 + 8u) & 3u)) return 0;
    const uint32_t cur = spu_ls_read32(ctx, r5 + 8u) & SPU_LS_MASK;
    if (cur + tam + 16u > SPU_LS_SIZE) return 0;
    if ((r5 + 8u < cur + tam && cur < r5 + 12u) || (r9 < cur + tam && cur < r9 + (uint32_t)n)) return 0;
    uint8_t* ls = SPU_LS_BASE(ctx);
    const uint32_t loc = g[6]._u32[0] << 31;
    uint32_t w[4];
    for (int i = 0; i < n; i++) {
        const uint32_t a = ls[r9 + (uint32_t)i];
        w[0] = __builtin_bswap32(0x00041740u + 4u * a);
        w[1] = __builtin_bswap32(fmt[i]);
        w[2] = __builtin_bswap32(0x00041680u + 4u * a);
        w[3] = __builtin_bswap32((r7 + desl[i]) | loc);
        memcpy(ls + cur + 16u * (uint32_t)i, w, 16);
    }
    spu_ls_write32(ctx, r5 + 8u, cur + tam);
    g[3] = spu_il(0);
    return 1;
}

/* 0x756880: o vértice de 54 bytes do plugin 0x74C580 (7 atributos) */
static __attribute__((noinline)) int pl_00756880_nativo(spu_context* ctx)
{
    static const uint32_t fmt[7] = { 0x3632u, 0x3641u, 0x3616u, 0x3632u, 0x3633u, 0x3633u, 0x3633u };
    static const uint32_t desl[7] = { 0u, 12u, 20u, 24u, 36u, 42u, 48u };
    static volatile long c[2];
    const int r = pl_atributos(ctx, 7, fmt, desl, 72);
    gowa_edge_conta("pl756880", c, r);
    return r;
}
/* 0x75CBC0: um atributo, vértice de 12 bytes */
static __attribute__((noinline)) int pl_0075CBC0_nativo(spu_context* ctx)
{
    static const uint32_t fmt[1] = { 0xC32u };
    static const uint32_t desl[1] = { 0u };
    static volatile long c[2];
    const int r = pl_atributos(ctx, 1, fmt, desl, 41);
    gowa_edge_conta("pl75CBC0", c, r);
    return r;
}
/* 0x757DD0: 6 atributos, vértice de 46 bytes */
static __attribute__((noinline)) int pl_00757DD0_nativo(spu_context* ctx)
{
    static const uint32_t fmt[6] = { 0x2E32u, 0x2E16u, 0x2E32u, 0x2E33u, 0x2E33u, 0x2E33u };
    static const uint32_t desl[6] = { 0u, 12u, 16u, 28u, 34u, 40u };
    static volatile long c[2];
    const int r = pl_atributos(ctx, 6, fmt, desl, 49);
    gowa_edge_conta("pl757DD0", c, r);
    return r;
}
#define PL_ATRIB(nome, rot, n, rd, ...)                                     \
    static __attribute__((noinline)) int nome(spu_context* ctx)             \
    {                                                                       \
        static const uint32_t t[2 * (n)] = { __VA_ARGS__ };                 \
        static volatile long c[2];                                          \
        const int r = pl_atributos(ctx, (n), t, t + (n), (rd));             \
        gowa_edge_conta(rot, c, r);                                         \
        return r;                                                           \
    }
/* formatos, depois deslocamentos */
PL_ATRIB(pl_007538C0_nativo, "pl7538C0", 8, -1, 0x3A32u, 0x3A41u, 0x3A16u, 0x3A44u, 0x3A32u, 0x3A33u, 0x3A33u, 0x3A33u,
         0u, 12u, 20u, 24u, 28u, 40u, 46u, 52u)
PL_ATRIB(pl_00756C40_nativo, "pl756C40", 7, -1, 0x3232u, 0x3216u, 0x3244u, 0x3232u, 0x3233u, 0x3233u, 0x3233u,
         0u, 12u, 16u, 20u, 32u, 38u, 44u)
PL_ATRIB(pl_0075B9B0_nativo, "pl75B9B0", 3, -1, 0x1C32u, 0x1C16u, 0x1C32u, 0u, 12u, 16u)
PL_ATRIB(pl_0075BB70_nativo, "pl75BB70", 3, -1, 0x1832u, 0x1841u, 0x1816u, 0u, 12u, 20u)
PL_ATRIB(pl_0075CA80_nativo, "pl75CA80", 2, -1, 0x1032u, 0x1016u, 0u, 12u)
/* 02/11: o 0x75A380 (dividido em 0x30000 + 0x30064, r9 + 13 em r59; gancho na 0x30064). Não roda nas gravações de
 * hoje (a entrada +0x64 veio de uma fase 1 de 29/10): conferido na bancada sintética. */
PL_ATRIB(pl_0075A380_nativo, "pl75A380", 4, 59, 0x2432u, 0x2441u, 0x2416u, 0x2432u, 0u, 12u, 20u, 24u)
#undef PL_ATRIB
#endif
