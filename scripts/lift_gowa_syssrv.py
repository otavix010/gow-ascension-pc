"""Traduz o módulo do SYSTEM SERVICE do SPURS (libsre, EA 0x30021480, que o kernel
SPURS copia para LS 0x0A00 quando roda o workload 32) -- o mesmo esquema do
GOWA_PM_NATIVE (lift_gowa_pm.py): a tradução inteira vira uma imagem com região
residente (spu_overlay_register_region), e o despachante e o interpretador entram
nas funções nativas dela quando o GET do kernel traz esse código.

Por quê (24/10): no cap. 16 o system service era ~90% de todas as instruções SPU
interpretadas (GOWA_INTERP_ENTRY_LOG: w32 LS 0x288C, ~148 mil entradas por 10 s,
~4.300 instruções por entrada, ~63 M instruções/s), em todas as SPUs -- cada vez
que uma SPU fica sem trabalho ela passa por ele antes de bloquear em LS 0x11A8.

Bytes: fw/lle/libsre_image.bin (a imagem que o carregador põe em 0x30000000).
O tamanho vai até o policy module de taskset (EA 0x30023680).
Saída: port/spu/gowass_30021480/spu_recomp.{c,h}
"""
import os as _os_raiz
RAIZ = _os_raiz.environ.get('GOWA_RAIZ') or _os_raiz.path.dirname(_os_raiz.path.dirname(_os_raiz.path.abspath(__file__)))  # raiz do projeto
import os
import subprocess
import sys

R = RAIZ
TOOLS = os.path.join(R, 'ps3recomp', 'tools')
sys.path.insert(0, TOOLS)
from wrap_spu_elf import wrap  # noqa: E402

LIFTER = os.path.join(TOOLS, 'spu_lifter.py')
EA, SIZE, BASE = 0x30021480, 0x2200, 0x0A00
IMG, IMG_BASE = os.path.join(R, 'fw', 'lle', 'libsre_image.bin'), 0x30000000
NOME = f'gowass_{EA:08X}'
SAIDA = os.path.join(R, 'port', 'spu', NOME)
# entradas que o tradutor não acha sozinho (vistas no GOWA_INTERP_ENTRY_LOG)
EXTRA = (BASE, 0xA34, 0x288C)


def main():
    d = open(IMG, 'rb').read()
    raw = d[EA - IMG_BASE:EA - IMG_BASE + SIZE]
    tmp = os.path.join(R, 'port', 'spu', '_tmp_gowass.elf')
    with open(tmp, 'wb') as f:
        f.write(wrap(raw, base=BASE, entry=BASE))
    cmd = [sys.executable, LIFTER, '--auto-functions', tmp, '--base', hex(BASE),
           '--symbol-prefix', NOME + '_', '-o', SAIDA,
           '--source-name', 'spu_recomp.c', '--header-name', 'spu_recomp.h',
           '--extra-funcs', ','.join(hex(x) for x in EXTRA),
           '--merge-chunks']
    r = subprocess.run(cmd, capture_output=True, text=True)
    os.remove(tmp)
    if r.returncode != 0:
        print('FALHOU', r.stderr[-1200:])
        return
    print('\n'.join(r.stdout.strip().splitlines()[-6:]))


if __name__ == '__main__':
    main()
