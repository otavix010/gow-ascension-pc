"""Lista as funções "folha seguras" de um spu_recomp.c gerado pelo lifter.

Folha segura = o corpo não chama outra função traduzida (só desvia por
trampolim de cauda), não usa canais (rdch/wrch/rchcnt), não faz desvio
indireto (spu_indirect_branch) e não para o SPU. Uma função assim pode ser
chamada em linha pelo interpretador (spu_interp_hot_register): roda nativa e
devolve o controle em ctx->pc, sem deixar quadro de host para trás.

Uso: python folhas_spu.py <spu_recomp.c> <prefixo> [--inc VA arquivo.inc]
     imprime "LS tamanho" por função; com --inc acrescenta a tabela C.
"""
import re
import sys

src, prefixo = sys.argv[1], sys.argv[2]
txt = open(src, encoding='utf-8', errors='replace').read()
cab = re.compile(r'^void ' + re.escape(prefixo) + r'spu_func_([0-9A-F]{8})\(spu_context\* ctx\) \{$', re.M)
ms = list(cab.finditer(txt))
proibido = re.compile(r'spu_rdch|spu_wrch|spu_rchcnt|spu_indirect_branch|spu_drain_call|spu_halt|spu_stop|'
                      r'spu_iret|spu_check_stack_reset|longjmp|SPU_STOP|spu_sync')
chamada = re.compile(re.escape(prefixo) + r'spu_func_[0-9A-F]{8}\(ctx\)')
MIN_BYTES = 32
folhas = []
for i, m in enumerate(ms):
    ini = m.end()
    fim = ms[i + 1].start() if i + 1 < len(ms) else len(txt)
    corpo = txt[ini:fim]
    addr = int(m.group(1), 16)
    if proibido.search(corpo):
        continue
    if chamada.search(corpo):          # chamada direta (trampolins usam o nome sem "(ctx)")
        continue
    prox = int(ms[i + 1].group(1), 16) if i + 1 < len(ms) else addr + 64
    n = max(4, min(prox - addr, 4096))
    # chamar em linha só compensa com corpo grande ou laço interno: uma folha de
    # 2-3 instruções custa mais na verificação + chamada do que interpretada
    if n < MIN_BYTES and 'goto loc_' not in corpo:
        continue
    folhas.append((addr, n))
print(f'{len(folhas)} folhas seguras de {len(ms)} funções', file=sys.stderr)
if '--inc' in sys.argv:
    k = sys.argv.index('--inc')
    va, saida = int(sys.argv[k + 1], 16), sys.argv[k + 2]
    with open(saida, 'a', encoding='utf-8') as f:
        f.write(f'/* folhas seguras de {prefixo} (scripts/folhas_spu.py) */\n')
        for a, n in folhas:
            f.write(f'    {{ 0x{va:08X}u, 0x{a:05X}u, {n}u }},\n')
else:
    for a, n in folhas:
        print(f'{a:05X} {n}')
