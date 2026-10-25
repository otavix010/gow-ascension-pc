"""Divide funções no alvo do idioma PIC de janela de interrupção (23/10):

    brsl $r2, a        ; r2 = a (a instrução seguinte)
 a: ai   $r2, $r2, 8
    bihz $rX, $r2 [E]  ; se rX == 0: liga as interrupções e desvia para a+8
 a+8: ...              ; senão segue reto para o mesmo a+8

Sem função traduzida em a+8, o desvio tomado cai no despacho indireto sem alvo
(micro-interpretador e interpretador, custo fixo alto por entrada). Aqui a
função `a` é partida em a+8: a nova função é registrada (o desvio indireto a
acha) e o caminho reto vai para ela por SPU_TAILX.

uso: python divide_pic.py <dir do módulo> [--aplicar]   (sem --aplicar só lista)
"""
import re
import sys
from pathlib import Path

d = Path(sys.argv[1])
aplicar = '--aplicar' in sys.argv
c = d / 'spu_recomp.c'
h = d / 'spu_recomp.h'
raw = c.read_bytes()
crlf = b'\r\n' in raw[:4000]
src = raw.decode('latin-1').replace('\r\n', '\n')
hdr_src = h.read_bytes().decode('latin-1').replace('\r\n', '\n')
fns = set(int(x, 16) for x in re.findall(r'\{ 0x([0-9A-F]{8})u, \w+, "', src))
L = src.split('\n')
cab = re.compile(r'^static SPU_PN void (\w+)_spu_func_([0-9A-F]{8})__b\(spu_context\* ctx\) \{$')
alvos = []
promovidas = []   # forma promovida (promover_regs: u128 locais + SPILL/RELOAD por função)
for i, l in enumerate(L):
    m = cab.match(l)
    if not m:
        continue
    pfx, a = m.group(1), int(m.group(2), 16)
    if L[i + 1].lstrip().startswith('u128 ') and L[i + 2].strip() == 'RELOAD;':
        p1 = re.match(r'\s*_r(\d+) = spu_ai\(_r\1, 8\);$', L[i + 3])
        if not p1:
            continue
        p2 = re.match(r'\s*if \(_r\d+\._u(16|32)\[0\] (==|!=) 0\) \{ ctx->int_enable = [01]; ctx->pc = _r%s\._u32\[0\]; '
                      r'g_spu_trampoline_fn = spu_indirect_branch; \{ SPILL; return; \} \}$' % p1.group(1), L[i + 4])
        t = a + 8
        if not p2 or t in fns:
            continue
        j = i + 5
        while L[j] != '}':
            j += 1
        resto = L[i + 5:j]
        if any(re.match(r'^loc_[0-9A-F]{8}:', x) and int(x[4:12], 16) < t for x in resto) or not resto:
            print('pulada (rótulo antes do alvo ou vazia):', hex(a))
            continue
        promovidas.append((i, j, pfx, a, t))
        continue
    m1 = re.match(r'\s*ctx->gpr\[(\d+)\] = spu_ai\(ctx->gpr\[\1\], 8\);$', L[i + 1])
    if not m1:
        continue
    r = m1.group(1)
    m2 = re.match(r'\s*if \(ctx->gpr\[\d+\]\._u(16|32)\[0\] (==|!=) 0\) \{ ctx->int_enable = [01]; ctx->pc = ctx->gpr\[%s\]\._u32\[0\]; '
                  r'g_spu_trampoline_fn = spu_indirect_branch; return; \}$' % r, L[i + 2])
    t = a + 8
    if not m2 or t in fns:
        continue
    j = i + 3
    while L[j] != '}':
        j += 1
    resto = L[i + 3:j]
    if any(re.match(r'^loc_[0-9A-F]{8}:', x) and int(x[4:12], 16) < t for x in resto) or not resto:
        print('pulada (rótulo antes do alvo ou vazia):', hex(a))
        continue
    alvos.append((i, j, pfx, a, t))
for i, j, pfx, a, t in alvos:
    print('%s: 0x%X -> nova 0x%X (%d linhas)' % (pfx, a, t, j - i - 3))
for i, j, pfx, a, t in promovidas:
    print('%s: 0x%X -> nova 0x%X (%d linhas, promovida)' % (pfx, a, t, j - i - 5))
if not aplicar or not (alvos or promovidas):
    sys.exit(0)
# de baixo para cima, as duas formas juntas (os índices de linha continuam valendo)
for forma, i, j, pfx, a, t in sorted([(0,) + x for x in alvos] + [(1,) + x for x in promovidas], key=lambda x: -x[1]):
    T = '%08X' % t
    nb = '%s_spu_func_%s' % (pfx, T)
    if forma == 0:
        L[i + 3:j] = ['        SPU_TAILX(ctx, 0x%Xu, %s__b, %s);' % (t, nb, nb), '}', '',
                      'static SPU_PN void %s__b(spu_context* ctx) {' % nb] + L[i + 3:j]
    else:
        # a nova logo depois, antes dos #undef: os mesmos SPILL/RELOAD (mesmo conjunto de
        # registradores); a passagem pelo trampolim, como as transferências do módulo
        L[i + 5:j] = ['        { ctx->pc = 0x%X; g_spu_trampoline_fn = %s; { SPILL; return; } }' % (t, nb), '}', '',
                      'static SPU_PN void %s__b(spu_context* ctx) {' % nb, L[i + 1], L[i + 2]] + L[i + 5:j]
alvos = alvos + promovidas
out = '\n'.join(L)
for i, j, pfx, a, t in alvos:
    A, T = '%08X' % a, '%08X' % t
    na, nb = '%s_spu_func_%s' % (pfx, A), '%s_spu_func_%s' % (pfx, T)
    for velho, novo in (('static SPU_PN void %s__b(spu_context* ctx);\n' % na, 'static SPU_PN void %s__b(spu_context* ctx);\n' % nb),
                        ('    { 0x%su, %s, "%s" },\n' % (A, na, na), '    { 0x%su, %s, "%s" },\n' % (T, nb, nb)),
                        ('void %s(spu_context* ctx) { %s__b(ctx); }\n' % (na, na), 'void %s(spu_context* ctx) { %s__b(ctx); }\n' % (nb, nb))):
        assert out.count(velho) == 1, (velho, out.count(velho))
        out = out.replace(velho, velho + novo)
    pv = 'void %s(spu_context* ctx);\n' % na
    assert hdr_src.count(pv) == 1, pv
    hdr_src = hdr_src.replace(pv, pv + 'void %s(spu_context* ctx);\n' % nb)
nl = '\r\n' if crlf else '\n'
c.write_bytes(out.replace('\n', nl).encode('latin-1'))
h.write_bytes(hdr_src.replace('\n', nl).encode('latin-1') if b'\r\n' in h.read_bytes()[:4000] else hdr_src.encode('latin-1'))
print('aplicado: %d funções novas' % len(alvos))
