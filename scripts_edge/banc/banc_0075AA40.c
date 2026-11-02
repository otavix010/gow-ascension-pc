#include "../../port/spu/gowapl_0075AA40/limpo_banc.c"
#include "../../port/edge/pl_0075AA40.h"
#define PL_ENTRADA gowapl_0075AA40_spu_func_00030000
#define PL_LACO PL_ENTRADA
#define PL_NA_ENTRADA 1
#define PL_NATIVO pl_0075AA40_nativo
#define PL_IMG "pl_0075AA40.bin"
#include "banc_comum.h"
static void monta(spu_context* c, uint32_t s)
{
    (void)s;
    uint8_t* ls = c->ls_store;
    const uint32_t r3 = 0x10000, dst = 0x20000, n = 1 + rnd() % 300u;
    const uint32_t f1 = 0x4000, f2 = 0x9000;
    memset(ls + r3 + 0x160, 0, 16);
    const uint32_t k1 = rnd() % 8u, k2 = (k1 + 1 + rnd() % 7u) % 8u;
    for (uint32_t i = 0; i < 8; i++) if (i != k1 && i != k2) ls[r3 + 0x160 + i] = (uint8_t)(20 + i);
    ls[r3 + 0x160 + k1] = 1; ls[r3 + 0x160 + k2] = 2;
    be32(ls + r3 + 0x10 + 4 * k1, f1); be32(ls + r3 + 0x10 + 4 * k2, f2);
    for (uint32_t i = 0; i < 4 * (n + 8); i++) { be32(ls + f1 + 4 * i, fvert()); be32(ls + f2 + 4 * i, fvert()); }
    c->gpr[1]._u32[0] = 0x3F000; c->gpr[3]._u32[0] = r3; c->gpr[4]._u32[0] = n; c->gpr[5]._u32[0] = dst;
    c->gpr[0]._u32[0] = 0x1234;
}
