/* GOWA (30/10, EDGE nativo): o laço 0x300D0 do plugin 0x75C570 (posições float4 -> float3 empacotado).
 *
 * A volta i grava em out + 24i os xyz dos dois vértices do par i (in + 32i e in + 32i + 16), com três gravações
 * alinhadas de 16 bytes que mesclam o começo do bloco (r17, lido da LS) e deixam lixo depois; o laço lê dois pares
 * à frente. O nativo grava os vértices das voltas 0..n-2 (out + 12k <- in + 16k, 12 bytes, a LS é big-endian dos
 * dois lados) e monta os registradores vivos da última volta, que o laço traduzido roda: o lixo do fim e a mescla
 * saem exatos. Conferido pelo oráculo (scripts_edge/pl75c_modelo.py: 12/12 fotos com a LS inteira; só o
 * temporário r39 muda). Só o formato visto nas fotos (os controles de shufb conferidos abaixo); fora disso devolve
 * 0 e o laço faz tudo. GOWA_EDGE_NATIVO=0 desliga. Gancho 'entrada_segue' (edge_nativo.py): devolvendo 1, o
 * traduzido recarrega os registradores e segue no laço. */
#ifndef GOWA_PL_0075C570_H
#define GOWA_PL_0075C570_H
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

static inline int pl75c_igual(u128 a, uint32_t w0, uint32_t w1, uint32_t w2, uint32_t w3)
{
    return a._u32[0] == w0 && a._u32[1] == w1 && a._u32[2] == w2 && a._u32[3] == w3;
}
static inline int pl75c_mesmo(u128 a, u128 b) { return memcmp(&a, &b, 16) == 0; }

static __attribute__((noinline)) int pl_0075C570_nativo(spu_context* ctx)
{
    if (!gowa_edge_nativo_on()) return 0;
    u128* g = ctx->gpr;
    const uint32_t tot = 0u - g[10]._u32[0];
    if (!tot || tot % 24u || tot > 0x20000u || g[14]._u32[0] != g[10]._u32[0]) return 0;
    if (!pl75c_igual(g[8], 0x10111213u, 0x14151617u, 0x18191A1Bu, 0x20212223u) ||
        !pl75c_igual(g[24], 0x24252627u, 0x28292A2Bu, 0x18191A1Bu, 0x1C1D1E1Fu) ||
        !pl75c_igual(g[25], 0x03030303u, 0x03030303u, 0x03030303u, 0x03030303u) ||
        !pl75c_igual(g[26], 0xFFFFFFFFu, 0xFFFFFFFFu, 0xFFFFFFFFu, 0) ||
        !pl75c_igual(g[18], 0x20212223u, 0x24252627u, 0x28292A2Bu, 0x2C2D2E2Fu))
        return 0;
    const uint32_t fim = g[23]._u32[0];
    if (g[21]._u32[0] != fim || g[20]._u32[0] != fim + 16u || g[19]._u32[0] != fim + 32u || g[22]._u32[0] != fim + 24u) return 0;
    const uint32_t n = tot / 24u, J = n - 1u;
    if (n < 2u || (g[12]._u32[0] & 15u)) return 0;
    const uint32_t out = fim - tot, ent = g[12]._u32[0] - 96u;
    if (out > SPU_LS_SIZE || out + tot + 48u > SPU_LS_SIZE || ent > SPU_LS_SIZE || ent + 32u * (n + 2u) > SPU_LS_SIZE) return 0;
    if (out < ent + 32u * (n + 2u) && ent < out + tot + 48u) return 0;
    uint8_t* ls = SPU_LS_BASE(ctx);
    /* os registradores da entrada têm de ser os do par 0 lido de ent (confere o ponteiro de entrada) */
    {
        const u128 A = spu_ls_read128(ctx, ent), B = spu_ls_read128(ctx, ent + 16u);
        if (g[9]._u32[0] != A._u32[0] || g[9]._u32[1] != A._u32[1] || g[9]._u32[2] != A._u32[2] || g[9]._u32[3] != B._u32[0] ||
            g[11]._u32[0] != B._u32[1] || g[11]._u32[1] != B._u32[2])
            return 0;
        if (!pl75c_mesmo(spu_ls_read128(ctx, ent + 32u), g[16]) || !pl75c_mesmo(spu_ls_read128(ctx, ent + 48u), g[13]) ||
            !pl75c_mesmo(spu_ls_read128(ctx, ent + 64u), g[2]) || !pl75c_mesmo(spu_ls_read128(ctx, ent + 80u), g[15]))
            return 0;
    }
    /* vértices 0..2J-1: 16 bytes gravados por vez (os 4 a mais são cobertos pelo vértice seguinte), o último com 12 */
    const uint32_t nv = 2u * J;
    for (uint32_t v = 0; v + 1u < nv; v++)
        _mm_storeu_si128((__m128i*)(ls + out + 12u * v), _mm_loadu_si128((const __m128i*)(ls + ent + 16u * v)));
    memcpy(ls + out + 12u * (nv - 1u), ls + ent + 16u * (nv - 1u), 12);
    /* os vivos da entrada da volta J */
    const uint32_t a = ent + 32u * J, b = a + 16u;
    {
        const u128 A = spu_ls_read128(ctx, a), B = spu_ls_read128(ctx, b);
        g[9]._u32[0] = A._u32[0]; g[9]._u32[1] = A._u32[1]; g[9]._u32[2] = A._u32[2]; g[9]._u32[3] = B._u32[0];
        g[11]._u32[0] = B._u32[1]; g[11]._u32[1] = B._u32[2];
    }
    g[16] = spu_ls_read128(ctx, a + 32u); g[13] = spu_ls_read128(ctx, a + 48u);
    g[2] = spu_ls_read128(ctx, a + 64u); g[15] = spu_ls_read128(ctx, a + 80u);
    for (int i = 0; i < 4; i++) { g[10]._u32[i] += 24u * J; g[14]._u32[i] += 24u * J; g[12]._u32[i] += 32u * J; }
    g[17] = spu_ls_read128(ctx, out + 24u * J);
    return 1;
}
#endif
