"""Buracos na saída de áudio gravada (GOWA_AUDIO_WAV: float32, cabeçalho de 58 bytes, canais no cabeçalho).
Conta NaN/inf, e trechos de silêncio exato (todas as amostras 0) entre trechos com som: os blocos que o jogo
não escreveu a tempo viram silêncio de 256 quadros (5,33 ms) ou mais no meio da música."""
import struct
import sys
import numpy as np

for arq in sys.argv[1:]:
    b = open(arq, 'rb').read()
    ch = struct.unpack('<H', b[22:24])[0] or 8
    x = np.frombuffer(b[58:len(b) - (len(b) - 58) % (4 * ch)], dtype='<f4').reshape(-1, ch)
    nan = int(np.count_nonzero(~np.isfinite(x)))
    q = len(x)
    som = np.any(x != 0, axis=1)
    # só a parte depois do 1º som (o começo é silêncio de carregamento)
    idx = np.flatnonzero(som)
    if not len(idx):
        print(f'{arq}: {q} quadros, sem som'); continue
    a, z = idx[0], idx[-1]
    s = som[a:z + 1]
    # trechos de silêncio
    d = np.diff(np.concatenate([[1], s.astype(np.int8), [1]]))
    ini = np.flatnonzero(d == -1); fim = np.flatnonzero(d == 1)
    dur = fim - ini
    curtos = dur[(dur >= 200) & (dur < 4800)]      # 4 ms a 100 ms: engasgo
    longos = dur[dur >= 4800]
    print(f'{arq}: {q / 48000:.1f} s gravados, {ch} canais, som de {a / 48000:.1f} s a {z / 48000:.1f} s; NaN/inf {nan}; '
          f'buracos 4-100 ms: {len(curtos)} (total {curtos.sum() / 48:.0f} ms); silêncios >100 ms: {len(longos)} '
          f'(total {longos.sum() / 48000:.1f} s); pico {float(np.nanmax(np.abs(x))):.3f}')
