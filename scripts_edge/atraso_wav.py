"""Lag between picture motion (<name>_mov.tsv: wall ms, mean diff, luma) and the AUDIO_WAV mix (8 ch f32 48 kHz,
first sample at wav_t0_ms). Positive lag = the sound comes AFTER the picture."""
import sys, os
import numpy as np
mov, wav, t0, ch = sys.argv[1], sys.argv[2], int(sys.argv[3]), 8
d = np.loadtxt(mov, delimiter='\t', converters={1: lambda s: float(s.replace(',', '.')), 2: lambda s: float(s.replace(',', '.'))}, encoding='utf-8')
tm = (d[:, 0] - t0) / 1000.0
dm = d[:, 1]
sr, dt = 48000, 0.05
_n = (os.path.getsize(wav) - 58) // 4; a = np.memmap(wav, dtype='<f4', mode='r', offset=58, shape=(_n,))
grid = np.arange(tm[1], tm[-1], dt)
env = np.empty(grid.size)
for k, t in enumerate(grid):
    x = np.asarray(a[int(t * sr) * ch:int((t + dt) * sr) * ch])
    env[k] = 10 * np.log10((x * x).mean() + 1e-12)
onset = np.maximum(np.diff(env, prepend=env[0]), 0)
motion = np.interp(grid, tm, dm)
def lag(o, m):
    o = (o - o.mean()) / (o.std() + 1e-9); m = (m - m.mean()) / (m.std() + 1e-9)
    best = None
    for s in range(-int(5 / dt), int(5 / dt) + 1):
        if s >= 0: c = np.mean(o[s:] * m[:m.size - s]) if s < m.size else 0
        else: c = np.mean(o[:s] * m[-s:])
        if best is None or c > best[1]: best = (s * dt, c)
    return best
l, c = lag(onset, motion)
print('janela inteira %.1f-%.1f s: atraso do som %+.2f s (corr %.2f)' % (grid[0], grid[-1], l, c))
for w0 in np.arange(grid[0], grid[-1] - 20, 10):
    m = (grid >= w0) & (grid < w0 + 20)
    l, c = lag(onset[m], motion[m])
    print('  %.0f-%.0f s: %+.2f s (corr %.2f)' % (w0, w0 + 20, l, c))
