"""Insere (ou remove) o gancho de bissecção GOWA_FNSKIP nas funções de um módulo de job traduzido.

uso: python gancho_fnskip.py <dir do módulo, ex. port/spu/gowajm_006FC080> [--remover]

Em cada função `void <prefixo>_spu_func_XXXXXXXX(spu_context* ctx) {` acrescenta, logo na
primeira linha do corpo:
    if (gowa_fnskip(ctx, 0xXXXXXXXX)) return;
gowa_fnskip (port/gowa_offline.cpp) roda a função no interpretador quando o endereço está
numa faixa de GOWA_FNSKIP; sem a variável, custa uma comparação. Idempotente; antes de
alterar grava <arquivo>.sem_gancho (cópia do original) se ainda não existir.
"""
import re
import shutil
import sys
from pathlib import Path

MARCA = '/* gowa_fnskip */'
RE_DEF = re.compile(r'^void (\w+_spu_func_([0-9A-F]{8}))\(spu_context\* ctx\) \{\s*$')


def main():
    d = Path(sys.argv[1])
    remover = '--remover' in sys.argv
    for f in sorted(d.glob('*.c')):
        txt = f.read_text(encoding='utf-8', errors='replace')
        nl = '\r\n' if '\r\n' in txt[:5000] else '\n'
        linhas = txt.split(nl)
        if remover:
            novas = [l for l in linhas if MARCA not in l]
            f.write_text(nl.join(novas), encoding='utf-8', newline='')
            print(f'{f.name}: {len(linhas) - len(novas)} linhas removidas')
            continue
        if any(MARCA in l for l in linhas):
            print(f'{f.name}: já tem o gancho')
            continue
        bak = f.with_suffix(f.suffix + '.sem_gancho')
        if not bak.exists():
            shutil.copy2(f, bak)
        novas, n = [], 0
        decl_feita = False
        for l in linhas:
            if not decl_feita and l.startswith('#include "../../runtime/spu/spu_helpers.h"'):
                novas.append(l)
                novas.append('extern int gowa_fnskip(spu_context* ctx, unsigned addr); ' + MARCA)
                decl_feita = True
                continue
            novas.append(l)
            m = RE_DEF.match(l)
            if m:
                novas.append(f'        if (gowa_fnskip(ctx, 0x{m.group(2)}u)) return; {MARCA}')
                n += 1
        if not decl_feita:
            novas.insert(0, 'extern int gowa_fnskip(spu_context* ctx, unsigned addr); ' + MARCA)
        f.write_text(nl.join(novas), encoding='utf-8', newline='')
        print(f'{f.name}: gancho em {n} funções')


main()
