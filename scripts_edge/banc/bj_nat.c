/* bancada genérica dos nativos do job 0x72DF80 para o oráculo (bj_jm.py): lê um estado (128 registradores + LS, o
 * formato das fotos), roda o nativo pedido e grava o estado de saída; o código de saída é o que o nativo devolveu
 * (0 = recusou; k > 0 = feito, ou o destino k dos ganchos 'multi').
 * uso: bj_nat <nativo> <entrada.bin> <saída.bin> */
#include "../../port/spu/gowapl_0075AA40/spu_recomp.h"
SPU_THREAD_LOCAL void (*g_spu_trampoline_fn)(spu_context*);
SPU_THREAD_LOCAL int32_t g_spu_hot_reloc;
void spu_indirect_branch(spu_context* c) { (void)c; }
void spu_register_function(uint32_t a, void (*f)(spu_context*)) { (void)a; (void)f; }
#include "spu_helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
/* os cabeçalhos dos ganchos do job 0x72DF80 (edge_nativo.py), na ordem de lá */
#include "../../port/edge/fn_0000D640.h"
#include "../../port/edge/fn_0000E550_ec08.h"
#include "../../port/edge/fn_0000DCC0_e3c4.h"
#include "../../port/edge/fn_0000DCC0_e210.h"
#include "../../port/edge/fn_0000CC98_ce60.h"
#include "../../port/edge/fn_0000D250.h"
#include "../../port/edge/fn_0000F538.h"
#include "../../port/edge/fn_0000A880.h"
#include "../../port/edge/fn_00008930.h"
#include "../../port/edge/fn_0000B1A8.h"
#include "../../port/edge/fn_0000B2D0.h"
#include "../../port/edge/fn_0000B3D0.h"
#include "../../port/edge/fn_0000BFB8.h"
#include "../../port/edge/fn_0000BBC8.h"
#include "../../port/edge/fn_00008930_8988.h"
#include "../../port/edge/fn_0000DCC0_e134.h"
#include "../../port/edge/fn_0000DCC0_e30c.h"
#include "../../port/edge/fn_0000CC98_cdbc.h"
#include "../../port/edge/fn_00009090_lacos.h"
#include "../../port/edge/fn_0000CC98_ccd0.h"
#include "../../port/edge/fn_000073E0_83e8.h"
#include "../../port/edge/fn_000073E0_lacos.h"
#include "../../port/edge/fn_0000DCC0_de6c.h"
#include "../../port/edge/fn_0000DCC0_dff0.h"
#include "../../port/edge/fn_0000E550_f104.h"
#include "../../port/edge/fn_00009090_9458.h"

static spu_context C;
static const struct { const char* nome; int (*f)(spu_context*); } nats[] = {
    { "fn_0000D640_laco_nativo", fn_0000D640_laco_nativo },
    { "fn_0000E550_ec08_nativo", fn_0000E550_ec08_nativo },
    { "fn_0000DCC0_e3c4_nativo", fn_0000DCC0_e3c4_nativo },
    { "fn_0000DCC0_e210_nativo", fn_0000DCC0_e210_nativo },
    { "fn_0000CC98_ce60_nativo", fn_0000CC98_ce60_nativo },
    { "fn_0000D250_nativo", fn_0000D250_nativo },
    { "fn_0000F538_nativo", fn_0000F538_nativo },
    { "fn_0000A880_nativo", fn_0000A880_nativo },
    { "fn_00008930_nativo", fn_00008930_nativo },
    { "fn_0000B1A8_nativo", fn_0000B1A8_nativo },
    { "fn_0000B2D0_nativo", fn_0000B2D0_nativo },
    { "fn_0000B3D0_nativo", fn_0000B3D0_nativo },
    { "fn_0000BFB8_nativo", fn_0000BFB8_nativo },
    { "fn_0000BBC8_nativo", fn_0000BBC8_nativo },
    { "fn_00008930_8988_nativo", fn_00008930_8988_nativo },
    { "fn_0000DCC0_e134_nativo", fn_0000DCC0_e134_nativo },
    { "fn_0000DCC0_e30c_nativo", fn_0000DCC0_e30c_nativo },
    { "fn_0000CC98_cdbc_nativo", fn_0000CC98_cdbc_nativo },
    { "fn_00009090_9620_nativo", fn_00009090_9620_nativo },
    { "fn_00009090_96e0_nativo", fn_00009090_96e0_nativo },
    { "fn_0000CC98_ccd0_nativo", fn_0000CC98_ccd0_nativo },
    { "fn_000073E0_83e8_nativo", fn_000073E0_83e8_nativo },
    { "fn_000073E0_75d8_nativo", fn_000073E0_75d8_nativo },
    { "fn_000073E0_7670_nativo", fn_000073E0_7670_nativo },
    { "fn_000073E0_7828_nativo", fn_000073E0_7828_nativo },
    { "fn_0000DCC0_de6c_nativo", fn_0000DCC0_de6c_nativo },
    { "fn_0000DCC0_dff0_nativo", fn_0000DCC0_dff0_nativo },
    { "fn_0000E550_f104_nativo", fn_0000E550_f104_nativo },
    { "fn_00009090_9458_nativo", fn_00009090_9458_nativo },
};

int main(int argc, char** argv)
{
    if (argc < 4) return 255;
#ifdef SPU_SX_RZ
    _mm_setcsr((_mm_getcsr() & ~0xE040u) | 0x6000u | 0x0040u);   /* como a thread da SPU: DAZ + para zero, sem FTZ */
#else
    _mm_setcsr((_mm_getcsr() & ~0xE040u) | 0x0040u);
#endif
    int (*f)(spu_context*) = 0;
    for (size_t i = 0; i < sizeof nats / sizeof nats[0]; i++) if (!strcmp(nats[i].nome, argv[1])) f = nats[i].f;
    if (!f) { fprintf(stderr, "nativo desconhecido: %s\n", argv[1]); return 255; }
    FILE* fp = fopen(argv[2], "rb");
    if (!fp) return 255;
    memset(&C, 0, sizeof C);
    C.ls = C.ls_store;
    if (fread(C.gpr, 16, 128, fp) != 128 || fread(C.ls_store, 1, SPU_LS_SIZE, fp) != SPU_LS_SIZE) return 255;
    fclose(fp);
    const int r = f(&C);
    fp = fopen(argv[3], "wb");
    fwrite(C.gpr, 16, 128, fp);
    fwrite(C.ls_store, 1, SPU_LS_SIZE, fp);
    fclose(fp);
    return r;
}
