"""Desvio indireto local: os saltos indiretos de CAUDA de um módulo SPU traduzido que caem numa função
do próprio módulo vão direto para ela (musttail), sem voltar ao dreno nem passar pelo resolvedor.

Por quê (03/11, áudio picotado em CPU fraca): o decodificador do módulo de áudio 0x7D8600 termina cada
símbolo num salto calculado (selb -> ctx->pc -> spu_indirect_branch). Cada símbolo pagava SPILL, volta
ao dreno, spu_indirect_branch, spu_lookup, as checagens de código residente e de pilha, e o RELOAD na
função seguinte: ~21% da SPU 5 (a do som) era esse despacho.

Regras (as mesmas garantias do SPU_TAILX):
  - só com orçamento de saltos (tail_budget): um evento pendente zera o orçamento e o salto volta ao
    dreno, que trata a interrupção como antes;
  - só destinos que são funções DESTE módulo com corpo SPU_PN (o __b/__m), achados numa tabela ordenada;
    fora dela (o gerenciador de jobs, o kernel, plugins, LS 0) segue o caminho de sempre;
  - NUNCA os endereços de retorno das chamadas do módulo (os return_pc de spu_drain_call): voltar a um
    deles direto executaria a continuação aninhada na função chamada e o resto da chamadora duas vezes;
  - g_gowa_ibl (GOWA_IBL=0 no jogo) desliga em tempo de execução.

Roda sobre a forma final (depois do encadeia.py). Backup em <arquivo>.sem_ibl; cada aplicação parte dele.
uso: python ib_local.py <dir do módulo> [...]   |   python ib_local.py --tirar <dir> [...]"""
import re
import shutil
import sys
from pathlib import Path

MARCA = '/* ib_local.py */'


def aplicar(d: Path):
    f = d / 'spu_recomp.c'
    bak = f.with_name(f.name + '.sem_ibl')
    # arquivo sem a marca = recém-gerado (a etapa 5 refaz tudo): ele é o original e vira o backup
    if not bak.exists() or MARCA.encode() not in f.read_bytes():
        shutil.copy2(f, bak)
    raw = bak.read_bytes().decode('latin-1')
    nl = '\r\n' if '\r\n' in raw[:20000] else '\n'
    mod = d.name
    # corpos SPU_PN declarados (os __b / __m)
    corpos = set(re.findall(r'^static SPU_PN void (' + mod + r'_spu_func_[0-9A-F]{8}__[bm])\(spu_context\* ctx\);', raw, re.M))
    # entradas registradas: endereço -> público
    tab = re.findall(r'\{ 0x([0-9A-F]{8})u, (' + mod + r'_spu_func_[0-9A-F]{8}),', raw)
    # endereços de retorno das chamadas do módulo
    rets = {int(x, 16) for x in re.findall(r'spu_drain_call\(ctx, 0x([0-9A-Fa-f]+)u?\)', raw)}
    ents = []
    for a, pub in tab:
        addr = int(a, 16)
        if addr in rets:
            continue
        corpo = pub + '__m' if pub + '__m' in corpos else (pub + '__b' if pub + '__b' in corpos else None)
        if corpo:
            ents.append((addr, corpo))
    ents.sort()
    if not ents:
        print(f'{mod}: nenhuma entrada'); return
    # 03/11 (1a versão errada: 247/300 e 267/300 jobs de áudio diferentes): os módulos de áudio trocam
    # código dentro da própria faixa da LS (overlays). O caminho normal confere isso (spu_resident_fn_dirty)
    # e o atalho saltava para a tradução de um endereço que já tinha outro código. Agora cada destino leva
    # os 8 primeiros bytes do código original (do ELF do jogo); se a LS não tiver esses bytes, caminho normal.
    va = int(mod.split('_')[-1], 16)
    elf = d.parents[2] / 'jogo' / 'GOWA.elf'
    img = elf.read_bytes()
    import struct
    phoff = struct.unpack_from('>Q', img, 0x20)[0]
    phn, phsz = struct.unpack_from('>H', img, 0x38)[0], struct.unpack_from('>H', img, 0x36)[0]
    base_off = None
    for i in range(phn):
        _t, _f, off, pva, _pa, fsz = struct.unpack_from('>IIQQQQ', img, phoff + i * phsz)
        if pva <= va < pva + fsz:
            base_off = off + va - pva
    assert base_off is not None, f'VA 0x{va:X} fora do ELF'
    def assinatura(a):
        o = base_off + a - 0x4000
        return int.from_bytes(img[o:o + 8], 'little')   # como 8 bytes da LS lidos num uint64 do host
    ents = [(a, c, assinatura(a)) for a, c in ents]
    # a tabela vai antes da primeira definição de corpo (as declarações já estão em cima)
    m = re.search(r'^static SPU_PN void ' + mod + r'_spu_func_[0-9A-F]{8}__[bm]\(spu_context\* ctx\) \{', raw, re.M)
    assert m, 'sem definição de corpo'
    linhas = [MARCA,
              'extern int g_gowa_ibl;   /* gowa_offline.cpp: GOWA_IBL=0 desliga */',
              '/* gowa_confere.cpp: na reprodução, o PC onde a gravação parou o job -- a parada só é vista no dreno,',
              ' * então o atalho não pode passar por cima dela (no jogo fica 0xFFFFFFFF) */',
              'extern volatile uint32_t g_gowa_ibl_parar;',
              'typedef SPU_PN void (*ibl_fn_t)(spu_context*);',
              f'static const struct {{ uint32_t a; ibl_fn_t f; uint64_t w; }} ibl_tab[{len(ents)}] = {{']
    linhas += [f'    {{ 0x{a:05X}u, {c}, 0x{w:016X}ull }},' for a, c, w in ents]
    linhas += ['};',
               '/* destino do módulo E a LS ainda com o código original ali (os 8 primeiros bytes) */',
               'static inline ibl_fn_t ibl_acha(const spu_context* ctx, uint32_t a)',
               '{',
               f'    unsigned lo = 0, hi = {len(ents)};',
               '    while (lo < hi) { const unsigned m = (lo + hi) >> 1; if (ibl_tab[m].a < a) lo = m + 1; else hi = m; }',
               f'    if (lo >= {len(ents)} || ibl_tab[lo].a != a || a + 8u > SPU_LS_SIZE) return 0;',
               '    uint64_t w; memcpy(&w, ctx->ls + a, 8);',
               '    return w == ibl_tab[lo].w ? ibl_tab[lo].f : 0;',
               '}',
               '#define SPU_IBL(ctx, t) do { const uint32_t _ibt = (t); (ctx)->pc = _ibt; \\',
               '        if ((ctx)->tail_budget && g_gowa_ibl && (_ibt & SPU_LS_MASK) != g_gowa_ibl_parar) { \\',
               '            const ibl_fn_t _ibf = ibl_acha((ctx), _ibt & SPU_LS_MASK); \\',
               '            if (_ibf) { (ctx)->tail_budget--; __attribute__((musttail)) return _ibf(ctx); } } \\',
               '        g_spu_trampoline_fn = spu_indirect_branch; return; } while (0)',
               '']
    raw = raw[:m.start()] + nl.join(linhas) + nl + raw[m.start():]
    # sites: com SPILL e sem
    n1 = n2 = 0
    def r1(mm):
        nonlocal n1; n1 += 1
        return '{ const uint32_t _ibx = ' + mm.group(1) + '; SPILL; SPU_IBL(ctx, _ibx); }'
    def r2(mm):
        nonlocal n2; n2 += 1
        return '{ const uint32_t _ibx = ' + mm.group(1) + '; SPU_IBL(ctx, _ibx); }'
    raw = re.sub(r'ctx->pc = ([^;{}]+); g_spu_trampoline_fn = spu_indirect_branch; \{ SPILL; return; \}', r1, raw)
    raw = re.sub(r'ctx->pc = ([^;{}]+); g_spu_trampoline_fn = spu_indirect_branch; return;', r2, raw)
    # 03/11 (2o passo): o laço do decodificador salta para a PRÓPRIA função (0x13F00 -> 0x13F00 até o símbolo
    # acabar); aí o salto vira goto para logo depois do RELOAD -- os registradores ficam nas variáveis locais,
    # sem SPILL/RELOAD de ~25 registradores por volta. Mesmas condições do SPU_IBL (orçamento, chave, parada da
    # conferência, bytes originais na LS).
    n3 = 0
    defs = list(re.finditer(r'^static SPU_PN void ' + mod + r'_spu_func_([0-9A-F]{8})__[bm]\(spu_context\* ctx\) \{', raw, re.M))
    pedacos, ult = [], 0
    for i, dm in enumerate(defs):
        fim = defs[i + 1].start() if i + 1 < len(defs) else len(raw)
        corpo = raw[dm.start():fim]
        auto = int(dm.group(1), 16)
        sig = dict((a, w) for a, _c, w in ents).get(auto)
        k = corpo.find(nl + '        RELOAD;' + nl)
        if sig is not None and k >= 0 and '{ const uint32_t _ibx = ' in corpo and '; SPILL; SPU_IBL(ctx, _ibx); }' in corpo:
            def r3(mm):
                nonlocal n3; n3 += 1
                return ('{ const uint32_t _ibx = ' + mm.group(1) + '; '
                        f'if ((_ibx & SPU_LS_MASK) == 0x{auto:05X}u && ctx->tail_budget && g_gowa_ibl && '
                        f'0x{auto:05X}u != g_gowa_ibl_parar && ibl_mesmo(ctx, 0x{auto:05X}u, 0x{sig:016X}ull)) '
                        '{ ctx->tail_budget--; goto _ibl_topo; } SPILL; SPU_IBL(ctx, _ibx); }')
            corpo2 = re.sub(r'\{ const uint32_t _ibx = ([^;{}]+); SPILL; SPU_IBL\(ctx, _ibx\); \}', r3, corpo)
            if corpo2 != corpo:
                k = corpo2.find(nl + '        RELOAD;' + nl) + len(nl + '        RELOAD;')
                corpo = corpo2[:k] + nl + '_ibl_topo: ;' + corpo2[k:]
        pedacos.append(raw[ult:dm.start()]); pedacos.append(corpo); ult = fim
    pedacos.append(raw[ult:])
    raw = ''.join(pedacos)
    raw = raw.replace('#define SPU_IBL(ctx, t)', 'static inline int ibl_mesmo(const spu_context* ctx, uint32_t a, uint64_t w0)' + nl +
                      '{ uint64_t w; memcpy(&w, ctx->ls + a, 8); return w == w0; }' + nl + '#define SPU_IBL(ctx, t)', 1)
    f.write_bytes(raw.encode('latin-1'))
    print(f'{mod}: {len(ents)} destinos na tabela ({len(rets)} retornos fora), {n1} saltos com SPILL e {n2} sem, {n3} laços locais')


def tirar(d: Path):
    f = d / 'spu_recomp.c'
    bak = f.with_name(f.name + '.sem_ibl')
    if bak.exists():
        shutil.copy2(bak, f)
        print('restaurado:', f)


if __name__ == '__main__':
    args = sys.argv[1:]
    if not args:
        sys.exit(__doc__)
    if args[0] == '--tirar':
        for a in args[1:]:
            tirar(Path(a))
    else:
        for a in args:
            aplicar(Path(a))
