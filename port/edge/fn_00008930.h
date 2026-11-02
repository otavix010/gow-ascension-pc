/* GOWA (30/10, EDGE nativo, marco E3): o laço 8B90 da 0x8930 do job 0x72DF80 (compactação dos registros de 8 bytes).
 *
 * Para cada entrada i < cnt (cnt = byte 0 do quadword em r35), e = LS[r33 + i]; e == 0xFF pula. Senão os bytes 8..15
 * do registro (origem r27 = r35 + 8i) vão para o destino r28, destino+8 recebe o byte baixo de r32 e destino+13 o e;
 * o destino só avança (+8, r34 + 1, r32 + byte 12 do registro) se não for (byte 11 - 27) & 0xFF <= 3 com o bit 4 de
 * r36 desligado. As outras 3 faixas de r28/r32/r34 seguem a mesma regra com os bytes 11+4w e 12+4w do quadword
 * (rotqby): reproduzidas aqui. Modelo: scripts_edge/f8930_modelo.py (300/300 contra o oráculo).
 * O nativo faz as voltas até antes da última entrada que não pula; o laço traduzido faz o resto, e os temporários
 * dele saem como no original. Só quando o destino não encosta na origem, no cnt nem nos bytes e (senão devolve 0
 * e o traduzido faz tudo). GOWA_EDGE_NATIVO=0 desliga. Gancho antes do rótulo do laço (scripts_edge/edge_nativo.py). */
#ifndef GOWA_FN_00008930_H
#define GOWA_FN_00008930_H
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

static int f8930_encosta(uint32_t a0, uint32_t a1, uint32_t b0, uint32_t b1) { return a0 < b1 && b0 < a1; }

static __attribute__((noinline)) int fn_00008930_nativo(spu_context* ctx)
{
    if (!gowa_edge_nativo_on()) return 0;
    u128* r = ctx->gpr;
    uint8_t* ls = SPU_LS_BASE(ctx);
    const uint32_t r35 = r[35]._u32[0], r33 = r[33]._u32[0];
    if (r35 > SPU_LS_SIZE - 16u || r33 > SPU_LS_SIZE - 256u) return 0;
    const uint32_t cnt = ls[r35 & ~15u];
    /* a última entrada que não pula: o traduzido faz dela em diante */
    uint32_t j = 0;
    for (uint32_t i = 0; i < cnt; i++) if (ls[r33 + i] != 0xFF) j = i;
    if (j == 0) return 0;
    /* o estado de entrada do laço (o que o trecho antes dele deixa) */
    for (int w = 0; w < 4; w++)
        if (r[29]._u32[w] != 0u || r[32]._u32[w] != 0u || r[34]._u32[w] != 0u || r[27]._u32[w] != r[35]._u32[w]) return 0;
    uint32_t d[4], s32[4], s34[4], z36[4];
    for (int w = 0; w < 4; w++) { d[w] = r[28]._u32[w]; s32[w] = 0; s34[w] = 0; z36[w] = (r[36]._u32[w] & 4u) == 0; }
    /* o destino (no máximo 8 por volta, mais o quadword) longe da origem, do cnt e dos bytes e */
    const uint32_t d0 = d[0] & ~15u, d1 = d[0] + 8u * j + 32u;
    const uint32_t o0 = r35 & ~15u, o1 = r35 + 8u * cnt + 32u;
    if (d1 > SPU_LS_SIZE || o1 > SPU_LS_SIZE || f8930_encosta(d0, d1, o0, o1) || f8930_encosta(d0, d1, r33 & ~15u, r33 + 272u))
        return 0;
    uint32_t src = r35;
    for (uint32_t i = 0; i < j; i++, src += 8u) {
        const uint8_t e = ls[r33 + i];
        if (e == 0xFF) continue;
        const uint32_t dst = d[0];
        memcpy(ls + dst + 8u, ls + src + 8u, 8);
        ls[dst + 8u] = (uint8_t)s32[0];
        ls[dst + 13u] = e;
        const uint8_t* q11 = ls + ((src + 11u) & ~15u);
        const uint8_t* q12 = ls + ((src + 12u) & ~15u);
        for (int w = 0; w < 4; w++) {
            const uint8_t tipo = (uint8_t)(q11[(src + 11u + 4u * (uint32_t)w) & 15u] - 27u);
            if (tipo <= 3u && z36[w]) continue;   /* não avança */
            d[w] += 8u; s34[w] += 1u; s32[w] += q12[(src + 12u + 4u * (uint32_t)w) & 15u];
        }
    }
    for (int w = 0; w < 4; w++) {
        r[29]._u32[w] = j; r[27]._u32[w] += 8u * j;
        r[28]._u32[w] = d[w]; r[32]._u32[w] = s32[w]; r[34]._u32[w] = s34[w];
    }
    return 1;
}
#endif
