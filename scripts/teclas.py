"""Gera ps3recomp/libs/video/rsx_teclas.h: os ícones de botão do PS3 que o HUD do jogo usa (texturas DXT5 de 64x64
com mipmaps), desenhados de novo com as teclas e o mouse de cellPad.c (report 37 do launcher). O renderizador
reconhece cada ícone pelo hash FNV-1a do nível base (4096 bytes) e, com o teclado em uso, entrega estes no lugar.
Os hashes saíram do despejo GOWA_TEX_DUMP_AROUND do jogo (só os números ficam aqui, nada do jogo).
uso: py -I scripts/teclas.py [pasta para a prévia]"""
import os
import sys

from PIL import Image, ImageDraw, ImageFont

FONTE = 'C:/Windows/Fonts/arialbd.ttf'
BRANCO, LARANJA = (245, 245, 245, 255), (255, 176, 32, 255)


def fonte(t):
    return ImageFont.truetype(FONTE, t)


def tecla(img, caixa, raio=12):
    """uma tecla escura com borda clara, como os botões do jogo"""
    d = ImageDraw.Draw(img)
    x0, y0, x1, y1 = caixa
    d.rounded_rectangle(caixa, raio, fill=(20, 20, 22, 255), outline=(150, 150, 155, 255), width=3)
    d.rounded_rectangle((x0 + 4, y0 + 4, x1 - 4, (y0 + y1) // 2), raio - 4, fill=(48, 48, 52, 255))


def texto(img, s, tam, cor, centro=(32, 32)):
    d = ImageDraw.Draw(img)
    f = fonte(tam)
    l, t, r, b = d.textbbox((0, 0), s, font=f)
    x, y = centro[0] - (r - l) / 2 - l, centro[1] - (b - t) / 2 - t
    d.text((x, y), s, font=f, fill=cor, stroke_width=2, stroke_fill=(0, 0, 0, 255))


def desenhar(nome, cor):
    img = Image.new('RGBA', (64, 64), (0, 0, 0, 0))
    c = LARANJA if cor == 'laranja' else BRANCO
    if len(nome) == 1:  # uma tecla com uma letra
        tecla(img, (4, 4, 60, 60))
        texto(img, nome, 34, c, (32, 33))
    elif nome == 'espaco':
        tecla(img, (1, 16, 63, 48), 10)
        texto(img, 'ESPAÇO', 13, c, (32, 33))
    elif nome in ('mouse_esq', 'mouse_dir'):
        d = ImageDraw.Draw(img)
        d.rounded_rectangle((14, 4, 50, 60), 17, fill=(20, 20, 22, 255), outline=(150, 150, 155, 255), width=3)
        d.line((32, 6, 32, 26), fill=(150, 150, 155, 255), width=2)
        d.line((16, 26, 48, 26), fill=(150, 150, 155, 255), width=2)
        lado = (17, 7, 30, 24) if nome == 'mouse_esq' else (34, 7, 47, 24)
        d.rounded_rectangle(lado, 5, fill=c)
    elif nome in ('wasd', 'shift_wasd'):
        tecla(img, (2, 2, 62, 62))
        if nome == 'wasd':
            texto(img, 'W', 20, c, (32, 18))
            texto(img, 'ASD', 18, c, (32, 43))
        else:
            texto(img, 'SHIFT', 14, c, (32, 20))
            texto(img, 'WASD', 15, c, (32, 42))
    elif nome == 'setas':
        d = ImageDraw.Draw(img)
        for (x, y), (dx, dy) in zip(((22, 4), (1, 33), (22, 33), (43, 33)), ((0, -1), (-1, 0), (0, 1), (1, 0))):
            d.rounded_rectangle((x, y, x + 20, y + 26), 4, fill=(20, 20, 22, 255), outline=(150, 150, 155, 255), width=2)
            cx, cy, r = x + 10, y + 13, 6
            ponta = (cx + dx * r, cy + dy * r)
            base = [(cx - dx * r + dy * r, cy - dy * r + dx * r), (cx - dx * r - dy * r, cy - dy * r - dx * r)]
            d.polygon([ponta] + base, fill=c)
    return img


def bloco_dxt5(px):
    """um bloco 4x4 (16 RGBA) em DXT5: alfa por 8 níveis, cor pelos extremos do eixo de maior variação"""
    a = [p[3] for p in px]
    a0, a1 = max(a), min(a)
    if a0 == a1:
        aidx = [0] * 16
    else:
        niveis = [a0, a1] + [((7 - i) * a0 + i * a1) // 7 for i in range(1, 7)]
        aidx = [min(range(8), key=lambda k: abs(niveis[k] - v)) for v in a]
    bits = 0
    for i, k in enumerate(aidx):
        bits |= k << (3 * i)
    alfa = bytes([a0, a1]) + bits.to_bytes(6, 'little')
    vis = [p for p in px if p[3] > 8] or px
    eixo = max(range(3), key=lambda k: max(p[k] for p in vis) - min(p[k] for p in vis))
    hi = max(vis, key=lambda p: p[eixo])
    lo = min(vis, key=lambda p: p[eixo])

    def q(p):
        return (p[0] * 31 // 255) << 11 | (p[1] * 63 // 255) << 5 | p[2] * 31 // 255

    def dq(c):
        return ((c >> 11) * 255 // 31, (c >> 5 & 63) * 255 // 63, (c & 31) * 255 // 31)

    c0, c1 = q(hi), q(lo)
    p0, p1 = dq(c0), dq(c1)
    pal = [p0, p1, tuple((2 * x + y) // 3 for x, y in zip(p0, p1)), tuple((x + 2 * y) // 3 for x, y in zip(p0, p1))]
    idx = 0
    for i, p in enumerate(px):
        k = min(range(4), key=lambda j: sum((pal[j][m] - p[m]) ** 2 for m in range(3)))
        idx |= k << (2 * i)
    return alfa + c0.to_bytes(2, 'little') + c1.to_bytes(2, 'little') + idx.to_bytes(4, 'little')


def dxt5(img):
    w, h = img.size
    px = img.load()
    out = b''
    for by in range(0, max(h, 4), 4):
        for bx in range(0, max(w, 4), 4):
            out += bloco_dxt5([px[min(bx + i % 4, w - 1), min(by + i // 4, h - 1)] for i in range(16)])
    return out


def cadeia(img):
    """o nível base e os mipmaps até 1x1, como o jogo guarda (5488 bytes; o renderizador lê o que o jogo pede)"""
    out, n = b'', 64
    while n >= 1:
        out += dxt5(img if n == 64 else img.resize((n, n), Image.LANCZOS))
        n //= 2
    return out


TECLAS = {  # botão do PS3 -> tecla de cellPad.c (pad_poll_teclado_mouse)
    'cruz': 'espaco', 'circulo': 'E', 'quadrado': 'mouse_esq', 'triangulo': 'mouse_dir',
    'L1': 'Q', 'R1': 'F', 'L2': 'C', 'R2': 'R', 'L3': 'Z', 'R3': 'V',
    'direcional': 'setas', 'analogico_esq': 'wasd', 'analogico': 'wasd', 'analogico_dir': 'shift_wasd',
}
# (hash FNV-1a do nível base do ícone do PS3, botão, cor do texto)
PS3 = [
    (0xdeec3c904732f1f3, 'analogico_esq', 'branco'),
    (0xaff3481924c5214d, 'analogico', 'branco'),
    (0x846bbbefd5207009, 'analogico_dir', 'branco'),
    (0xec09f8f0e6bce98a, 'L3', 'branco'),
    (0x56199a70e116ea1e, 'R2', 'laranja'),
    (0xbdfbcb96d917bdc2, 'quadrado', 'branco'),
    (0x6293eb3991f0dbc5, 'triangulo', 'branco'),
    (0x0a5cfb48bcd55d29, 'R2', 'branco'),
    (0xee319f394653a351, 'L2', 'branco'),
    (0x4a6af8ce5da0c463, 'direcional', 'branco'),
    (0x3671d1babf4e952b, 'R3', 'branco'),
    (0x89f5e9549a832483, 'R1', 'laranja'),
    (0x153f17dfa9d1c6d0, 'L2', 'laranja'),
    (0x225488db6b49953a, 'L1', 'laranja'),
    (0xc7921e38a93b1be0, 'L3', 'laranja'),
    (0xd3fd3b478891836f, 'R1', 'branco'),
    (0xce21fc8a915d407d, 'circulo', 'branco'),
    (0x431f90c272ed09c7, 'cruz', 'branco'),
    (0x47b376b618659e5b, 'R3', 'laranja'),
    (0xc0b219199540d888, 'L1', 'branco'),
]


def main():
    linhas = ['/* Gerado por scripts/teclas.py: os ícones de botão do HUD (DXT5 64x64 com mipmaps) desenhados com as',
              ' * teclas de cellPad.c, pelo hash FNV-1a do nível base do ícone do PS3. Não editar à mão. */',
              '#pragma once', '']
    prev = Image.new('RGBA', (len(PS3) * 68, 70), (60, 60, 90, 255))
    for i, (h, botao, cor) in enumerate(PS3):
        img = desenhar(TECLAS[botao], cor)
        prev.paste(img, (i * 68, 3), img)
        dados = cadeia(img)
        linhas.append('static const unsigned char k_tecla_%d[%d] = {' % (i, len(dados)))
        for j in range(0, len(dados), 24):
            linhas.append('    ' + ','.join('0x%02x' % b for b in dados[j:j + 24]) + ',')
        linhas.append('};')
    linhas += ['', 'static const struct { unsigned long long hash_ps3; const unsigned char* dados; unsigned tam; } k_teclas[] = {']
    for i, (h, botao, cor) in enumerate(PS3):
        linhas.append('    {0x%016xull, k_tecla_%d, sizeof k_tecla_%d},  /* %s (%s) */' % (h, i, i, botao, cor))
    linhas.append('};')
    raiz = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
    with open(os.path.join(raiz, 'ps3recomp', 'libs', 'video', 'rsx_teclas.h'), 'w', encoding='utf-8', newline='\n') as f:
        f.write('\n'.join(linhas) + '\n')
    if len(sys.argv) > 1:
        prev.save(os.path.join(sys.argv[1], 'teclas_previa.png'))
    print(len(PS3), 'ícones')


if __name__ == '__main__':
    main()
