/* GOWA (02/11, EDGE nativo, marco E4): o laço CDBC da 0xCC98 do job 0x72DF80 -- varre uma lista de entradas de
 * tamanho variável (palavra w no começo; tamanho = (w >> 16) & 0x1FFC, mais 4 bytes de cabeçalho) até o fim r20.
 * Cada volta continua se a entrada anterior não cruzou uma divisa de 128 bytes (r18 == r3 na faixa 0); senão o laço
 * cai na correção (grava o cabeçalho, desloca o resto com o CE60) e volta ao rótulo. O nativo roda logo depois do
 * rótulo e faz as voltas que continuam, com as mesmas operações da SPU sobre os registradores de estado (r3, r5, r9,
 * r18, r25, r26; quadword inteiro, faixa a faixa); para antes da volta que sai (fim da lista ou divisa cruzada), que
 * o traduzido faz -- os temporários dela saem como no original. Não grava a LS. GOWA_EDGE_NATIVO=0 desliga.
 * Gancho 'depois_rotulo' em loc_0000CDBC (edge_nativo.py). */
#ifndef GOWA_FN_0000CC98_CDBC_H
#define GOWA_FN_0000CC98_CDBC_H
#include <stdlib.h>
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

static int fn_0000CC98_cdbc_corpo(spu_context* ctx)
{
    if (!gowa_edge_nativo_on()) return 0;
    u128* g = ctx->gpr;
    u128 r3 = g[3], r5 = g[5], r9 = g[9], r18 = g[18], r25 = g[25], r26 = g[26];
    const u128 r32 = g[32];
    const uint32_t fim = g[20]._u32[0];
    uint32_t voltas = 0;
    for (;;) {
        if (!(fim > r5._u32[0])) break;              /* fim da lista: a volta que sai fica com o traduzido */
        if (r18._u32[0] != r3._u32[0]) break;        /* a entrada anterior cruzou a divisa: idem (correção) */
        const u128 q = spu_ls_read128(ctx, r9._u32[0]);
        r25 = spu_rotqby(q, r9);
        r26 = spu_and(r32, spu_rotqmbyi(r25, -2));
        const u128 r37 = spu_a(r9, r26);
        r18 = spu_andi(r9, -128);
        r5 = r9;
        r3 = spu_andi(r37, -128);
        r9 = spu_ai(r37, 4);
        if (++voltas > 1u << 16) return 0;           /* lista corrompida: nada feito (o estado ainda não foi gravado) */
    }
    if (!voltas) return 0;
    g[3] = r3; g[5] = r5; g[9] = r9; g[18] = r18; g[25] = r25; g[26] = r26;
    return 1;
}

static __attribute__((noinline)) int fn_0000CC98_cdbc_nativo(spu_context* ctx)
{
    static volatile long n[2];
    const int r = fn_0000CC98_cdbc_corpo(ctx);
    gowa_edge_conta("CC98/CDBC", n, r);
    return r;
}
#endif
