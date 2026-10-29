"""Sonda temporária de fotos da LS num plugin do EDGE (só no código gerado da bancada; nunca commitar o
spu_recomp.c com ela). GOWA_PL_FOTO=<N> grava a LS + registradores na entrada (_0) e na saída (_1) das N
primeiras chamadas em GOWA_PL_FOTO_DIR\\pl_foto_<k>_<fase>.bin.

uso: python -I sonda_fotos.py <pasta do plugin, ex. gowapl_00752710> <função de entrada> <função do laço>
     (a entrada é a 0x30020: a ponte dos plugins relocáveis entra direto nela)"""
import os
import sys

pasta, entrada, laco = sys.argv[1], sys.argv[2], sys.argv[3]
raiz = os.path.join(os.path.dirname(__file__), '..')
p = os.path.join(raiz, 'port', 'spu', pasta, 'spu_recomp.c')
s = open(p, encoding='latin-1').read()
assert 'pl_foto(' not in s, 'sonda já aplicada'
cab = r'''#include "spu_recomp.h"
#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
static void pl_foto(spu_context* ctx, int fase)   /* SONDA temporaria (sonda_fotos.py) */
{
    static volatile long n; static SPU_THREAD_LOCAL long minha;
    static int alvo = -1;
    if (alvo < 0) { const char* e = getenv("GOWA_PL_FOTO"); alvo = e ? atoi(e) : 0; }
    if (!alvo) return;
    if (fase == 0) minha = InterlockedIncrement(&n);
    if (minha > alvo) return;
    char nome[MAX_PATH];
    snprintf(nome, sizeof nome, "%s\\pl_foto_%d_%d.bin", getenv("GOWA_PL_FOTO_DIR") ? getenv("GOWA_PL_FOTO_DIR") : ".", (int)minha, fase);
    FILE* f = fopen(nome, "wb");
    if (!f) return;
    fwrite(ctx->gpr, 16, 128, f);
    fwrite(ctx->ls, 1, 0x40000, f);
    fclose(f);
}
'''
s = s.replace('#include "spu_recomp.h"\n', cab, 1)
c = f'void {pasta}_spu_func_{entrada}(spu_context* ctx) {{\n'
i = s.index(c)
j = s.index('RELOAD;\n', i) + len('RELOAD;\n')
s = s[:j] + '        pl_foto(ctx, 0);   /* SONDA */\n' + s[j:]
c = f'void {pasta}_spu_func_{laco}(spu_context* ctx) {{\n'
i = s.index(c)
k = s.index('SPILL; SPU_RET(ctx); RELOAD;', i)
s = s[:k] + 'SPILL; pl_foto(ctx, 1); SPU_RET(ctx); RELOAD;' + s[k + len('SPILL; SPU_RET(ctx); RELOAD;'):]
open(p, 'w', encoding='latin-1', newline='').write(s)
print('ok')
