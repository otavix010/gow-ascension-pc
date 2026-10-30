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
#include <immintrin.h>

#ifndef GOWA_EDGE_NATIVO_ON_DEF
#define GOWA_EDGE_NATIVO_ON_DEF
static int gowa_edge_nativo_on(void)
{
    static int on = -1;
    if (on < 0) { const char* e = getenv("GOWA_EDGE_NATIVO"); on = !(e && *e == '0'); }   /* 30/10 (E7): ligado por padrão; =0 desliga */
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
#if defined(__SSSE3__) || defined(__AVX2__)
    /* SIMD (30/10): as tabelas do próprio jogo, em bytes na ordem da SPU (sem troca de ordem). Por nibble (2
     * triângulos): resultado = shufb(P, N, T23[c]), com P = os 2 triângulos anteriores (o último nos bytes 6-11) e N =
     * o fluxo na posição atual; o fluxo anda T26[c][0] bytes (T26 são deslocamentos: conferido abaixo). */
    const uint32_t t23 = r[23]._u32[0], t26 = r[26]._u32[0];
    if ((t23 & 15u) || (t26 & 15u) || t23 + 256u > SPU_LS_SIZE || t26 + 256u > SPU_LS_SIZE) return 0;
    __m128i T[16];
    uint32_t adv[16];
    for (int c = 0; c < 16; c++) {
        T[c] = _mm_loadu_si128((const __m128i*)(ls + t23 + 16u * (uint32_t)c));
        const uint8_t* d = ls + t26 + 16u * (uint32_t)c;
        adv[c] = d[0];
        for (int b = 0; b < 16; b++) if (d[b] != (uint8_t)(d[0] + b)) return 0;
        for (int b = 0; b < 16; b++) if (ls[t23 + 16u * (uint32_t)c + (uint32_t)b] >= 0x20u) return 0;
    }
    __m128i P = _mm_setzero_si128();
    for (uint32_t v = 0; v < n; v++) {
        const int cur = (int)(v & 1u);
        cw[cur] = e3c4_u16(ls + ctl + 2u * v);
        const uint32_t al = pos & ~15u;
        memcpy(jan[cur], ls + al, 80);
        desl[cur] = pos - al;
        const uint32_t c = cw[cur], n3 = (uint32_t)__builtin_popcount(c & (c << 1) & 0xAAAAu);
        pos += 16u + 4u * n3;
        if (v == 0) continue;
        const int ant = cur ^ 1;
        uint32_t qo = desl[ant];
        uint8_t tmp[64];
        for (int j = 0; j < 4; j++) {
            const uint32_t k = (cw[ant] >> (12 - 4 * j)) & 15u;
            const __m128i N = _mm_loadu_si128((const __m128i*)(jan[ant] + qo));
            P = _mm_blendv_epi8(_mm_shuffle_epi8(P, T[k]), _mm_shuffle_epi8(N, T[k]), _mm_slli_epi16(T[k], 3));
            _mm_storeu_si128((__m128i*)(tmp + 12 * j), P);
            qo += adv[k];
        }
        memcpy(ls + out + 48u * (v - 1u), tmp, 48);
    }
    (void)p0; (void)p1; (void)p2;
    for (int i = 0; i < 4; i++) r[17]._u32[i] += 48u * blocos;
    return 1;
#endif
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
