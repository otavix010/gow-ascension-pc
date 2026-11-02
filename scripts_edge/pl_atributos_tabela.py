"""Tira a tabela (formato, deslocamento) de um plugin de atributos do vértice do EDGE (pl_atributos.h) rodando as
funções 0x30000 e 0x3000C dele pelo oráculo com entradas sintéticas: índices 0..15 em r9, cursor 0x20000 na palavra
em r5 + 8, r7 = 0x1000, r6 = 0. Imprime n, os formatos, os deslocamentos e qual registro guarda r9 + 13.
uso: python -I pl_atributos_tabela.py <pasta do plugin, ex. gowapl_007538C0>"""
import importlib.util
import os
import sys

aqui = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location('si', os.path.join(aqui, 'spu_interp_c.py'))
si = importlib.util.module_from_spec(spec); spec.loader.exec_module(si)
pasta = sys.argv[1]
s = open(os.path.join(aqui, '..', 'port', 'spu', pasta, 'spu_recomp.c.sem_promocao'), encoding='latin-1').read()
corpo = ''
for fn in ("00030000", "0003000C", "00030064"):
    cab = f'void {pasta}_spu_func_{fn}(spu_context* ctx) {{' + chr(10)
    if cab not in s:
        continue
    i = s.index(cab) + len(cab)
    b = s[i:s.index(chr(10) + '}' + chr(10), i)]
    linhas = []
    for l in b.split(chr(10)):
        t = l.strip()
        if 'g_spu_trampoline_fn' in t:   # 0x30000 -> 0x3000C: segue
            continue
        if t.startswith('SPU_RET'):
            linhas.append('SPU_RET(ctx);'); continue
        linhas.append(t)
    corpo += chr(10).join(linhas) + chr(10)
prog, rot = si.compila(corpo)
R5, R9, CUR = 0x10000, 0x10100, 0x20000
g = [[0, 0, 0, 0] for _ in range(128)]
for k in range(4):
    g[1][k] = 0x3FF00; g[5][k] = R5; g[9][k] = R9; g[7][k] = 0x1000; g[6][k] = 0
ls = bytearray(0x40000)
ls[R5 + 8:R5 + 12] = CUR.to_bytes(4, 'big')
for i in range(16):
    ls[R9 + i] = i
m = si.Maquina(g, ls)
pc, prep = 0, None
while pc < len(prog):
    ins = prog[pc]; pc += 1
    t = ins[0]
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
    else:
        break
    if prep is None:
        for r in range(2, 80):
            if m.g[r][0] == R9 + 13:
                prep = r
fim = int.from_bytes(m.ls[R5 + 8:R5 + 12], 'big')
n = (fim - CUR) // 16
fmt, desl = [], []
for i in range(n):
    w = [int.from_bytes(m.ls[CUR + 16 * i + 4 * k:CUR + 16 * i + 4 * k + 4], 'big') for k in range(4)]
    assert w[0] == 0x41740 + 4 * i and w[2] == 0x41680 + 4 * i, (i, [hex(x) for x in w])
    fmt.append(w[1]); desl.append(w[3] - 0x1000)
print('funções', '3000C' if 'spu_func_0003000C(' in s else 'só 30000', 'n', n, 'fmt', ', '.join(f'0x{x:X}u' for x in fmt), 'desl', ', '.join(f'{x}u' for x in desl), 'prep r%d' % prep)
