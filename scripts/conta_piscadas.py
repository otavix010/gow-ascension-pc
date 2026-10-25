"""Conta quadros "lavados" (a piscada do Capítulo 16: tela inteira azul-lavanda
clara) numa pasta de capturas da janela (captura_rajada.ps1).
uso: conta_piscadas.py <pasta> [<pasta> ...]"""
import glob
import os
import sys

from PIL import Image, ImageStat

for pasta in sys.argv[1:]:
    arqs = sorted(glob.glob(os.path.join(pasta, 'cap_*.png')))
    lav = []
    for a in arqs:
        im = Image.open(a).convert('RGB')
        w, h = im.size
        im = im.crop((8, 40, w - 8, h - 8)).resize((160, 90))
        r, g, b = ImageStat.Stat(im).mean
        if b > 150 and b > r + 15 and (r + g + b) / 3 > 140:
            lav.append(os.path.basename(a))
    print(f'{pasta}: {len(lav)} de {len(arqs)} quadros lavados ({100.0 * len(lav) / max(1, len(arqs)):.1f}%)'
          + (f' -- {", ".join(lav[:8])}' if lav else ''))
