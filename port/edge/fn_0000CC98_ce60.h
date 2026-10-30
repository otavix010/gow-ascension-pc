/* GOWA (30/10, EDGE nativo, marco E3): o laço CE60 da CC98 do job 0x72DF80, um memmove na LS.
 *
 * De cima para baixo, em blocos de 128 bytes (B = r6, depois B - 128, ... até B = r23 + 128), cada bloco recebe os
 * bytes de 4 abaixo dele (out[B + i] = in[B + i - 4], pelo shufb com a constante 0x0C0D0E0F...): a faixa
 * [r23 + 128, r6 + 128) sobe 4 bytes, abrindo lugar para uma palavra. Descendo, o original nunca lê o que já
 * gravou, então é um memmove. Depois do laço a CC98 usa r15 (o qword em r23 + 128 lido antes de a última volta
 * gravar, ou seja, o conteúdo antigo) e r6 (= r23); os outros temporários do laço não são lidos.
 * GOWA_EDGE_NATIVO=0 desliga. Gancho antes do rótulo do laço, saltando para depois dele (scripts_edge/edge_nativo.py). */
#ifndef GOWA_FN_0000CC98_CE60_H
#define GOWA_FN_0000CC98_CE60_H
#include <stdlib.h>
#include <string.h>

#ifndef GOWA_EDGE_NATIVO_ON_DEF
#define GOWA_EDGE_NATIVO_ON_DEF
static int gowa_edge_nativo_on(void)
{
    static int on = -1;
    if (on < 0) { const char* e = getenv("GOWA_EDGE_NATIVO"); on = !(e && *e == '0'); }   /* 30/10 (E7): ligado por padrão; =0 desliga */
    return on;
}
#endif

static __attribute__((noinline)) int fn_0000CC98_ce60_nativo(spu_context* ctx)
{
    if (!gowa_edge_nativo_on()) return 0;
    u128* r = ctx->gpr;
    const uint32_t b0 = r[6]._u32[0], fim = r[23]._u32[0];
    /* o laço acaba quando r6 - 128 == r23 (comparação de 32 bits): só os casos com a distância em blocos inteiros */
    if (b0 <= fim || ((b0 - fim) & 127u) || (b0 - fim) > SPU_LS_SIZE) return 0;
    const uint32_t L = fim + 128u, H = b0 + 128u;
    if ((L & 15u) || L < 16u || H > SPU_LS_SIZE || (b0 & SPU_LS_MASK) != b0) return 0;
    /* a constante do shufb: os bytes 12..27 do par (o deslocamento de 4) */
    const uint32_t k[4] = {0x0C0D0E0Fu, 0x10111213u, 0x14151617u, 0x18191A1Bu};
    for (int i = 0; i < 4; i++) if (r[17]._u32[i] != k[i]) return 0;
    uint8_t* ls = SPU_LS_BASE(ctx);
    r[15] = spu_ls_read128(ctx, L);   /* o conteúdo antigo, antes do memmove */
    memmove(ls + L, ls + L - 4u, H - L);
    for (int i = 0; i < 4; i++) {
        r[6]._u32[i] -= b0 - fim;   /* r6 desce 128 por volta nas 4 faixas */
        r[56]._u32[i] = L;
    }
    return 1;
}
#endif
