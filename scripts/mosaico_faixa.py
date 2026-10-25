"""Mosaico de variantes numa faixa de quadros: uma linha por variante, os quadros
de índice INI a FIM (passo PASSO) de cada pasta. uso:
mosaico_faixa.py RAIZ SAIDA INI FIM PASSO v1 v2 ..."""
import os
import sys

from PIL import Image, ImageDraw

raiz, saida, ini, fim, passo = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5])
nomes = sys.argv[6:]
idx = list(range(ini, fim + 1, passo))
W, H = 400, 225
m = Image.new('RGB', (W * len(idx), H * len(nomes)), 'black')
d = ImageDraw.Draw(m)
for r, n in enumerate(nomes):
    for c, i in enumerate(idx):
        f = os.path.join(raiz, n, 'frame_%06d.ppm' % i)
        if os.path.exists(f):
            m.paste(Image.open(f).convert('RGB').resize((W, H)), (c * W, r * H))
    d.text((6, r * H + 4), n, fill='yellow')
m.save(saida)
print(saida, m.size)
