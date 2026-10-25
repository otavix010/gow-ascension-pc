"""Mosaico das variantes de imagem: uma linha por variante (os 4 últimos quadros
despejados de cada uma), com o nome no canto. uso: mosaico_variantes.py RAIZ v1 v2 ..."""
import os
import sys

from PIL import Image, ImageDraw

raiz, nomes = sys.argv[1], sys.argv[2:]
W, H = 480, 270
m = Image.new('RGB', (W * 4, H * len(nomes)), 'black')
d = ImageDraw.Draw(m)
for r, n in enumerate(nomes):
    pasta = os.path.join(raiz, n)
    fs = sorted(f for f in os.listdir(pasta) if f.endswith('.ppm'))[-4:] if os.path.isdir(pasta) else []
    for c, f in enumerate(fs):
        m.paste(Image.open(os.path.join(pasta, f)).convert('RGB').resize((W, H)), (c * W, r * H))
    d.text((6, r * H + 4), n, fill='yellow')
saida = os.path.join(raiz, 'mosaico.png')
m.save(saida)
print(saida, m.size)
