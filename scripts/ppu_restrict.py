"""`ppu_context* __restrict ctx` nas definições das funções PPU traduzidas
(port/recompiled/ppu_recomp_*.cpp).

O contexto (gpr, cr, xer, lr...) nunca se sobrepõe à memória do jogo. Sem o
__restrict, todo store na memória do jogo (vm_write*, um uint64_t* sobre
vm_base) pode, para o compilador, ter mudado ctx->gpr[], e cada leitura seguinte
de um registrador vai de novo à memória; com ele, os registradores ficam em
registradores do host entre os acessos à memória do jogo.

uso: python ppu_restrict.py aplicar|tirar   (backup <arquivo>.sem_restrict)
"""
import os as _os_raiz
RAIZ = _os_raiz.environ.get('GOWA_RAIZ') or _os_raiz.path.dirname(_os_raiz.path.dirname(_os_raiz.path.abspath(__file__)))  # raiz do projeto
import glob
import os
import re
import shutil
import sys
import time

R = RAIZ
DEF = re.compile(r'^(void func_[0-9A-F]+)\(ppu_context\* ctx\) \{(?=\r?$)', re.M)   # arquivos em CRLF
LWARX = re.compile(r'(ctx->reserve_addr = \(uint32_t\)ea; ctx->reserve_value = ctx->gpr\[\d+\];) \}')


def main():
    modo = sys.argv[1]
    for c in sorted(glob.glob(os.path.join(R, 'port', 'recompiled', 'ppu_recomp_*.cpp'))):
        bak = c + '.sem_restrict'
        if modo == 'aplicar':
            src = open(c, encoding='latin-1', newline='').read()
            if 'ppu_context* __restrict ctx' in src:
                print(f'{os.path.basename(c)}: já aplicado')
                continue
            shutil.copyfile(c, bak)
            src, n = DEF.subn(r'\1(ppu_context* __restrict ctx) {', src)
            # lwarx/ldarx: a reserva vai para a memória na hora (PPU_CTX_SYNC, ppu_recomp.h);
            # com o __restrict o compilador podia atrasar essa gravação para depois de
            # uma quebra de reserva feita por outra thread
            src = LWARX.sub(r'\1 PPU_CTX_SYNC(ctx); }', src)
            open(c, 'w', encoding='latin-1', newline='').write(src)
            print(f'{os.path.basename(c)}: {n} funções')
        elif os.path.exists(bak):
            shutil.copyfile(bak, c)
            os.utime(c, (time.time(), time.time()))
            os.remove(bak)
            print(f'{os.path.basename(c)}: restaurado')


if __name__ == '__main__':
    main()
