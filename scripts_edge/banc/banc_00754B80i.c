#include "../../port/spu/gowapl_00754B80/limpo_banc.c"
#include "../../port/edge/pl_00755540.h"
#define PL_ENTRADA gowapl_00754B80_spu_func_00030000
#define PL_LACO gowapl_00754B80_spu_func_00030014
#define PL_NATIVO pl_00754B80_inteiro
#define PL_IMG "pl_00754B80.bin"
#include "banc_comum.h"
static void monta(spu_context* c, uint32_t s)
{
    uint8_t* ls = c->ls_store;
    const uint32_t r3 = 0x10000, r6 = 0x11000 + (rnd() & 15u), r4 = 0x8000 + (rnd() & 15u), n = 1 + rnd() % 300u;
    for (int i = 0; i < 3; i++) {
        const uint32_t fr = (s % 11u == 0) ? (rnd() % 26u) : 1u + rnd() % 15u;
        ls[r6 + 16 + 2 * i] = (uint8_t)((s % 13u == 0) ? (rnd() & 31u) : 16u - fr); ls[r6 + 17 + 2 * i] = (uint8_t)fr;
        const int32_t v = (s % 7u == 0) ? (int32_t)rnd() : (int32_t)(rnd() % 0x200000u) - 0x100000;
        uint8_t w[4] = { (uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v };
        for (int b = 0; b < 4; b++) ls[(r6 + 4 * i + b) & ~0u] = w[b];   /* a palavra corrida (o descritor real não dá a volta) */
    }
    memset(ls + r3 + 0x160, 0, 16);
    const uint32_t ini = rnd() % 8u;
    for (uint32_t i = 0; i < ini; i++) ls[r3 + 0x160 + i] = (uint8_t)(3 + i);
    for (int k = 0; k < 16; k++) be32(ls + r3 + 0x10 + 4 * k, 0x14000 + 0x2000 * (uint32_t)k);
    c->gpr[1]._u32[0] = 0x3F000; c->gpr[3]._u32[0] = r3; c->gpr[4]._u32[0] = r4; c->gpr[5]._u32[0] = n; c->gpr[6]._u32[0] = r6;
    c->gpr[0]._u32[0] = 0x1234;
}
