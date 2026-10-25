"""C decompilado pelo Ghidra (out/ghidra/decompiled.json).

Uso:
  python dec.py 0x0039C634 [0x...]       mostra as funções
  python dec.py --str texto              strings que contêm o texto + quem as referencia
  python dec.py --usa FUN_0039c634       funções cujo C chama/menciona o símbolo
"""
import os as _os_raiz
RAIZ = _os_raiz.environ.get('GOWA_RAIZ') or _os_raiz.path.dirname(_os_raiz.path.dirname(_os_raiz.path.abspath(__file__)))  # raiz do projeto
import json
import sys
from pathlib import Path

D = Path(_os_raiz.path.join(RAIZ, r'out\ghidra'))
_dec = None


def dec():
    global _dec
    if _dec is None:
        _dec = json.load(open(D / 'decompiled.json'))
    return _dec


def por_endereco(a):
    alvo = int(a, 16)
    for f in dec():
        if int(f['addr'], 16) == alvo:
            return f
    # função que contém o endereço: a de maior início <= alvo
    melhor = None
    for f in dec():
        x = int(f['addr'], 16)
        if x <= alvo and (melhor is None or x > int(melhor['addr'], 16)):
            melhor = f
    return melhor


def main(args):
    if args and args[0] == '--str':
        s = json.load(open(D / 'strings.json'))
        for e in s:
            if args[1].lower() in e['val'].lower():
                print(e['addr'], repr(e['val'][:100]), 'refs:', ' '.join(e['refs'][:8]))
        return
    if args and args[0] == '--usa':
        for f in dec():
            if args[1] in f['c']:
                print(f['addr'], f['name'])
        return
    for a in args:
        f = por_endereco(a)
        if f:
            print(f"// {f['addr']} {f['name']}")
            print(f['c'].replace('\r\n', '\n'))


if __name__ == '__main__':
    main(sys.argv[1:])
