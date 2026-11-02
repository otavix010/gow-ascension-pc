#include "../../port/spu/gowapl_00751450/limpo_banc.c"
#include "../../port/edge/pl_00751450.h"
#define PL_ENTRADA gowapl_00751450_spu_func_00030000
#define PL_LACO gowapl_00751450_spu_func_00030014
#define PL_NATIVO pl_00751450_inteiro
#define PL_IMG "pl_00751450.bin"
#include "banc_comum.h"
static void monta(spu_context* c, uint32_t s)
{
    uint8_t* ls = c->ls_store;
    const uint32_t r3 = 0x10000, r6 = 0x11000 + (rnd() & 15u), r4 = 0x8000 + (rnd() & 15u), n = 1 + rnd() % 300u;
    /* larguras que somam 48 (às vezes não) */
    uint32_t wd[3];
    wd[0] = 8 + rnd() % 15u; wd[1] = 8 + rnd() % 15u; wd[2] = 48u - wd[0] - wd[1];
    if (s % 9u == 0) wd[2] = 1 + rnd() % 22u;
    for (int i = 0; i < 3; i++) {
        uint32_t fr = 1 + rnd() % (wd[i] < 24 ? wd[i] : 24);
        if (fr > wd[i]) fr = wd[i];
        if (s % 11u == 0) fr = rnd() % 30u;
        ls[r6 + 16 + 2 * i] = (uint8_t)(wd[i] - (fr <= wd[i] ? fr : 0)); ls[r6 + 17 + 2 * i] = (uint8_t)fr;
        const int32_t v = (s % 7u == 0) ? (int32_t)rnd() : (int32_t)(rnd() % 0x100000u) - 0x80000;
        uint8_t w[4] = { (uint8_t)(v >> 24), (uint8_t)(v >> 16), (uint8_t)(v >> 8), (uint8_t)v };
        for (int b = 0; b < 4; b++) ls[r6 + 4 * i + b] = w[b];
    }
    memset(ls + r3 + 0x160, 0, 16);
    const uint32_t ini = rnd() % 8u;
    for (uint32_t i = 0; i < ini; i++) ls[r3 + 0x160 + i] = (uint8_t)(3 + i);
    if (s % 5u == 0) ls[r3 + 0x160 + ini + 1] = 9;   /* um buraco: o segundo zero depois */
    for (int k = 0; k < 16; k++) be32(ls + r3 + 0x10 + 4 * k, 0x14000 + 0x2000 * (uint32_t)k);
    c->gpr[1]._u32[0] = 0x3F000; c->gpr[3]._u32[0] = r3; c->gpr[4]._u32[0] = r4; c->gpr[5]._u32[0] = n; c->gpr[6]._u32[0] = r6;
    c->gpr[0]._u32[0] = 0x1234;
}
