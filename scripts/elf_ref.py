"""Procura no GOWA.elf (big-endian) as palavras de 32 bits iguais a um valor
(ex.: endereço de uma string) e diz o endereço virtual de cada ocorrência —
para achar a entrada da TOC usada pelo código.

uso: python elf_ref.py <valor hex> [<valor hex> ...]"""
import os as _os_raiz
RAIZ = _os_raiz.environ.get('GOWA_RAIZ') or _os_raiz.path.dirname(_os_raiz.path.dirname(_os_raiz.path.abspath(__file__)))  # raiz do projeto
import struct
import sys

elf = open(_os_raiz.path.join(RAIZ, r'jogo\GOWA.elf'), 'rb').read()
phoff = struct.unpack_from('>Q', elf, 0x20)[0]
phentsize, phnum = struct.unpack_from('>HH', elf, 0x36)
segs = []
for i in range(phnum):
    p_type, p_flags, p_off, p_vaddr, p_paddr, p_filesz = struct.unpack_from('>IIQQQQ', elf, phoff + i * phentsize)
    if p_type == 1:
        segs.append((p_off, p_vaddr, p_filesz))
for a in sys.argv[1:]:
    v = struct.pack('>I', int(a, 16))
    for off, va, sz in segs:
        blob = elf[off:off + sz]
        k = blob.find(v)
        while k >= 0:
            if k % 4 == 0:
                print(f'{a}: em 0x{va + k:08X}')
            k = blob.find(v, k + 1)
