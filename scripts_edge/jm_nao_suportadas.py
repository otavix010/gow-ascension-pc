"""Quais instruções o lifter não traduz num módulo de job (o lift_gowa_jobmods.py só conta).
Traduz num diretório temporário, sem mexer em port/spu. uso: python -I jm_nao_suportadas.py 7B6680 [saída]"""
import collections
import os
import re
import struct
import subprocess
import sys

R = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
sys.path.insert(0, os.path.join(R, 'scripts'))
sys.path.insert(0, os.path.join(R, 'ps3recomp', 'tools'))
import lift_gowa_jobmods as L  # noqa: E402
from wrap_spu_elf import wrap  # noqa: E402

alvo = int(sys.argv[1], 16)
saida = sys.argv[2] if len(sys.argv) > 2 else os.path.join(R, 'out', 'jm_tmp')
d = open(L.ELF, 'rb').read()
segs = L.segmentos(d)
inicios = [m.start() - 0x20 for m in re.finditer(rb'\xC0\xDE\xC0\xDE', d)]
for i, s in enumerate(inicios):
    va = next(v + (s - o) for o, v, f in segs if o <= s < o + f)
    if va != alvo:
        continue
    entry_off, img_size = struct.unpack_from('>II', d, s + 0x10)
    prox = inicios[i + 1] if i + 1 < len(inicios) else len(d)
    n = min(img_size, prox - s)
    n -= n % 4
    raw = d[s:s + n]
    os.makedirs(saida, exist_ok=True)
    tmp = os.path.join(saida, 'mod.elf')
    open(tmp, 'wb').write(wrap(raw, base=L.BASE, entry=L.BASE + entry_off))
    r = subprocess.run([sys.executable, L.LIFTER, '--auto-functions', tmp, '--base', hex(L.BASE), '--symbol-prefix', 'x_',
                        '-o', saida, '--source-name', 'spu_recomp.c', '--header-name', 'spu_recomp.h'],
                       capture_output=True, text=True)
    linhas = (r.stdout + r.stderr).splitlines()
    for l in linhas:
        if 'unsupported' in l.lower() or 'lifted' in l:
            print(l[:200])
    # o C gerado marca cada instrução não suportada
    c = open(os.path.join(saida, 'spu_recomp.c'), encoding='latin-1').read()
    cnt = collections.Counter(re.findall(r'unsupported[^\n]*?([a-z][a-z0-9_.]+)', c, re.I))
    print('no C:', cnt.most_common(15))
    exemplos = [m.group(0)[:160] for m in re.finditer(r'[^\n]*unsupported[^\n]*', c, re.I)][:6]
    print('\n'.join(exemplos))
