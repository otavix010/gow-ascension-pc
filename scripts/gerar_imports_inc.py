"""Gera port/gowa_imports.inc (tabela C com os imports do GOWA) a partir de out/GOWA.imports.json."""
import os as _os_raiz
RAIZ = _os_raiz.environ.get('GOWA_RAIZ') or _os_raiz.path.dirname(_os_raiz.path.dirname(_os_raiz.path.abspath(__file__)))  # raiz do projeto
import json
from pathlib import Path

R = Path(RAIZ)
d = json.load(open(R / 'out' / 'GOWA.imports.json'))
linhas = ['/* Gerado por scripts/gerar_imports_inc.py a partir de out/GOWA.imports.json:',
          ' * os imports do GOWA.elf (nid, biblioteca). */',
          'static const struct { uint32_t nid; const char* lib; } g_gowa_imports[] = {']
for x in d:
    linhas.append('    { 0x%08Xu, "%s" },' % (int(x['nid'], 16), x['library']))
linhas.append('};')
(R / 'port' / 'gowa_imports.inc').write_text('\n'.join(linhas) + '\n', encoding='utf-8')
print(len(d), 'imports')
