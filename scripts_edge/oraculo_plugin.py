"""Oráculo de um plugin do EDGE: interpreta a versão sem promoção de registradores (spu_recomp.c.sem_promocao)
das funções dadas, em sequência (as passagens por trampolim viram desvios), a partir das fotos da entrada e
compara com as fotos da saída (sonda_fotos.py). O deslocamento do plugin na LS (g_spu_hot_reloc) é achado
procurando os bytes do código dele (do GOWA.elf) na LS.

uso: python -I oraculo_plugin.py <pasta do plugin> <ea do plugin hex> <função1,função2,...> <pasta das fotos>"""
import glob
import importlib.util
import os
import struct
import subprocess
import sys

aqui = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location('si', os.path.join(aqui, 'spu_interp_c.py'))
si = importlib.util.module_from_spec(spec); spec.loader.exec_module(si)

pasta, ea, fns, fotos = sys.argv[1], int(sys.argv[2], 16), sys.argv[3].split(','), sys.argv[4]
raiz = os.path.join(aqui, '..')
s = open(os.path.join(raiz, 'port', 'spu', pasta, 'spu_recomp.c.sem_promocao'), encoding='latin-1').read()
corpo = ''
for fn in fns:
    cab = f'void {pasta}_spu_func_{fn}(spu_context* ctx) {{\n'
    i = s.index(cab) + len(cab)
    corpo += f'loc_{int(fn, 16):08X}:\n' if f'loc_{int(fn, 16):08X}:' not in s[i:s.index(chr(10) + '}' + chr(10), i)] else ''
    corpo += s[i:s.index('\n}\n', i)] + '\n'
prog, rot = si.compila(corpo)
rot.setdefault('loc_FIM', len(prog))
codigo = bytes.fromhex(subprocess.run([sys.executable, '-I', os.path.join(aqui, 'elf_ea.py'), os.path.join(raiz, 'jogo', 'GOWA.elf'),
                                       f'{ea:X}', '40'], capture_output=True, text=True).stdout.strip())
ok = dif = 0
for p0 in sorted(glob.glob(os.path.join(fotos, 'pl_foto_*_0.bin')), key=lambda x: int(os.path.basename(x).split('_')[2])):
    k = int(os.path.basename(p0).split('_')[2])
    r0, ls0 = si.carrega(p0)
    r1, ls1 = si.carrega(p0.replace('_0.bin', '_1.bin'))
    pos = ls0.find(codigo)
    if pos < 0:
        print(f'chamada {k}: código do plugin não achado na LS'); continue
    maq = si.Maquina([list(r) for r in r0], ls0)
    maq.reloc = pos - 0x30000
    n = si.roda(prog, rot, maq)
    dr = [j for j in range(128) if maq.g[j] != r1[j]]
    dl = [j for j in range(0x40000) if maq.ls[j] != ls1[j]]
    bom = not dr and not dl
    ok += bom; dif += not bom
    print(f'chamada {k:2d}: plugin em 0x{pos:05X}, {n} instruções | regs dif {dr[:8]} | LS dif {len(dl)}' + (f' (1a em 0x{dl[0]:05X})' if dl else ''))
print(f'{ok} iguais, {dif} diferentes')
