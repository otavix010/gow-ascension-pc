"""Estabilidade quadro a quadro de uma rajada de capturas (captura_rajada.ps1)
com a cena parada: para cada pixel, o desvio entre capturas seguidas, sem os
quadros lavados (conta_piscadas.py). Grava um mapa de calor (onde a imagem
"mexe") e imprime a média por faixa da tela.
uso: estabilidade.py <pasta> [<pasta> ...]   -> <pasta>_mexe.png"""
import glob
import os
import sys

import numpy as np
from PIL import Image


def lavado(a):
    r, g, b = a[..., 0].mean(), a[..., 1].mean(), a[..., 2].mean()
    return b > 150 and b > r + 15 and (r + g + b) / 3 > 140


for pasta in sys.argv[1:]:
    arqs = sorted(glob.glob(os.path.join(pasta, 'cap_*.png')))
    quadros = []
    for a in arqs:
        im = Image.open(a).convert('RGB')
        w, h = im.size
        x = np.asarray(im.crop((8, 40, w - 8, h - 8)).resize((640, 360)), dtype=np.float32)
        if not lavado(x):
            quadros.append(x)
    if len(quadros) < 3:
        print(pasta, 'poucos quadros')
        continue
    q = np.stack(quadros)
    lum = q @ np.array([0.299, 0.587, 0.114], dtype=np.float32)
    dif = np.abs(np.diff(lum, axis=0))           # entre capturas seguidas
    mapa = dif.mean(axis=0)
    # picos: fração de pares com salto > 25 níveis no pixel
    picos = (dif > 25).mean(axis=0)
    print(f'{pasta}: {len(quadros)} quadros; desvio médio {mapa.mean():.2f}; '
          f'pixels com salto >25 em >10% dos pares: {(picos > 0.10).mean() * 100:.2f}%')
    for nome, (y0, y1) in (('céu (topo)', (0, 90)), ('meio', (90, 250)), ('chão', (250, 360))):
        print(f'   {nome:11s} desvio {mapa[y0:y1].mean():.2f}  picos {picos[y0:y1].mean() * 100:.2f}%')
    img = np.clip(mapa * 8, 0, 255).astype(np.uint8)
    Image.fromarray(img).save(pasta.rstrip('\\/') + '_mexe.png')
