"""Tamanho e derramamentos de zmm/ymm na pilha dos núcleos gowa_vk_* de um exe.
uso: python medir_nucleos.py <exe sem extensão, ex. port/build/gowa_vk4>"""
import re
import subprocess
import sys
import collections
OBJ = r'C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\Llvm\x64\bin\llvm-objdump.exe'
base = sys.argv[1]
mp, exe = base + '.map', base + '.exe'
syms = []
for l in open(mp, encoding='latin-1'):
    m = re.match(r'\s*0001:([0-9a-f]{8})\s+(\S+)\s+([0-9a-f]{16})', l)
    if m:
        syms.append((int(m.group(3), 16), m.group(2)))
syms.sort()
for i, (a, n) in enumerate(syms):
    if n.startswith('gowa_vk_') and (n.endswith('_nucleo') or n.endswith('_corre')):
        j = i + 1
        while syms[j][0] == a:
            j += 1
        b = syms[j][0]
        out = subprocess.run([OBJ, '-d', '--no-show-raw-insn', '--x86-asm-syntax=intel', f'--start-address=0x{a:X}',
                              f'--stop-address=0x{b:X}', exe], capture_output=True, text=True).stdout
        c = collections.Counter(); tot = 0; sp = 0
        for l in out.splitlines():
            mm = re.match(r'\s*[0-9a-f]+:\s+(\S+)\s*(.*)', l)
            if mm:
                tot += 1; c[mm.group(1)] += 1
                if re.search(r'(zmm|ymm)word ptr \[r(sp|bp)', mm.group(2)):
                    sp += 1
        print(f'{n[8:]:48s} {tot:5d} instr; derramamentos zmm/ymm: {sp:4d}; call: {c["call"]}')
