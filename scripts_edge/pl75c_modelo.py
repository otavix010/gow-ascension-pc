"""Modelo do laço 0x300D0 do plugin 0x75C570 (float4 -> float3 empacotado) contra o oráculo: roda o plugin até a
entrada do laço; num caminho o oráculo faz o laço inteiro, no outro o modelo grava os vértices das voltas
0..n-2 (12 de cada 16 bytes: out + 12k <- in + 16k) e monta os registradores vivos da última volta, que o oráculo
roda. Compara a LS inteira e os registradores no fim.
uso: python -I pl75c_modelo.py <pasta das fotos>"""
import glob
import importlib.util
import os
import subprocess
import sys

aqui = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location('si', os.path.join(aqui, 'spu_interp_c.py'))
si = importlib.util.module_from_spec(spec); spec.loader.exec_module(si)
O = si.OPS
fotos = sys.argv[1]
pasta = 'gowapl_0075C570'
raiz = os.path.join(aqui, '..')
s = open(os.path.join(raiz, 'port', 'spu', pasta, 'spu_recomp.c.sem_promocao'), encoding='latin-1').read()
corpo = ''
for fn in ('00030000', '00030010', '000300D0'):
    cab = f'void {pasta}_spu_func_{fn}(spu_context* ctx) {{\n'
    i = s.index(cab) + len(cab)
    b = s[i:s.index('\n}\n', i)]
    corpo += (f'loc_{int(fn, 16):08X}:\n' if f'loc_{int(fn, 16):08X}:' not in b else '') + b + '\n'
prog, rot = si.compila(corpo)
rot.setdefault('loc_FIM', len(prog))
codigo = bytes.fromhex(subprocess.run([sys.executable, '-I', os.path.join(aqui, 'elf_ea.py'), os.path.join(raiz, 'jogo', 'GOWA.elf'),
                                       '75C570', '40'], capture_output=True, text=True).stdout.strip())
alvo = rot['loc_000300D0']


def passo(m, pc):
    ins = prog[pc]; pc += 1
    t = ins[0]
    if t == 'op':
        _, d, nome, args = ins; m.g[d] = list(O[nome](*[m.arg(a) for a in args]))
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
    return pc


def ld16(ls, a):
    a &= 0x3FFF0
    return si.w4(bytes(ls[a:a + 16]))


TEMP = set(range(33, 45))
ok = ruim = 0
for p0 in sorted(glob.glob(os.path.join(fotos, 'pl_foto_*_0.bin')), key=lambda x: int(os.path.basename(x).split('_')[2])):
    k = int(os.path.basename(p0).split('_')[2])
    r0, ls0 = si.carrega(p0)
    m = si.Maquina([list(r) for r in r0], ls0)
    m.reloc = ls0.find(codigo) - 0x30000
    pc = 0
    while pc != alvo:
        pc = passo(m, pc)
    g0 = [list(r) for r in m.g]; ls_e = bytearray(m.ls)
    # caminho do oráculo
    while pc < len(prog):
        pc = passo(m, pc)
    g_o, ls_o = [list(r) for r in m.g], bytes(m.ls)
    # caminho do modelo
    tot = (0x100000000 - g0[10][0]) & 0xFFFFFFFF
    n = tot // 24
    out = (g0[23][0] - tot) & 0x3FFFF
    ent = (g0[12][0] - 96) & 0x3FFFF
    J = n - 1
    conf = (g0[9] == si.w4(bytes(ls_e[ent:ent + 12] + ls_e[ent + 16:ent + 20])) and
            g0[11][:2] == si.w4(bytes(ls_e[ent + 20:ent + 28]) + bytes(8))[:2] and
            g0[16] == ld16(ls_e, ent + 32) and g0[13] == ld16(ls_e, ent + 48) and
            g0[2] == ld16(ls_e, ent + 64) and g0[15] == ld16(ls_e, ent + 80))
    ls_m = bytearray(ls_e)
    for v in range(2 * J):
        ls_m[out + 12 * v:out + 12 * v + 12] = ls_e[ent + 16 * v:ent + 16 * v + 12]
    m2 = si.Maquina([list(r) for r in g0], ls_m)
    m2.reloc = m.reloc
    a, b = ent + 32 * J, ent + 32 * J + 16
    m2.g[9] = si.w4(bytes(ls_e[a:a + 12] + ls_e[b:b + 4]))
    m2.g[11] = si.w4(bytes(ls_e[b + 4:b + 12]) + bytes(8))[:2] + g0[11][2:]
    m2.g[16] = ld16(ls_e, ent + 32 * (J + 1)); m2.g[13] = ld16(ls_e, ent + 32 * (J + 1) + 16)
    m2.g[2] = ld16(ls_e, ent + 32 * (J + 2)); m2.g[15] = ld16(ls_e, ent + 32 * (J + 2) + 16)
    for r in (10, 14):
        m2.g[r] = [(x + 24 * J) & 0xFFFFFFFF for x in g0[r]]
    m2.g[12] = [(x + 32 * J) & 0xFFFFFFFF for x in g0[12]]
    m2.g[17] = ld16(ls_m, out + 24 * J)
    pc = alvo
    while pc < len(prog):
        pc = passo(m2, pc)
    dr = [j for j in range(128) if m2.g[j] != g_o[j] and j not in TEMP]
    drt = [j for j in range(128) if m2.g[j] != g_o[j] and j in TEMP]
    dl = [j for j in range(0x40000) if m2.ls[j] != ls_o[j]]
    bom = not dr and not dl and conf
    ok += bom; ruim += not bom
    print(f'foto {k:2d}: n={n:3d} out=0x{out:05X} ent=0x{ent:05X} conf={conf} | regs dif {dr} (temp {drt}) | LS dif {len(dl)}'
          + (f' 1a 0x{dl[0]:05X}' if dl else ''))
print(f'{ok} iguais, {ruim} diferentes')
