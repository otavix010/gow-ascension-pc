"""Remendo de teste para GOWA_FNDUMP (gowa_offline.cpp): põe gowa_fndump antes da
chamada direta de uma função SPU do 0x72DF80 e depois do spu_drain_call que a
completa, em todos os pontos de chamada. Não é para o padrão: aplicar, compilar
uma build de teste, tirar.

uso: python fndump_remendo.py aplicar 73E0
     python fndump_remendo.py tirar        (volta o spu_recomp.c do backup .antes_fndump)"""
import os as _os_raiz
RAIZ = _os_raiz.environ.get('GOWA_RAIZ') or _os_raiz.path.dirname(_os_raiz.path.dirname(_os_raiz.path.abspath(__file__)))  # raiz do projeto
import re
import shutil
import sys
from pathlib import Path

D = Path(_os_raiz.path.join(RAIZ, r'port\spu\gowajm_0072DF80'))
F = D / 'spu_recomp.c'
BAK = D / 'spu_recomp.c.antes_fndump'

if sys.argv[1] == 'tirar':
    shutil.copy2(BAK, F)
    F.touch()
    BAK.unlink()
    print('restaurado')
    sys.exit(0)

fn = sys.argv[2].upper().rjust(8, '0')
s = F.read_text(encoding='latin-1')
if not BAK.exists():
    shutil.copy2(F, BAK)
pat = re.compile(r'gowajm_0072DF80_spu_func_' + fn + r'__b\(ctx\); spu_drain_call\(ctx, (0x[0-9A-F]+)\);')
n = 0


def troca(m):
    global n
    n += 1
    return (f'gowa_fndump(ctx->gpr, SPU_LS_BASE(ctx), 0x{fn}u, 0); '
            f'gowajm_0072DF80_spu_func_{fn}__b(ctx); spu_drain_call(ctx, {m.group(1)}); '
            f'gowa_fndump(ctx->gpr, SPU_LS_BASE(ctx), 0x{fn}u, 1);')


s = pat.sub(troca, s)
s = s.replace('#include "spu_recomp.h"', '#include "spu_recomp.h"\nvoid gowa_fndump(const void* gpr, const void* ls, unsigned fn, int fase);', 1)
F.write_text(s, encoding='latin-1')
print(f'{n} pontos de chamada de {fn} remendados')
