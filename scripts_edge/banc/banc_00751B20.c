#include "../../port/spu/gowapl_00751B20/limpo_banc.c"
#include "../../port/edge/pl_00751B20.h"
#define PL_ENTRADA gowapl_00751B20_spu_func_00030000
#define PL_LACO gowapl_00751B20_spu_func_00030358
#define PL_NATIVO pl_00751B20_nativo
#define PL_IMG "pl_00751B20.bin"
#include "banc_comum.h"
static void monta(spu_context* c, uint32_t s)
{
    (void)s;
    uint8_t* ls = c->ls_store;
    const uint32_t r3 = 0x10000, n = 1 + rnd() % 250u, ini = rnd() % 6u;
    memset(ls + r3 + 0x160, 0, 16);
    for (uint32_t i = 0; i < ini; i++) ls[r3 + 0x160 + i] = (uint8_t)(5 + i);
    for (int k = 0; k < 16; k++) be32(ls + r3 + 0x10 + 4 * k, 0x14000 + 0x2000 * (uint32_t)k);
    c->gpr[1]._u32[0] = 0x3F000; c->gpr[3]._u32[0] = r3; c->gpr[4]._u32[0] = 0x8000 + (rnd() & 15u); c->gpr[5]._u32[0] = n;
    c->gpr[0]._u32[0] = 0x1234;
}
