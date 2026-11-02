/* bancada do plugin 0x755F80 na entrada 0x30030 sobre estados REAIS (gravados na reprodução, out/d755/d755_<k>.bin:
 * 128 registradores + LS, o plugin realocado na LS): o traduzido contra o nativo pl_00755F80_n30.
 * uso: bj755 <arquivo de estado> <endereço da imagem na LS, hex> */
#include "../../port/spu/gowapl_00755F80/limpo_banc.c"
#include "../../port/edge/pl_00755F80.h"
SPU_THREAD_LOCAL void (*g_spu_trampoline_fn)(spu_context*);
SPU_THREAD_LOCAL int32_t g_spu_hot_reloc;
void spu_indirect_branch(spu_context* c) { (void)c; }
void spu_register_function(uint32_t a, void (*f)(spu_context*)) { (void)a; (void)f; }
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static spu_context A, B;

int main(int argc, char** argv)
{
    if (argc < 3) return 2;
#ifdef SPU_SX_RZ
    _mm_setcsr((_mm_getcsr() & ~0xE040u) | 0x6000u | 0x0040u);
#else
    _mm_setcsr((_mm_getcsr() & ~0xE040u) | 0x0040u);
#endif
    FILE* f = fopen(argv[1], "rb");
    if (!f) return 2;
    memset(&A, 0, sizeof A); A.ls = A.ls_store; A.host_depth = 1;
    if (fread(A.gpr, 16, 128, f) != 128 || fread(A.ls_store, 1, SPU_LS_SIZE, f) != SPU_LS_SIZE) return 2;
    fclose(f);
    g_spu_hot_reloc = (int32_t)strtoul(argv[2], 0, 16) - 0x30000;
    memcpy(&B, &A, sizeof A); B.ls = B.ls_store;
    g_spu_trampoline_fn = 0;
    gowapl_00755F80_spu_func_00030030(&A);
    while (g_spu_trampoline_fn) { void (*t)(spu_context*) = g_spu_trampoline_fn; g_spu_trampoline_fn = 0; t(&A); }
    if (!pl_00755F80_n30(&B)) { printf("nativo recusou\n"); return 0; }
    int d = 0;
    for (uint32_t i = 0; i < SPU_LS_SIZE; i++)
        if (A.ls_store[i] != B.ls_store[i]) {
            if (d < (getenv("BANC_V") ? 400 : 6)) printf("LS 0x%05X traduzido %02X nativo %02X\n", i, A.ls_store[i], B.ls_store[i]);
            d++;
        }
    for (int r = 0; r < 128; r++)
        if (memcmp(&A.gpr[r], &B.gpr[r], 16)) {
            printf("r%d difere: trad %08X %08X %08X %08X nat %08X %08X %08X %08X\n", r, A.gpr[r]._u32[0], A.gpr[r]._u32[1],
                   A.gpr[r]._u32[2], A.gpr[r]._u32[3], B.gpr[r]._u32[0], B.gpr[r]._u32[1], B.gpr[r]._u32[2], B.gpr[r]._u32[3]);
            if (r == 1 || r == 3 || r >= 80) d++;
        }
    printf("%d diferenças\n", d);
    return d ? 1 : 0;
}
