"""Converte um despejo de memória A8R8G8B8 big-endian (bytes A,R,G,B) em PNG.
Uso: python argb_para_png.py entrada.bin largura altura [saida.png] [pitch]
Imprime também a média de cada canal e quantos pixels não pretos."""
import sys
from PIL import Image

src, w, h = sys.argv[1], int(sys.argv[2]), int(sys.argv[3])
out = sys.argv[4] if len(sys.argv) > 4 else src.rsplit('.', 1)[0] + '.png'
pitch = int(sys.argv[5]) if len(sys.argv) > 5 else w * 4
d = open(src, 'rb').read()
img = Image.new('RGB', (w, h))
px = img.load()
sa = sr = sg = sb = 0
nb = 0
for y in range(h):
    row = d[y * pitch:(y * pitch) + w * 4]
    for x in range(w):
        a, r, g, b = row[x * 4:x * 4 + 4]
        px[x, y] = (r, g, b)
        sa += a; sr += r; sg += g; sb += b
        nb += (r or g or b) != 0
n = w * h
print(f'{out}: media A={sa/n:.1f} R={sr/n:.1f} G={sg/n:.1f} B={sb/n:.1f} naopretos={nb}')
img.save(out)
