"""Bancada do nativo DFF0 (índices de 9 a 16 bits da DCC0) contra o traduzido, pelo oráculo (spu_interp_c.py):
pega fotos da DCC0 (out/dcc0), troca a largura do cabeçalho por 9..16 e enche o fluxo empacotado com bits
aleatórios; roda a DCC0 traduzida inteira no oráculo (A) e, de novo, só até o laço DFF0, onde o nativo (bj_dff0.exe)
faz o laço e o oráculo segue de loc_0000E0E4 (B). Compara os estados finais.
uso: python -I bj_dff0.py [sementes por largura] [dff0|de6c]"""
import glob
import os
import random
import struct
import subprocess
import sys

aqui = os.path.dirname(os.path.abspath(__file__))
raiz = os.path.abspath(os.path.join(aqui, '..', '..'))
sys.path.insert(0, os.path.join(raiz, 'scripts_edge'))
import importlib.util
spec = importlib.util.spec_from_file_location('si', os.path.join(raiz, 'scripts_edge', 'spu_interp_c.py'))
si = importlib.util.module_from_spec(spec); spec.loader.exec_module(si)

MARCA = '/* GOWA_EDGE_NATIVO */'
FN = 'gowajm_0072DF80_spu_func_0000DCC0__b'


def programa():
    s = open(os.path.join(raiz, 'port', 'spu', 'gowajm_0072DF80', 'spu_recomp.c'), encoding='latin-1').read()
    cab = f'static SPU_PN void {FN}(spu_context* ctx) {{\n'
    i = s.index(cab) + len(cab)
    linhas = []
    for l in s[i:s.index('\n}\n', i)].split('\n'):
        t = l.strip()
        if MARCA in t or t.startswith('/*VK') or 'HALT-ASSERT' in t:
            continue
        if t.startswith('SPU_TAILX('):
            linhas.append('SPU_RET(ctx);'); continue
        linhas.append(t)
    prog, rot = si.compila('\n'.join(linhas))
    rot.setdefault('loc_FIM', len(prog))
    return prog, rot


def roda(prog, rot, m, pc=0, para=None, limite=10 ** 7):
    """como si.roda, começando em pc; para no índice 'para' (devolve True) ou no fim (False)"""
    n = 0
    while pc < len(prog):
        if pc == para:
            return True
        n += 1
        if n > limite: raise RuntimeError('laço sem fim?')
        ins = prog[pc]; pc += 1
        t = ins[0]
        if t == 'op':
            _, d, nome, args = ins
            m.g[d] = list(si.OPS[nome](*[m.arg(a) for a in args]))
        elif t == 'ld':
            a = m.end(ins[2]); m.g[ins[1]] = si.w4(bytes(m.ls[a:a + 16]))
        elif t == 'st':
            a = m.end(ins[1]); m.ls[a:a + 16] = si.b16(m.g[ins[2]])
        elif t == 'k':
            m.g[ins[1]] = list(ins[2])
        elif t == 'if':
            _, r, bits, cmp, alvo = ins
            v = m.g[r][0] & (0xFFFFFFFF if bits == 32 else 0xFFFF)
            if (v != 0) == (cmp == '!='): pc = rot.get(alvo, len(prog))
        elif t == 'goto':
            pc = rot.get(ins[1], len(prog))
    return False


def grava(p, m):
    with open(p, 'wb') as f:
        for r in m.g: f.write(struct.pack('<4I', *r))
        f.write(bytes(m.ls))


def main():
    sementes = int(sys.argv[1]) if len(sys.argv) > 1 else 3
    laco = sys.argv[2] if len(sys.argv) > 2 else 'dff0'   # dff0 (w 9..16) ou de6c (w 1..8)
    prog, rot = programa()
    p_dff0, p_e0e4 = rot['loc_0000DFF0' if laco == 'dff0' else 'loc_0000DE6C'], rot['loc_0000E0E4']
    exe = os.path.join(aqui, f'bj_{laco}.exe')
    tmp_in, tmp_out = os.path.join(aqui, 'bj_dff0_in.bin'), os.path.join(aqui, 'bj_dff0_out.bin')
    fotos = sorted(glob.glob(os.path.join(raiz, 'out', 'dcc0', 'pl_foto_*_0.bin')))
    tot = {}
    for p0 in fotos:
        r0, ls0 = si.carrega(p0)
        r4 = r0[4][0]
        larguras = range(9, 17) if laco == 'dff0' else range(1, 9)
        if (ls0[r4 + 6] >= 9) != (laco == 'dff0'):   # só as fotos que já vão por esse laço
            continue
        for w in larguras:
            for sem in range(sementes):
                rnd = random.Random(w * 1000 + sem + r4)
                ls = bytearray(ls0)
                ls[r4 + 6] = w
                n = (ls[r4] << 8 | ls[r4 + 1]) * 3 + 64   # bits suficientes para os índices (cabeçalho: contagem)
                for k in range(r4 + 16, min(r4 + 16 + n * 2, 0x40000)):
                    ls[k] = rnd.randrange(256)
                a = si.Maquina([list(r) for r in r0], ls)
                b = si.Maquina([list(r) for r in r0], bytearray(ls))
                roda(prog, rot, a)
                if not roda(prog, rot, b, para=p_dff0):
                    tot.setdefault(w, [0, 0, 0, 0])[3] += 1   # não passou pelo DFF0
                    continue
                grava(tmp_in, b)
                feito = subprocess.run([exe, tmp_in, tmp_out]).returncode
                c = tot.setdefault(w, [0, 0, 0, 0])
                if feito != 1:
                    c[1] += 1
                    continue
                rb, lb = si.carrega(tmp_out)
                b = si.Maquina([list(r) for r in rb], bytearray(lb))
                roda(prog, rot, b, pc=p_e0e4)
                dr = [j for j in range(128) if a.g[j] != b.g[j]]
                dl = [j for j in range(0x40000) if a.ls[j] != b.ls[j]]
                c[0] += 1
                if dr or dl:
                    c[2] += 1
                    print(f'{os.path.basename(p0)} w={w} semente {sem}: regs {dr[:6]} LS {len(dl)} bytes' +
                          (f' (1o 0x{dl[0]:05X})' if dl else ''))
    for w in sorted(tot):
        f, rec, d, nao = tot[w]
        print(f'w={w}: {f} feitos, {rec} recusas, {d} com diferença, {nao} sem DFF0')


main()
