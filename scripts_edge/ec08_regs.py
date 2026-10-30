"""Registradores de entrada do laço EC08 (E550, skinning) numa foto de loc_0000EB5C, em hex e como float."""
import struct
import sys

b = open(sys.argv[1], 'rb').read()
regs = [struct.unpack('<4I', b[i * 16:(i + 1) * 16]) for i in range(128)]
ls = b[2048:]
f = lambda w: struct.unpack('<f', struct.pack('<I', w))[0]
for r in (9, 34, 35, 36, 37, 48, 49, 50, 51, 63, 67, 69, 70, 77, 81, 82, 83, 84, 86, 89, 91, 93, 94, 95, 97, 100, 101, 102):
    v = regs[r]
    print(f'r{r:<3} ' + ' '.join(f'{w:08X}' for w in v) + '   ' + ' '.join(f'{f(w):11.4g}' for w in v))
r83 = regs[83][0] & 0x3FFF0
print('fluxo r83 (ossos/pesos):', ls[r83:r83 + 32].hex(' ', 2))
r86 = regs[86][0] & 0x3FFF0
print('posições r86:', [round(x, 3) for x in struct.unpack('>8f', ls[r86:r86 + 32])])
