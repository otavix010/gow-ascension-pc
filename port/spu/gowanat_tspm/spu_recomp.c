/* Escrito à mão (NÃO é saída do spu_lifter.py): trechos nativos do policy
 * module de taskset do libsre (EA 0x30023680, copiado pelo kernel SPURS para
 * LS 0x0A00; workloads w1/w2/w3/w5). Registrados como funções "quentes" do
 * interpretador em gowa_offline.cpp (GOWA_TSPM_HOT=0 desliga): o
 * interpretador confere os bytes na LS contra o libsre na memória do guest
 * antes de chamar.
 *
 * Fica neste diretório para o glob de port/spu/<x>/spu_recomp.c compilá-lo
 * com as flags e o include das SPUs. */
#include "spu_context.h"
#include "spu_helpers.h"

/* LS 0x2238..0x2267: laço que zera a área da tarefa antes de lançá-la, 128
 * bytes por volta (o salto calculado em 0x2218 entra no meio do corpo para a
 * primeira volta parcial; o brnz volta sempre para 0x2238):
 *   0x2238 ai   $8,$8,-128
 *   0x223C stqd $9,0($7)
 *   0x2240 stqd $9,0x10($16) ... 0x2254 stqd $9,0x60($16)
 *   0x2258 ai   $7,$16,128
 *   0x225C stqd $9,0x70($16)
 *   0x2260 ai   $16,$16,128
 *   0x2264 brnz $8,0x2238
 * Interpretado, era o bloco mais executado dos tasksets w1/w2 (~6% das
 * instruções interpretadas cada um, 19/10). Até 2048 voltas por chamada (a LS
 * inteira); se r8 ainda não zerou, volta com pc = 0x2238 e o interpretador
 * chama de novo -- a mesma semântica, sem prender a thread aqui dentro. */
void gowanat_tspm_zera_2238(spu_context* ctx)
{
    u128 r7 = ctx->gpr[7], r8 = ctx->gpr[8], r16 = ctx->gpr[16];
    const u128 r9 = ctx->gpr[9];
    int n = 0;
    do {
        r8 = spu_ai(r8, -128);
        spu_ls_write128(ctx, r7._u32[0], r9);
        spu_ls_write128(ctx, r16._u32[0] + 0x10u, r9);
        spu_ls_write128(ctx, r16._u32[0] + 0x20u, r9);
        spu_ls_write128(ctx, r16._u32[0] + 0x30u, r9);
        spu_ls_write128(ctx, r16._u32[0] + 0x40u, r9);
        spu_ls_write128(ctx, r16._u32[0] + 0x50u, r9);
        spu_ls_write128(ctx, r16._u32[0] + 0x60u, r9);
        r7 = spu_ai(r16, 128);
        spu_ls_write128(ctx, r16._u32[0] + 0x70u, r9);
        r16 = spu_ai(r16, 128);
    } while (r8._u32[0] != 0 && ++n < 2048);
    ctx->gpr[7] = r7; ctx->gpr[8] = r8; ctx->gpr[16] = r16;
    ctx->pc = r8._u32[0] != 0 ? 0x2238u : 0x2268u;
}
