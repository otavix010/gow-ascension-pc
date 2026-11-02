#include "../../port/spu/gowapl_0075B710/limpo_banc.c"
#include "../../port/edge/pl_0075AF20.h"
#define PL_ENTRADA gowapl_0075B710_spu_func_00030000
#define PL_LACO PL_ENTRADA
#define PL_NA_ENTRADA 1
#define PL_NATIVO pl_0075B710_inteiro
#define PL_IMG "pl_0075B710.bin"
#include "banc_comum.h"
static void monta(spu_context* c, uint32_t s)
{
    uint8_t* ls = c->ls_store;
    const uint32_t r3 = 0x10000, n = 1 + rnd() % 400u;
    memset(ls + r3 + 0x160, 0, 16);
    const uint32_t ini = rnd() % 8u;
    for (uint32_t i = 0; i < ini; i++) ls[r3 + 0x160 + i] = (uint8_t)(5 + i);
    for (int k = 0; k < 16; k++) be32(ls + r3 + 0x10 + 4 * k, 0x14000 + 0x2000 * (uint32_t)k);
    c->gpr[1]._u32[0] = 0x3F000; c->gpr[3]._u32[0] = r3; c->gpr[4]._u32[0] = 0x8000 + (rnd() & 15u); c->gpr[5]._u32[0] = n;
    c->gpr[0]._u32[0] = 0x1234; (void)s;
}
