"""Promoção de registradores (promover_regs.py) só nas funções escolhidas, sobre
o spu_recomp.c ATUAL (o promover_regs.py parte do .sem_promocao, que no
0x72DF80 é anterior à mescla, às constantes e ao cwd vetorial).

uso: python promover_sel.py <dir do módulo> <sufixo hex>[,<sufixo hex>...]
     python promover_sel.py --tirar <dir do módulo>
  ex.: promover_sel.py port/spu/gowajm_0072DF80 73E0,F538,A880

Backup antes da primeira aplicação: <arquivo>.antes_sel (o --tirar volta a
ele). Funções com chamada entram com SPILL/RELOAD em volta das chamadas (a
regra PROMOVER_LACOS do promover_regs.py) -- vale a pena quando os laços
quentes não têm chamada (as do EDGE no capítulo 16)."""
import os
import re
import shutil
import sys
from pathlib import Path

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import promover_regs as pr  # noqa: E402

args = [a for a in sys.argv[1:] if not a.startswith('--')]
d = Path(args[0])
f = d / 'spu_recomp.c'
bak = f.with_name(f.name + '.antes_sel')
if '--tirar' in sys.argv:
    if bak.exists():
        shutil.copy2(bak, f)
        os.utime(f, None)   # data nova: o ninja recompila
        print('restaurado:', f)
    sys.exit(0)

sufixos = {s.strip().upper().rjust(8, '0') for s in args[1].split(',')}
alvo = re.compile(r'^(?:static )?(?:SPU_PN )?void (\w+_spu_func_([0-9A-F]{8})(?:__b)?)\(spu_context\* ctx\) \{[ \t]*$')
todos = re.compile(r'^(?:static )?(?:SPU_PN )?void (\w+)\(spu_context\* ctx\) \{[ \t]*$')


class HdrSel:
    """RE_HDR do promover_regs: casa só as escolhidas (grupo 1 = nome)."""
    def match(self, l):
        m = alvo.match(l)
        return m if m and m.group(2) in sufixos else None


src = (bak if bak.exists() else f).read_bytes().decode('latin-1')
if not bak.exists():
    shutil.copy2(f, bak)
pr.RE_HDR = HdrSel()
pr.SAFE = pr.SAFE | {'spu_ls_const128'}
# 23/10: as chamadas dos núcleos vk (gowa_vk_*, lêem e gravam ctx->gpr e a LS) são
# sincronização total, como SPU_RET; SPU_TAILX é saída (SPILL antes)
class _Full(set):
    def __contains__(self, n):
        return set.__contains__(self, n) or n.startswith('gowa_vk_')
pr.FULL = _Full(pr.FULL | {'SPU_TAILX'})
os.environ['PROMOVER_LACOS'] = '1'
# o eh_lifted do processar() deriva os prefixos dos nomes casados: como só as
# escolhidas casam, o prefixo do módulo sai delas mesmas (mesmo módulo)
novo, nf, np_ = pr.processar(src)
f.write_bytes(novo.encode('latin-1'))
print(f'{f}: {np_} de {nf} funções escolhidas promovidas')
