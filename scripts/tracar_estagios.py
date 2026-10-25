"""Ganchos de rastreio na entrada das funções-estágio do 0x72DF80 (22/10, EDGE na GPU): põe
`gowa_fntrace(ctx, 0xADDR);` como primeiro comando de <mód>_spu_func_<ADDR>__b. Na entrada de uma
__b os registradores estão em ctx->gpr (quem chama grava antes), também nas promovidas. O
gowa_fntrace (gowa_offline.cpp) só escreve nos jobs que o GOWA_JOBDMA está registrando.

uso: python tracar_estagios.py aplicar [ADDR ...]   (sem endereços: a lista de estágios abaixo)
     python tracar_estagios.py tirar"""
import os as _os_raiz
RAIZ = _os_raiz.environ.get('GOWA_RAIZ') or _os_raiz.path.dirname(_os_raiz.path.dirname(_os_raiz.path.abspath(__file__)))  # raiz do projeto
import re
import sys
from pathlib import Path

F = Path(_os_raiz.path.join(RAIZ, r'port\spu\gowajm_0072DF80\spu_recomp.c'))
MOD = 'gowajm_0072DF80'
MARCA = '/* tracar_estagios.py */'
ESTAGIOS = ('53B0 E4D0 67C0 5E30 B600 D618 BEE0 A880 B638 73E0 6AC8 7040 9A60 BFA8 B618 B630 E550 F538 '
            'BBC8 61B0 5C88 DCC0 8930 9090 9AB8 D640 D250 D92C 5160 5194 5388 53A4').split()
s = F.read_text(encoding='latin-1')
nl = '\r\n' if '\r\n' in s else '\n'
if sys.argv[1] == 'tirar':
    linhas = [l for l in s.split(nl) if MARCA not in l]
    F.write_text(nl.join(linhas), encoding='latin-1')
    print('tirado')
    sys.exit(0)
alvos = [int(a, 16) for a in (sys.argv[2:] or ESTAGIOS)]
n = 0
for a in alvos:
    h = f'static SPU_PN void {MOD}_spu_func_{a:08X}__b(spu_context* ctx) {{'
    i = s.find(h)
    if i < 0:
        print(f'{a:X}: sem __b')
        continue
    j = i + len(h)
    if MARCA in s[j:j + 200]:
        continue
    s = s[:j] + nl + f'        gowa_fntrace(ctx, 0x{a:X}u); {MARCA}' + s[j:]
    n += 1
if 'void gowa_fntrace(' not in s:
    k = s.index('#include "spu_recomp.h"')
    k = s.index(nl, k) + len(nl)
    s = s[:k] + f'void gowa_fntrace(void* ctx, unsigned fn); {MARCA}' + nl + s[k:]
F.write_text(s, encoding='latin-1')
print(f'{n} ganchos')
