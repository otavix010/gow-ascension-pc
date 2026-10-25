"""Troca o preâmbulo do port/recompiled/ppu_recomp.h gerado pelo lifter pelo do port
(port/ppu_recomp_preambulo.h): PPU_CTX_SYNC antes de stwcx./stdcx. (exigido pelo
ppu_restrict.py), as checagens vm_inl_* das leituras/gravações em linha e o resto do
código de suporte. O preâmbulo é tudo antes da 1ª declaração "void func_".

uso: python scripts/ppu_preambulo.py   (depois do lifter, antes do ppu_restrict.py)"""
import os
import sys

RAIZ = os.environ.get('GOWA_RAIZ') or os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
H = os.path.join(RAIZ, 'port', 'recompiled', 'ppu_recomp.h')
P = os.path.join(RAIZ, 'port', 'ppu_recomp_preambulo.h')

s = open(H, 'rb').read().decode('latin-1')
k = s.find('\nvoid func_')
if k < 0:
    sys.exit('nenhuma declaração "void func_" em ' + H)
pre = open(P, 'rb').read().decode('latin-1')
nl = '\r\n' if '\r\n' in s[:k] else '\n'
pre = pre.replace('\r\n', '\n').replace('\n', nl).rstrip() + nl + nl + nl
novo = pre + s[k + 1:].lstrip('\r\n')
if novo == s:
    print('preâmbulo já é o do port')
else:
    if not os.path.exists(H + '.do_lifter'):
        open(H + '.do_lifter', 'wb').write(s.encode('latin-1'))
    open(H, 'wb').write(novo.encode('latin-1'))
    print('preâmbulo trocado (original em ppu_recomp.h.do_lifter)')
