"""Modelo do laço D4DC da D250 (projeção dos vértices) contra o oráculo: roda a D250 inteira pelo oráculo em cada
foto e compara, vértice a vértice, x e y de tela e o código de recorte com o modelo; também lista as condições de
cada vértice ao lado do código, para achar o mapa dos bits.
uso: python -I d250_modelo.py <spu_recomp.c original> <pasta das fotos>"""
import glob
import importlib.util
import os
import sys

aqui = os.path.dirname(os.path.abspath(__file__))
spec = importlib.util.spec_from_file_location('si', os.path.join(aqui, 'spu_interp_c.py'))
si = importlib.util.module_from_spec(spec); spec.loader.exec_module(si)
src, fotos = sys.argv[1], sys.argv[2]
fn = 'gowajm_0072DF80_spu_func_0000D250__b'
s = open(src, encoding='latin-1').read()
cab = f'static SPU_PN void {fn}(spu_context* ctx) {{\n'
i = s.index(cab) + len(cab)
corpo = s[i:s.index('\n}\n', i)]
linhas = [l.strip() for l in corpo.split('\n') if not l.strip().startswith('/*VK') and 'HALT-ASSERT' not in l]
prog, rot = si.compila('\n'.join(linhas))
alvo = rot['loc_0000D4DC']
O = si.OPS
sp = lambda w: [w] * 4


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


def vertice(g, x, y, z):
    """palavras de x, y, z -> (sx, sy, flags) pelas mesmas contas do laço (uma faixa)"""
    X, Y, Z = sp(x), sp(y), sp(z)
    def lin(cx, cy, cz, cw):
        return O['fma'](Z, sp(cz), O['fma'](Y, sp(cy), O['fma_r'](X, sp(cx), sp(cw))))
    xc = lin(g[48][0], g[45][0], g[42][0], g[52][0])
    yc = lin(g[47][0], g[44][0], g[41][0], g[51][0])
    zc = lin(g[46][0], g[43][0], g[40][0], g[49][0])
    w = lin(g[55][0], g[54][0], g[53][0], g[56][0])
    y0 = O['fi'](w, O['frest'](w))
    inv = O['fma'](O['fnms_r'](y0, w, sp(0x3F800000)), y0, y0)
    sg = w[0] & 0x80000000
    xn = [O['fm'](xc, inv)[0] ^ sg] * 4
    yn = [O['fm'](yc, inv)[0] ^ sg] * 4
    zn = [O['fm'](zc, inv)[0] ^ sg] * 4
    sx = O['fma'](xn, sp(g[77][0]), sp(g[76][0]))[0]
    sy = O['fma'](yn, sp(g[73][0]), sp(g[72][0]))[0]
    sz = O['fma'](zn, sp(g[68][0]), sp(g[67][0]))[0]
    gt = lambda a, b: O['fcgt'](sp(a), sp(b))[0] != 0
    f = {'x>max': gt(sx, g[60][0]), 'x<min': gt(g[62][0], sx), 'y>max': gt(sy, g[57][0]), 'y<min': gt(g[61][0], sy),
         'z>max': gt(sz, g[63][0]), 'z<min': gt(g[64][0], sz), 'w>0': gt(w[0], g[58][0])}
    return sx, sy, f


def main():
    pares = []
    ok_xy = ruim_xy = 0
    for k in sorted({int(os.path.basename(f).split('_')[2]) for f in glob.glob(os.path.join(fotos, 'pl_foto_*_0.bin'))}):
        r0, ls0 = si.carrega(os.path.join(fotos, f'pl_foto_{k}_0.bin'))
        m = si.Maquina([list(r) for r in r0], ls0)
        pc = 0
        while pc != alvo:
            pc = passo(m, pc)
        g = [list(r) for r in m.g]; ls_e = bytes(m.ls)
        while pc < len(prog):
            pc = passo(m, pc)
        n = (0x100000000 - g[23][0]) // 64 if g[23][0] else 0
        ent = g[39][0] + g[23][0]   # 1o vértice (r39 + r23)
        out = (g[66][0] + g[21][0]) & 0x3FFFF
        for j in range(max(0, n - 2)):
            for v in range(4):
                a = (ent + 64 * j + 16 * v) & 0x3FFFF
                x, y, z, _ = si.w4(ls_e[a:a + 16])
                sx, sy, f = vertice(g, x, y, z)
                o = si.w4(bytes(m.ls[out + 64 * j + 16 * v:out + 64 * j + 16 * v + 16]))
                if (o[0], o[1]) == (sx, sy): ok_xy += 1
                else:
                    ruim_xy += 1
                    if ruim_xy < 5: print('xy dif', k, j, v, hex(o[0]), hex(sx), hex(o[1]), hex(sy))
                pares.append((f, o[2], o[3]))
        print(f'foto {k}: {n} voltas', flush=True)
    print(f'xy: {ok_xy} iguais, {ruim_xy} diferentes')
    nomes = list(pares[0][0]) if pares else []
    for bit in range(32):
        col = [(c >> bit) & 1 for _, c, _ in pares]
        if not any(col): continue
        for nm in nomes:
            if all(int(f[nm]) == b for (f, _, _), b in zip(pares, col)): print(f'bit {bit} = {nm}'); break
            if all(int(not f[nm]) == b for (f, _, _), b in zip(pares, col)): print(f'bit {bit} = não {nm}'); break
        else:
            print(f'bit {bit}: sem condição única ({sum(col)} de {len(col)})')
    print('palavra 3 sempre 0:', all(w3 == 0 for _, _, w3 in pares))


main()
