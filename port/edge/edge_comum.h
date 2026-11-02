/* GOWA (01/11, EDGE nativo E4): utilidades comuns dos nativos de função inteira. */
#ifndef GOWA_EDGE_COMUM_H
#define GOWA_EDGE_COMUM_H
#include <stdio.h>
#include <stdlib.h>
#include <intrin.h>
#ifndef GOWA_EDGE_CONTA_DEF
#define GOWA_EDGE_CONTA_DEF
/* GOWA_EDGE_CONTA=1: quantas chamadas cada nativo do E4 cobriu e quantas devolveu ao traduzido (a cada potência de 2) */
static void gowa_edge_conta(const char* nome, volatile long* n, int feito)
{
    static int on = -1;
    if (on < 0) on = getenv("GOWA_EDGE_CONTA") != NULL;
    if (!on) return;
    const long k = _InterlockedIncrement(&n[feito ? 0 : 1]);
    if ((k & (k - 1)) == 0) fprintf(stderr, "[edge-conta] %s: %ld feitas, %ld devolvidas\n", nome, n[0], n[1]);
}
#endif

/* o prólogo de uma função da SPU grava na pilha os registradores que ela salva; o nativo grava o mesmo (a conferência
 * compara a LS inteira). regs[k] vai para sp + desl[k]. */
static inline void gowa_edge_prologo(spu_context* ctx, uint32_t sp, const int* regs, const int* desl, int n)
{
    for (int k = 0; k < n; k++) spu_ls_write128(ctx, sp + (uint32_t)desl[k], ctx->gpr[regs[k]]);
}
#endif
