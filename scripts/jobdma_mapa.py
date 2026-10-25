"""Mapa de entradas e saídas dos jobs do 0x72DF80 a partir de out/jobdma_<pid>.txt
(GOWA_JOBDMA, gowa_offline.cpp). As linhas das threads se intercalam no arquivo; cada
registro D/L traz o número do job. Por job: o que entra (GET e elementos de listas GETL, com
o destino na LS) e o que sai (PUT/PUTF), agrupado por região de EA; no fim, o resumo de
todos os jobs e a decodificação dos pacotes de comando da RSX (PUTF pequenos: palavras
NV40/NV4097 "cabeçalho (contagem << 18 | método) + argumentos").

uso: python jobdma_mapa.py out/jobdma_<pid>.txt [--job N] [--rsx]"""
import re
import sys
from collections import Counter, defaultdict

arq = sys.argv[1]
so_job = int(sys.argv[sys.argv.index('--job') + 1]) if '--job' in sys.argv else None
mostra_rsx = '--rsx' in sys.argv

CMDS = {0x20: 'PUT', 0x21: 'PUTB', 0x22: 'PUTF', 0x24: 'PUTL', 0x25: 'PUTLB', 0x26: 'PUTLF',
        0x40: 'GET', 0x41: 'GETB', 0x42: 'GETF', 0x44: 'GETL', 0x45: 'GETLB', 0x46: 'GETLF'}
# métodos NV4097 conhecidos (RSX, nouveau/rpcs3)
METODOS = {0x1680: 'VERTEX_DATA_ARRAY_OFFSET[0]', 0x1740: 'VERTEX_DATA_ARRAY_FORMAT[0]', 0x1808: 'BEGIN_END',
           0x1814: 'DRAW_ARRAYS', 0x1818: 'INLINE_ARRAY', 0x181C: 'INDEX_ARRAY_ADDRESS', 0x1820: 'INDEX_ARRAY_DMA',
           0x1824: 'DRAW_INDEX_ARRAY', 0x0394: 'CLIP_MIN/MAX', 0x0A00: 'VIEWPORT_H/V', 0x0A20: 'VIEWPORT_OFFSET/SCALE',
           0x08C0: 'SCISSOR_H/V', 0x1EFC: 'TRANSFORM_CONSTANT_LOAD+DADOS', 0x1D6C: 'SEMAPHORE_OFFSET',
           0x1D70: 'BACK_END_WRITE_SEMAPHORE_RELEASE', 0x1D74: 'TEXTURE_READ_SEMAPHORE_RELEASE', 0x1D94: 'CLEAR',
           0x1E9C: 'TRANSFORM_PROGRAM_LOAD', 0x0B00: 'TRANSFORM_PROGRAM', 0x08E4: 'SHADER_PROGRAM', 0x1D60: 'SHADER_CONTROL',
           0x1A00: 'TEXTURE_OFFSET', 0x1FC0: 'VERTEX_ATTRIB_INPUT_MASK', 0x1738: 'VERTEX_DATA_BASE_OFFSET',
           0x173C: 'VERTEX_DATA_BASE_INDEX'}
for _i in range(1, 16):
    METODOS[0x1680 + 4 * _i] = f'VERTEX_DATA_ARRAY_OFFSET[{_i}]'
    METODOS[0x1740 + 4 * _i] = f'VERTEX_DATA_ARRAY_FORMAT[{_i}]'


def regiao(ea):
    if 0x00700000 <= ea < 0x00800000:
        return f'código 0x{ea & 0xFFFF000:07X}..(módulo/plugin)'
    return f'0x{ea >> 20:03X}xxxxx'


jobs = defaultdict(lambda: {'in': [], 'out': [], 'outros': []})
cab = {}
for ln in open(arq, encoding='utf-8', errors='replace'):
    if ln.startswith('J '):
        m = re.match(r'J (\d+) (.*)', ln)
        cab[int(m.group(1))] = m.group(2).strip()
        continue
    m = re.match(r'D (\d+) img=\S+ cmd=0x([0-9A-F]+) lsa=0x([0-9A-F]+) ea=0x([0-9A-F]+) size=0x([0-9A-F]+) tag=\d+ pc=0x([0-9A-F]+) src=(\S*)', ln)
    if m:
        j, cmd, lsa, ea, sz, pc, src = int(m.group(1)), int(m.group(2), 16), int(m.group(3), 16), int(m.group(4), 16), int(m.group(5), 16), int(m.group(6), 16), m.group(7)
        nome = CMDS.get(cmd, f'0x{cmd:02X}')
        reg = dict(cmd=nome, lsa=lsa, ea=ea, size=sz, pc=pc, src=src)
        if nome.startswith('GET') and 'L' not in nome:
            jobs[j]['in'].append(reg)
        elif nome.startswith('PUT'):
            jobs[j]['out'].append(reg)
        else:
            jobs[j]['outros'].append(reg)
        continue
    m = re.match(r'L (\d+)\s+elem ea=0x([0-9A-F]+) size=0x([0-9A-F]+) lsa=0x([0-9A-F]+) src=(\S*)', ln)
    if m:
        j, ea, sz, lsa, src = int(m.group(1)), int(m.group(2), 16), int(m.group(3), 16), int(m.group(4), 16), m.group(5)
        if sz:
            jobs[j]['in'].append(dict(cmd='GETL', lsa=lsa, ea=ea, size=sz, pc=0, src=src))


def mostra_job(j):
    d = jobs[j]
    print(f'=== job {j}: {cab.get(j, "")}')
    for r in d['in']:
        print(f'  ENTRA {r["cmd"]:6s} {r["size"]:6X} B  ea=0x{r["ea"]:08X} -> LS 0x{r["lsa"]:05X}  {regiao(r["ea"])}  {r["src"][:48]}')
    for r in d['out']:
        print(f'  SAI   {r["cmd"]:6s} {r["size"]:6X} B  LS 0x{r["lsa"]:05X} -> ea=0x{r["ea"]:08X}  {regiao(r["ea"])}  pc=0x{r["pc"]:05X}  {r["src"][:48]}')


def rsx(src):
    w = [int(src[i:i + 8], 16) for i in range(0, len(src) - 7, 8)]
    out = []
    i = 0
    while i < len(w):
        h = w[i]
        cnt = (h >> 18) & 0x7FF
        met = h & 0xFFFC
        if h & 0xE0030003 or cnt == 0:
            out.append(f'?{h:08X}')
            i += 1
            continue
        args = w[i + 1:i + 1 + cnt]
        out.append(f'{METODOS.get(met, f"0x{met:04X}")}({",".join(f"{a:08X}" for a in args[:4])}{"..." if cnt > 4 else ""})')
        i += 1 + cnt
    return ' '.join(out)


if so_job is not None:
    mostra_job(so_job)
    if mostra_rsx:
        for r in jobs[so_job]['out']:
            if r['cmd'] == 'PUTF':
                print('   RSX:', rsx(r['src']))
    sys.exit(0)

# resumo de todos os jobs
n = len(jobs)
print(f'{n} jobs')
ent = Counter(); ent_b = Counter(); sai = Counter(); sai_b = Counter(); dest = defaultdict(Counter)
for j, d in jobs.items():
    for r in d['in']:
        k = (r['cmd'] if r['cmd'] != 'GETL' else 'GETL', regiao(r['ea']))
        ent[k] += 1; ent_b[k] += r['size']; dest[k][r['lsa'] & 0x3F000] += 1
    for r in d['out']:
        k = (r['cmd'], regiao(r['ea']))
        sai[k] += 1; sai_b[k] += r['size']
print('--- entradas (por comando e região): vezes, bytes médios, destinos na LS mais comuns')
for k, v in sorted(ent.items(), key=lambda x: -ent_b[x[0]]):
    print(f'  {k[0]:6s} {k[1]:36s} {v:5d}x  {ent_b[k] // v:6d} B  LS ' + ' '.join(f'0x{a:05X}({c})' for a, c in dest[k].most_common(3)))
print('--- saídas')
for k, v in sorted(sai.items(), key=lambda x: -sai_b[x[0]]):
    print(f'  {k[0]:6s} {k[1]:36s} {v:5d}x  {sai_b[k] // v:6d} B')
if mostra_rsx:
    print('--- pacotes RSX (PUTF), primeiros 12')
    k = 0
    for j, d in sorted(jobs.items()):
        for r in d['out']:
            if r['cmd'] == 'PUTF' and k < 12:
                print(f'  job {j} ea=0x{r["ea"]:08X}: {rsx(r["src"])[:300]}')
                k += 1
