"""Decodifica texturas DXT1/DXT5 de um despejo de memória (GOWA_MEMDUMP) para PNG,
para ver a cor que está de fato na textura do jogo.
uso: dxt_png.py despejo.bin base_hex saida_dir off_hex:fmt:LxA [off_hex:fmt:LxA ...]
  fmt = 86 (DXT1) ou 88 (DXT5); off é o endereço da textura (mesma base do despejo)."""
import os
import struct
import sys

from PIL import Image


def c565(c):
    r, g, b = (c >> 11) & 31, (c >> 5) & 63, c & 31
    return (r * 255 // 31, g * 255 // 63, b * 255 // 31)


def bloco_cor(b, dxt1):
    c0, c1 = struct.unpack_from('<HH', b, 0)
    p0, p1 = c565(c0), c565(c1)
    if c0 > c1 or not dxt1:
        p2 = tuple((2 * x + y) // 3 for x, y in zip(p0, p1))
        p3 = tuple((x + 2 * y) // 3 for x, y in zip(p0, p1))
    else:
        p2 = tuple((x + y) // 2 for x, y in zip(p0, p1))
        p3 = (0, 0, 0)
    pal = [p0, p1, p2, p3]
    idx = struct.unpack_from('<I', b, 4)[0]
    return [pal[(idx >> (2 * i)) & 3] for i in range(16)]


def bloco_alfa(b):
    a0, a1 = b[0], b[1]
    if a0 > a1:
        pal = [a0, a1] + [((6 - i) * a0 + (i + 1) * a1) // 7 for i in range(6)]
    else:
        pal = [a0, a1] + [((4 - i) * a0 + (i + 1) * a1) // 5 for i in range(4)] + [0, 255]
    bits = int.from_bytes(b[2:8], 'little')
    return [pal[(bits >> (3 * i)) & 7] for i in range(16)]


dados = open(sys.argv[1], 'rb').read()
base = int(sys.argv[2], 16)
saida = sys.argv[3]
os.makedirs(saida, exist_ok=True)
for esp in sys.argv[4:]:
    off, fmt, tam = esp.split(':')
    off = int(off, 16)
    w, h = (int(x) for x in tam.split('x'))
    dxt1 = fmt == '86'
    bs = 8 if dxt1 else 16
    im = Image.new('RGBA', (w, h))
    px = im.load()
    p = off - base
    for by in range(h // 4):
        for bx in range(w // 4):
            b = dados[p:p + bs]
            p += bs
            cores = bloco_cor(b if dxt1 else b[8:], dxt1)
            alfas = [255] * 16 if dxt1 else bloco_alfa(b)
            for i in range(16):
                px[bx * 4 + i % 4, by * 4 + i // 4] = cores[i] + (alfas[i],)
    nome = os.path.join(saida, '%08X_%s_%dx%d.png' % (off, fmt, w, h))
    im.save(nome)
    rgb = im.convert('RGB').resize((1, 1), Image.BOX).getpixel((0, 0))
    print(nome, 'média', rgb)
