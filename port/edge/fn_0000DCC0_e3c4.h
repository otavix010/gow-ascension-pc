/* GOWA (30/10, EDGE nativo, marco E3): a reconstrução dos triângulos da DCC0 (laço E3C4) do job 0x72DF80.
 *
 * Índices comprimidos: 1 palavra de controle u16 por volta (em r29-2, 2 bytes por volta) = 8 triângulos x 2 bits,
 * do bit mais alto para o mais baixo; p = o triângulo anterior (começa em 0,0,0):
 *     c=0 -> (p0, p2, novo)   c=1 -> (p2, p1, novo)   c=2 -> (p1, p0, novo)   c=3 -> (novo, novo', novo'')
 * com os índices novos (u16) em sequência a partir de r20. Cada volta grava 8 triângulos (48 bytes) em r17; o laço
 * original, em software pipeline, roda N = r21 voltas e grava só N-1 blocos (a última volta calcula e não grava), e
 * r17 anda 48 por bloco gravado. Conferido contra o oráculo (scripts_edge/e3c4_modelo.py, 16/16 fotos: os N-1 blocos
 * e nada mais na LS). Depois do laço a DCC0 só usa r17 (e r63) e chama a B600 (lê r1, r3, r4): os outros
 * temporários ficam diferentes (GOWA_CONF_REGS_ABI=1). GOWA_EDGE_NATIVO=1 liga. Gancho antes do rótulo do laço,
 * saltando para depois dele (scripts_edge/edge_nativo.py). */
#ifndef GOWA_FN_0000DCC0_E3C4_H
#define GOWA_FN_0000DCC0_E3C4_H
#include <stdlib.h>
#include <string.h>

#ifndef GOWA_EDGE_NATIVO_ON_DEF
#define GOWA_EDGE_NATIVO_ON_DEF
static int gowa_edge_nativo_on(void)
{
    static int on = -1;
    if (on < 0) { const char* e = getenv("GOWA_EDGE_NATIVO"); on = e && *e == '1'; }
    return on;
}
#endif

static inline uint32_t e3c4_u16(const uint8_t* p) { return ((uint32_t)p[0] << 8) | p[1]; }

static __attribute__((noinline)) int fn_0000DCC0_e3c4_nativo(spu_context* ctx)
{
    if (!gowa_edge_nativo_on()) return 0;
    u128* r = ctx->gpr;
    const uint32_t n = r[21]._u32[0];
    if (n == 0u || n > 0x4000u || r[27]._u32[0] != 0xFFFFFFFEu) return 0;
    const uint32_t ctl = r[29]._u32[0] - 2u, fluxo = r[20]._u32[0], out = r[17]._u32[0];
    const uint32_t blocos = n - 1u;
    if (ctl + 2u * n + 16u > SPU_LS_SIZE || fluxo + 48u * n + 80u > SPU_LS_SIZE || out + 48u * blocos > SPU_LS_SIZE) return 0;
    /* A saída pode ser o próprio fluxo (no lugar). A ordem do original: na volta v ele lê a palavra de controle v e a
     * janela de 64 bytes do fluxo a partir de alinha16(pos_v), e só depois grava o bloco v-1. Aqui igual: instantâneo
     * da volta v, depois o bloco v-1 a partir do instantâneo da volta v-1. */
    uint8_t* ls = SPU_LS_BASE(ctx);
    uint8_t jan[2][80];
    uint32_t cw[2], desl[2];
    uint32_t p0 = 0, p1 = 0, p2 = 0, pos = fluxo;
    for (uint32_t v = 0; v < n; v++) {
        const int cur = (int)(v & 1u);
        cw[cur] = e3c4_u16(ls + ctl + 2u * v);
        const uint32_t al = pos & ~15u;
        memcpy(jan[cur], ls + al, 64);
        desl[cur] = pos - al;
        /* o avanço do fluxo deste bloco: 16 + 4 por triângulo novo (c=3) */
        const uint32_t c = cw[cur], n3 = (uint32_t)__builtin_popcount(c & (c << 1) & 0xAAAAu);
        pos += 16u + 4u * n3;
        if (v == 0) continue;
        const int ant = cur ^ 1;
        const uint8_t* q = jan[ant] + desl[ant];
        const uint32_t ca = cw[ant];
        uint8_t* o = ls + out + 48u * (v - 1u);
        for (int t = 0; t < 8; t++) {
            uint32_t a, b, d;
            switch ((ca >> (14 - 2 * t)) & 3u) {
            case 0: a = p0; b = p2; d = e3c4_u16(q); q += 2; break;
            case 1: a = p2; b = p1; d = e3c4_u16(q); q += 2; break;
            case 2: a = p1; b = p0; d = e3c4_u16(q); q += 2; break;
            default: a = e3c4_u16(q); b = e3c4_u16(q + 2); d = e3c4_u16(q + 4); q += 6; break;
            }
            o[0] = (uint8_t)(a >> 8); o[1] = (uint8_t)a; o[2] = (uint8_t)(b >> 8); o[3] = (uint8_t)b;
            o[4] = (uint8_t)(d >> 8); o[5] = (uint8_t)d; o += 6;
            p0 = a; p1 = b; p2 = d;
        }
    }
    for (int i = 0; i < 4; i++) r[17]._u32[i] += 48u * blocos;   /* r17 += r5 (48 nas 4 faixas) por bloco gravado */
    return 1;
}
#endif
