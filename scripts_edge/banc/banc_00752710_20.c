#include "../../port/spu/gowapl_00752710/limpo_banc.c"
#include "../../port/edge/pl_00752710.h"
#define PL_ENTRADA gowapl_00752710_spu_func_00030000
#define PL_LACO gowapl_00752710_spu_func_00030020
#define PL_NATIVO pl_00752710_n20
#define PL_IMG "pl_00752710.bin"
#include "banc_comum.h"
static void monta(spu_context* c, uint32_t s)
{
    uint8_t* ls = c->ls_store;
    const uint32_t r3 = 0x10000, r4 = 0x8000 + (rnd() & 15u), n = 1 + rnd() % 300u;
    memset(ls + r3 + 0x160, 0, 16);
    const uint32_t ini = rnd() % 8u;
    for (uint32_t i = 0; i < ini; i++) ls[r3 + 0x160 + i] = (uint8_t)(5 + i);
    if (s % 5u == 0) ls[r3 + 0x160 + ini + 1] = 9;
    { const uint32_t est = (s & 1) ? 0x2000u : 16u * ((n + 1u) & ~1u); for (int k = 0; k < 16; k++) be32(ls + r3 + 0x10 + 4 * k, 0x14000 + est * (uint32_t)k); }
    c->gpr[1]._u32[0] = 0x3F000; c->gpr[3]._u32[0] = r3; c->gpr[4]._u32[0] = r4; c->gpr[5]._u32[0] = n;
    c->gpr[0]._u32[0] = 0x1234;
}
