"""Quadros de referência de um vídeo do YouTube pelo storyboard (as folhas de
miniaturas da barra de busca), sem baixar o vídeo: lê a especificação na página
já baixada (watch_<id>.html), baixa as folhas do nível de maior resolução até
`ate_s` segundos e grava cada quadro como PNG com o tempo no nome.

uso: python yt_storyboard.py PAGINA.html PASTA_SAIDA [ate_s]"""
import os
import re
import sys
import urllib.parse
import urllib.request

from PIL import Image

pagina, saida = sys.argv[1], sys.argv[2]
ate = float(sys.argv[3]) if len(sys.argv) > 3 else 900
html = open(pagina, encoding='utf-8', errors='replace').read()
m = re.search(r'"spec":"(https://i\.ytimg\.com/sb/[^"]+)"', html)
if not m:
    m = re.search(r'(https://i\.ytimg\.com/sb/[^"]+)', html)
spec = m.group(1).encode().decode('unicode_escape')
partes = spec.split('|')
base = partes[0]
niveis = []
for i, p in enumerate(partes[1:]):
    c = p.split('#')
    w, h, n, cols, rows, iv = int(c[0]), int(c[1]), int(c[2]), int(c[3]), int(c[4]), int(c[5])
    nome, sigh = c[6], c[7]
    niveis.append((i, w, h, n, cols, rows, iv, nome, sigh))
lv = max(niveis, key=lambda x: x[1])
i, w, h, n, cols, rows, iv, nome, sigh = lv
print(f'nível {i}: {w}x{h}, {n} quadros, {cols}x{rows} por folha, intervalo {iv} ms')
os.makedirs(saida, exist_ok=True)
por_folha = cols * rows
quadros = min(n, int(ate * 1000 / iv) + 1) if iv else n
folhas = (quadros + por_folha - 1) // por_folha
for f in range(folhas):
    url = base.replace('$L', str(i)).replace('$N', nome.replace('$M', str(f)))
    url += ('&' if '?' in url else '?') + 'sigh=' + urllib.parse.quote(sigh, safe='')
    req = urllib.request.Request(url, headers={'User-Agent': 'Mozilla/5.0'})
    arq = os.path.join(saida, f'folha_{f:03d}.jpg')
    with urllib.request.urlopen(req, timeout=30) as r, open(arq, 'wb') as o:
        o.write(r.read())
    img = Image.open(arq)
    for k in range(por_folha):
        q = f * por_folha + k
        if q >= quadros:
            break
        x, y = (k % cols) * w, (k // cols) * h
        t = q * iv / 1000.0
        img.crop((x, y, x + w, y + h)).save(os.path.join(saida, f'q_{int(t):05d}s.png'))
print(f'{quadros} quadros em {saida}')
