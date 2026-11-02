/* bancada do nativo DE6C da DCC0: lê um estado (128 registradores + LS, formato das fotos), roda o nativo e grava o
 * estado de saída. A comparação com o traduzido é do bj_dff0.py de6c (oráculo).
 * uso: bj_dff0 <entrada.bin> <saída.bin>; sai com 1 se o nativo fez, 0 se recusou */
#include "../../port/spu/gowapl_0075AA40/spu_recomp.h"
SPU_THREAD_LOCAL void (*g_spu_trampoline_fn)(spu_context*);
SPU_THREAD_LOCAL int32_t g_spu_hot_reloc;
void spu_indirect_branch(spu_context* c) { (void)c; }
void spu_register_function(uint32_t a, void (*f)(spu_context*)) { (void)a; (void)f; }
#include "spu_helpers.h"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "../../port/edge/fn_0000DCC0_de6c.h"

static spu_context C;

int main(int argc, char** argv)
{
    if (argc < 3) return 2;
    FILE* f = fopen(argv[1], "rb");
    if (!f) return 2;
    memset(&C, 0, sizeof C);
    C.ls = C.ls_store;
    if (fread(C.gpr, 16, 128, f) != 128 || fread(C.ls_store, 1, SPU_LS_SIZE, f) != SPU_LS_SIZE) return 2;
    fclose(f);
    const int r = fn_0000DCC0_de6c_corpo(&C);
    f = fopen(argv[2], "wb");
    fwrite(C.gpr, 16, 128, f);
    fwrite(C.ls_store, 1, SPU_LS_SIZE, f);
    fclose(f);
    return r;
}
