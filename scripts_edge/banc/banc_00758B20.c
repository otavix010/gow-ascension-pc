#include "../../port/spu/gowapl_00758B20/limpo_banc.c"
#include "../../port/edge/pl_00758B20.h"
#define PL_ENTRADA gowapl_00758B20_spu_func_00030000
#define PL_LACO PL_ENTRADA
#define PL_NA_ENTRADA 1
#define PL_NATIVO pl_00758B20_nativo
#define PL_IMG "pl_00758B20.bin"
#include "banc_comum.h"
static void monta(spu_context* c, uint32_t s)
{
    uint8_t* ls = c->ls_store;
    const uint32_t r3 = 0x10000, dst = 0x20000 + 0, n = 1 + rnd() % 300u;
    static const uint8_t id[3] = {1, 2, 31};
    const uint32_t f[3] = {0x4000, 0x9000, 0xE000};
    memset(ls + r3 + 0x160, 0, 16);
    uint32_t perm[8] = {0, 1, 2, 3, 4, 5, 6, 7};
    for (int i = 7; i > 0; i--) { const uint32_t j = rnd() % (uint32_t)(i + 1), t = perm[i]; perm[i] = perm[j]; perm[j] = t; }
    for (uint32_t i = 0; i < 8; i++) ls[r3 + 0x160 + i] = (uint8_t)(40 + i);
    for (int a = 0; a < 3; a++) {
        ls[r3 + 0x160 + perm[a]] = id[a];
        be32(ls + r3 + 0x10 + 4 * perm[a], f[a]);
        for (uint32_t i = 0; i < 4 * (n + 8); i++) be32(ls + f[a] + 4 * i, fvert());
    }
    c->gpr[1]._u32[0] = 0x3F000; c->gpr[3]._u32[0] = r3; c->gpr[4]._u32[0] = n; c->gpr[5]._u32[0] = dst;
    c->gpr[0]._u32[0] = 0x1234;
}
