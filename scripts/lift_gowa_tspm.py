"""Traduz o policy module de taskset do libsre (EA 0x30023680, 0x1E40 bytes,
que o kernel SPURS copia para LS 0x0A00; workloads w1/w2/w3/w5) e lista as
funções folha seguras dele -- o mesmo esquema do lift_gowa_pm.py (PM do WWS).

O PM continua interpretado; as folhas (sem chamadas, canais, desvios
indiretos nem stop) rodam nativas, chamadas em linha pelo interpretador
(spu_interp_hot_register) depois de conferir os bytes da função na LS contra
a imagem do libsre na memória do guest. Na fase, o PM do taskset era ~13% das
instruções interpretadas (LS 0x2200), e a thread principal espera as tarefas
desses tasksets em func_003CC6B4 (~20% dela).

Bytes: fw/lle/libsre_image.bin (a imagem que o carregador põe em 0x30000000).
Saída: port/spu/gowatspm_30023680/spu_recomp.{c,h} e port/gowa_tspm_hot.inc
"""
import os as _os_raiz
RAIZ = _os_raiz.environ.get('GOWA_RAIZ') or _os_raiz.path.dirname(_os_raiz.path.dirname(_os_raiz.path.abspath(__file__)))  # raiz do projeto
import os
import re
import subprocess
import sys

R = RAIZ
TOOLS = os.path.join(R, 'ps3recomp', 'tools')
sys.path.insert(0, TOOLS)
from wrap_spu_elf import wrap  # noqa: E402

LIFTER = os.path.join(TOOLS, 'spu_lifter.py')
EA, SIZE, BASE = 0x30023680, 0x1E40, 0x0A00
IMG, IMG_BASE = os.path.join(R, 'fw', 'lle', 'libsre_image.bin'), 0x30000000
NOME = f'gowatspm_{EA:08X}'
SAIDA = os.path.join(R, 'port', 'spu', NOME)
INC = os.path.join(R, 'port', 'gowa_tspm_hot.inc')
PROIBIDO = (r'spu_rdch|spu_wrch|spu_rchcnt|spu_indirect_branch|spu_drain_call|spu_halt|spu_stop|'
            r'spu_iret|spu_check_stack_reset|longjmp|SPU_STOP|spu_sync|spu_unsupported')


def main():
    d = open(IMG, 'rb').read()
    raw = d[EA - IMG_BASE:EA - IMG_BASE + SIZE]
    tmp = os.path.join(R, 'port', 'spu', '_tmp_gowatspm.elf')
    with open(tmp, 'wb') as f:
        f.write(wrap(raw, base=BASE, entry=BASE))
    cmd = [sys.executable, LIFTER, '--auto-functions', tmp, '--base', hex(BASE),
           '--symbol-prefix', NOME + '_', '-o', SAIDA,
           '--source-name', 'spu_recomp.c', '--header-name', 'spu_recomp.h',
           '--extra-funcs', hex(BASE)]
    r = subprocess.run(cmd, capture_output=True, text=True)
    os.remove(tmp)
    if r.returncode != 0:
        print('FALHOU', r.stderr[-600:])
        return
    print(r.stdout.strip().splitlines()[-3:])
    src = open(os.path.join(SAIDA, 'spu_recomp.c'), encoding='utf-8').read()
    cab = re.compile(r'^void ' + NOME + r'_spu_func_([0-9A-F]{8})\(spu_context\* ctx\) \{$', re.M)
    ms = list(cab.finditer(src))
    proib = re.compile(PROIBIDO + '|' + NOME + r'_spu_func_[0-9A-F]{8}\(ctx\)')
    fns = []
    for i, m in enumerate(ms):
        a = int(m.group(1), 16)
        fim = ms[i + 1].start() if i + 1 < len(ms) else len(src)
        if proib.search(src[m.end():fim]):
            continue
        n = (int(ms[i + 1].group(1), 16) if i + 1 < len(ms) else BASE + SIZE) - a
        if n < 32 and 'goto loc_' not in src[m.end():fim]:
            continue                      # pequena demais para compensar a chamada
        fns.append((a, max(4, min(n, 4096))))
    print(f'{len(fns)} folhas seguras de {len(ms)} funções')
    with open(INC, 'w', encoding='utf-8') as f:
        f.write('/* GERADO por scripts/lift_gowa_tspm.py: folhas seguras do policy module\n'
                ' * de taskset do libsre (EA 0x%X em LS 0x%X). LS, bytes, ponteiro. */\n' % (EA, BASE))
        for a, n in fns:
            f.write(f'extern "C" void {NOME}_spu_func_{a:08X}(void*);\n')
        f.write('static const uint32_t g_gowa_tspm_ea = 0x%Xu, g_gowa_tspm_ls = 0x%Xu;\n' % (EA, BASE))
        f.write('static const struct { uint32_t ls, len; void (*fn)(void*); } g_gowa_tspm_fns[] = {\n')
        for a, n in fns:
            f.write(f'    {{ 0x{a:05X}u, {n}u, {NOME}_spu_func_{a:08X} }},\n')
        f.write('};\n')
        for a, n in fns:
            pass


if __name__ == '__main__':
    main()
