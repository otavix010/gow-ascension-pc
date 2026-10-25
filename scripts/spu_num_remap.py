"""Aponta as leituras do contexto do kernel SPURS (quadword em LS 0x1C0, com o
spuNum em 0x1C8) do gerenciador de jobs WWS, dos jobs e dos plugins traduzidos
para spu_ls_read128_kc (endereço constante 0x1C0) e spu_ls_read128_kcx (base em
registrador + 0xC0, o ponteiro 0x100 que o kernel passa; o endereço é conferido
em tempo de execução), que com GOWA_SPU_NUM_REMAP=1 fazem as SPUs 6/7 se verem
como 4/5. O kernel e as tarefas de taskset ficam de fora.

uso: python spu_num_remap.py aplicar|tirar"""
import os as _os_raiz
RAIZ = _os_raiz.environ.get('GOWA_RAIZ') or _os_raiz.path.dirname(_os_raiz.path.dirname(_os_raiz.path.abspath(__file__)))  # raiz do projeto
import glob
import os
import re
import shutil
import sys

R = _os_raiz.path.join(RAIZ, r'port\spu')
CONST_DE = 'spu_ls_read128(ctx, 0x1C0)'
CONST_PARA = 'spu_ls_read128_kc(ctx)'
# spu_ls_read128(ctx, <registrador>._u32[0] + 0xC0)
RE_REG = re.compile(r'spu_ls_read128\(ctx, ((?:ctx->gpr\[\d+\]|_r\d+)\._u32\[0\] \+ 0xC0)\)')
RE_REG_VOLTA = re.compile(r'spu_ls_read128_kcx\(ctx, ((?:ctx->gpr\[\d+\]|_r\d+)\._u32\[0\] \+ 0xC0)\)')

modo = sys.argv[1] if len(sys.argv) > 1 else ''
if modo not in ('aplicar', 'tirar'):
    sys.exit(__doc__)
tot = 0
for pref in ('gowapm_', 'gowajm_', 'gowapl_'):
    for f in sorted(glob.glob(os.path.join(R, pref + '*', 'spu_recomp.c'))):
        s = open(f, encoding='latin-1', newline='').read()
        if modo == 'aplicar':
            n1 = s.count(CONST_DE)
            s2 = s.replace(CONST_DE, CONST_PARA)
            s2, n2 = RE_REG.subn(r'spu_ls_read128_kcx(ctx, \1)', s2)
        else:
            n1 = s.count(CONST_PARA)
            s2 = s.replace(CONST_PARA, CONST_DE)
            s2, n2 = RE_REG_VOLTA.subn(r'spu_ls_read128(ctx, \1)', s2)
        if not (n1 or n2):
            continue
        if modo == 'aplicar' and not os.path.exists(f + '.sem_kc'):
            shutil.copy2(f, f + '.sem_kc')
        open(f, 'w', encoding='latin-1', newline='').write(s2)
        tot += n1 + n2
        print(f'{n1:4d} {n2:4d}  {os.path.basename(os.path.dirname(f))}')
print(f'{modo}: {tot} leituras')
