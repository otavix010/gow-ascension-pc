"""Sonda temporária de fotos da LS numa função do módulo do EDGE (gowajm_0072DF80): entrada e o último
SPU_RET(ctx) da função. Só no código gerado da bancada; restaurar o original depois.
GOWA_PL_FOTO=<N> grava as N primeiras chamadas em GOWA_PL_FOTO_DIR\\pl_foto_<k>_<fase>.bin.

uso: python -I sonda_fn.py <pasta, ex. gowajm_0072DF80> <função, ex. gowajm_0072DF80_spu_func_0000D640__b>"""
import os
import sys

pasta, fn = sys.argv[1], sys.argv[2]
raiz = os.path.join(os.path.dirname(__file__), '..')
p = os.path.join(raiz, 'port', 'spu', pasta, 'spu_recomp.c')
s = open(p, encoding='latin-1').read()
assert 'pl_foto(' not in s, 'sonda já aplicada'
cab = r'''#include <windows.h>
#include <stdio.h>
#include <stdlib.h>
static void pl_foto(spu_context* ctx, int fase)   /* SONDA temporaria (sonda_fn.py) */
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
inc = '#include "spu_recomp.h"\n'
assert s.count(inc) == 1
s = s.replace(inc, inc + cab, 1)
c = f'static SPU_PN void {fn}(spu_context* ctx) {{\n'
i = s.index(c) + len(c)
fim = s.index('\n}\n', i)
corpo = s[i:fim]
k = corpo.rindex('SPU_RET(ctx);') if 'SPU_RET(ctx);' in corpo else corpo.rindex('SPU_TAILX(')   # saída por chamada de cauda
corpo = corpo[:k] + 'pl_foto(ctx, 1); ' + corpo[k:]
s = s[:i] + '        pl_foto(ctx, 0);   /* SONDA */\n' + corpo + s[fim:]
open(p, 'w', encoding='latin-1', newline='').write(s)
print('ok')
