/* bancada sintética dos nativos dos laços da 0x73E0 do job 0x72DF80 (75D8, 7670, 7828): o nativo (que faz as voltas
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
#include "../../port/edge/fn_000073E0_lacos.h"
#include "../../port/edge/ll_000073E0_000075D8.h"
#include "../../port/edge/ll_000073E0_00007670.h"
#include "../../port/edge/ll_000073E0_00007828.h"
#include "../../port/edge/fn_000073E0_83e8.h"
#include "../../port/edge/ll_000073E0_000083E8.h"

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

static int caso_75d8(spu_context* c, int junto)
{
    const uint32_t n = 2u + rnd() % 40u, t = 0x10000u + rnd() % 0x8000u, b = base(t, junto), off = (rnd() % 64u) * 16u;
    pequenos(c, b + off, 16u * (n + 2u), junto ? 0x800u : 0x40000u);
    u128 r8 = v4(rnd() % 0x800u, rnd() % 0x800u, rnd() % 0x800u, rnd() % 0x800u);
    c->gpr[8] = r8; c->gpr[9] = spu_rotqbyi(r8, 4); c->gpr[11] = spu_rotqbyi(r8, 8); c->gpr[12] = spu_rotqbyi(r8, 12);
    c->gpr[13] = vs(n); c->gpr[14] = vs(off); c->gpr[80] = vs(b); c->gpr[81] = vs(t);
    return 0;
}
static int caso_7670(spu_context* c, int junto)
{
    const uint32_t n = 2u + rnd() % 30u;
    const int32_t x = -(int32_t)(64u * n) + (int32_t)(rnd() % 64u) * ((rnd() & 1u) ? 1 : 0);
    const uint32_t t = 0x10000u + rnd() % 0x8000u;
    c->gpr[9] = c->gpr[10] = vs((uint32_t)x);
    const int regs[4] = { 19, 18, 17, 16 };
    for (int i = 0; i < 4; i++) {
        const uint32_t a = base(t, junto) + 64u * n;   /* a + off >= base */
        c->gpr[regs[i]] = vs(a);
        pequenos(c, a + (uint32_t)x, 64u * (n + 1u), junto ? 0x100u : 0x40000u);
    }
    c->gpr[11] = v4(rnd() % 0x400u, rnd(), rnd(), rnd());
    c->gpr[81] = vs(t);
    return 0;
}
static int caso_7828(spu_context* c, int junto)
{
    const uint32_t n = 2u + rnd() % 30u;
    const int32_t x = -(int32_t)(64u * n);
    c->gpr[8] = v4(0, 0, 0, 0xFFFFFFFFu);
    c->gpr[6] = c->gpr[7] = vs((uint32_t)x);
    const uint32_t perto = 0x10000u + rnd() % 0x8000u;
    const int lr[4] = { 20, 19, 14, 13 }, gr[4] = { 23, 24, 22, 21 };
    for (int i = 0; i < 4; i++) {
        c->gpr[lr[i]] = vs(base(perto, junto) + 64u * n);
        c->gpr[gr[i]] = vs(base(perto, junto) + 64u * n);
    }
    return 0;
}

static int caso_83e8(spu_context* c, int junto)
{
    /* o w (4 últimos bytes) de cada quadword de A sobre o de B, no lugar; bloco 0 nos registradores */
    const int32_t r17 = (int32_t)(rnd() % 200u) - 8;
    const uint32_t J = r17 > 8 ? (uint32_t)(r17 + 7) / 8u : 1u;
    const uint32_t B0 = 0x10000u + (rnd() % 0x800u) * 16u;
    const uint32_t A0 = junto ? (B0 + (rnd() % (256u * J + 512u)) * 16u - 256u * 16u) & 0x3FFF0u : 0x30000u + (rnd() % 0x800u) * 16u;
    c->gpr[13] = v4(0, 0, 0, 0xFFFFFFFFu);
    c->gpr[17] = vs((uint32_t)r17);
    c->gpr[14] = vs(B0); c->gpr[15] = vs(B0 + 128u);
    c->gpr[84] = vs(A0); c->gpr[16] = vs(A0 + 128u);
    c->gpr[81] = vs(A0 - 128u); c->gpr[103] = vs(B0);
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
    roda("75D8", caso_75d8, fn_000073E0_75d8_nativo, ll_000073E0_000075D8, casos, 0);
    roda("7670", caso_7670, fn_000073E0_7670_nativo, ll_000073E0_00007670, casos, 0);
    roda("7828", caso_7828, fn_000073E0_7828_nativo, ll_000073E0_00007828, casos, 0);
    roda("83E8", caso_83e8, fn_000073E0_83e8_nativo, ll_000073E0_000083E8, casos, 1);
    return 0;
}
