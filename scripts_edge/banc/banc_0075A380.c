#include "../../port/spu/gowapl_0075A380/limpo_banc.c"
#include "../../port/edge/pl_atributos.h"
#define PL_ENTRADA gowapl_0075A380_spu_func_00030000
#define PL_LACO gowapl_0075A380_spu_func_00030064
#define PL_NATIVO pl_0075A380_nativo
#define PL_IMG "pl_0075A380.bin"
#include "banc_comum.h"
static void monta(spu_context* c, uint32_t s)
{
    (void)s;
    uint8_t* ls = c->ls_store;
    const uint32_t r9 = 0x11000 + (rnd() & 15u), r5 = 0x12000 + 4u * (rnd() & 3u), cur = 0x20000 + (rnd() & 0xFFu);
    for (int i = 0; i < 16; i++) ls[r9 + i] = (uint8_t)(rnd() & 15u);
    ls[r5 + 8] = cur >> 24; ls[r5 + 9] = cur >> 16; ls[r5 + 10] = cur >> 8; ls[r5 + 11] = cur;
    c->gpr[1]._u32[0] = 0x3F000; c->gpr[5]._u32[0] = r5; c->gpr[9]._u32[0] = r9;
    c->gpr[7]._u32[0] = rnd() & 0xFFFFFFu; c->gpr[6]._u32[0] = rnd() & 1u;
    c->gpr[0]._u32[0] = 0x1234;
}
