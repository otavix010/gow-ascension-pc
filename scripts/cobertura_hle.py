"""Cruza os imports de um ELF (GOWA.imports.json do ppu_loader) com as funções HLE que o ps3recomp registra.

Uso: python cobertura_hle.py PS3RECOMP_DIR IMPORTS_JSON [--nomes]
Marca como 'stub' a função registrada cujo corpo parece placeholder (só log/return CELL_OK, TODO, UNIMPLEMENTED).
"""
import hashlib
import json
import re
import struct
import sys
from collections import defaultdict
from pathlib import Path

SUFIXO = bytes([0x67, 0x59, 0x65, 0x99, 0x04, 0x25, 0x04, 0x90, 0x56, 0x64, 0x27, 0x49, 0x94, 0x89, 0x74, 0x1A])


def nid(nome):
    return struct.unpack('<I', hashlib.sha1(nome.encode() + SUFIXO).digest()[:4])[0]


REG = re.compile(r'\b(?:REGISTER_FUNC|REG_FUNC)\s*\(\s*(?:\w+\s*,\s*)?(\w+)\s*\)')
REG_N = re.compile(r'\b(?:REGISTER_FNID|REG_FNID)\s*\(\s*(?:\w+\s*,\s*)?(0x[0-9A-Fa-f]+)\s*,\s*(\w+)\s*\)')
PLACEHOLDER = re.compile(r'UNIMPLEMENTED|TODO|stub|not implemented', re.I)


def corpo(fonte, nome):
    m = re.search(r'\b' + re.escape(nome) + r'\s*\([^;{]*\)\s*\{', fonte)
    if not m:
        return None
    i, prof = m.end(), 1
    while prof and i < len(fonte):
        prof += {'{': 1, '}': -1}.get(fonte[i], 0)
        i += 1
    return fonte[m.end():i - 1]


def main():
    raiz, imports = Path(sys.argv[1]), json.load(open(sys.argv[2]))
    # Mesma descoberta que o gen_hle_nids.py --all usa para montar a tabela HLE do runtime
    sys.path.insert(0, str(raiz / 'tools'))
    import gen_hle_nids
    registradas = {}  # nid -> (nome, arquivo, stub?)
    dirs = ["system", "filesystem", "input", "audio", "video", "network",
            "spurs", "sync", "codec", "font", "misc", "hardware"]
    arquivos = [p for d in dirs for p in (raiz / 'libs' / d).glob('*.c')]
    for arq in arquivos:
        fonte = arq.read_text(encoding='latin1')
        pares = gen_hle_nids.scan(str(arq)) + [(nid(n), n) for n in REG.findall(fonte)] + [(int(h, 16), n) for h, n in REG_N.findall(fonte)]
        for v, n in pares:
            b = corpo(fonte, n)
            stub = b is not None and (len(b.strip().splitlines()) <= 3 or bool(PLACEHOLDER.search(b)))
            registradas.setdefault(v, (n, arq.relative_to(raiz).as_posix(), stub))

    por_lib = defaultdict(lambda: [0, 0, 0, []])
    for imp in imports:
        v = int(imp['nid'], 16)
        e = por_lib[imp['library']]
        e[0] += 1
        if v in registradas:
            e[1] += 1
            if registradas[v][2]:
                e[2] += 1
        else:
            e[3].append(imp['nid'])

    tot = [0, 0, 0]
    print(f"{'biblioteca':26} {'usa':>4} {'tem':>4} {'stub?':>5}  faltando")
    for lib, (n, ok, st, falt) in sorted(por_lib.items(), key=lambda x: -x[1][0]):
        tot[0] += n; tot[1] += ok; tot[2] += st
        print(f"{lib:26} {n:4} {ok:4} {st:5}  {len(falt)}")
    print(f"{'TOTAL':26} {tot[0]:4} {tot[1]:4} {tot[2]:5}  {tot[0] - tot[1]}")
    print(f'({len(registradas)} NIDs registrados no ps3recomp)')
    if '--nomes' in sys.argv:
        for lib, (_n, _ok, _st, falt) in sorted(por_lib.items()):
            if falt:
                print(lib, ' '.join(falt))


if __name__ == '__main__':
    main()
