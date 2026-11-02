"""Confere o modelo do laço DE6C da DCC0 (índices empacotados com largura w <= 8 bits, MSB primeiro) contra o
oráculo: roda a DCC0 até a 1a entrada do DE6C, guarda o estado, roda até loc_0000E0E4 e compara cada halfword que
o laço gravou com bits(fluxo, j*w, w). uso: python -I de6c_modelo.py <spu_recomp.c original> <pasta das fotos>"""
import glob
import importlib.util
import os
import sys

aqui = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location('si', os.path.join(aqui, 'spu_interp_c.py'))
si = importlib.util.module_from_spec(spec); spec.loader.exec_module(si)
src, fotos = sys.argv[1], sys.argv[2]
fn = 'gowajm_0072DF80_spu_func_0000DCC0__b'
s = open(src, encoding='latin-1').read()
cab = f'static SPU_PN void {fn}(spu_context* ctx) {{' + chr(10)
i = s.index(cab) + len(cab)
corpo = s[i:s.index(chr(10) + '}' + chr(10), i)]
linhas = [('SPU_RET(ctx);' if l.strip().startswith('SPU_TAILX(') else l.strip()) for l in corpo.split(chr(10))
          if not l.strip().startswith('/*VK') and 'HALT-ASSERT' not in l and 'GOWA_EDGE_NATIVO' not in l]
prog, rot = si.compila(chr(10).join(linhas))


def roda(m, pc, ate):
    while pc < len(prog) and pc not in ate:
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
            return len(prog)
    return pc


DE6C, FIM = rot['loc_0000DE6C'], rot['loc_0000E0E4']
for p0 in sorted(glob.glob(os.path.join(fotos, 'pl_foto_*_0.bin')), key=lambda x: int(os.path.basename(x).split('_')[2])):
    r0, ls0 = si.carrega(p0)
    m = si.Maquina([list(r) for r in r0], ls0)
    pc = roda(m, 0, {DE6C, FIM})
    if pc != DE6C:
        continue
    g0 = [list(r) for r in m.g]
    antes = bytes(m.ls)
    pc = roda(m, pc, {FIM})
    depois = bytes(m.ls)
    w = g0[27][0] & 0xFFFF
    w = w.bit_length()
    mudou = [a for a in range(0, len(antes), 2) if antes[a:a + 2] != depois[a:a + 2]]
    if not mudou:
        print(os.path.basename(p0), 'nada gravado'); continue
    o0, o1 = min(mudou), max(mudou)
    out_base = o0 & ~15
    # o fluxo: procura a base b tal que bits(b, j*w) bate com as halfwords gravadas
    def bits(buf, base, j):
        bo = j * w
        a = base + (bo >> 3)
        hw = (buf[a] << 8) | buf[a + 1]
        return (hw >> (16 - w - (bo & 7))) & ((1 << w) - 1)
    hws = [(depois[out_base + 2 * j] << 8) | depois[out_base + 2 * j + 1] for j in range((o1 + 2 - out_base) // 2)]
    achou = None
    for base in range(max(0, out_base - 0x400), out_base + 0x2000):
        if all(bits(antes, base, j) == hws[j] for j in range(min(64, len(hws)))):
            achou = base; break
    ok = achou is not None and all(bits(antes, achou, j) == hws[j] for j in range(len(hws)))
    print(os.path.basename(p0), f'w={w} saída 0x{out_base:05X}..0x{o1 + 2:05X} ({len(hws)} halfwords) fluxo',
          hex(achou) if achou is not None else None, 'TODAS IGUAIS' if ok else 'DIFERENTE',
          f'r41=0x{g0[41][0]:05X} r10={g0[10][0] & 0xFFFFFFFF:08X} r32=0x{g0[32][0]:05X} r36={g0[36][0]:08X} r18=0x{g0[18][0]:05X}')
