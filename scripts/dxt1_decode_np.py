"""Decodificação completa (numpy) de uma textura DXT1 crua despejada por
GOWA_TEX_DUMP_SEC, para conferir cores em UVs específicos.
uso: dxt1_decode_np.py arquivo.bin L A saida.png"""
import sys

import numpy as np
from PIL import Image


def decode(d, w, h):
    nb = (w // 4) * (h // 4)
    b = d[:nb * 8].reshape(nb, 8)
    c0 = b[:, 0].astype(np.uint32) | (b[:, 1].astype(np.uint32) << 8)
    c1 = b[:, 2].astype(np.uint32) | (b[:, 3].astype(np.uint32) << 8)

    def rgb(c):
        return np.stack([((c >> 11) & 31) * 255 // 31, ((c >> 5) & 63) * 255 // 63, (c & 31) * 255 // 31], -1).astype(np.int32)
    p0, p1 = rgb(c0), rgb(c1)
    four = (c0 > c1)[:, None]
    p2 = np.where(four, (2 * p0 + p1) // 3, (p0 + p1) // 2)
    p3 = np.where(four, (p0 + 2 * p1) // 3, 0)
    pal = np.stack([p0, p1, p2, p3], 1)                       # nb x 4 x 3
    idx = (b[:, 4].astype(np.uint32) | (b[:, 5].astype(np.uint32) << 8) |
           (b[:, 6].astype(np.uint32) << 16) | (b[:, 7].astype(np.uint32) << 24))
    sel = np.stack([(idx >> (2 * i)) & 3 for i in range(16)], 1)  # nb x 16
    px = np.take_along_axis(pal, sel[:, :, None].repeat(3, 2), 1)  # nb x 16 x 3
    px = px.reshape(h // 4, w // 4, 4, 4, 3).transpose(0, 2, 1, 3, 4).reshape(h, w, 3)
    return px.astype(np.uint8)


if __name__ == '__main__':
    d = np.fromfile(sys.argv[1], dtype=np.uint8)
    w, h = int(sys.argv[2]), int(sys.argv[3])
    Image.fromarray(decode(d, w, h)).save(sys.argv[4])
    print('ok', sys.argv[4])
