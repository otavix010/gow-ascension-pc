"""Bancada do General6 (0x727200), função 0x4930 (ruído de Perlin melhorado, 4 pontos): roda o C traduzido
(forma __m, registradores _rN) pelo oráculo spu_interp_c.py sobre as fotos da sonda_fn.py e, com --traco,
mostra a árvore das contas de float de um lane da saída r3 (a ordem exata das somas e produtos).

uso: python -I g6_traco.py <spu_recomp.c> <pasta das fotos> [--traco <foto> <lane>]"""
import glob
import os
import re
import sys

sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..'))
import spu_interp_c as O

FN = 'gowajm_00727200_spu_func_00004930__m'
O.OPS.setdefault('xori', lambda a, i: [(w ^ (O.sx(i, 10) & O.M32)) & O.M32 for w in a])


def corpo(src):
    s = open(src, encoding='latin-1').read()
    i = s.index(f'static SPU_PN void {FN}(spu_context* ctx) {{')
    f = s[i:s.index('\n}\n', i)]
    f = f[f.index('m_00004930:;'):]
    out = []
    for l in f.split('\n'):
        l = l.strip()
        if l in ('RELOAD;', 'SPILL;') or l.startswith('/*') or not l:
            continue
        l = l.replace('SPILL; SPU_RET(ctx); RELOAD;', 'SPU_RET(ctx);')
        m = re.fullmatch(r'_r(\d+) = spu_link\((0x[0-9A-F]+)\); goto m_[0-9A-F]+;', l)
        if m:   # o link: endereço de volta em todos os lanes da palavra 0 (só a palavra 0 importa)
            out.append(f'ctx->gpr[{m.group(1)}] = spu_ila({m.group(2)});')
            continue
        l = re.sub(r'^m_([0-9A-F]+):;$', r'loc_\1:', l)
        l = re.sub(r'goto m_([0-9A-F]+);', r'goto loc_\1;', l)
        l = re.sub(r'_r(\d+)', r'ctx->gpr[\1]', l)
        out.append(l)
    return '\n'.join(out)


# ---- traço: cada lane de cada registrador carrega a expressão que o produziu (só contas de float)
FLOAT = {'fa', 'fa_r', 'fs', 'fs_r', 'fm', 'fm_r', 'fma', 'fma_r', 'fms', 'fms_r', 'fnms', 'fnms_r'}
SIMB = {'fa': '+', 'fa_r': '+', 'fs': '-', 'fs_r': '-', 'fm': '*', 'fm_r': '*'}


def traco(prog, rot, maq, foto, lane):
    ex = [[f'r{r}.{k}' for k in range(4)] for r in range(128)]
    for r in (3, 4, 5, 6):
        ex[r] = [f'P{r - 3}{"xyzw"[k]}' for k in range(4)]
    pc = 0
    while pc < len(prog):
        ins = prog[pc]; pc += 1
        t = ins[0]
        if t == 'op':
            _, d, nome, args = ins
            vals = [maq.arg(a) for a in args]
            res = list(O.OPS[nome](*vals))
            regs = [int(re.fullmatch(O.R_REG, a.strip()).group(1)) if re.fullmatch(O.R_REG, a.strip()) else None for a in args]
            if nome in FLOAT:
                e = []
                for k in range(4):
                    o = [ex[r][k] if r is not None else str(v) for r, v in zip(regs, args)]
                    if nome in SIMB: e.append(f'({o[0]} {SIMB[nome]} {o[1]})')
                    elif nome.startswith('fma'): e.append(f'fma({o[0]}, {o[1]}, {o[2]})')
                    elif nome.startswith('fms'): e.append(f'fms({o[0]}, {o[1]}, {o[2]})')
                    else: e.append(f'fnms({o[0]}, {o[1]}, {o[2]})')
                ex[d] = e
            elif nome in ('shufb', 'rotqby', 'rotqbyi', 'shlqbyi'):
                # palavra a palavra: de onde vem cada palavra da saída (se vier inteira)
                src = []
                for r, v in zip(regs, vals):
                    if r is not None and nome != 'shufb' or (nome == 'shufb' and r is not None and len(src) < 2):
                        src.append(r)
                e = []
                for k in range(4):
                    w = res[k]
                    cand = [ex[r][j] for r in src[:2] if r is not None for j in range(4) if maq.g[r][j] == w]
                    e.append(cand[0] if len(set(cand)) == 1 else (cand[0] if cand else f'?{nome}'))
                ex[d] = e
            else:
                ex[d] = [f'{nome}#{d}'] * 4
            maq.g[d] = res
        elif t == 'ld':
            a = maq.end(ins[2]); maq.g[ins[1]] = O.w4(bytes(maq.ls[a:a + 16]))
            ex[ins[1]] = [f'T{a:05X}.{k}' for k in range(4)]
        elif t == 'st':
            a = maq.end(ins[1]); maq.ls[a:a + 16] = O.b16(maq.g[ins[2]])
        elif t == 'k':
            maq.g[ins[1]] = list(ins[2]); ex[ins[1]] = ['k'] * 4
        elif t == 'goto':
            pc = rot.get(ins[1], len(prog))
    return ex[3][lane]


def main():
    src, pasta = sys.argv[1], sys.argv[2]
    prog, rot = O.compila(corpo(src))
    if '--traco' in sys.argv:
        i = sys.argv.index('--traco')
        foto, lane = sys.argv[i + 1], int(sys.argv[i + 2])
        r0, ls0 = O.carrega(foto)
        print(traco(prog, rot, O.Maquina([list(r) for r in r0], ls0), foto, lane))
        return
    ok = dif = 0
    for p0 in sorted(glob.glob(os.path.join(pasta, 'pl_foto_*_0.bin'))):
        r0, ls0 = O.carrega(p0)
        r1, _ = O.carrega(p0.replace('_0.bin', '_1.bin'))
        maq = O.Maquina([list(r) for r in r0], ls0)
        O.roda(prog, rot, maq)
        if maq.g[3] == r1[3]: ok += 1
        else:
            dif += 1
            if dif <= 3: print('difere', os.path.basename(p0), [hex(x) for x in maq.g[3]], [hex(x) for x in r1[3]])
    print(f'oráculo x fotos (r3): {ok} iguais, {dif} diferentes')


if __name__ == '__main__':
    main()
