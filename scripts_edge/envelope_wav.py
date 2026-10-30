"""Energia (dB) do WAV do GOWA_AUDIO_WAV em passos de 0,5 s, numa janela de tempo do jogo: <wav> <log> <de s> <até s>.
O tempo do jogo vem do t= do [jogo-log] e da linha "primeira amostra" (hora de parede); aqui usa o tempo do arquivo."""
import os
import sys
import numpy as np

wav, de, ate = sys.argv[1], float(sys.argv[2]), float(sys.argv[3])
n = (os.path.getsize(wav) - 58) // 4
a = np.memmap(wav, dtype='<f4', mode='r', offset=58, shape=(n,))
sr, ch = 48000, 8
for t in np.arange(de, ate, 0.5):
    x = np.asarray(a[int(t * sr) * ch:int((t + 0.5) * sr) * ch])
    if not x.size:
        break
    db = 10 * np.log10((x * x).mean() + 1e-12)
    print(f'{t:6.1f} s  {db:6.1f} dB  ' + '#' * max(0, int((db + 80) / 2)))
