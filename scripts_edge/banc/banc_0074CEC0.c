#include "../../port/spu/gowapl_0074CEC0/limpo_banc.c"
#include "../../port/edge/pl_0074CEC0.h"
#define PL_ENTRADA gowapl_0074CEC0_spu_func_00030000
#define PL_LACO PL_ENTRADA
#define PL_NA_ENTRADA 1
#define PL_NATIVO pl_0074CEC0_nativo
#define PL_IMG "pl_0074CEC0.bin"
#include "banc_comum.h"
static void monta(spu_context* c, uint32_t s)
{
    uint8_t* ls = c->ls_store;
    const uint32_t r3 = 0x10000, dst = 0x20000 + 16u * (rnd() % 64u), n = (s % 7u) ? 8u * (1u + rnd() % 30u) : 1u + rnd() % 300u;
    static const uint8_t id[7] = {1, 2, 20, 27, 28, 29, 30};
    const uint32_t f[8] = {0x2000, 0x4000, 0x6000, 0x8000, 0xA000, 0xC000, 0xE000, 0x24000};
    memset(ls + r3 + 0x160, 0, 16);
    uint32_t perm[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    for (int i = 7; i > 0; i--) { const uint32_t j = rnd() % (uint32_t)(i + 1), t = perm[i]; perm[i] = perm[j]; perm[j] = t; }
    for (uint32_t i = 0; i < 8; i++) ls[r3 + 0x160 + i] = (uint8_t)(40 + i);
    for (int a = 0; a < 7; a++) {
        ls[r3 + 0x160 + perm[a]] = id[a];
        be32(ls + r3 + 0x10 + 4 * perm[a], f[a]);
        for (uint32_t i = 0; i < 4 * (n + 8); i++) be32(ls + f[a] + 4 * i, fvert());
    }
    c->gpr[1]._u32[0] = 0x3F000; c->gpr[3]._u32[0] = r3; c->gpr[4]._u32[0] = n; c->gpr[5]._u32[0] = dst;
    c->gpr[0]._u32[0] = 0x1234;
}
