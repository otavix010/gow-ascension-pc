"""Extrai funções da tradução SPU (spu_recomp.c.sem_promocao: a forma original, sem os passes)
para um .inc autocontido de banco de teste: as funções pedidas ficam com o nome original, e toda
chamada a uma função NÃO pedida vira chamada a um stub `oraculo_stub(ctx, 0xADDR)` (o banco decide
o que fazer). Os saltos de cauda pelo trampolim para funções não pedidas também viram o stub.

uso: python extrai_funcoes_spu.py <dir do módulo> <saida.inc> <ADDR> [<ADDR> ...]"""
import re
import sys
from pathlib import Path

d = Path(sys.argv[1])
out = Path(sys.argv[2])
_args = sys.argv[3:]
if '--sondas' in _args:
    _i = _args.index('--sondas'); _args = _args[:_i] + _args[_i + 2:]
pedidas = [int(a, 16) for a in _args]
mod = d.name
src = (d / 'spu_recomp.c.sem_promocao').read_text(encoding='latin-1')
nl = '\r\n' if '\r\n' in src else '\n'
blocos = []
for a in pedidas:
    nome = f'{mod}_spu_func_{a:08X}'
    h = f'void {nome}(spu_context* ctx) {{'
    i = src.index(h)
    fim = src.index(nl + '}' + nl, i) + len(nl + '}')
    blocos.append(src[i:fim])
txt = (nl * 2).join(blocos)
pedidos = {f'{mod}_spu_func_{a:08X}' for a in pedidas}


def chamada(m):
    nome = m.group(1)
    if nome in pedidos:
        return m.group(0)
    return f'oraculo_stub(ctx, 0x{int(nome.rsplit("_", 1)[1], 16):X}u);'


txt = re.sub(r'\b(' + mod + r'_spu_func_[0-9A-F]{8})\(ctx\);', chamada, txt)
# cauda pelo trampolim para função não pedida: { ctx->pc = X; g_spu_trampoline_fn = F; return; }
def cauda(m):
    nome = m.group(2)
    if nome in pedidos:
        return m.group(0)
    return f'{{ ctx->pc = 0x{m.group(1)}; oraculo_cauda(ctx, 0x{m.group(1)}u); return; }}'


txt = re.sub(r'\{ ctx->pc = 0x([0-9A-F]+); g_spu_trampoline_fn = (' + mod + r'_spu_func_[0-9A-F]{8}); return; \}', cauda, txt)
# --sondas L1,L2,...: `oraculo_sonda(ctx, 0xL);` logo depois de cada `loc_L:` (o banco grava os registradores)
if '--sondas' in sys.argv:
    for lab in sys.argv[sys.argv.index('--sondas') + 1].split(','):
        a = int(lab, 16)
        txt = txt.replace(f'loc_{a:08X}:', f'loc_{a:08X}: oraculo_sonda(ctx, 0x{a:X}u);', 1)
cab = ('/* GERADO por scripts/extrai_funcoes_spu.py -- funções ' + ', '.join(f'0x{a:X}' for a in pedidas) +
       f' de {mod} (forma original). */' + nl)
protos = ''.join(f'void {mod}_spu_func_{a:08X}(spu_context* ctx);' + nl for a in pedidas)
out.write_text(cab + protos + txt + nl, encoding='latin-1')
print(f'{len(pedidas)} funções -> {out}')
