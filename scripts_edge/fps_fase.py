"""FPS médio (campo jogo= das linhas [fps]) a partir de 40 s depois de "go to playing", como o fps_fase.ps1."""
import re
import sys

for log in sys.argv[1:]:
    ls = open(log, encoding='utf-8', errors='replace').read().splitlines()
    t_go = None
    for l in ls:
        m = re.search(r'\[jogo-log\] t=(\d+)ms .*\[go to playing\]', l)
        if m:
            t_go = int(m[1]) / 1000
            break
    if t_go is None:
        print(f'{log}: sem "go to playing"')
        continue
    # as linhas [fps] saem a cada 5 s; a posição delas no tempo vem da contagem a partir do primeiro [fps]
    fps = [float(re.search(r'jogo=([0-9.]+)', l)[1]) for l in ls if l.startswith('[fps]') and 'jogo=' in l]
    # tempo aproximado de cada linha: o jogo imprime a primeira ~5 s depois de abrir a janela
    t0 = 5.0
    vals = [v for i, v in enumerate(fps) if t0 + 5 * i >= t_go + 40]
    print(f'{log.split(chr(92))[-1]}: go to playing aos {t_go:.0f} s; {len(vals)} amostras; fase {sum(vals) / max(1, len(vals)):.1f} fps')
