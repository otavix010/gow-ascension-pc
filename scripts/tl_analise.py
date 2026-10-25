"""Análise da linha do tempo do GOWA_TL (out/tl_<pid>.bin, boot_main_gowa.cpp, 22/10).

Por SPU: fração do tempo por workload e por imagem (módulo de job), e bloqueada num canal; RSX:
fração estacionada (esperando o EDGE); threads PPU: em qual syscall/HLE ficam; e um gráfico de
Gantt em texto de alguns quadros (um caractere por amostra).

uso: python tl_analise.py <tl_N.bin> [--quadros K] [--desde Q]"""
import os as _os_raiz
RAIZ = _os_raiz.environ.get('GOWA_RAIZ') or _os_raiz.path.dirname(_os_raiz.path.dirname(_os_raiz.path.abspath(__file__)))  # raiz do projeto
import re
import struct
import sys
from collections import Counter, defaultdict

arq = sys.argv[1]
nq = int(sys.argv[sys.argv.index('--quadros') + 1]) if '--quadros' in sys.argv else 4
desde = int(sys.argv[sys.argv.index('--desde') + 1]) if '--desde' in sys.argv else 100

b = open(arq, 'rb').read()
assert b[:7] in (b'GOWATL1', b'GOWATL2', b'GOWATL3', b'GOWATL4', b'GOWATL5')
n, tam = struct.unpack_from('<II', b, 8)
nh, = struct.unpack_from('<I', b, 16)
p = 20
hle = ['']
for _ in range(nh):
    hle.append(b[p:p + 64].split(b'\0')[0].decode('latin-1')); p += 64
tnome = []
for _ in range(16):
    tnome.append(b[p:p + 32].split(b'\0')[0].decode('latin-1')); p += 32
MODH = {}
if b[:7] != b'GOWATL1':
    nm, = struct.unpack_from('<I', b, p); p += 4
    for _ in range(nm):
        va, h = struct.unpack_from('<II', b, p); p += 8
        MODH[h] = va
SPU = struct.Struct('<BBBBII')
PPU = struct.Struct('<HBBI')
amostras = []
for i in range(n):
    o = p + i * tam
    t, rg, rf, rp = struct.unpack_from('<IIIB', b, o)
    q = o + 16
    spus = [SPU.unpack_from(b, q + 12 * k) for k in range(8)]
    q += 96
    ppus = [PPU.unpack_from(b, q + 8 * k) for k in range(16)]
    amostras.append((t, rg, rf, rp, spus, ppus))

# imagens dos módulos de job: 620000 + índice em gowa_jobmods.inc
mods = re.findall(r'\{ 0x([0-9A-F]{8})u, 0x[0-9A-F]+u, gowajm_', open(
    _os_raiz.path.join(RAIZ, r'port/gowa_jobmods.inc'), encoding='latin-1').read())
def img_nome(img):
    if img in MODH:
        return f'jm{MODH[img]:X}'
    if 620000 <= img < 620000 + len(mods):
        return 'jm' + mods[img - 620000][2:]
    return f'img{img:08X}'
WNOME = {0: 'sys', 1: 'tsGP', 2: 'tsGPH', 3: 'tsHP5', 4: 'CollisL', 5: 'tsEdgeDec', 6: 'HighP6', 7: 'Gen6',
         8: 'SysMisc', 9: 'ModelSrv', 10: 'Net', 11: 'Batch', 12: 'som', 13: 'Ren13', 14: 'Ren14', 15: 'Ren15',
         16: 'Ren16', 17: 'RenderJobs', 18: 'MeshProc', 19: 'EDGE0', 20: 'EDGE1', 21: 'EDGE2', 22: 'RsxSync'}
dt = (amostras[-1][0] - amostras[0][0]) / max(1, n - 1)
print(f'{n} amostras, período médio {dt:.0f} µs, {(amostras[-1][0] - amostras[0][0]) / 1e6:.1f} s; '
      f'flips {amostras[-1][2] - amostras[0][2]} ({(amostras[-1][2] - amostras[0][2]) / ((amostras[-1][0] - amostras[0][0]) / 1e6):.1f}/s)')

# --- SPUs
for k in range(8):
    ok = [a[4][k] for a in amostras if a[4][k][3]]
    if not ok:
        continue
    esp = sum(1 for s in ok if s[1] & 8)
    w = Counter()
    im = Counter()
    for s in ok:
        if s[1] & 8:
            continue
        w[WNOME.get(s[0], f'w{s[0]}')] += 1
        im[img_nome(s[4])] += 1
    print(f'SPU{k}: bloqueada {100 * esp / len(ok):4.1f}% | ' +
          ' '.join(f'{x}:{100 * c / len(ok):.0f}%' for x, c in w.most_common(7)) + ' | ' +
          ' '.join(f'{x}:{100 * c / len(ok):.0f}%' for x, c in im.most_common(6)))

# --- RSX
park = sum(a[3] for a in amostras)
print(f'RSX: estacionada (esperando o próximo trecho) {100 * park / n:.1f}% das amostras')

# --- PPU
for k in range(16):
    st = Counter()
    for a in amostras:
        sc, h, state, pc = a[5][k]
        if state == 0:
            continue
        st['sc%d' % sc if sc else (('hle:' + hle[h]) if h else 'guest')] += 1
    tot = sum(st.values())
    if tot == 0:
        continue
    g = st.get('guest', 0)
    print(f'PPU t{k + 1} {tnome[k][:22]:22s}: guest {100 * g / tot:4.1f}% | ' +
          ' '.join(f'{x}:{100 * c / tot:.0f}%' for x, c in st.most_common(5) if x != 'guest'))

# --- Gantt de alguns quadros (a partir do flip `desde`)
LET = {19: '0', 20: '1', 21: '2', 17: 'R', 13: 'r', 14: 'r', 15: 'r', 16: 'r', 18: 'M', 7: 'g', 6: 'h', 3: 'T',
       1: 't', 2: 't', 12: 's', 11: 'b', 9: 'm', 4: 'c', 22: 'y', 8: 'x', 0: 'k'}
f0 = amostras[0][2] + desde
idx = [i for i, a in enumerate(amostras) if f0 <= a[2] < f0 + nq]
if idx:
    print(f'\nGantt (1 caractere = 1 amostra ~{dt:.0f} µs; SPU: 0/1/2 EDGE, R RenderJobs, r RenJobSet, M MeshProc,'
          f' g General6, T taskset HP5, t taskset, s som, . bloqueada, + polling no PM; RSX: P estacionada, = andando,'
          f' | flip; PPU principal: g guest, w espera)')
    linhas = defaultdict(str)
    ult = None
    for i in idx:
        t, rg, rf, rp, spus, ppus = amostras[i]
        marca = '|' if ult is not None and rf != ult else ''
        ult = rf
        for k in range(8):
            s = spus[k]
            if not s[3]:
                continue
            c = '.' if s[1] & 8 else LET.get(s[0], '?')
            if c in '012' and s[5] < 0x4000 and not (s[1] & 8):
                c = '+'
            linhas[f'SPU{k}'] += marca + c
        linhas['RSX '] += marca + ('P' if rp else '=')
        for k in range(16):
            sc, h, state, pc = ppus[k]
            if tnome[k].startswith('main') or k == 0:
                linhas[f'PPU{k + 1}'] += marca + ('g' if not sc and not h else 'w')
    for nome, l in linhas.items():
        print(f'{nome:5s} {l}')

# --- por quadro, nas SPUs 0-3: ms (por SPU) em cada classe
cl = defaultdict(float)
nquad = 0
ant = None
for t, rg, rf, rp, spus, ppus in amostras:
    if ant is not None and rf != ant:
        nquad += 1
    ant = rf
    for k in range(4):
        s = spus[k]
        if not s[3]:
            continue
        if s[1] & 8:
            c = 'bloqueada'
        elif s[0] in (19, 20, 21):
            c = 'EDGE job' if s[5] >= 0x4000 else 'EDGE PM'
        elif s[0] in (13, 14, 15, 16, 17):
            c = 'Render*'
        else:
            c = WNOME.get(s[0], f'w{s[0]}')
        cl[c] += dt / 1000.0 / 4
print(f'\nSPUs 0-3, ms por quadro e por SPU ({nquad} quadros):')
for c, v in sorted(cl.items(), key=lambda x: -x[1]):
    print(f'  {c:12s} {v / max(1, nquad):6.2f} ms')
print(f'  total        {sum(cl.values()) / max(1, nquad):6.2f} ms')
