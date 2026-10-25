"""Operações SPU inteiras explícitas em SSE/AVX-512 no x86 (23/10).

No x86 elas eram os laços _ref de spu_helpers.h. O clang costuma vetorizar
esses laços, mas não sempre. No laço do plugin 0x74C580 um `a` virou quatro
somas escalares com vpextrd/vpinsrd, e um `andbi` virou 16 vpinsrb.
Escritas com intrínsecos, cada uma é uma ou duas instruções,
independentemente da heurística do vetorizador.

O que este passe faz em ps3recomp/runtime/spu/spu_helpers.h:
  - insere o bloco SPU__XV (ld/st em __m128i) logo depois do bloco NEON;
  - para cada operação de OPS, troca a definição usada no x86 por
        #if defined(SPU__XV)  <intrínsecos>  #else  <a de antes>  #endif
  - as que eram definidas direto, sem _ref (andbi, shli...), passam a se
    chamar _ref, e a forma pública escolhe entre as duas.
SPU_NO_XV na compilação volta às formas de antes. Conferência:
scratchpad t_xv.c (cada uma contra a _ref, entradas aleatórias com bordas).

uso: python spu_ops_xv.py aplicar|tirar   (backup spu_helpers.h.sem_xv)"""
import os as _os_raiz
RAIZ = _os_raiz.environ.get('GOWA_RAIZ') or _os_raiz.path.dirname(_os_raiz.path.dirname(_os_raiz.path.abspath(__file__)))  # raiz do projeto
import re
import shutil
import sys
from pathlib import Path

H = Path(_os_raiz.path.join(RAIZ, r'ps3recomp\runtime\spu\spu_helpers.h'))
BAK = H.with_name(H.name + '.sem_xv')

L = 'spu__xv_ld'
S = 'spu__xv_st'
# nome: (argumentos, corpo com A/B/T = __m128i dos operandos, I = imediato)
OPS = {
    'a':      ('u128 a, u128 b', '_mm_add_epi32(A, B)'),
    'sf':     ('u128 a, u128 b', '_mm_sub_epi32(B, A)'),
    'ai':     ('u128 a, int32_t imm', '_mm_add_epi32(A, _mm_set1_epi32(imm))'),
    'sfi':    ('u128 a, int32_t imm', '_mm_sub_epi32(_mm_set1_epi32(imm), A)'),
    'ah':     ('u128 a, u128 b', '_mm_add_epi16(A, B)'),
    'sfh':    ('u128 a, u128 b', '_mm_sub_epi16(B, A)'),
    'ahi':    ('u128 a, int32_t imm', '_mm_add_epi16(A, _mm_set1_epi16((short)imm))'),
    'sfhi':   ('u128 a, int32_t imm', '_mm_sub_epi16(_mm_set1_epi16((short)imm), A)'),
    'ceq':    ('u128 a, u128 b', '_mm_cmpeq_epi32(A, B)'),
    'cgt':    ('u128 a, u128 b', '_mm_cmpgt_epi32(A, B)'),
    'clgt':   ('u128 a, u128 b', '_mm_movm_epi32(_mm_cmpgt_epu32_mask(A, B))'),
    'ceqi':   ('u128 a, int32_t imm', '_mm_cmpeq_epi32(A, _mm_set1_epi32(imm))'),
    'cgti':   ('u128 a, int32_t imm', '_mm_cmpgt_epi32(A, _mm_set1_epi32(imm))'),
    'clgti':  ('u128 a, int32_t imm', '_mm_movm_epi32(_mm_cmpgt_epu32_mask(A, _mm_set1_epi32(imm)))'),
    'ceqh':   ('u128 a, u128 b', '_mm_cmpeq_epi16(A, B)'),
    'cgth':   ('u128 a, u128 b', '_mm_cmpgt_epi16(A, B)'),
    'clgth':  ('u128 a, u128 b', '_mm_movm_epi16(_mm_cmpgt_epu16_mask(A, B))'),
    'ceqhi':  ('u128 a, int32_t imm', '_mm_cmpeq_epi16(A, _mm_set1_epi16((short)imm))'),
    'cgthi':  ('u128 a, int32_t imm', '_mm_cmpgt_epi16(A, _mm_set1_epi16((short)imm))'),
    'clgthi': ('u128 a, int32_t imm', '_mm_movm_epi16(_mm_cmpgt_epu16_mask(A, _mm_set1_epi16((short)imm)))'),
    'ceqb':   ('u128 a, u128 b', '_mm_cmpeq_epi8(A, B)'),
    'cgtb':   ('u128 a, u128 b', '_mm_cmpgt_epi8(A, B)'),
    'clgtb':  ('u128 a, u128 b', '_mm_movm_epi8(_mm_cmpgt_epu8_mask(A, B))'),
    'ceqbi':  ('u128 a, int32_t imm', '_mm_cmpeq_epi8(A, _mm_set1_epi8((char)imm))'),
    'cgtbi':  ('u128 a, int32_t imm', '_mm_cmpgt_epi8(A, _mm_set1_epi8((char)imm))'),
    'clgtbi': ('u128 a, int32_t imm', '_mm_movm_epi8(_mm_cmpgt_epu8_mask(A, _mm_set1_epi8((char)imm)))'),
    'andi':   ('u128 a, int32_t imm', '_mm_and_si128(A, _mm_set1_epi32(imm))'),
    'ori':    ('u128 a, int32_t imm', '_mm_or_si128(A, _mm_set1_epi32(imm))'),
    'xori':   ('u128 a, int32_t imm', '_mm_xor_si128(A, _mm_set1_epi32(imm))'),
    'andhi':  ('u128 a, int32_t imm', '_mm_and_si128(A, _mm_set1_epi16((short)imm))'),
    'orhi':   ('u128 a, int32_t imm', '_mm_or_si128(A, _mm_set1_epi16((short)imm))'),
    'xorhi':  ('u128 a, int32_t imm', '_mm_xor_si128(A, _mm_set1_epi16((short)imm))'),
    'andbi':  ('u128 a, int32_t imm', '_mm_and_si128(A, _mm_set1_epi8((char)imm))'),
    'orbi':   ('u128 a, int32_t imm', '_mm_or_si128(A, _mm_set1_epi8((char)imm))'),
    'xorbi':  ('u128 a, int32_t imm', '_mm_xor_si128(A, _mm_set1_epi8((char)imm))'),
    'eqv':    ('u128 a, u128 b', '_mm_ternarylogic_epi32(A, B, B, 0xC3)'),
    # PSLLD/PSRLD com contagem > 31 dão 0, PSRAD dá o sinal -- como as _ref
    'shli':   ('u128 a, int sh', '_mm_sll_epi32(A, _mm_cvtsi32_si128(sh & 0x3F))'),
    'rotmi':  ('u128 a, int i7', '_mm_srl_epi32(A, _mm_cvtsi32_si128((0 - i7) & 0x3F))'),
    'rotmai': ('u128 a, int i7', '_mm_sra_epi32(A, _mm_cvtsi32_si128((0 - i7) & 0x3F))'),
    'roti':   ('u128 a, int sh', '_mm_rolv_epi32(A, _mm_set1_epi32(sh & 31))'),
    'shlhi':  ('u128 a, int sh', '_mm_sll_epi16(A, _mm_cvtsi32_si128(sh & 0x1F))'),
    'rothmi': ('u128 a, int i7', '_mm_srl_epi16(A, _mm_cvtsi32_si128((0 - i7) & 0x1F))'),
    'rotmahi': ('u128 a, int i7', '_mm_sra_epi16(A, _mm_cvtsi32_si128((0 - i7) & 0x1F))'),
    'rothi':  ('u128 a, int sh', '_mm_or_si128(_mm_sll_epi16(A, _mm_cvtsi32_si128(sh & 15)), _mm_srl_epi16(A, _mm_cvtsi32_si128(16 - (sh & 15))))'),
    # VPSLLVD/VPSRLVD com contagem > 31 dão 0, VPSRAVD dá o sinal; VPROLVD usa a contagem mod 32
    'shl':    ('u128 a, u128 b', '_mm_sllv_epi32(A, _mm_and_si128(B, _mm_set1_epi32(0x3F)))'),
    'rot':    ('u128 a, u128 b', '_mm_rolv_epi32(A, B)'),
    'rotm':   ('u128 a, u128 b', '_mm_srlv_epi32(A, _mm_and_si128(_mm_sub_epi32(_mm_setzero_si128(), B), _mm_set1_epi32(0x3F)))'),
    'rotma':  ('u128 a, u128 b', '_mm_srav_epi32(A, _mm_and_si128(_mm_sub_epi32(_mm_setzero_si128(), B), _mm_set1_epi32(0x3F)))'),
    'shlh':   ('u128 a, u128 b', '_mm_sllv_epi16(A, _mm_and_si128(B, _mm_set1_epi16(0x1F)))'),
    'roth':   ('u128 a, u128 b', '_mm_or_si128(_mm_sllv_epi16(A, _mm_and_si128(B, _mm_set1_epi16(15))), _mm_srlv_epi16(A, _mm_sub_epi16(_mm_set1_epi16(16), _mm_and_si128(B, _mm_set1_epi16(15)))))'),
    'rothm':  ('u128 a, u128 b', '_mm_srlv_epi16(A, _mm_and_si128(_mm_sub_epi16(_mm_setzero_si128(), B), _mm_set1_epi16(0x1F)))'),
    'rothma': ('u128 a, u128 b', '_mm_srav_epi16(A, _mm_and_si128(_mm_sub_epi16(_mm_setzero_si128(), B), _mm_set1_epi16(0x1F)))'),
    'cg':     ('u128 a, u128 b', '_mm_maskz_mov_epi32(_mm_cmplt_epu32_mask(_mm_add_epi32(A, B), A), _mm_set1_epi32(1))'),
    'bg':     ('u128 a, u128 b', '_mm_maskz_mov_epi32(_mm_cmpge_epu32_mask(B, A), _mm_set1_epi32(1))'),
    'avgb':   ('u128 a, u128 b', '_mm_avg_epu8(A, B)'),
    'absdb':  ('u128 a, u128 b', '_mm_sub_epi8(_mm_max_epu8(A, B), _mm_min_epu8(A, B))'),
    'addx':   ('u128 a, u128 b, u128 t', '_mm_add_epi32(_mm_add_epi32(A, B), _mm_and_si128(T, _mm_set1_epi32(1)))'),
    'sfx':    ('u128 a, u128 b, u128 t', '_mm_add_epi32(_mm_add_epi32(B, _mm_xor_si128(A, _mm_set1_epi32(-1))), _mm_and_si128(T, _mm_set1_epi32(1)))'),
}
PRELUDIO = [
    '/* GOWA (23/10): SPU__XV -- the integer operations as explicit SSE/AVX-512 on x86',
    ' * (scripts/spu_ops_xv.py; the _ref loops were usually, but not always, vectorized',
    ' * by the compiler). SPU_NO_XV turns them off. */',
    '#if (defined(__x86_64__) || defined(_M_X64)) && defined(__SSE4_1__) && defined(__AVX512F__) && \\',
    '    defined(__AVX512VL__) && defined(__AVX512BW__) && !defined(__ARM_NEON) && \\',
    '    !defined(SPU_SCALAR_HELPERS) && !defined(SPU_SCALAR_INT) && !defined(SPU_NO_XV)',
    '#define SPU__XV 1',
    '#include <immintrin.h>',
    'static inline __m128i spu__xv_ld(u128 v) { __m128i r; memcpy(&r, &v, 16); return r; }',
    'static inline u128 spu__xv_st(__m128i v) { u128 r; memcpy(&r, &v, 16); return r; }',
    '#endif',
]


def corpo_xv(nome, args, expr):
    nomes = [p.strip().rsplit(' ', 1)[1] for p in args.split(',')]
    decl = []
    for n in nomes:
        if n in ('a', 'b', 't'):
            decl.append(f'const __m128i {n.upper()} = {L}({n});')
    return f'static inline u128 spu_{nome}({args}) {{ {" ".join(decl)} return {S}({expr}); }}'


def main():
    modo = sys.argv[1]
    if modo == 'tirar':
        shutil.copyfile(BAK, H)
        print('restaurado de', BAK)
        return
    src = (BAK if BAK.exists() else H).read_bytes().decode('utf-8')
    if not BAK.exists():
        shutil.copyfile(H, BAK)
    nl = '\r\n' if '\r\n' in src else '\n'
    linhas = src.split(nl)
    # o prelúdio: depois do #endif do bloco NEON do começo (o primeiro #endif após a linha do arm_neon.h)
    i_neon = next(i for i, l in enumerate(linhas) if '#include <arm_neon.h>' in l)
    i_fim = next(i for i in range(i_neon, len(linhas)) if linhas[i].startswith('#endif'))
    linhas[i_fim + 1:i_fim + 1] = [''] + PRELUDIO
    feitos = []
    for nome, (args, expr) in OPS.items():
        novo = corpo_xv(nome, args, expr)
        # 1) forma x86 que só chama a _ref
        rx = re.compile(r'^static inline u128 spu_' + nome + r'\((?:[^)]*)\) \{ return spu_' + nome + r'_ref\([^)]*\); \}\s*$')
        idx = [i for i, l in enumerate(linhas) if rx.match(l)]
        if len(idx) == 1:
            i = idx[0]
            linhas[i:i + 1] = ['#if defined(SPU__XV)', novo, '#else', linhas[i], '#endif']
            feitos.append(nome)
            continue
        # 2) definida direto numa linha só (sem _ref): vira _ref
        rd = re.compile(r'^static inline u128 spu_' + nome + r'\(([^)]*)\)(\s*\{.*\}\s*)$')
        idx = [i for i, l in enumerate(linhas) if rd.match(l)]
        if len(idx) == 1:
            i = idx[0]
            m = rd.match(linhas[i])
            chama = ', '.join(p.strip().rsplit(' ', 1)[1] for p in m.group(1).split(','))
            ref = f'static inline u128 spu_{nome}_ref({m.group(1)}){m.group(2).rstrip()}'
            pub = f'static inline u128 spu_{nome}({m.group(1)}) {{ return spu_{nome}_ref({chama}); }}'
            linhas[i:i + 1] = [ref, '#if defined(SPU__XV)', novo, '#else', pub, '#endif']
            feitos.append(nome + '*')
            continue
        print('NAO ACHEI a forma de', nome, len(idx))
    H.write_bytes(nl.join(linhas).encode('utf-8'))
    print(len(feitos), 'operações:', ' '.join(feitos))


main()
