"""Gera um núcleo nativo AVX-512 para QUATRO chamadas independentes de uma
cadeia SPU sem desvios (função pura dos argumentos): cada faixa de 128 bits do
zmm é uma chamada. Exato bit a bit; se alguma conta de float cair no alcance
estendido, ou se uma leitura puder pegar a pilha que a cadeia grava, devolve 0
e o chamador faz as quatro chamadas pelo código traduzido.

Caso de uso (23/10): o laço 0x63B4 do job 0x727200 (General6) chama a cadeia
0x4930 -> 0x4AB0 -> 0x4E90 -> 0x4E9C -> 0x4F00 quatro vezes por iteração, cada
uma com r3..r6 próprios, e soma os quatro r3 (ruído em oitavas). A cadeia só lê
r1, r3..r6 e tabelas da LS; grava na pilha os registradores salvos (r80..r94,
r126) e os relê no fim.

Lê a cadeia da fonte sem promoção (spu_recomp.c.sem_promocao), segue os saltos
de trampolim até o SPU_RET. Saída: <dir>/vc_<início>.inc com
  int gowa_vc_<mod>_<início>(spu_context* ctx, const u128* in, u128 r1, u128* res, u128* fim)
     in[4*L + j] = r(3+j) da chamada L; res[L] = r3 devolvido pela chamada L;
     fim[2..79] = os registradores voláteis no fim da ÚLTIMA chamada
  void gowa_vc_<mod>_<início>_fim(spu_context* ctx, const u128* fim)
     o estado que a última chamada deixa: as gravações da pilha + fim[] em ctx->gpr
  e a conferência GOWA_VC_CHECK=N (a cada N chamadas refaz as quatro pelo código
  traduzido numa cópia do contexto e compara; diferença -> [vc] DIFERENÇA e o
  núcleo desliga). GOWA_VC=0 desliga.

uso: python vetorizar_chamadas.py <dir do módulo> <função hex de início>"""
import re
import sys
from pathlib import Path

d, ini = Path(sys.argv[1]), sys.argv[2].upper().rjust(8, '0')
mod = d.name
src = (d / 'spu_recomp.c.sem_promocao').read_text(encoding='latin-1')


def corpo(fn):
    m = re.search(r'^void ' + mod + r'_spu_func_' + fn + r'\(spu_context\* ctx\) \{\n', src, re.M)
    assert m, 'função não achada: ' + fn
    return src[m.end():src.find('\n}\n', m.end())]


# ---------------------------------------------------------------- cadeia
linhas, fn, cadeia = [], ini, []
while fn:
    cadeia.append(fn)
    ls_ = [t.strip() for t in corpo(fn).split('\n')]
    ls_ = [t for t in ls_ if t and not t.startswith('/*')]
    prox = None
    for t in ls_:
        assert not t.startswith('if ') and 'goto' not in t, 'desvio na cadeia: ' + t
        mt = re.search(r'\{ ctx->pc = 0x([0-9A-F]+); g_spu_trampoline_fn = \w+; return; \}', t)
        if t == 'SPU_RET(ctx);':
            prox = 'RET'
            break
        if mt:
            prox = mt.group(1).rjust(8, '0')
            t = t[:mt.start()].strip()
            if t:
                linhas.append(t)
            break
        linhas.append(t)
    assert prox, 'fim da função sem salto: ' + fn
    fn = None if prox == 'RET' else prox
R = re.compile(r'ctx->gpr\[(\d+)\]')
P = []
for t in linhas:
    mm = re.fullmatch(r'ctx->gpr\[(\d+)\] = spu_(\w+)\((.*)\);', t)
    ms = re.fullmatch(r'spu_ls_write128\(ctx, (.*), ctx->gpr\[(\d+)\]\);', t)
    if mm:
        w, op, args = int(mm.group(1)), mm.group(2), mm.group(3)
        if op == 'ls_read128':
            P.append(dict(w=w, op='ld', addr=args[len('ctx, '):]))
        else:
            P.append(dict(w=w, op=op, args=[x.strip() for x in args.split(',')]))
    elif ms:
        P.append(dict(w=None, op='st', addr=ms.group(1), val=int(ms.group(2))))
    else:
        raise SystemExit('comando não suportado: ' + t)
N = len(P)
# ---------------------------------------------------------------- SSA e uniformidade
# valor: ('in', r) entrada; ('d', k) definição k. Classe: 'U' igual nas 4 chamadas
# (r1, imediatos, LS de endereço uniforme fora da pilha), 'V' por chamada, 'X' só salvo
ARGS = (3, 4, 5, 6)
cur = {}
cls = {}


def val(r):
    return cur.get(r, ('in', r))


def cl(v):
    if v[0] == 'in':
        return 'U' if v[1] == 1 else 'V' if v[1] in ARGS else 'X'
    return cls[v[1]]


def termos(addr):
    out = []
    for t in addr.split(' + '):
        t = t.strip()
        mm = re.fullmatch(r'ctx->gpr\[(\d+)\]\._u32\[0\]', t)
        out.append(('r', int(mm.group(1))) if mm else ('c', int(t.replace('-0x', '-0x'), 0)))
    return out


pilha = {}          # deslocamento a partir de r1 de entrada -> registrador salvo
morto = set()       # leituras de restauração (valor = o salvo; ninguém usa)
r1off = {('in', 1): 0}   # valores que são r1 de entrada + deslocamento (ai sobre r1)


def em_r1(ts):
    """endereço [r, c] com r = r1 de entrada + o: devolve o + c, senão None"""
    if len(ts) == 2 and ts[0][0] == 'r' and ts[1][0] == 'c' and val(ts[0][1]) in r1off:
        return r1off[val(ts[0][1])] + ts[1][1]
    return None


for k, c in enumerate(P):
    if c['op'] == 'st':
        ts = termos(c['addr'])
        off = em_r1(ts)
        assert off is not None and off == ts[1][1], 'gravação fora da pilha: ' + c['addr']
        pilha[off] = c['val']
        c['in'] = [val(c['val'])]
        continue
    if c['op'] == 'ld':
        ts = termos(c['addr'])
        c['in'] = [val(t[1]) for t in ts if t[0] == 'r']
        if em_r1(ts) is not None and em_r1(ts) in pilha:
            morto.add(k)
            cls[k] = 'X'
        else:
            cls[k] = 'U' if all(cl(v) == 'U' for v in c['in']) else 'V'
            assert all(cl(v) != 'X' for v in c['in'])
    else:
        c['in'] = [val(int(R.fullmatch(x).group(1))) if R.fullmatch(x) else None for x in c['args']]
        rs = [v for v in c['in'] if v is not None]
        assert all(cl(v) != 'X' for v in rs), 'registrador só salvo usado em conta: %s' % c
        cls[k] = 'U' if all(cl(v) == 'U' for v in rs) else 'V'
        if c['op'] == 'ai' and c['in'][0] in r1off and re.fullmatch(r'-?\d+', c['args'][1]):
            r1off[('d', k)] = r1off[c['in'][0]] + int(c['args'][1])
    cur[c['w']] = ('d', k)
fim_def = dict(cur)
assert r1off.get(fim_def.get(1)) == 0, 'r1 não volta ao de entrada'
for r in range(80, 95):
    v = fim_def.get(r)
    assert v is None or v[1] in morto, 'r%d não volta ao valor salvo' % r
# ---------------------------------------------------------------- geração
FLOAT = {'fa', 'fs', 'fm', 'fma', 'fms', 'fnms'}
VEC2 = {'fcgt', 'fceq', 'fcmgt', 'fcmeq', 'and', 'or', 'xor', 'andc', 'nor', 'nand', 'orc',
        'a', 'sf', 'ceq', 'cgt', 'clgt', 'fi', 'clgth', 'ceqh', 'cgth', 'rotm', 'rotqby', 'shl',
        'rotqmbybi', 'rotqmbi', 'rotma', 'rothm', 'rot', 'sfh', 'ah'}
VEC3 = {'selb', 'shufb'}
VEC1 = {'frsqest', 'frest', 'cntb', 'gbh', 'gb'}
VECI = {'ai', 'sfi', 'andi', 'ori', 'xori', 'ceqi', 'cgti', 'clgti', 'andbi', 'shli', 'rotmi', 'roti',
        'rotqbii', 'shlqbii', 'rotqbyi', 'shlqbyi', 'rotqmbyi', 'cflts', 'cfltu', 'csflt', 'cuflt', 'rotmai'}
COPIA = {('ori', '0'), ('shlqbyi', '0'), ('rotqbyi', '0'), ('ai', '0')}
nome = f'gowa_vc_{mod}_{ini}'
alias = {}          # definição -> definição de que é cópia


def nomev(v):
    """expressão do valor v: u128 (U) ou vk (V)"""
    if v[0] == 'in':
        return 'r1_in' if v[1] == 1 else f'vin{v[1]}'
    k = v[1]
    while k in alias:
        k = alias[k]
    return f'u{k}' if cls[k] == 'U' else f'v{k}'


def comov(v):
    """o valor como vk (uniforme vira difusão)"""
    e = nomev(v)
    return f'vk_bc({e})' if cl(v) == 'U' else e


out = []
o = out.append
o(f'/* Gerado por scripts/vetorizar_chamadas.py -- {mod} cadeia {" -> ".join(cadeia)}: 4 chamadas por vez. Não editar. */')
o('#include "spu_vk.h"')
o('#include <stdio.h>')
o('#include <stdlib.h>')
o(f'/* {N} comandos; {sum(1 for k in cls if cls[k] == "V")} por chamada, {sum(1 for k in cls if cls[k] == "U")} uniformes */')
o(f'static int {nome}_nucleo(spu_context* ctx, const u128* in, u128 r1_in, u128* res, u128* fim) {{')
o('    uint8_t* const ls = SPU_LS_BASE(ctx);')
o('    __mmask16 bad = 0, al = 0;')
lo_p = min(pilha)
tam_p = max(pilha) + 16 - lo_p
o(f'    if (r1_in._u32[0] < 0x1000u || (r1_in._u32[0] & 0xFu)) return 0;')
o(f'    const uint32_t plo = (r1_in._u32[0] + (uint32_t)({lo_p})) & (SPU_LS_MASK & ~0xFu);   /* pilha gravada: [plo, plo + 0x{tam_p:X}) */')
o('    const vk vplo = _mm512_set1_epi32((int)plo);')
for j in ARGS:
    o(f'    const vk vin{j} = vk_join(in[{j - 3}], in[{4 + j - 3}], in[{8 + j - 3}], in[{12 + j - 3}]);')
nlen = 0
for k, c in enumerate(P):
    op = c['op']
    if op == 'st' or k in morto:
        continue
    if op == 'ld':
        ts = termos(c['addr'])
        if cls[k] == 'U':
            ex = ' + '.join(f'{nomev(val_)}._u32[0]' for val_ in c['in'])
            cs = sum(t[1] for t in ts if t[0] == 'c')
            ex = (ex + ' + ' if ex else '') + f'(uint32_t)({cs})'
            o(f'    const uint32_t a{k} = ({ex}) & (SPU_LS_MASK & ~0xFu);')
            o(f'    al |= (__mmask16)((a{k} - plo) < 0x{tam_p:X}u);')
            o(f'    const u128 u{k} = spu_ls_read128(ctx, a{k});')
        else:
            partes = [comov(v_) for v_ in c['in']]
            cs = sum(t[1] for t in ts if t[0] == 'c')
            e = partes[0]
            for p_ in partes[1:]:
                e = f'_mm512_add_epi32({e}, {p_})'
            if cs:
                e = f'_mm512_add_epi32({e}, _mm512_set1_epi32({cs}))'
            o(f'    const vk a{k} = _mm512_and_si512({e}, _mm512_set1_epi32((int)(SPU_LS_MASK & ~0xFu)));')
            o(f'    al |= _mm512_mask_cmplt_epu32_mask(0x1111, _mm512_sub_epi32(a{k}, vplo), _mm512_set1_epi32(0x{tam_p:X}));')
            o(f'    const vk v{k} = vk_ld4a(ls, a{k});')
            nlen += 1
        continue
    a = c['args']
    ins = c['in']
    if cls[k] == 'U':
        ar = [nomev(v_) if v_ is not None else x for x, v_ in zip(a, ins)]
        o(f'    const u128 u{k} = spu_{op}({", ".join(ar)});')
        continue
    if (op, a[-1]) in COPIA and len(a) == 2 and ins[0] is not None and cl(ins[0]) == 'V':
        if ins[0][0] == 'd':
            alias[k] = ins[0][1]
        else:
            o(f'    const vk v{k} = {nomev(ins[0])};')
        continue
    regs = [x for x in ins if x is not None]
    if op in FLOAT and len(regs) == len(a):
        o(f'    const vk v{k} = vk_{op}({", ".join(comov(v_) for v_ in ins)}, &bad);')
    elif (op in VEC2 and len(a) == 2 or op in VEC3 and len(a) == 3 or op in VEC1 and len(a) == 1) and len(regs) == len(a):
        o(f'    const vk v{k} = vk_{op}({", ".join(comov(v_) for v_ in ins)});')
    elif op in VECI and len(a) == 2 and ins[0] is not None and ins[1] is None and re.fullmatch(r'-?\d+', a[1]):
        o(f'    const vk v{k} = vk_{op}({comov(ins[0])}, {a[1]});')
    else:
        # sem forma de 512 bits: faixa a faixa com o auxiliar de 128 bits
        fx = []
        for L in range(4):
            ar = []
            for x, v_ in zip(a, ins):
                if v_ is None:
                    ar.append(x)
                elif cl(v_) == 'U':
                    ar.append(nomev(v_))
                else:
                    ar.append(f'vk_lane({nomev(v_)}, {L})')
            fx.append(f'vk_u2x(spu_{op}({", ".join(ar)}))')
        o(f'    const vk v{k} = vk_joinx({", ".join(fx)});   /* {op}: faixa a faixa */')
        print('aviso: faixa a faixa:', op, a)
o('    if (__builtin_expect((bad | al) != 0, 0)) return 0;')
r3 = fim_def[3]
for L in range(4):
    o(f'    res[{L}] = {"vk_lane(" + nomev(r3) + f", {L})" if cl(r3) == "V" else nomev(r3)};')
for r in range(2, 80):
    v_ = fim_def.get(r)
    if v_ is None:
        continue
    o(f'    fim[{r}] = {"vk_lane(" + nomev(v_) + ", 3)" if cl(v_) == "V" else nomev(v_)};')
o('    return 1;')
o('}')
o('')
# estado da última chamada: as gravações da pilha (valores que a chamada lê de ctx->gpr) + voláteis
o(f'void {nome}_fim(spu_context* ctx, const u128* fim) {{')
o('    const u128 r1 = ctx->gpr[1];')
for off, r in sorted(pilha.items(), key=lambda x: -x[0]):
    o(f'    spu_ls_write128(ctx, r1._u32[0] + (uint32_t)({off}), ctx->gpr[{r}]);')
escritos = [r for r in range(2, 80) if fim_def.get(r) is not None]
o(f'    memcpy(&ctx->gpr[{escritos[0]}], &fim[{escritos[0]}], sizeof(u128) * {escritos[-1] - escritos[0] + 1});')
assert escritos == list(range(escritos[0], escritos[-1] + 1))
o('}')
o('')
# conferência: as quatro chamadas pelo código traduzido numa cópia do contexto
o(f'extern void {mod}_spu_func_{ini}(spu_context* ctx);')
o(f'static int {nome}_conf(spu_context* ctx, const u128* in, u128 r1_in, const u128* res, const u128* fim) {{')
o('    spu_context* ref = (spu_context*)malloc(sizeof(spu_context));')
o('    if (!ref) return 1;')
o('    memcpy(ref, ctx, sizeof(spu_context));')
o('    ref->ls = ref->ls_store;   /* o ponteiro copiado apontaria a LS do contexto de verdade */')
o('    ref->gpr[1] = r1_in;')
o('    int dif = 0;')
o('    for (int L = 0; L < 4; L++) {')
o('        for (int j = 0; j < 4; j++) ref->gpr[3 + j] = in[4 * L + j];')
o('        ref->gpr[0] = spu_link(0x0FFF0u);')
o('        ref->host_depth++;')
o(f'        {mod}_spu_func_{ini}(ref);')
o('        ref->host_depth--;')
o('        if ((ref->pc & SPU_LS_MASK) != 0x0FFF0u) { free(ref); return 1; }   /* saiu pelo despacho: sem conferência */')
o('        if (memcmp(&ref->gpr[3], &res[L], 16)) {')
o(f'            fprintf(stderr, "[vc] DIFERENÇA {mod} {ini} chamada %d r3: %08X %08X %08X %08X x ref %08X %08X %08X %08X\\n", L,')
o('                    res[L]._u32[0], res[L]._u32[1], res[L]._u32[2], res[L]._u32[3],')
o('                    ref->gpr[3]._u32[0], ref->gpr[3]._u32[1], ref->gpr[3]._u32[2], ref->gpr[3]._u32[3]);')
o('            dif = 1;')
o('        }')
o('    }')
o(f'    for (int r = {escritos[0]}; r <= {escritos[-1]}; r++)')
o('        if (r != 3 && memcmp(&ref->gpr[r], &fim[r], 16)) {')
o(f'            if (!dif) fprintf(stderr, "[vc] DIFERENÇA {mod} {ini} r%d no fim: %08X %08X %08X %08X x ref %08X %08X %08X %08X\\n", r,')
o('                    fim[r]._u32[0], fim[r]._u32[1], fim[r]._u32[2], fim[r]._u32[3],')
o('                    ref->gpr[r]._u32[0], ref->gpr[r]._u32[1], ref->gpr[r]._u32[2], ref->gpr[r]._u32[3]);')
o('            dif = 1;')
o('        }')
o('    free(ref);')
o('    return !dif;')
o('}')
o('')
o(f'int {nome}(spu_context* ctx, const u128* in, u128 r1_in, u128* res, u128* fim) {{')
o('    static int liga = -1, conf = 0, est = 0; static volatile long ncham, nfeitas;')
o('    if (liga < 0) { const char* e = getenv("GOWA_VC"); liga = e ? atoi(e) : 1;')
o('                    const char* c = getenv("GOWA_VC_CHECK"); conf = c ? atoi(c) : 0; est = getenv("GOWA_VC_STATS") ? 1 : 0; }')
o('    if (!liga) return 0;')
o(f'    const int ok = {nome}_nucleo(ctx, in, r1_in, res, fim);')
o('    if (est) { const long n = ++ncham; if (ok) nfeitas++;')
o(f'               if ((n & 0xFFFFF) == 0) fprintf(stderr, "[vc] {ini}: %ld grupos de 4, %.1f%% no núcleo\\n", n, 100.0 * nfeitas / n); }}')
o('    if (ok && conf > 0) {')
o('        static volatile long nc, nconf;')
o('        if ((nc++ % conf) == 0) {')
o(f'            if (!{nome}_conf(ctx, in, r1_in, res, fim)) {{')
o(f'                fprintf(stderr, "[vc] {ini}: núcleo desligado\\n");')
o('                liga = 0;')
o('                return 0;')
o('            }')
o('            nconf++;')
o(f'            if ((nconf & (nconf - 1)) == 0) fprintf(stderr, "[vc] {ini}: %ld conferências iguais\\n", nconf);')
o('        }')
o('    }')
o('    return ok;')
o('}')
arq = d / f'vc_{ini}.inc'
arq.write_text('\n'.join(out) + '\n', encoding='latin-1')
print(f'{arq}: {N} comandos, cadeia {cadeia}, {nlen} leituras por faixa, pilha {sorted(pilha)}')
