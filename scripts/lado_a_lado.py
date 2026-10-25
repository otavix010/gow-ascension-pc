"""Lado a lado: quadros da nossa build contra quadros de referência (storyboard do
YouTube), cada par numa linha, com média de cor de uma faixa do céu (topo).
uso: lado_a_lado.py saida.png nosso1.ppm:ref1.png nosso2.ppm:ref2.png ..."""
import sys

from PIL import Image, ImageDraw, ImageStat

W, H = 640, 360
pares = [a.split('|') for a in sys.argv[2:]]
m = Image.new('RGB', (W * 2, H * len(pares)), 'black')
d = ImageDraw.Draw(m)
for r, (a, b) in enumerate(pares):
    for c, f in enumerate((a, b)):
        im = Image.open(f).convert('RGB').resize((W, H), Image.BICUBIC)
        m.paste(im, (c * W, r * H))
        topo = ImageStat.Stat(im.crop((0, 0, W, H // 5))).mean
        d.text((c * W + 6, r * H + 4), ('nossa ' if c == 0 else 'original ') + f.replace('\\', '/').split('/')[-1]
               + '  topo=(%d,%d,%d)' % tuple(int(x) for x in topo), fill='yellow')
m.save(sys.argv[1])
print(sys.argv[1], m.size)
