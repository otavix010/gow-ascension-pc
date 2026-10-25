"""Mosaico das variantes de out/zvar/<variante>/frame_*.ppm (variantes_zcolor.ps1):
uma linha por variante, os primeiros quadros de cada uma lado a lado.
uso: python mosaico_zvar.py [saida.png]"""
import os as _os_raiz
RAIZ = _os_raiz.environ.get('GOWA_RAIZ') or _os_raiz.path.dirname(_os_raiz.path.dirname(_os_raiz.path.abspath(__file__)))  # raiz do projeto
import glob
import os
import sys
from PIL import Image, ImageDraw

R = _os_raiz.path.join(RAIZ, r'out\zvar')
out = sys.argv[1] if len(sys.argv) > 1 else os.path.join(R, 'mosaico.png')
vars_ = [v for v in ['o0', 'o1', 'o2', 'o3', 'off'] if os.path.isdir(os.path.join(R, v))]
cols = 3
W, H = 640, 360
m = Image.new('RGB', (W * cols, H * len(vars_)))
for r, v in enumerate(vars_):
    fs = sorted(glob.glob(os.path.join(R, v, '*.ppm')))[:cols]
    for c, f in enumerate(fs):
        m.paste(Image.open(f).convert('RGB').resize((W, H)), (c * W, r * H))
    ImageDraw.Draw(m).text((6, r * H + 6), v, fill=(255, 255, 0))
m.save(out)
print(out, m.size)
