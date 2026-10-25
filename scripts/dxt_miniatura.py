"""Miniaturas rápidas de texturas DXT1/DXT5 despejadas (GOWA_TEX_DUMP_SEC):
um pixel por bloco 4x4 (média das duas cores-base, e o alfa médio do bloco
no DXT5 num segundo quadro ao lado). Para reconhecer texturas, não para
conferir cor fina (para isso, dxt_png.py).
uso: dxt_miniatura.py saida.png arquivo.bin [arquivo.bin ...]
  (nome no formato do despejo: <loc>_<offset>_<fmt>_<L>x<A>_r<remap>.bin)"""
import os
import re
import sys

import numpy as np
from PIL import Image, ImageDraw

saida = sys.argv[1]
thumbs = []
for arq in sys.argv[2:]:
    m = re.search(r'_([0-9A-F]{2})_(\d+)x(\d+)_r', os.path.basename(arq))
    fmt, w, h = m.group(1), int(m.group(2)), int(m.group(3))
    d = np.fromfile(arq, dtype=np.uint8)
    bs = 8 if fmt == '86' else 16
    nb = (w // 4) * (h // 4)
    if fmt not in ('86', '87', '88') or d.size < nb * bs:
        continue
    b = d[:nb * bs].reshape(nb, bs)
    cb = b if bs == 8 else b[:, 8:]
    c0 = cb[:, 0].astype(np.uint32) | (cb[:, 1].astype(np.uint32) << 8)
    c1 = cb[:, 2].astype(np.uint32) | (cb[:, 3].astype(np.uint32) << 8)

    def rgb(c):
        return np.stack([((c >> 11) & 31) * 255 // 31, ((c >> 5) & 63) * 255 // 63, (c & 31) * 255 // 31], -1)
    col = ((rgb(c0) + rgb(c1)) // 2).astype(np.uint8).reshape(h // 4, w // 4, 3)
    im = Image.fromarray(col)
    if bs == 16:
        a = ((b[:, 0].astype(np.uint32) + b[:, 1]) // 2).astype(np.uint8).reshape(h // 4, w // 4)
        al = Image.fromarray(a).convert('RGB')
        both = Image.new('RGB', (im.width * 2, im.height))
        both.paste(im, (0, 0)); both.paste(al, (im.width, 0))
        im = both
    im.thumbnail((384, 384))
    thumbs.append((os.path.basename(arq), im))
cols = 4
cw, ch = 384, 404
folha = Image.new('RGB', (cols * cw, ((len(thumbs) + cols - 1) // cols) * ch), (40, 40, 40))
dr = ImageDraw.Draw(folha)
for i, (nome, im) in enumerate(thumbs):
    x, y = (i % cols) * cw, (i // cols) * ch
    folha.paste(im, (x, y + 20))
    dr.text((x + 2, y + 2), nome[:48], fill=(255, 255, 0))
folha.save(saida)
print(len(thumbs), 'miniaturas em', saida)
