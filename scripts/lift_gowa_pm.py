"""Traduz o policy module do gerenciador de jobs WWS do God of War Ascension
(código cru do GOWA.elf em EA 0x79E500, 0x3000 bytes, que o kernel SPURS
copia para LS 0x0A00) e lista as funções folha seguras dele.

O PM continua interpretado (ele usa interrupções, canais e desvios indiretos),
mas as folhas -- sem chamadas, canais, desvios indiretos nem stop -- rodam
nativas, chamadas em linha pelo interpretador (spu_interp_hot_register) depois
de conferir os bytes do PM em LS 0x0A00 e os da função contra a imagem.
Fora do registro global de funções SPU: o .inc traz os ponteiros direto.

Saída: port/spu/gowapm_0079E500/spu_recomp.{c,h} e port/gowa_pm_hot.inc
"""
import os as _os_raiz
RAIZ = _os_raiz.environ.get('GOWA_RAIZ') or _os_raiz.path.dirname(_os_raiz.path.dirname(_os_raiz.path.abspath(__file__)))  # raiz do projeto
import os
import re
import struct
import subprocess
import sys

R = RAIZ
TOOLS = os.path.join(R, 'ps3recomp', 'tools')
sys.path.insert(0, TOOLS)
from wrap_spu_elf import wrap  # noqa: E402

LIFTER = os.path.join(TOOLS, 'spu_lifter.py')
EA, SIZE, BASE = 0x0079E500, 0x3000, 0x0A00
ELF = os.path.join(R, 'jogo', 'GOWA.elf')
NOME = f'gowapm_{EA:08X}'
SAIDA = os.path.join(R, 'port', 'spu', NOME)
INC = os.path.join(R, 'port', 'gowa_pm_hot.inc')
PROIBIDO = (r'spu_rdch|spu_wrch|spu_rchcnt|spu_indirect_branch|spu_drain_call|spu_halt|spu_stop|'
            r'spu_iret|spu_check_stack_reset|longjmp|SPU_STOP|spu_sync')


def main():
    d = open(ELF, 'rb').read()
    phoff = struct.unpack_from('>Q', d, 0x20)[0]
    phnum = struct.unpack_from('>H', d, 0x38)[0]
    phsz = struct.unpack_from('>H', d, 0x36)[0]
    off = None
    for i in range(phnum):
        p = phoff + i * phsz
        t = struct.unpack_from('>I', d, p)[0]
        o, va, _pa, fsz = struct.unpack_from('>QQQQ', d, p + 8)
        if t == 1 and va <= EA < va + fsz:
            off = o + (EA - va)
    raw = d[off:off + SIZE]
    tmp = os.path.join(R, 'port', 'spu', '_tmp_gowapm.elf')
    with open(tmp, 'wb') as f:
        f.write(wrap(raw, base=BASE, entry=BASE))
    cmd = [sys.executable, LIFTER, '--auto-functions', tmp, '--base', hex(BASE),
           '--symbol-prefix', NOME + '_', '-o', SAIDA,
           '--source-name', 'spu_recomp.c', '--header-name', 'spu_recomp.h',
           # 21/10: + 0x3378 (o tradutor não a descobre: só desvio indireto); rodava
           # interpretada ~22 mil vezes por segundo em cada workload do EDGE
           '--extra-funcs', ','.join(hex(x) for x in (BASE, 0x3378, 0xA68)),   # 23/10: + 0xA68 (meio do laço que gera o stub do tratador de interrupção; ~52 mil entradas/s por workload do EDGE)
           '--merge-chunks']   # 18/10: PM nativo (GOWA_PM_NATIVE); goto em vez de trampolim, A/B 23,25 x 22,90
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
        f.write('/* GERADO por scripts/lift_gowa_pm.py: folhas seguras do policy module WWS\n'
                ' * (EA 0x%X em LS 0x%X). LS, bytes, ponteiro. */\n' % (EA, BASE))
        for a, n in fns:
            f.write(f'extern "C" void {NOME}_spu_func_{a:08X}(void*);\n')
        f.write('static const uint32_t g_gowa_pm_ea = 0x%Xu, g_gowa_pm_ls = 0x%Xu;\n' % (EA, BASE))
        f.write('static const struct { uint32_t ls, len; void (*fn)(void*); } g_gowa_pm_fns[] = {\n')
        for a, n in fns:
            f.write(f'    {{ 0x{a:05X}u, {n}u, {NOME}_spu_func_{a:08X} }},\n')
        f.write('};\n')


if __name__ == '__main__':
    main()
