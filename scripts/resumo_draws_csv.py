"""Resume um RSX_DRAW_CSV do live-draw: para um frame (o 2º completo por
padrão), a sequência de superfícies-alvo com contagem de draws, vértices,
resultados, blend/teste de profundidade/máscara de cor e formatos de textura.
Uso: python resumo_draws_csv.py arquivo.csv [frame]"""
import csv
import sys
from collections import Counter, OrderedDict

rows = list(csv.DictReader(open(sys.argv[1], newline='')))
frames = sorted({int(r['frame']) for r in rows})
fr = int(sys.argv[2]) if len(sys.argv) > 2 else (frames[1] if len(frames) > 1 else frames[0])
sel = [r for r in rows if int(r['frame']) == fr and (r['verts'] or '').isdigit()]
print(f'frames no CSV: {frames[0]}..{frames[-1]} ({len(frames)}); frame {fr}: {len(sel)} draws')
print('resultados:', dict(Counter(r['outcome'] for r in sel)))
# sequência de passes (troca de superfície = novo passe)
passes = []
for r in sel:
    if not passes or passes[-1]['surf'] != r['surf']:
        passes.append({'surf': r['surf'], 'n': 0, 'verts': 0, 'out': Counter(), 'blend': Counter(),
                       'dtest': Counter(), 'cmask': Counter(), 'texfmt': Counter(), 'prim': Counter(), 'first': r['draw']})
    p = passes[-1]
    p['n'] += 1
    p['verts'] += int(r['verts'] or 0)
    p['out'][r['outcome']] += 1
    p['blend'][r['blend']] += 1
    p['dtest'][r['dtest'] + '/' + r['dwrite']] += 1
    p['cmask'][r['cmask']] += 1
    p['prim'][r['prim']] += 1
    for t in r['tex'].split():
        if t.count(':') >= 4 and not t.startswith(('at=', 'sc=', 'A=', 'B=', 'C=', 'D=', 'fmt=', 'rt=', 'ct=')):
            parts = t.split(':')
            p['texfmt'][parts[3] + '@' + parts[2]] += 1
for i, p in enumerate(passes):
    tf = ', '.join(f'{k}x{v}' for k, v in p['texfmt'].most_common(4))
    print(f"{i:3d} surf={p['surf']:>10} draws={p['n']:4d} verts={p['verts']:7d} out={dict(p['out'])} "
          f"blend={dict(p['blend'])} dtest/dwrite={dict(p['dtest'])} cmask={dict(p['cmask'])} prim={dict(p['prim'])} tex[{tf}]")
