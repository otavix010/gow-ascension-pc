"""Grafo de chamadas do código lifted do GOWA.

Uso:
  python quem_chama.py chamadores func_0063FEC4 [profundidade]
  python quem_chama.py chamados   func_0063FEC4
  python quem_chama.py opd 0x0063FEC4      (de onde vem o OPD -> chamadas indiretas)

Na 1ª execução indexa port/recompiled/*.cpp em out/callgraph.json (chamadas diretas
'func_X(ctx)' e tail calls 'g_trampoline_fn = ...func_X'). O índice é refeito
quando algum .cpp é mais novo que ele.
"""
import os as _os_raiz
RAIZ = _os_raiz.environ.get('GOWA_RAIZ') or _os_raiz.path.dirname(_os_raiz.path.dirname(_os_raiz.path.abspath(__file__)))  # raiz do projeto
import json
import re
import struct
import sys
from collections import defaultdict
from pathlib import Path

R = Path(RAIZ)
SRC = R / 'port' / 'recompiled'
IDX = R / 'out' / 'callgraph.json'
RE_DEF = re.compile(r'^void (func_[0-9A-F]{8})\(ppu_context\* ctx\) \{')
RE_CALL = re.compile(r'(func_[0-9A-F]{8})(?:\(ctx\)|;)')


def indexar():
    fontes = sorted(SRC.glob('ppu_recomp_[0-9][0-9][0-9].cpp'))
    if IDX.exists() and all(f.stat().st_mtime < IDX.stat().st_mtime for f in fontes):
        return json.loads(IDX.read_text())
    chama = defaultdict(set)
    onde = {}
    for f in fontes:
        atual = None
        for ln in f.read_bytes().decode('latin-1').splitlines():
            m = RE_DEF.match(ln)
            if m:
                atual = m.group(1)
                onde[atual] = f.name
                continue
            if atual:
                for c in RE_CALL.findall(ln):
                    if c != atual:
                        chama[atual].add(c)
    dados = {'chama': {k: sorted(v) for k, v in chama.items()}, 'onde': onde}
    IDX.write_text(json.dumps(dados))
    return dados


def main():
    d = indexar()
    chama = d['chama']
    inv = defaultdict(set)
    for a, cs in chama.items():
        for c in cs:
            inv[c].add(a)
    acao, alvo = sys.argv[1], sys.argv[2]
    if acao == 'chamados':
        print('\n'.join(chama.get(alvo, [])))
    elif acao == 'chamadores':
        prof = int(sys.argv[3]) if len(sys.argv) > 3 else 1
        def rec(f, n, ind):
            for c in sorted(inv.get(f, [])):
                print('  ' * ind + f'{c} ({d["onde"].get(c, "?")})')
                if n > 1:
                    rec(c, n - 1, ind + 1)
        rec(alvo, prof, 0)
        if not inv.get(alvo):
            print('(nenhuma chamada direta: provavelmente chamada indireta, ver "opd")')
    elif acao == 'opd':
        a = int(alvo, 16)
        fs = json.load(open(R / 'out' / 'GOWA.functions.json'))
        opds = [int(x['opd'], 16) for x in fs if int(x['start'], 16) == a]
        print('OPD:', [hex(o) for o in opds])
        elf = (R / 'jogo' / 'GOWA.elf').read_bytes()
        def rd(ea):
            return struct.unpack('>I', elf[0x810000 + ea - 0x820000:0x810000 + ea - 0x820000 + 4])[0]
        for o in opds:
            refs = [ea for ea in range(0x820000, 0x820000 + 0x3C470, 4) if rd(ea) == o]
            print(f'  referências a {o:#x} em .data/TOC: {[hex(r) for r in refs]} '
                  f'(TOC-rel: {[hex(r - 0x83F1F0) for r in refs]})')


if __name__ == '__main__':
    main()
