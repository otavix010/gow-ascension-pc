/* bancada sintética dos nativos dos laços E134 e E30C da 0xDCC0 do job 0x72DF80 (feita do bj_73e0.c): o nativo (que faz as voltas
 * 0..n-2) + o laço traduzido (ll_*, cópia bit a bit do traduzido com registradores locais) contra só o laço traduzido,
 * sobre as mesmas LS e registradores. Casos com os fluxos lidos e gravados próximos (sobrepostos) e espalhados.
 * uso: bj_73e0 [casos] (BANC_V=1 mostra cada diferença) */
#include "../../port/spu/gowapl_0075AA40/spu_recomp.h"
SPU_THREAD_LOCAL void (*g_spu_trampoline_fn)(spu_context*);
SPU_THREAD_LOCAL int32_t g_spu_hot_reloc;
void spu_indirect_branch(spu_context* c) { (void)c; }
void spu_register_function(uint32_t a, void (*f)(spu_context*)) { (void)a; (void)f; }
#include "spu_helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <immintrin.h>
#include "../../port/edge/fn_0000DCC0_e134.h"
#include "../../port/edge/ll_0000DCC0_0000E134.h"
#include "../../port/edge/fn_0000DCC0_e30c.h"
#include "../../port/edge/ll_0000DCC0_0000E30C.h"

static spu_context A, B;
static uint32_t rnd_s = 1;
static uint32_t rnd(void) { rnd_s = rnd_s * 1103515245u + 12345u; return (rnd_s >> 8) ^ (rnd_s << 13); }
static void be32(uint8_t* p, uint32_t v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }
static u128 v4(uint32_t a, uint32_t b, uint32_t c, uint32_t d) { u128 r; r._u32[0] = a; r._u32[1] = b; r._u32[2] = c; r._u32[3] = d; return r; }
static u128 vr(void) { return v4(rnd(), rnd(), rnd(), rnd()); }
static u128 vs(uint32_t a) { return v4(a, a, a, a); }

static void zera(spu_context* c)
{
    memset(c, 0, sizeof *c);
    c->ls = c->ls_store;
    for (uint32_t i = 0; i < SPU_LS_SIZE; i += 4) { const uint32_t v = rnd(); memcpy(c->ls_store + i, &v, 4); }
    for (int r = 0; r < 128; r++) c->gpr[r] = vr();
    c->host_depth = 1;
}
/* palavras pequenas (deslocamentos curtos, múltiplos de 16 ou não) numa faixa da LS */
static void pequenos(spu_context* c, uint32_t a, uint32_t n, uint32_t lim)
{
    for (uint32_t i = 0; i < n; i += 4) be32(c->ls_store + ((a + i) & (SPU_LS_SIZE - 4)), rnd() % lim);
}
/* um endereço-base: perto de "perto" (sobreposição) ou longe */
static uint32_t base(uint32_t perto, int junto)
{
    if (junto) return (perto + (rnd() % 1024u) - 512u) & 0x3FFF0u & ~(rnd() & 1u ? 0u : 0xFu);
    return 0x2000u + rnd() % 0x38000u;
}

static int caso_e134(spu_context* c, int junto)
{
    (void)junto;
    const uint32_t voltas = 2u + rnd() % 200u, base = 0x8000u + (rnd() % 0x1000u) * 16u, k = rnd() & 0xFFFFu;
    const int zeros[] = { 3, 13, 9, 10, 18, 19, 20, 21 };
    for (int i = 0; i < 8; i++) c->gpr[zeros[i]] = vs(0);
    c->gpr[12] = vs(voltas); c->gpr[17] = vs(base); c->gpr[16] = vs(base + 16u); c->gpr[15] = vs(base + 32u); c->gpr[14] = vs(base + 48u);
    c->gpr[11] = vs(k | (k << 16));
    return 0;
}
static int caso_e30c(spu_context* c, int junto)
{
    const uint32_t len = rnd() % 4u == 0 ? rnd() % 16u : rnd() % 3000u;
    const uint32_t dst = 0x10000u + (rnd() % 0x800u) * 16u;
    const uint32_t src = junto ? dst + 16u + rnd() % 64u : 0x20000u + rnd() % 0x8000u;   /* src = r6 + 16 */
    c->gpr[8] = vs(0); c->gpr[3] = vs(0); c->gpr[10] = vs(0); c->gpr[11] = vs(0);
    c->gpr[9] = vs(0xFFFFFFF0u);
    c->gpr[61] = vs(len);
    c->gpr[7] = vs(dst); c->gpr[6] = vs(src - 16u);
    c->gpr[53] = v4(0x00010203u, 0x04050607u, 0x08090A0Bu, 0x0C0D0E0Fu);
    c->gpr[58] = vs(0x03030303u);
    return 0;
}

static int compara(const char* nome, int k)
{
    int d = 0;
    for (int r = 0; r < 128; r++)
        if (memcmp(&A.gpr[r], &B.gpr[r], 16)) {
            if (getenv("BANC_V")) printf("%s caso %d: r%d %08X %08X %08X %08X x %08X %08X %08X %08X\n", nome, k, r, A.gpr[r]._u32[0],
                                         A.gpr[r]._u32[1], A.gpr[r]._u32[2], A.gpr[r]._u32[3], B.gpr[r]._u32[0], B.gpr[r]._u32[1],
                                         B.gpr[r]._u32[2], B.gpr[r]._u32[3]);
            d++;
        }
    for (uint32_t a = 0; a < SPU_LS_SIZE; a += 16)
        if (memcmp(A.ls_store + a, B.ls_store + a, 16)) {
            if (getenv("BANC_V") && d < 8) printf("%s caso %d: LS 0x%05X\n", nome, k, a);
            d++;
        }
    return d;
}

typedef int (*caso_fn)(spu_context*, int);
/* ate_fim: o nativo faz o laço inteiro (o gancho salta para o fim); senão faz as voltas 0..n-2 e o traduzido a última */
static void roda(const char* nome, caso_fn monta, int (*nat)(spu_context*), int (*ll)(spu_context*), int casos, int ate_fim)
{
    int feitos = 0, recusas = 0, dif = 0;
    for (int k = 0; k < casos; k++) {
        rnd_s = 0x9E3779B9u * (uint32_t)(k + 1) ^ (uint32_t)(uintptr_t)nome;
        zera(&A);
        monta(&A, k & 1);
        memcpy(&B, &A, sizeof A); B.ls = B.ls_store;
        if (!nat(&A)) { recusas++; continue; }
        if (!ate_fim) ll(&A);
        ll(&B);
        feitos++;
        if (compara(nome, k)) dif++;
    }
    printf("%s: %d feitos, %d recusas, %d com diferença\n", nome, feitos, recusas, dif);
}

int main(int argc, char** argv)
{
    const int casos = argc > 1 ? atoi(argv[1]) : 400;
    _putenv("GOWA_LACO_LOCAL=1");
#ifdef SPU_SX_RZ
    _mm_setcsr((_mm_getcsr() & ~0xE040u) | 0x6000u | 0x0040u);
#else
    _mm_setcsr((_mm_getcsr() & ~0xE040u) | 0x0040u);
#endif
    roda("E134", caso_e134, fn_0000DCC0_e134_nativo, ll_0000DCC0_0000E134, casos, 1);
    roda("E30C", caso_e30c, fn_0000DCC0_e30c_nativo, ll_0000DCC0_0000E30C, casos, 1);
    return 0;
}
