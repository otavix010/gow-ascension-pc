"""Diferenças entre dois despejos de LS do gowa_fntrace (out/fntrace/<fn>_j<job>_<n>.bin: 2 KB de
registradores + 256 KB de LS): as faixas de LS que mudaram (o que o estágio entre os dois escreveu),
com as primeiras palavras antes/depois; e os registradores que mudaram.

uso: python ls_diff.py <antes.bin> <depois.bin> [--max 40] [--min-gap 16] [--floats]"""
import struct
import sys

a = open(sys.argv[1], 'rb').read()
b = open(sys.argv[2], 'rb').read()
mx = int(sys.argv[sys.argv.index('--max') + 1]) if '--max' in sys.argv else 40
gap = int(sys.argv[sys.argv.index('--min-gap') + 1]) if '--min-gap' in sys.argv else 16
flo = '--floats' in sys.argv
ga, gb = a[:2048], b[:2048]
la, lb = a[2048:], b[2048:]
regs = [r for r in range(128) if ga[16 * r:16 * r + 16] != gb[16 * r:16 * r + 16]]
print('registradores mudados:', ' '.join(f'r{r}' for r in regs[:40]))
faixas = []
i = 0
n = len(la)
while i < n:
    if la[i] == lb[i]:
        i += 1
        continue
    j = i
    k = i
    while j < n and (j - k) < gap:
        if la[j] != lb[j]:
            k = j
        j += 1
    faixas.append((i, k + 1))
    i = k + 1
tot = sum(e - s for s, e in faixas)
print(f'{len(faixas)} faixas, {tot} bytes mudados')
for s, e in faixas[:mx]:
    w = lambda buf: ' '.join(f'{struct.unpack(">f", buf[x:x+4])[0]:.4g}' if flo else buf[x:x+4].hex() for x in range(s & ~3, min(e, (s & ~3) + 32), 4))
    print(f'  0x{s:05X}..0x{e:05X} ({e - s:5d} B)  antes: {w(la)}')
    print(f'  {"":22s}  depois: {w(lb)}')
