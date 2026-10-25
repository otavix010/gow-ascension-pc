"""Plota UVs (out/uv_draw<n>.txt, da sonda GOWA_ATTR_DUMP_KEY) sobre a textura
(PNG decodificado), em três leituras: como estão com WRAP (frac), com u
espelhado (|u|) e com u -> -u e v -> 1-v. Uma cor por desenho.
uso: uv_plot.py textura.png saida.png uv_draw1.txt [uv_draw2.txt ...]"""
import math
import sys

from PIL import Image, ImageDraw

tex = Image.open(sys.argv[1]).convert('RGB').resize((512, 1024))
cores = [(255, 0, 0), (0, 255, 0), (0, 128, 255), (255, 0, 255), (255, 255, 0), (0, 255, 255),
         (255, 128, 0), (128, 0, 255), (255, 255, 255), (0, 0, 0), (128, 255, 128), (255, 128, 128)]
leituras = [('frac', lambda u, v: (u - math.floor(u), v - math.floor(v))),
            ('|u|', lambda u, v: (abs(u) % 1.0, v - math.floor(v))),
            ('-u,1-v', lambda u, v: ((-u) % 1.0, (1.0 - v) % 1.0))]
folha = Image.new('RGB', (512 * len(leituras), 1024 + 20), (40, 40, 40))
for k, (nome, f) in enumerate(leituras):
    im = tex.copy()
    d = ImageDraw.Draw(im)
    for j, arq in enumerate(sys.argv[3:]):
        c = cores[j % len(cores)]
        for ln in open(arq):
            p = ln.split()
            if len(p) < 2:
                continue
            x, y = f(float(p[0]), float(p[1]))
            d.ellipse((x * 511 - 3, y * 1023 - 3, x * 511 + 3, y * 1023 + 3), fill=c)
    folha.paste(im, (512 * k, 20))
    ImageDraw.Draw(folha).text((512 * k + 4, 4), nome, fill=(255, 255, 0))
folha.save(sys.argv[2])
print('ok', sys.argv[2])
