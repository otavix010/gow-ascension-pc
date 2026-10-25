"""Perfil por bloco SPU (rótulo loc_XXXX) de um módulo traduzido.

As amostras de GOWA_HOSTPROF_RIP=2 ("rva n") caem em funções do exe; para saber
em que trecho do código SPU elas estão, o módulo é recompilado à parte com as
mesmas flags + -gline-tables-only (o código gerado não muda; dentro de uma
função os deslocamentos no .obj e no .exe são os mesmos), e cada amostra vira
função + deslocamento -> linha do spu_recomp.c -> o último rótulo loc_ antes
dela.

uso: python perfil_rotulo.py <rip.txt> <exe sem extensão> <dir do módulo> [N]
     ex.: python perfil_rotulo.py out/hp_vk7_c16_rip.txt port/build/gowa_vk7 port/spu/gowajm_0072DF80 40
O .obj com linhas é gerado em out/perfil_rotulo/<módulo>.obj (refeito se o
spu_recomp.c for mais novo)."""
import os as _os_raiz
RAIZ = _os_raiz.environ.get('GOWA_RAIZ') or _os_raiz.path.dirname(_os_raiz.path.dirname(_os_raiz.path.abspath(__file__)))  # raiz do projeto
import bisect
import os
import re
import subprocess
import sys
from collections import defaultdict

R = RAIZ
LLVM = r'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\Llvm\x64\bin'
VCVARS = r'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat'

rip, exe, moddir = sys.argv[1], sys.argv[2], os.path.abspath(sys.argv[3])
top = int(sys.argv[4]) if len(sys.argv) > 4 else 40
mod = os.path.basename(moddir.rstrip('\\/'))
src = os.path.join(moddir, 'spu_recomp.c')
outdir = os.path.join(R, 'out', 'perfil_rotulo')
os.makedirs(outdir, exist_ok=True)
obj = os.path.join(outdir, mod + '.obj')

# ---- 1. o .obj com tabela de linhas (mesmo comando do ninja)
if not os.path.exists(obj) or os.path.getmtime(obj) < os.path.getmtime(src):
    alvo = f'CMakeFiles\\gowa.dir\\spu\\{mod}\\spu_recomp.c.obj'
    nbat = os.path.join(outdir, 'ninja_cmd.cmd')
    with open(nbat, 'w', encoding='latin-1') as f:   # (aspas dentro de subprocess viram \" -- o cmd não entende)
        f.write('@echo off\n')
        f.write(f'call "{VCVARS}" >nul\n')
        f.write(f'cd /d {R}\\port\\build\n')
        f.write(f'ninja -t commands {alvo}\n')
    cmd = subprocess.run(['cmd.exe', '/c', nbat], capture_output=True, text=True).stdout.strip().splitlines()
    linha = [c for c in cmd if 'spu_recomp.c' in c and mod in c][-1]
    linha = re.sub(r'/Fo\S+', lambda m_: '/Fo' + obj, linha)
    linha = re.sub(r'-o\s+\S+\.obj', lambda m_: '-o ' + obj, linha)
    linha = linha.replace(' -- ', ' /clang:-gdwarf-4 /clang:-gline-tables-only -- ', 1) if ' -- ' in linha else linha + ' /clang:-gline-tables-only'
    linha = re.sub(r'-clang:-MT\S+|-clang:-MF\S+|-clang:-MD', '', linha)   # sem depfile
    bat = os.path.join(outdir, 'compila.cmd')
    with open(bat, 'w', encoding='latin-1') as f:
        f.write('@echo off\n')
        f.write(f'call "{VCVARS}" >nul\n')
        f.write(f'set "PATH={LLVM};%PATH%"\n')
        f.write(f'cd /d {R}\\port\\build\n')
        f.write(linha + '\n')
        f.write('echo exit=%ERRORLEVEL%\n')
    r = subprocess.run(['cmd.exe', '/c', bat], capture_output=True, text=True)
    if 'exit=0' not in r.stdout:
        print(r.stdout[-3000:], r.stderr[-3000:])
        sys.exit('falhou a compilação com linhas')

# ---- 2. deslocamento -> linha, por função, do .obj
dis = subprocess.run([os.path.join(LLVM, 'llvm-objdump.exe'), '-d', '-l', '--no-show-raw-insn', obj],
                     capture_output=True, text=True, errors='replace').stdout
linhas_fn = defaultdict(list)          # função -> [(deslocamento, linha)]
fn = None
lin = None
re_fn = re.compile(r'^[0-9a-f]+ <(.+)>:$')
re_lin = re.compile(r'^; .*spu_recomp\.c:(\d+)')
re_ins = re.compile(r'^\s*([0-9a-f]+):\s')
base_fn = 0
for l in dis.splitlines():
    m = re_fn.match(l)
    if m:
        fn = m.group(1)
        base_fn = None
        continue
    m = re_lin.match(l)
    if m:
        lin = int(m.group(1))
        continue
    m = re_ins.match(l)
    if m and fn is not None:
        a = int(m.group(1), 16)
        if base_fn is None:
            base_fn = a
        if lin is not None:
            linhas_fn[fn].append((a - base_fn, lin))

# ---- 3. linha -> rótulo SPU (o último loc_ antes, dentro da mesma função)
codigo = open(src, encoding='latin-1').read().split('\n')
rot_da_linha = {}
atual = '(entrada)'
for i, t in enumerate(codigo, 1):
    m = re.match(r'^loc_([0-9A-F]{8}):', t)
    if m:
        atual = m.group(1)
    elif re.match(r'^(?:static )?(?:SPU_PN )?void \w+_spu_func_([0-9A-F]{8})', t):
        atual = '(entrada)'
    rot_da_linha[i] = atual

# ---- 4. amostras do exe -> função + deslocamento
syms = []
base = 0x140000000
for l in open(exe + '.map', encoding='latin-1'):
    m = re.search(r'Preferred load address is ([0-9a-fA-F]+)', l)
    if m:
        base = int(m.group(1), 16)
        continue
    m = re.match(r'\s*[0-9a-fA-F]{4}:[0-9a-fA-F]{8}\s+(\S+)\s+([0-9a-fA-F]{16})\s', l)
    if m:
        va = int(m.group(2), 16)
        if va >= base:
            syms.append((va - base, m.group(1)))
syms.sort()
rvs = [r for r, _ in syms]
por_rot = defaultdict(int)
por_fn = defaultdict(int)
tot = 0
tot_mod = 0
for l in open(rip):
    p = l.split()
    if len(p) < 2 or p[0].startswith('#'):
        continue
    rva, n = int(p[0], 16), int(p[1])
    tot += n
    i = bisect.bisect_right(rvs, rva) - 1
    if i < 0:
        continue
    nome = syms[i][1]
    if nome not in linhas_fn:
        continue
    tot_mod += n
    off = rva - syms[i][0]
    tab = linhas_fn[nome]
    j = bisect.bisect_right(tab, (off, 1 << 30)) - 1
    if j < 0:
        continue
    lin = tab[j][1]
    f = re.search(r'_spu_func_([0-9A-F]{8})', nome)
    chave = (f.group(1) if f else nome[:64], rot_da_linha.get(lin, '?'))
    por_rot[chave] += n
    por_fn[chave[0]] += n
print(f'{tot} amostras no total; {tot_mod} ({100.0 * tot_mod / max(tot, 1):.2f}%) no módulo {mod}')
print('--- funções')
for k, v in sorted(por_fn.items(), key=lambda x: -x[1])[:15]:
    print(f'{100.0 * v / tot:6.2f}%  {k}')
print('--- blocos (função / rótulo SPU)')
for (f, r_), v in sorted(por_rot.items(), key=lambda x: -x[1])[:top]:
    print(f'{100.0 * v / tot:6.2f}%  {f} / loc_{r_}')
