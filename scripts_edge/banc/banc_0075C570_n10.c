#include "../../port/spu/gowapl_0075C570/limpo_banc.c"
#include "../../port/edge/pl_0075C570.h"
#define PL_ENTRADA gowapl_0075C570_spu_func_00030000
#define PL_LACO gowapl_0075C570_spu_func_00030010
#define PL_NATIVO pl_0075C570_n10
#define PL_IMG "pl_0075C570.bin"
#include "banc_comum.h"
static void monta(spu_context* c, uint32_t s)
{
    uint8_t* ls = c->ls_store;
    const uint32_t r3 = 0x10000, n = 1 + rnd() % 400u, k = rnd() % 8u;
    memset(ls + r3 + 0x160, 0, 16);
    for (uint32_t i = 0; i < k; i++) ls[r3 + 0x160 + i] = (uint8_t)(5 + i);
    ls[r3 + 0x160 + k] = 1;
    for (int j = 0; j < 16; j++) be32(ls + r3 + 0x10 + 4 * j, 0x14000 + 0x2000 * (uint32_t)j);
    c->gpr[1]._u32[0] = 0x3F000; c->gpr[3]._u32[0] = r3; c->gpr[4]._u32[0] = n; c->gpr[5]._u32[0] = 0x30000 - 0x2000 + (rnd() & 0xFFu);
    c->gpr[0]._u32[0] = 0x1234; (void)s;
}
