#include "../../port/spu/gowapl_PLID/limpo_banc.c"
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
SPU_THREAD_LOCAL void (*g_spu_trampoline_fn)(spu_context*);
SPU_THREAD_LOCAL int32_t g_spu_hot_reloc;
void spu_indirect_branch(spu_context* c) { (void)c; }
void spu_register_function(uint32_t a, void (*f)(spu_context*)) { (void)a; (void)f; }
static spu_context A;
static void be32(uint8_t* p, uint32_t v) { p[0] = v >> 24; p[1] = v >> 16; p[2] = v >> 8; p[3] = v; }
int main(int argc, char** argv)
{
    const uint32_t n = argc > 1 ? atoi(argv[1]) : 4, dst = 0x20000 + (argc > 2 ? atoi(argv[2]) : 0);
    A.ls = A.ls_store; A.host_depth = 1;
    FILE* f = fopen("pl_PLID.bin", "rb"); fread(A.ls_store + 0x30000, 1, 0x1000, f); fclose(f);
    memset(A.ls_store + 0x20000 - 64, 0xEE, 0x1000);
    uint8_t* ls = A.ls_store; const uint32_t r3 = 0x10000;
    static const uint8_t ids[] = { IDS }; for (unsigned i = 0; i < sizeof ids; i++) ls[r3 + 0x160 + i] = ids[i];
    for (unsigned i = 0; i < sizeof ids; i++) be32(ls + r3 + 0x10 + 4 * i, 0x4000 + 0x1000 * i);
    for (unsigned a = 0; a < sizeof ids; a++) for (uint32_t i = 0; i < 64 * 4; i++) be32(ls + 0x4000 + 0x1000 * a + 4 * i, (ids[a] == 3 || ids[a] == 4 || ids[a] == 2 || ids[a] == 20) ? 0x3E000000u + (i << 16) : ((0x10u + a) << 24) + i);
    A.gpr[1]._u32[0] = 0x3F000; A.gpr[3]._u32[0] = r3; A.gpr[4]._u32[0] = n; A.gpr[5]._u32[0] = dst;
    gowapl_PLID_spu_func_00030000(&A);
    while (g_spu_trampoline_fn) { void (*g)(spu_context*) = g_spu_trampoline_fn; g_spu_trampoline_fn = 0; g(&A); }
    for (uint32_t i = 0x20000 - 16; i < dst + REC * ((n + 1) & ~1u) + 96; i += 4) {
        printf("%05X: %08X%s", i, (ls[i] << 24) | (ls[i + 1] << 16) | (ls[i + 2] << 8) | ls[i + 3], ((i - dst) % REC == REC - 4 && i >= dst) ? "  |\n" : "\n");
    }
    printf("r3=%08X\n", A.gpr[3]._u32[0]);
}
