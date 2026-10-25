"""Analisa o WAV gravado com GOWA_AUDIO_WAV (float32 estéreo, 48 kHz).

Uso: python analisar_audio.py out/audio.wav [--de 150]

Mostra, por janela de 5 s: nível RMS e pico (dBFS), fração de amostras
saturadas (|x| >= 0.999, o mixer corta em 1.0), trechos de silêncio total
dentro de som (buracos > 5 ms, sinal de underrun do produtor) e saltos
bruscos entre amostras vizinhas (estalos). No fim, o espectro grosso em
bandas, para ver se o som tem graves/médios/agudos plausíveis.
"""
import struct
import sys

import numpy as np


def ler(p):
    b = open(p, 'rb').read()
    if b[:4] != b'RIFF':
        raise SystemExit('não é RIFF')
    i = 12
    fmt = None
    dados = None
    while i + 8 <= len(b):
        cid, tam = b[i:i + 4], struct.unpack_from('<I', b, i + 4)[0]
        if cid == b'fmt ':
            fmt = struct.unpack_from('<HHIIHH', b, i + 8)
        elif cid == b'data':
            dados = b[i + 8:i + 8 + tam]
            if tam == 0 or i + 8 + tam > len(b):      # cabeçalho ainda não atualizado
                dados = b[i + 8:]
            break
        i += 8 + tam + (tam & 1)
    tag, nch, taxa = fmt[0], fmt[1], fmt[2]
    x = np.frombuffer(dados[:len(dados) // (4 * nch) * 4 * nch], dtype='<f4').reshape(-1, nch)
    return x, taxa


def db(v):
    return 20 * np.log10(max(v, 1e-9))


def main():
    p = sys.argv[1]
    de = float(sys.argv[sys.argv.index('--de') + 1]) if '--de' in sys.argv else 0.0
    x, taxa = ler(p)
    print(f'{p}: {len(x) / taxa:.1f} s, {taxa} Hz, {x.shape[1]} canais')
    jan = 5 * taxa
    for k in range(int(de * taxa), len(x), jan):
        w = x[k:k + jan]
        if len(w) < taxa // 2:
            break
        m = np.abs(w).max(axis=1)
        rms = float(np.sqrt((w.astype(np.float64) ** 2).mean()))
        sat = float((m >= 0.999).mean())
        # buracos: sequências de zeros exatos com som em volta
        z = (m == 0).astype(np.int8)
        buracos = 0
        if z.any() and rms > 1e-3:
            d = np.diff(np.concatenate(([0], z, [0])))
            ini, fim = np.where(d == 1)[0], np.where(d == -1)[0]
            buracos = int(((fim - ini) > taxa * 0.005).sum())
        salto = np.abs(np.diff(w, axis=0)).max(axis=1)
        estalos = int((salto > 0.5).sum())
        print(f'  {k / taxa:6.1f}s  rms {db(rms):6.1f} dBFS  pico {db(float(m.max())):6.1f}  '
              f'saturado {100 * sat:5.2f}%  buracos {buracos:3d}  saltos>0.5 {estalos:4d}')
    seg = x[int(de * taxa):].mean(axis=1).astype(np.float64)
    if len(seg) > taxa:
        n = 1 << int(np.log2(min(len(seg), 1 << 22)))
        f = np.abs(np.fft.rfft(seg[:n] * np.hanning(n))) ** 2
        fr = np.fft.rfftfreq(n, 1 / taxa)
        tot = f.sum()
        print('  bandas:', '  '.join(f'{a}-{b} Hz {100 * f[(fr >= a) & (fr < b)].sum() / tot:4.1f}%'
                                   for a, b in ((20, 250), (250, 2000), (2000, 8000), (8000, 24000))))


if __name__ == '__main__':
    main()
