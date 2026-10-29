"""Sonda temporária de fotos da LS entre dois rótulos de uma função traduzida (ex. o laço de descarte da
D640: loc_0000D6C0 até loc_0000DB70). Só no código gerado da bancada; restaurar o original depois.
GOWA_PL_FOTO=<N>, GOWA_PL_FOTO_DIR como em sonda_fn.py.

uso: python -I sonda_rotulos.py <pasta> <função> <rótulo de entrada> <rótulo de saída>"""
import os
import sys

pasta, fn, r_in, r_out = sys.argv[1:5]
raiz = os.path.join(os.path.dirname(__file__), '..')
p = os.path.join(raiz, 'port', 'spu', pasta, 'spu_recomp.c')
s = open(p, encoding='latin-1').read()
assert 'pl_foto(' not in s, 'sonda já aplicada'
cab = open(os.path.join(os.path.dirname(__file__), 'sonda_fn.py'), encoding='utf-8').read()
cab = cab[cab.index("cab = r'''") + len("cab = r'''"):]
cab = cab[:cab.index("'''")]
inc = '#include "spu_recomp.h"\n'
assert s.count(inc) == 1
s = s.replace(inc, inc + cab, 1)
c = f'static SPU_PN void {fn}(spu_context* ctx) {{\n'
i = s.index(c) + len(c)
fim = s.index('\n}\n', i)
corpo = s[i:fim]
a = f'{r_in}:\n'
b = f'{r_out}:\n'
assert corpo.count(a) == 1 and corpo.count(b) == 1, (corpo.count(a), corpo.count(b))
corpo = corpo.replace(a, a + '        pl_foto(ctx, 0);   /* SONDA */\n').replace(b, b + '        pl_foto(ctx, 1);   /* SONDA */\n')
s = s[:i] + corpo + s[fim:]
open(p, 'w', encoding='latin-1', newline='').write(s)
print('ok')
