"""Para cada .ppm de uma pasta (dumps do live-draw), imprime pixels não pretos,
média RGB e o maior valor. Uso: python contar_ppm.py <pasta> [filtro]"""
import os
import sys
from PIL import Image

d = sys.argv[1]
filt = sys.argv[2] if len(sys.argv) > 2 else ''
for f in sorted(os.listdir(d)):
    if not f.endswith('.ppm') or filt not in f:
        continue
    im = Image.open(os.path.join(d, f)).convert('RGB')
    px = im.getdata()
    n = len(px)
    nb = sum(1 for p in px if p != (0, 0, 0))
    sr = sum(p[0] for p in px) / n
    sg = sum(p[1] for p in px) / n
    sb = sum(p[2] for p in px) / n
    mx = max(max(p) for p in px)
    print(f'{f:40s} {im.size[0]}x{im.size[1]} naopretos={nb:7d} media=({sr:5.1f},{sg:5.1f},{sb:5.1f}) max={mx}')
