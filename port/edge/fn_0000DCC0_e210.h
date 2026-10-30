/* GOWA (30/10, EDGE nativo, marco E3): a descompressão dos índices por máscara da DCC0 (laço E210) do job 0x72DF80.
 *
 * N = r19 voltas; cada uma lê uma máscara u16 em r25+r23 (r23 anda 2 por volta) e monta 16 índices u16: do bit mais
 * alto para o mais baixo, 1 = o próximo índice explícito do fluxo (u16 em r8, anda 2 bytes), 0 = o próximo índice
 * sequencial (contador = halfword 0 de r21, que traz base+0..7). Em software pipeline: grava N-1 blocos de 32 bytes em
 * r17 (a volta 0 grava um bloco provisório no lugar do bloco 0, que a volta 1 sobrescreve). Conferido contra o oráculo
 * (scripts_edge/e210_modelo.py, 16/16 fotos: LS, r10, r8 e o contador). Depois do laço a DCC0 usa só r10 (= o fim da
 * saída) dos registradores que ele muda; os outros temporários ficam diferentes (GOWA_CONF_REGS_ABI=1).
 *
 * SIMD: por byte da máscara (8 índices), uma tabela de pshufb puxa os explícitos dos 16 bytes do fluxo, outra soma
 * ao contador o número de zeros antes de cada posição, e uma terceira escolhe entre os dois. GOWA_EDGE_NATIVO=0
 * desliga. Gancho antes do vk do laço, saltando para depois dele (scripts_edge/edge_nativo.py). */
#ifndef GOWA_FN_0000DCC0_E210_H
#define GOWA_FN_0000DCC0_E210_H
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

#if defined(__SSSE3__) || defined(__AVX2__)
/* por byte b da máscara (bit 7 = o 1o índice): T = pshufb que leva o k-ésimo explícito (bytes 2k, 2k+1 do fluxo, ordem
 * da SPU) à posição i; Z = zeros antes da posição i (u16 do host); S = 0xFFFF nas posições explícitas */
static __m128i e210_T[256], e210_Z[256], e210_S[256];
static void e210_tabelas(void)
{
    for (int b = 0; b < 256; b++) {
        uint8_t t[16]; uint16_t z[8], sl[8];
        int uns = 0, zeros = 0;
        for (int i = 0; i < 8; i++) {
            const int bit = (b >> (7 - i)) & 1;
            if (bit) { t[2 * i] = (uint8_t)(2 * uns); t[2 * i + 1] = (uint8_t)(2 * uns + 1); z[i] = 0; sl[i] = 0xFFFF; uns++; }
            else { t[2 * i] = 0x80; t[2 * i + 1] = 0x80; z[i] = (uint16_t)zeros; sl[i] = 0; zeros++; }
        }
        e210_T[b] = _mm_loadu_si128((const __m128i*)t);
        e210_Z[b] = _mm_loadu_si128((const __m128i*)z);
        e210_S[b] = _mm_loadu_si128((const __m128i*)sl);
    }
}
#endif

static __attribute__((noinline)) int fn_0000DCC0_e210_nativo(spu_context* ctx)
{
#if defined(__SSSE3__) || defined(__AVX2__)
    if (!gowa_edge_nativo_on()) return 0;
    static int pronto = 0;
    if (!pronto) { e210_tabelas(); pronto = 1; }
    u128* r = ctx->gpr;
    const uint32_t n = r[19]._u32[0];
    if (n < 2u || n > 0x4000u) return 0;
    /* o prólogo do software pipeline: os acumuladores da volta anterior zerados, como em todas as fotos */
    if (r[26]._u32[0] != 0u || r[13]._u32[0] != 0u || r[20]._u32[0] != 0u) return 0;
    const uint32_t r25 = r[25]._u32[0], r23 = r[23]._u32[0];
    if ((r25 + r23) & 1u) return 0;   /* máscara em halfword (nunca atravessa o qword) */
    uint32_t mk = (r25 + r23) & SPU_LS_MASK, fx = r[8]._u32[0] & SPU_LS_MASK;
    const uint32_t out = r[17]._u32[0] & SPU_LS_MASK;
    const uint32_t blocos = n - 1u;
    if (mk + 2u * n > SPU_LS_SIZE || out + 32u * blocos > SPU_LS_SIZE || fx + 32u * blocos + 32u > SPU_LS_SIZE) return 0;
    /* o contador: halfwords base+0..7 em r21 */
    uint32_t base = r[21]._u32[0] >> 16;
    for (int i = 0; i < 4; i++)
        if (r[21]._u32[i] != ((((base + 2u * (uint32_t)i) & 0xFFFFu) << 16) | ((base + 2u * (uint32_t)i + 1u) & 0xFFFFu))) return 0;
    uint8_t* ls = SPU_LS_BASE(ctx);
    const __m128i bsw = _mm_setr_epi8(1, 0, 3, 2, 5, 4, 7, 6, 9, 8, 11, 10, 13, 12, 15, 14);
    /* A saída pode alcançar o fluxo? (no lugar): o original lê a máscara e o fluxo da volta v antes de gravar o bloco
     * v-1; aqui cada bloco lê o fluxo e só depois grava. Se a saída cruzar o fluxo ou as máscaras, deixa o traduzido. */
    if (out < fx + 32u * blocos + 32u && fx < out + 32u * blocos) return 0;
    if (out < mk + 2u * n && mk < out + 32u * blocos) return 0;
    for (uint32_t v = 0; v < blocos; v++) {
        const uint32_t m16 = ((uint32_t)ls[mk + 2u * v] << 8) | ls[mk + 2u * v + 1u];
        const uint32_t hi = m16 >> 8, lo = m16 & 0xFFu;
        const uint32_t nhi = (uint32_t)__builtin_popcount(hi), nlo = (uint32_t)__builtin_popcount(lo);
        const __m128i f0 = _mm_loadu_si128((const __m128i*)(ls + fx));
        const __m128i f1 = _mm_loadu_si128((const __m128i*)(ls + fx + 2u * nhi));
        const __m128i c0 = _mm_add_epi16(_mm_set1_epi16((short)base), e210_Z[hi]);
        const __m128i c1 = _mm_add_epi16(_mm_set1_epi16((short)(base + 8u - nhi)), e210_Z[lo]);
        const __m128i o0 = _mm_blendv_epi8(_mm_shuffle_epi8(c0, bsw), _mm_shuffle_epi8(f0, e210_T[hi]), e210_S[hi]);
        const __m128i o1 = _mm_blendv_epi8(_mm_shuffle_epi8(c1, bsw), _mm_shuffle_epi8(f1, e210_T[lo]), e210_S[lo]);
        _mm_storeu_si128((__m128i*)(ls + out + 32u * v), o0);
        _mm_storeu_si128((__m128i*)(ls + out + 32u * v + 16u), o1);
        fx += 2u * (nhi + nlo);
        base += 16u - nhi - nlo;
    }
    /* o estado do fim que a DCC0 usa (r10) e os ponteiros do laço */
    for (int i = 0; i < 4; i++) {
        r[10]._u32[i] = r[17]._u32[i] + 32u * blocos;
        r[17]._u32[i] = r[10]._u32[i];
        r[8]._u32[i] = fx;
        r[23]._u32[i] = r23 + 2u * n;
        r[21]._u32[i] = (((base + 2u * (uint32_t)i) & 0xFFFFu) << 16) | ((base + 2u * (uint32_t)i + 1u) & 0xFFFFu);
        r[19]._u32[i] = 0u;
        r[26]._u32[i] = 32u;
    }
    return 1;
#else
    (void)ctx;
    return 0;
#endif
}
#endif
