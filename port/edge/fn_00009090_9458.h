/* GOWA (02/11, EDGE nativo, marco E4): o bloco 9458 do orquestrador 0x9090 do job 0x72DF80 -- um desenho no buffer
 * de comandos do RSX, a partir do cursor r13 (a palavra 2 do quadword em r82 + 8 acompanha o cursor):
 *     0x00041714, r92                  (método 0x1714, 1 palavra)
 *     0x0008181C, r80 & 0x1FFFFFFF, 17 (método 0x181C, 2 palavras: endereço e formato do índice)
 *     r93, 5                           (método 0x1808, begin)
 * cada palavra gravada como o original (ler-modificar-gravar do quadword; aqui a palavra direto, na mesma ordem,
 * inclusive as 3 atualizações do cursor), r13 += 28. Depois a decisão do original: r68 (r81 = metade do resto até
 * o bloco de 128 seguinte, r81 != 0 e r89 > r81) vai a loc_00009888; senão r80 = r13 antigo + 36 e, se passar do
 * fim do buffer (r15), loc_00009958; senão loc_00009570. Os registradores vivos nessas saídas (scripts_edge/vivos.py
 * --abi: r2, r5-r10, r13, r14, r80, r81, r86; o desvio indireto do buffer cheio pela ABI) saem como o original os
 * deixa; os outros temporários não são lidos por ninguém. r83 != 8, r85 != 4 ou cursor desalinhado voltam 0 (fica o
 * traduzido). GOWA_EDGE_NATIVO=0 desliga. Gancho 'multi' logo depois do rótulo. */
#ifndef GOWA_FN_00009090_9458_H
#define GOWA_FN_00009090_9458_H
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

static inline void n9458_w(uint8_t* ls, uint32_t a, uint32_t v)
{
    const uint32_t be = __builtin_bswap32(v);
    memcpy(ls + (a & SPU_LS_MASK & ~3u), &be, 4);
}

static int fn_00009090_9458_corpo(spu_context* ctx)
{
    if (!gowa_edge_nativo_on()) return 0;
    u128* g = ctx->gpr;
    const uint32_t c = g[13]._u32[0], r82 = g[82]._u32[0];
    if (g[83]._u32[0] != 8u || g[85]._u32[0] != 4u || (c & 3u) || (r82 & 3u)) return 0;
    uint8_t* ls = SPU_LS_BASE(ctx);
    const u128 r13 = g[13];
    const u128 r17 = spu_and(g[80], spu_iohl(spu_ilhu(0x1FFF), 0xFFFF));
    n9458_w(ls, c, 0x00041714u);
    n9458_w(ls, c + 4u, g[92]._u32[0]);
    n9458_w(ls, r82 + 8u, c + 8u);
    n9458_w(ls, c + 8u, 0x0008181Cu);
    n9458_w(ls, c + 12u, r17._u32[0]);
    g[8] = spu_ls_read128(ctx, c + 16u);
    n9458_w(ls, c + 16u, 17u);
    g[6] = spu_ls_read128(ctx, c + 16u);
    g[7] = spu_ls_read128(ctx, r82 + 8u);
    n9458_w(ls, r82 + 8u, c + 20u);
    g[2] = spu_ls_read128(ctx, r82 + 8u);
    n9458_w(ls, c + 20u, g[93]._u32[0]);
    n9458_w(ls, c + 24u, 5u);
    n9458_w(ls, r82 + 8u, c + 28u);
    /* registradores vivos nas saídas */
    g[9] = spu_ai(r13, 8);
    g[10] = spu_il(17);
    g[13] = spu_ai(r13, 28);
    g[86] = spu_cwd(g[82], 8);
    const u128 r33 = spu_andi(spu_ai(r17, 127), -128);
    g[81] = spu_rotmi(spu_sf(r17, r33), -1);
    g[14] = r13;
    const u128 r22 = spu_and(spu_sfi(spu_nor(spu_ceqi(g[81], 0), spu_ceqi(g[81], 0)), 0), spu_sfi(spu_clgt(g[89], g[81]), 0));
    g[5] = spu_ceqbi(r22, 0);
    if (spu_xsbh(g[5])._u16[0] != 0) return 2;          /* loc_00009888 */
    g[80] = spu_ai(g[14], 36);
    g[14] = spu_clgt(g[80], g[15]);
    return g[14]._u32[0] != 0 ? 3 : 1;                                /* loc_00009958 : loc_00009570 */
}

static __attribute__((noinline)) int fn_00009090_9458_nativo(spu_context* ctx)
{
    static volatile long n[2];
    const int r = fn_00009090_9458_corpo(ctx);
    gowa_edge_conta("9090/9458", n, r);
    return r;
}
#endif
