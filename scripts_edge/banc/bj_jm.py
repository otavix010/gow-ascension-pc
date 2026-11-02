"""Bancada de uma função do job 0x72DF80 com seus nativos contra a função traduzida, pelo oráculo (spu_interp_c.py),
sobre fotos reais (out/<pasta>) com os floats trocados por valores aleatórios da mesma grandeza (ponteiros e inteiros
ficam). A: a função traduzida inteira no oráculo. B: o oráculo até cada gancho, o nativo em C (bj_nat.exe) e, pelo
que ele devolver, o destino do gancho (ou segue o traduzido se recusou). Compara os estados finais.
Os ganchos vêm do edge_nativo.py (tipos 'multi', 'antes', 'antes_vk' da função).
uso: python -I bj_jm.py <função, ex. E550> <pasta das fotos em out/> [variações por foto] [prob. de troca] [rótulo de
entrada rótulo de saída (fotos de um trecho)]"""
import glob
import os
import random
import struct
import subprocess
import sys

aqui = os.path.dirname(os.path.abspath(__file__))
raiz = os.path.abspath(os.path.join(aqui, '..', '..'))
import importlib.util
spec = importlib.util.spec_from_file_location('si', os.path.join(raiz, 'scripts_edge', 'spu_interp_c.py'))
si = importlib.util.module_from_spec(spec); spec.loader.exec_module(si)

MARCA = '/* GOWA_EDGE_NATIVO */'


def ganchos(fn):
    src = open(os.path.join(raiz, 'scripts_edge', 'edge_nativo.py'), encoding='utf-8').read()
    i = src.index('GANCHOS = ['); j = src.index('\n]\n', i)
    out = []
    for g in eval(src[i + len('GANCHOS = '):j + 2]):
        if g['fn'] != fn: continue
        if g['tipo'] == 'multi':
            out.append((g['rotulo'], g['nat'], g['destinos'], bool(g.get('vk')) or g.get('pos') == 'antes'))
        elif g['tipo'] in ('antes', 'antes_vk'):
            rot = g.get('rotulo') or 'loc_9' + g['vk'][4:].split('/')[1][:8][1:]
            out.append((rot, g['nat'], [g['destino']], True))
        else:
            print('gancho não tratado:', g['tipo'], g['nat'])
    return out


def fim_rot(r):
    """loc_0000E134_fim -> loc_8000E134 (o oráculo só aceita rótulos loc_<hex>)"""
    return 'loc_8' + r[5:12] if r.endswith('_fim') else r


def programa(fn, r_in=None, r_out=None):
    """a função inteira ou só o trecho de r_in até r_out (as fotos tiradas pelo sonda_rotulos.py)"""
    s = open(os.path.join(raiz, 'port', 'spu', 'gowajm_0072DF80', 'spu_recomp.c'), encoding='latin-1').read()
    cab = f'static SPU_PN void {fn}(spu_context* ctx) {{\n'
    i = s.index(cab) + len(cab)
    corpo = s[i:s.index('\n}\n', i)]
    if r_in:
        corpo = corpo[corpo.index(f'\n{r_in}:\n'):corpo.index(f'\n{r_out}:\n')]
    linhas = []
    for l in corpo.split('\n'):
        t = l.strip()
        if MARCA in t and t.split()[0].endswith('_fim:'):   # rótulo do fim de laço dos ganchos: loc_8 + endereço
            linhas.append(fim_rot(t.split()[0][:-1]) + ':'); continue
        if MARCA in t or 'HALT-ASSERT' in t:
            continue
        if t.startswith('/*VK '):   # vira rótulo (loc_9 + endereço do laço): os ganchos 'antes_vk' ficam aqui
            linhas.append('loc_9%s:' % t[4:].split('/')[1][:8][1:]); continue
        if t.startswith('/*LUZGPU') or t.startswith('/*NINHO'):
            continue
        if t.startswith('SPU_TAILX('):
            linhas.append('SPU_RET(ctx);'); continue
        linhas.append(t)
    prog, rot = si.compila('\n'.join(linhas))
    rot.setdefault('loc_FIM', len(prog))
    return prog, rot


def roda(prog, rot, m, ganchos_pc=None, chama=None, limite=2 * 10 ** 7):
    """como si.roda; em ganchos_pc[pc] (só chegando em sequência se o gancho é 'antes') chama o nativo"""
    pc, n, saltou = 0, 0, False
    while pc < len(prog):
        if ganchos_pc and pc in ganchos_pc:
            nat, destinos, so_seq = ganchos_pc[pc]
            if not (so_seq and saltou):
                k = chama(nat, m)
                if k:
                    pc = rot.get(fim_rot(destinos[k - 1]), len(prog)); saltou = True
                    continue
        n += 1
        if n > limite: raise RuntimeError('laço sem fim?')
        ins = prog[pc]; pc += 1; saltou = False
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
            if (v != 0) == (cmp == '!='): pc = rot.get(alvo, len(prog)); saltou = True
        elif t == 'goto':
            pc = rot.get(ins[1], len(prog)); saltou = True
    return n


def grava(p, m):
    with open(p, 'wb') as f:
        for r in m.g: f.write(struct.pack('<4I', *r))
        f.write(bytes(m.ls))


def troca_floats(ls, rnd, prob, regs=()):
    """troca floats "normais" (expoente entre 2^-20 e 2^20) por outro da mesma grandeza e sinal aleatório; os
    quadwords com cópia num registrador (já carregados antes da foto, que os nativos conferem) ficam"""
    copias = {si.b16(r) for r in regs}
    for a in range(0, len(ls) - 3, 4):
        if bytes(ls[a & ~15:(a & ~15) + 16]) in copias:
            continue
        w = ls[a] << 24 | ls[a + 1] << 16 | ls[a + 2] << 8 | ls[a + 3]
        e = (w >> 23) & 0xFF
        if 107 <= e <= 147 and rnd.random() < prob:
            dx = int(os.environ.get('BJ_EXP', '2'))   # o quanto o expoente pode mudar
            e2 = max(1, min(254, e + rnd.randint(-dx, dx)))
            w = (rnd.getrandbits(1) << 31) | (e2 << 23) | rnd.getrandbits(23)
            ls[a:a + 4] = bytes((w >> 24, (w >> 16) & 255, (w >> 8) & 255, w & 255))


def main():
    fn = 'gowajm_0072DF80_spu_func_0000%s__b' % sys.argv[1].upper()
    pasta = os.path.join(raiz, 'out', sys.argv[2])
    variacoes = int(sys.argv[3]) if len(sys.argv) > 3 else 2
    prob = float(sys.argv[4]) if len(sys.argv) > 4 else 0.5
    r_in, r_out = (sys.argv[5], sys.argv[6]) if len(sys.argv) > 6 else (None, None)
    prog, rot = programa(fn, r_in, r_out)
    # registradores vivos na saída (os outros são temporários que os nativos podem deixar diferentes); padrão: todos
    vivos = [int(x) for x in os.environ['BJ_VIVOS'].split(',')] if os.environ.get('BJ_VIVOS') else list(range(128))
    gs = ganchos(fn)
    gpc = {}
    for rotulo, nat, destinos, so_seq in gs:
        if rotulo not in rot:
            print('rótulo do gancho não achado:', rotulo, nat); continue
        gpc[rot[rotulo]] = (nat, destinos, so_seq)
    print('ganchos:', [(g[1], g[0]) for g in gs])
    exe = os.path.join(aqui, 'bj_nat.exe')
    t_in, t_out = os.path.join(aqui, 'bj_jm_in.bin'), os.path.join(aqui, 'bj_jm_out.bin')
    cont = {}

    def chama(nat, m):
        grava(t_in, m)
        k = subprocess.run([exe, nat, t_in, t_out]).returncode
        if k == 255: raise RuntimeError('bj_nat falhou em ' + nat)
        c = cont.setdefault(nat, [0, 0]); c[0 if k else 1] += 1
        if k:
            r, l = si.carrega(t_out)
            m.g[:] = [list(x) for x in r]; m.ls[:] = l
        return k

    tot = dif = 0
    for p0 in sorted(glob.glob(os.path.join(pasta, 'pl_foto_*_0.bin')), key=lambda x: int(os.path.basename(x).split('_')[2])):
        r0, ls0 = si.carrega(p0)
        for v in range(variacoes):
            rnd = random.Random(hash((os.path.basename(p0), v)) & 0xFFFFFFFF)
            ls = bytearray(ls0)
            if v: troca_floats(ls, rnd, prob, r0)   # a variação 0 é a foto como está
            a = si.Maquina([list(r) for r in r0], bytearray(ls))
            b = si.Maquina([list(r) for r in r0], bytearray(ls))
            try:
                roda(prog, rot, a)
                roda(prog, rot, b, gpc, chama)
            except RuntimeError as e:
                print(os.path.basename(p0), v, 'erro:', e); continue
            dr = [j for j in vivos if a.g[j] != b.g[j]]
            dl = [j for j in range(0x40000) if a.ls[j] != b.ls[j]]
            tot += 1
            if dr or dl:
                dif += 1
                print(f'{os.path.basename(p0)} variação {v}: regs {dr[:8]} LS {len(dl)} bytes' + (f' (1o 0x{dl[0]:05X})' if dl else ''))
    print(f'{tot} casos, {dif} com diferença; chamadas dos nativos (feitas, recusadas): {cont}')


main()
