#include "../../port/spu/gowapl_0075B470/limpo_banc.c"
#include "../../port/edge/pl_0075AF20.h"
#define PL_ENTRADA gowapl_0075B470_spu_func_00030000
#define PL_LACO LACO_0075B470
#define PL_NATIVO pl_0075B470_nativo
#define PL_IMG "pl_0075B470.bin"
#define LACO_0075B470 gowapl_0075B470_spu_func_00030154
#define LACO_0075AF20 gowapl_0075AF20_spu_func_00030160
#include "banc_comum.h"
static void monta(spu_context* c, uint32_t s)
{
    (void)s;
    uint8_t* ls = c->ls_store;
    const uint32_t r3 = 0x10000, ent = 0x8000 + (rnd() & 15u), dst = 0x20000, n = 1 + rnd() % 400u;
    memset(ls + r3 + 0x160, 0, 16);
    ls[r3 + 0x160] = 1;
    const uint32_t k = 1 + rnd() % 10u;   /* o primeiro zero: o slot do ponteiro */
    for (uint32_t i = 1; i < k; i++) ls[r3 + 0x160 + i] = (uint8_t)(2 + i);
    be32(ls + r3 + 0x10 + 4 * k, dst);
    c->gpr[1]._u32[0] = 0x3F000; c->gpr[3]._u32[0] = r3; c->gpr[4]._u32[0] = ent; c->gpr[5]._u32[0] = n;
    c->gpr[0]._u32[0] = 0x1234;
}
