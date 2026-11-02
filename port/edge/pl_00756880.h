/* GOWA (02/11, EDGE nativo, marco E4): o plugin 0x756880 do EDGE inteiro -- os comandos do RSX que descrevem os 7
 * atributos do vértice de 54 bytes (o formato que o plugin 0x74C580 grava): para o atributo i (índice a = byte em
 * r9 + i), um registro de 16 bytes no buffer de comandos, a partir do cursor (palavra em r5 + 8, sem alinhamento):
 *     0x00041740 + 4a (SET_VERTEX_DATA_ARRAY_FORMAT), formato_i, 0x00041680 + 4a (..._OFFSET), (r7 + desl_i) | (r6 << 31)
 * formatos 0x3632, 0x3641, 0x3616, 0x3632, 0x3633 x3; deslocamentos 0, 12, 20, 24, 36, 42, 48. O cursor anda 16 por
 * registro (gravado antes de cada um, como o original). O original monta cada registro desalinhado por ler-
 * misturar-gravar de dois quadwords; aqui os 112 bytes direto, com o buffer, o cursor e os índices sem se encostar
 * (senão volta 0 e fica o traduzido). Função folha sem pilha: sai com r3 = 0 (o que o original devolve); os
 * temporários ficam diferentes (só a ABI importa a quem chama). GOWA_EDGE_NATIVO=0 desliga. Gancho 'entrada'. */
#ifndef GOWA_PL_00756880_H
#define GOWA_PL_00756880_H
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

static int pl_00756880_corpo(spu_context* ctx)
{
    if (!gowa_edge_nativo_on()) return 0;
    u128* g = ctx->gpr;
    const uint32_t r5 = g[5]._u32[0] & SPU_LS_MASK, r9 = g[9]._u32[0] & SPU_LS_MASK, r7 = g[7]._u32[0];
    if (g[50]._u32[0] != 5952u || g[72]._u32[0] != g[9]._u32[0] + 13u) return 0;   /* o que a entrada 0x30000 deixa */
    if (r5 + 16u > SPU_LS_SIZE || r9 + 7u > SPU_LS_SIZE) return 0;
    const uint32_t cur = spu_ls_read32(ctx, (r5 + 8u) & ~3u) & SPU_LS_MASK;
    if (((r5 + 8u) & 3u) || cur + 128u > SPU_LS_SIZE) return 0;
    if ((r5 + 8u < cur + 128u && cur < r5 + 12u) || (r9 < cur + 128u && cur < r9 + 7u)) return 0;
    uint8_t* ls = SPU_LS_BASE(ctx);
    static const uint32_t fmt[7] = { 0x3632u, 0x3641u, 0x3616u, 0x3632u, 0x3633u, 0x3633u, 0x3633u };
    static const uint32_t desl[7] = { 0u, 12u, 20u, 24u, 36u, 42u, 48u };
    const uint32_t loc = g[6]._u32[0] << 31;
    uint32_t w[28];
    for (int i = 0; i < 7; i++) {
        const uint32_t a = ls[r9 + (uint32_t)i];
        w[4 * i + 0] = __builtin_bswap32(0x00041740u + 4u * a);
        w[4 * i + 1] = __builtin_bswap32(fmt[i]);
        w[4 * i + 2] = __builtin_bswap32(0x00041680u + 4u * a);
        w[4 * i + 3] = __builtin_bswap32((r7 + desl[i]) | loc);
    }
    memcpy(ls + cur, w, 112);
    spu_ls_write32(ctx, r5 + 8u, cur + 112u);
    g[3] = spu_il(0);
    return 1;
}

static __attribute__((noinline)) int pl_00756880_nativo(spu_context* ctx)
{
    static volatile long n[2];
    const int r = pl_00756880_corpo(ctx);
    gowa_edge_conta("pl756880", n, r);
    return r;
}
#endif
