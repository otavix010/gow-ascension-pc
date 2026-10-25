"""Lista os imports do GOWA que não têm handler: nem na tabela HLE gerada
(ppu_hle_nids.cpp), nem nos registros diretos do scaffold (ppu_sysprx.cpp e
afins, via hex no fonte), nem nos exports da libsre LLE.

Uso: python imports_sem_handler.py   (caminhos fixos do projeto)
"""
import os as _os_raiz
RAIZ = _os_raiz.environ.get('GOWA_RAIZ') or _os_raiz.path.dirname(_os_raiz.path.dirname(_os_raiz.path.abspath(__file__)))  # raiz do projeto
import json
import re
import sys
from pathlib import Path

R = Path(RAIZ)
sys.path.insert(0, str(R / 'ps3recomp' / 'tools'))
import nid_database  # noqa: E402

imports = json.load(open(R / 'out' / 'GOWA.imports.json'))
tabela = (R / 'port' / 'recompiled' / 'ppu_hle_nids.cpp').read_text(errors='replace')
nids_tab = {int(x, 16) for x in re.findall(r'0x[0-9A-Fa-f]{8}', tabela)}
scaffold = ''
for f in (R / 'ps3recomp' / 'runtime' / 'ppu').glob('*.cpp'):
    scaffold += f.read_text(errors='replace')
scaffold += (R / 'port' / 'stubs.cpp').read_text(errors='replace')
nids_scaf = {int(x, 16) for x in re.findall(r'0x[0-9A-Fa-f]{8}', scaffold)}
sre = json.load(open(R / 'fw' / 'lle' / 'libsre_exports.json'))
nids_sre = {int(n, 16) for lib in sre['exports'].values() for n in lib}

db = nid_database.get_default_db()


def nome(n):
    r = db.lookup_nid(n)
    return r[1] if r else '?'


falta = {}
for imp in imports:
    n = int(imp['nid'], 16)
    if n in nids_tab or n in nids_sre or n in nids_scaf:
        continue
    falta.setdefault(imp['library'], []).append((n, nome(n)))
tot = sum(len(v) for v in falta.values())
print(f'{tot} de {len(imports)} imports sem handler')
for lib, l in sorted(falta.items(), key=lambda kv: -len(kv[1])):
    print(f'  {lib} ({len(l)}): ' + ', '.join(f'{nm}[{n:08X}]' for n, nm in l))
