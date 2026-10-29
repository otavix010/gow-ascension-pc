"""Traduz para C os módulos de job WWS (C0DEC0DE) embutidos no GOWA.elf.

Cada módulo é copiado por DMA do seu endereço no GOWA.elf para o buffer de
código de job (LS 0x4000) e executado a partir de 0x4000 + entrada. Aqui cada
um é traduzido com base 0x4000 (como o tools/lift_jobmods.py do toolkit faz
para o LittleBigPlanet) e registrado como REGIÃO RESIDENTE pelo endereço de
origem (spu_overlay_register_region): o GET que traz o módulo marca a tradução
dele como residente e o runtime passa a executá-la em vez de interpretar.

Saída:
  port/spu/gowajm_<VA>/spu_recomp.{c,h}   (o CMake do port compila */spu_recomp.c)
  port/gowa_jobmods.inc                    (tabela usada por gowa_offline.cpp)

Uso: python lift_gowa_jobmods.py [--so VA,...]
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
from lift_jobmods import manual_link_targets, switch_table_targets  # noqa: E402

LIFTER = os.path.join(TOOLS, 'spu_lifter.py')
BASE = 0x4000
ELF = os.path.join(R, 'jogo', 'GOWA.elf')
OUTDIR = os.path.join(R, 'port', 'spu')
INC = os.path.join(R, 'port', 'gowa_jobmods.inc')
MERGE_CHUNKS = {0x0072DF80}
# 21/10: entradas que o tradutor não descobre (só se chega por desvio indireto) e que
# rodavam interpretadas ~20 mil vezes por segundo em cada workload do EDGE
# (GOWA_INTERP_ENTRY_LOG; endereços de LS, já com a base 0x4000)
ENTRADAS_EXTRA = {0x0072DF80: [0xC5E8, 0xC454, 0x63B0, 0x49F4, 0x6308, 0x6A68],   # 23/10: + 0x6A68 (~8 mil/s por workload)
                  # 23/10: GOWA_INTERP_ENTRY_LOG no cap. 16 com o xm1 -- entradas dos workloads de render
                  # (w13-w17), ~83 mil por segundo no total; 0x4260/0x4450/0x47C4/0x5304/0x4384/0x42DC
                  # são a mesma rotina nos dois módulos 0x781D80 e 0x792E00
                  0x006EF200: [0x48C4, 0x54D4, 0x44B0, 0x10240],
                  0x006FC080: [0x52E0, 0xCD58],
                  0x00781D80: [0x5754, 0x4260, 0x4450, 0x47C4, 0x68AC, 0x6830, 0x860C, 0x5304, 0x4384, 0x42DC],
                  0x00789F00: [0x6754, 0x4E58, 0x6534, 0x4F7C, 0x5048, 0x4ED4],
                  0x00792E00: [0x4260, 0x4450, 0x8B84, 0x47C4, 0x5304, 0x4384, 0x42DC],
                  # 29/10: GOWA_INTERP_ENTRY_LOG na fase 1 -- o workload 12, o que mixa o SOM (escreve os blocos do
                  # cellAudio), rodava ~0,3 M instruções/s interpretadas nesses dois módulos, o que atrasava o áudio
                  0x007B6680: [0x17008, 0x166A0, 0x170F4, 0x16F98, 0x1E060, 0x15300, 0x162E0, 0xA9D0, 0x153F4,
                               0x17AB4, 0x18690],
                  0x007D8600: [0xCD7C, 0xD05C, 0xCFA4, 0xCF0C, 0xCCD8, 0xA90C, 0xCE7C, 0x19090]}


def segmentos(d):
    phoff = struct.unpack_from('>Q', d, 0x20)[0]
    phnum = struct.unpack_from('>H', d, 0x38)[0]
    phsz = struct.unpack_from('>H', d, 0x36)[0]
    segs = []
    for i in range(phnum):
        p = phoff + i * phsz
        t = struct.unpack_from('>I', d, p)[0]
        off, va, _pa, fsz = struct.unpack_from('>QQQQ', d, p + 8)
        if t == 1 and fsz:
            segs.append((off, va, fsz))
    return segs


def main():
    so = None
    if '--so' in sys.argv:
        so = {int(x, 16) for x in sys.argv[sys.argv.index('--so') + 1].split(',')}
    d = open(ELF, 'rb').read()
    segs = segmentos(d)

    def f2va(o):
        for off, va, fsz in segs:
            if off <= o < off + fsz:
                return va + (o - off)
        return None

    inicios = [m.start() - 0x20 for m in re.finditer(rb'\xC0\xDE\xC0\xDE', d)]
    tabela = []
    tmp_elf = os.path.join(OUTDIR, '_tmp_gowajm.elf')
    for i, s in enumerate(inicios):
        va = f2va(s)
        entry_off, img_size = struct.unpack_from('>II', d, s + 0x10)
        prox = inicios[i + 1] if i + 1 < len(inicios) else len(d)
        n = min(img_size, prox - s)
        n -= n % 4
        raw = d[s:s + n]
        nome = f'gowajm_{va:08X}'
        if so and va not in so:
            tabela.append((va, n, nome, entry_off))
            continue
        entry = BASE + entry_off
        with open(tmp_elf, 'wb') as f:
            f.write(wrap(raw, base=BASE, entry=entry))
        extra = [hex(BASE + 0x30)]
        extra += [hex(a) for a in manual_link_targets(raw)]
        extra += [hex(a) for a in switch_table_targets(raw)]
        extra += [hex(a) for a in ENTRADAS_EXTRA.get(va, [])]
        cmd = [sys.executable, LIFTER, '--auto-functions', tmp_elf, '--base', hex(BASE),
               '--symbol-prefix', nome + '_', '-o', os.path.join(OUTDIR, nome),
               '--source-name', 'spu_recomp.c', '--header-name', 'spu_recomp.h',
               '--extra-funcs', ','.join(extra)]
        # 18/10: --merge-chunks (pedaços contíguos numa função C, goto em vez de
        # trampolim) só no módulo mais quente: +7% na fase com ele; nos outros
        # 30 juntos não acrescentou nada (A/B intercalado, PROGRESSO.md).
        if va in MERGE_CHUNKS:
            cmd.append('--merge-chunks')
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode != 0:
            print(f'FALHOU {nome}: {r.stderr[-400:]}')
            continue
        funcs = unsup = 0
        for linha in r.stdout.splitlines():
            t = linha.strip()
            if t.endswith('function(s) lifted'):
                funcs = int(t.split()[0])
            if 'unsupported instruction(s)' in t:
                unsup = int(t.split()[0])
        print(f'{nome}  arquivo=0x{s:06X} bytes=0x{n:05X} entrada=0x{entry:05X} funcoes={funcs} sem_traducao={unsup}')
        tabela.append((va, n, nome, entry_off))
    if os.path.exists(tmp_elf):
        os.remove(tmp_elf)

    with open(INC, 'w', encoding='utf-8') as f:
        f.write('/* GERADO por scripts/lift_gowa_jobmods.py: módulos de job WWS do GOWA.elf\n'
                ' * (VA de origem, bytes, função de registro). */\n')
        for va, n, nome, _e in tabela:
            f.write(f'extern "C" void {nome}_spu_recomp_register(void);\n')
        f.write('static const struct { uint32_t va, bytes; void (*reg)(void); } g_gowa_jobmods[] = {\n')
        for va, n, nome, _e in tabela:
            f.write(f'    {{ 0x{va:08X}u, 0x{n:X}u, {nome}_spu_recomp_register }},\n')
        f.write('};\n')
    print(f'{len(tabela)} módulos; tabela em {INC}')


if __name__ == '__main__':
    main()
