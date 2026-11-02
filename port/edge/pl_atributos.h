/* GOWA (02/11, EDGE nativo, marco E4): os plugins do EDGE que descrevem os atributos do vértice para o RSX (0x756880,
 * 0x75CBC0, 0x757DD0), inteiros. Para o atributo i (índice a = byte em r9 + i), um registro de 16 bytes no buffer de
 * comandos, a partir do cursor (palavra em r5 + 8, sem alinhamento):
 *     0x00041740 + 4a (SET_VERTEX_DATA_ARRAY_FORMAT), formato_i, 0x00041680 + 4a (..._OFFSET), (r7 + desl_i) | (r6 << 31)
 * O cursor anda 16 por registro (o original o grava antes de cada um). O original monta cada registro desalinhado por
 * ler-misturar-gravar de dois quadwords; aqui os 16n bytes direto, com o buffer, o cursor e os índices sem se encostar
 * (senão volta 0 e fica o traduzido). Funções folha sem pilha: saem com r3 = 0 (o que o original devolve); os
 * temporários ficam diferentes (só a ABI importa a quem chama). A entrada 0x30000 de cada um deixa r9 + 13 num
 * registro (o deslocamento do rotqby que tira o byte de r9): conferido. GOWA_EDGE_NATIVO=0 desliga. Gancho 'entrada'
 * na 0x3000C de cada plugin. */
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

/* n registros; rdesl = o registro em que a entrada 0x30000 deixou r9 + 13 */
static int pl_atributos(spu_context* ctx, int n, const uint32_t* fmt, const uint32_t* desl, int rdesl)
{
    if (!gowa_edge_nativo_on()) return 0;
    u128* g = ctx->gpr;
    const uint32_t r5 = g[5]._u32[0] & SPU_LS_MASK, r9 = g[9]._u32[0] & SPU_LS_MASK, r7 = g[7]._u32[0];
    const uint32_t tam = 16u * (uint32_t)n;
    if (g[rdesl]._u32[0] != g[9]._u32[0] + 13u) return 0;
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
#endif
