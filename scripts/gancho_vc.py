"""Liga um núcleo de 4 chamadas (scripts/vetorizar_chamadas.py) às quatro
chamadas seguidas de uma cadeia dentro de uma função promovida
(promover_chamadas.py: registradores em locais _rN, SPILL/RELOAD em volta da
chamada). Na 1ª chamada o núcleo calcula as quatro (argumentos r3..r6 de cada
uma tirados das cópias que o chamador faz entre as chamadas, conferidas aqui:
o registrador de origem não pode mudar entre a 1ª chamada e a cópia); nas
seguintes só entrega o r3; na 4ª deixa o estado que a última chamada deixaria
(pilha + voláteis). Se o núcleo devolve 0, as quatro vão pelo código traduzido.

uso: python gancho_vc.py <dir do módulo> <função chamadora hex> <cadeia hex> [--tirar]
backup: spu_recomp.c.sem_vc"""
import re
import shutil
import sys
from pathlib import Path

d, fn, cad = Path(sys.argv[1]), sys.argv[2].upper().rjust(8, '0'), sys.argv[3].upper().rjust(8, '0')
mod = d.name
arq = d / 'spu_recomp.c'
bak = d / 'spu_recomp.c.sem_vc'
if '--tirar' in sys.argv:
    shutil.copyfile(bak, arq)
    print('restaurado de', bak)
    sys.exit(0)
src = arq.read_text(encoding='latin-1')
assert '/*VC ' + cad not in src, 'já ligado'
m = re.search(r'^static SPU_PN void ' + mod + r'_spu_func_' + fn + r'__b\(spu_context\* ctx\) \{\n', src, re.M)
ini = m.end()
fimf = src.find('\n}\n', ini)
corpo = src[ini:fimf]
chamada = f'{mod}_spu_func_{cad}__m(ctx);'
blocos = [mm for mm in re.finditer(r'^(\s*)_r0 = spu_link\(0x([0-9A-F]+)\); \{(.*)$', corpo, re.M) if chamada in mm.group(3)]
assert len(blocos) == 4, 'esperava 4 chamadas da cadeia, achei %d' % len(blocos)
rets = [b.group(2) for b in blocos]
# entre a 1ª e a 4ª: sem rótulos (sempre as quatro em seguida)
entre = corpo[blocos[0].end():blocos[3].start()]
assert 'loc_' not in entre, 'rótulo entre as chamadas'
# argumentos de cada chamada: a última atribuição de _r3.._r6 antes do bloco
fontes = [[f'_r{j}' for j in (3, 4, 5, 6)]]
for k in (1, 2, 3):
    trecho = corpo[blocos[k - 1].end():blocos[k].start()]
    ls_ = [t.strip() for t in trecho.split('\n') if t.strip()]
    args = []
    for j in (3, 4, 5, 6):
        ult = [t for t in ls_ if t.startswith(f'_r{j} = ')]
        assert ult, f'chamada {k + 1}: r{j} não é atribuído'
        mc = re.fullmatch(rf'_r{j} = spu_(?:ori|shlqbyi|rotqbyi)\(_r(\d+), 0\);', ult[-1])
        assert mc, f'chamada {k + 1}: r{j} não é cópia: ' + ult[-1]
        y = int(mc.group(1))
        assert 80 <= y <= 127 and y != 126, f'r{y}: não é registrador salvo pela cadeia'
        # a origem não pode mudar entre a 1ª chamada e a cópia
        ate = corpo[blocos[0].end():blocos[k - 1].end()] + trecho[:trecho.find(ult[-1])]
        assert not re.search(rf'^\s*_r{y} = ', ate, re.M), f'r{y} muda antes da cópia da chamada {k + 1}'
        args.append(f'_r{y}')
    fontes.append(args)
print('argumentos:', fontes, 'retornos:', rets)
nome = f'gowa_vc_{mod}_{cad}'
novo = corpo
for k in (3, 2, 1, 0):
    b = blocos[k]
    linha = b.group(0)
    assert linha.count(chamada) == 1
    if k == 0:
        lst = ', '.join(a for grp in fontes for a in grp)
        sub = (f'/*VC {cad}*/ {{ const u128 _vcin[16] = {{ {lst} }}; _vc = {nome}(ctx, _vcin, _r1, _vcr, _vcf); }} '
               f'if (!_vc) {chamada} else {{ ctx->gpr[3] = _vcr[0]; ctx->pc = 0x{rets[0]}u; }}')
    elif k < 3:
        sub = f'if (!_vc) {chamada} else {{ ctx->gpr[3] = _vcr[{k}]; ctx->pc = 0x{rets[k]}u; }}'
    else:
        sub = (f'if (!_vc) {chamada} else {{ {nome}_fim(ctx, _vcf); ctx->gpr[3] = _vcr[3]; ctx->pc = 0x{rets[3]}u; }}')
    nl = linha.replace(chamada, sub)
    novo = novo[:b.start()] + nl + novo[b.end():]
# declarações logo depois do primeiro RELOAD da função
i = novo.find('RELOAD;')
assert i >= 0 and i < 4000
dec = (f'\n        /*VC {cad}*/ u128 _vcr[4], _vcf[80]; int _vc = 0;\n'
       f'        extern int {nome}(spu_context*, const u128*, u128, u128*, u128*); extern void {nome}_fim(spu_context*, const u128*);')
novo = novo[:i + len('RELOAD;')] + dec + novo[i + len('RELOAD;'):]
shutil.copyfile(arq, bak)
src = src[:ini] + novo + src[fimf:]
src = src.rstrip('\n') + f'\n#include "vc_{cad}.inc"\n'
arq.write_text(src, encoding='latin-1')
print('ligado; backup em', bak)
