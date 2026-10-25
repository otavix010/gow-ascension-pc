"""Lê out/hostprof_rip.txt (GOWA_HOSTPROF_RIP=2) e o map do linker: soma as
amostras por função (código SPU/PPU traduzido) e, com --fn NOME, desmonta a
função com llvm-objdump anotando as amostras por instrução.

uso: python rip_quente.py [--top 40] [--fn NOME] [--arq out/hostprof_rip.txt]"""
import os as _os_raiz
RAIZ = _os_raiz.environ.get('GOWA_RAIZ') or _os_raiz.path.dirname(_os_raiz.path.dirname(_os_raiz.path.abspath(__file__)))  # raiz do projeto
import argparse
import bisect
import re
import subprocess
from collections import defaultdict

R = RAIZ
MAPF = R + r'\port\build\gowa.map'
EXE = R + r'\port\build\gowa.exe'
OBJDUMP = (r'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\Llvm\x64\bin'
           r'\llvm-objdump.exe')

ap = argparse.ArgumentParser()
ap.add_argument('--arq', default=R + r'\out\hostprof_rip.txt')
ap.add_argument('--top', type=int, default=40)
ap.add_argument('--fn', default=None)
ap.add_argument('--min', type=int, default=2, help='amostras mínimas para anotar uma instrução')
ap.add_argument('--exe', default=None, help='exe sem extensão (o .map ao lado), se não for o gowa.exe atual')
a = ap.parse_args()
if a.exe:
    MAPF, EXE = a.exe + '.map', a.exe + '.exe'

base = 0x140000000
syms = []
re_base = re.compile(r'Preferred load address is ([0-9a-fA-F]+)')
re_sym = re.compile(r'^\s*[0-9a-fA-F]{4}:[0-9a-fA-F]{8}\s+(\S+)\s+([0-9a-fA-F]{16})\s')
for ln in open(MAPF, encoding='utf-8', errors='replace'):
    m = re_base.search(ln)
    if m:
        base = int(m.group(1), 16)
        continue
    m = re_sym.match(ln)
    if m:
        va = int(m.group(2), 16)
        if va >= base:
            syms.append((va - base, m.group(1)))
syms.sort()
rvas = [r for r, _ in syms]

amostras = {}
total = 0
for ln in open(a.arq):
    if ln.startswith('#'):
        continue
    rva, n = ln.split()
    amostras[int(rva, 16)] = int(n)
    total += int(n)

por_fn = defaultdict(int)
fn_de = {}
for rva, n in amostras.items():
    i = bisect.bisect_right(rvas, rva) - 1
    nome = syms[i][1] if i >= 0 else '?'
    por_fn[nome] += n
    fn_de[rva] = i

print(f'{total} amostras')
for nome, n in sorted(por_fn.items(), key=lambda x: -x[1])[:a.top]:
    print(f'{100.0 * n / total:6.2f}%  {nome}')

if a.fn:
    i = next(k for k, (_, nm) in enumerate(syms) if nm == a.fn)
    ini, fim = syms[i][0], syms[i + 1][0] if i + 1 < len(syms) else syms[i][0] + 0x10000
    n_fn = sum(n for r, n in amostras.items() if ini <= r < fim)
    print(f'\n--- {a.fn}: rva 0x{ini:X}..0x{fim:X}, {n_fn} amostras ---')
    out = subprocess.run([OBJDUMP, '-d', '--no-show-raw-insn', '--x86-asm-syntax=intel',
                          f'--start-address=0x{base + ini:X}', f'--stop-address=0x{base + fim:X}', EXE],
                         capture_output=True, text=True).stdout
    for ln in out.splitlines():
        m = re.match(r'\s*([0-9a-f]+):\s*(.*)', ln)
        if not m:
            continue
        rva = int(m.group(1), 16) - base
        n = amostras.get(rva, 0)
        marca = f'{n:6d} {100.0 * n / max(n_fn, 1):5.1f}%' if n >= a.min else ' ' * 13
        print(f'{marca}  {rva - ini:6X}  {m.group(2)}')
