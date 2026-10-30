"""Rastro do laço de um plugin do EDGE pelo oráculo numa foto: registradores na entrada do laço e, nas primeiras
voltas, as leituras e as gravações.
uso: python -I pl_rastro.py <pasta do plugin> <ea hex> <função1,função2> <rótulo do laço hex> <pasta das fotos> <k> [regs...]"""
import importlib.util
import os
import subprocess
import sys

aqui = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location('si', os.path.join(aqui, 'spu_interp_c.py'))
si = importlib.util.module_from_spec(spec); spec.loader.exec_module(si)
pasta, ea_pl, fns, rlaco, fotos, k = sys.argv[1], sys.argv[2], sys.argv[3].split(','), sys.argv[4], sys.argv[5], int(sys.argv[6])
regs = [int(x) for x in sys.argv[7:]]
raiz = os.path.join(aqui, '..')
s = open(os.path.join(raiz, 'port', 'spu', pasta, 'spu_recomp.c.sem_promocao'), encoding='latin-1').read()
corpo = ''
for fn in fns:
    cab = f'void {pasta}_spu_func_{fn}(spu_context* ctx) {{\n'
    i = s.index(cab) + len(cab)
    b = s[i:s.index('\n}\n', i)]
    corpo += (f'loc_{int(fn, 16):08X}:\n' if f'loc_{int(fn, 16):08X}:' not in b else '') + b + '\n'
prog, rot = si.compila(corpo)
rot.setdefault('loc_FIM', len(prog))
codigo = bytes.fromhex(subprocess.run([sys.executable, '-I', os.path.join(aqui, 'elf_ea.py'), os.path.join(raiz, 'jogo', 'GOWA.elf'),
                                       ea_pl, '40'], capture_output=True, text=True).stdout.strip())
r0, ls0 = si.carrega(os.path.join(fotos, f'pl_foto_{k}_0.bin'))
m = si.Maquina([list(r) for r in r0], ls0)
m.reloc = ls0.find(codigo) - 0x30000
alvo = rot[f'loc_{int(rlaco, 16):08X}']
pc, volta = 0, -1
h = lambda v: ' '.join(f'{w:08X}' for w in v)
while pc < len(prog):
    if pc == alvo:
        volta += 1
        if volta == 0:
            for r in regs: print(f'r{r:<3}', h(m.g[r]))
        if volta < 4: print(f'-- volta {volta}')
    ins = prog[pc]
    if volta >= 0 and volta < 4 and ins[0] in ('st', 'ld'):
        a = m.end(ins[1] if ins[0] == 'st' else ins[2])
        r = ins[2] if ins[0] == 'st' else ins[1]
        val = m.g[r] if ins[0] == 'st' else si.w4(bytes(m.ls[a:a + 16]))
        print(f'   {ins[0]} r{r} {a:#07x}: {h(val)}')
    t = ins[0]; pc += 1
    if t == 'op':
        _, d, nome, args = ins; m.g[d] = list(si.OPS[nome](*[m.arg(a) for a in args]))
    elif t == 'ld':
        a = m.end(ins[2]); m.g[ins[1]] = si.w4(bytes(m.ls[a:a + 16]))
    elif t == 'st':
        a = m.end(ins[1]); m.ls[a:a + 16] = si.b16(m.g[ins[2]])
    elif t == 'k':
        m.g[ins[1]] = list(ins[2])
    elif t == 'if':
        _, r, bits, cmp, al = ins
        v = m.g[r][0] & (si.M32 if bits == 32 else 0xFFFF)
        if (v != 0) == (cmp == '!='): pc = rot.get(al, len(prog))
    elif t == 'goto':
        pc = rot.get(ins[1], len(prog))
print('voltas', volta + 1)
