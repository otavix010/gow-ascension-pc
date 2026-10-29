"""Lê bytes do GOWA.elf (ELF64 big-endian da PPU) por endereço virtual.
uso: python -I elf_ea.py <GOWA.elf> <ea hex> <tamanho hex> [quadwords: 1 = mostra em quadwords de 16 B]"""
import struct
import sys

elf, ea, n = sys.argv[1], int(sys.argv[2], 16), int(sys.argv[3], 16)
qw = len(sys.argv) > 4
b = open(elf, 'rb').read()
phoff, = struct.unpack('>Q', b[0x20:0x28])
phentsize, phnum = struct.unpack('>HH', b[0x36:0x3A])
for i in range(phnum):
    h = b[phoff + i * phentsize:phoff + (i + 1) * phentsize]
    tipo, flags, off, vaddr, paddr, filesz, memsz, align = struct.unpack('>IIQQQQQQ', h)
    if tipo == 1 and vaddr <= ea < vaddr + filesz:
        d = b[off + ea - vaddr:off + ea - vaddr + n]
        if qw:
            for k in range(0, len(d), 16):
                w = struct.unpack('>4I', d[k:k + 16].ljust(16, b'\0'))
                print(f'+0x{k:04X} (ea 0x{ea + k:08X}): ' + ' '.join(f'{x:08X}' for x in w))
        else:
            print(d.hex())
        break
else:
    print('endereço fora dos segmentos')
