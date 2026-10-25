"""Troca as transferências diretas do trampolim SPU por saltos diretos (musttail).

Uso:
  python tail_calls.py aplicar [dirs...]   (padrão: todos os port/spu/*; idempotente)
  python tail_calls.py tirar   [dirs...]   (volta ao .sem_tail)

Por quê: uma transferência direta para outra função traduzida era
  { ctx->pc = X; g_spu_trampoline_fn = F; return; }
-- a função volta ao laço do trampolim (spu_drain_call / SPU_DRAIN), que chama
F por ponteiro: a chamada indireta mal prevista em cada passo era ~9% de cada
SPU no God of War Ascension. Com SPU_TAIL a transferência vira um salto direto
enquanto houver orçamento (ctx->tail_budget, recarregado a cada passo do laço);
sem orçamento, com interrupção pendente, para a LS 0 ou para uma entrada de
troca de pilha, cai no trampolim de antes (spu_context.h, spu_tail_ok).

Rodar DEPOIS do promover_regs.py (ele parte do .sem_promocao). Antes da primeira
alteração de um arquivo, grava <arquivo>.sem_tail.
"""
import os as _os_raiz
RAIZ = _os_raiz.environ.get('GOWA_RAIZ') or _os_raiz.path.dirname(_os_raiz.path.dirname(_os_raiz.path.abspath(__file__)))  # raiz do projeto
import re
import shutil
import sys
from pathlib import Path

R = Path(RAIZ)
MARCA = '/* tail_calls.py */'
# { ctx->pc = 0x...; g_spu_trampoline_fn = F; return; }
RE1 = re.compile(r'\{ ctx->pc = (0x[0-9A-Fa-f]+); g_spu_trampoline_fn = (\w+_spu_func_[0-9A-F]+); return; \}')
# { ctx->pc = 0x...; g_spu_trampoline_fn = F; { SPILL; return; } }
RE2 = re.compile(r'\{ ctx->pc = (0x[0-9A-Fa-f]+); g_spu_trampoline_fn = (\w+_spu_func_[0-9A-F]+); \{ SPILL; return; \} \}')


def main():
    args = sys.argv[1:]
    modo = args[0] if args else 'aplicar'
    dirs = [Path(a) for a in args[1:]] or sorted(p for p in (R / 'port' / 'spu').iterdir() if p.is_dir())
    tot = 0
    for d in dirs:
        f = d / 'spu_recomp.c'
        if not f.exists():
            continue
        bak = f.with_name(f.name + '.sem_tail')
        if modo == 'tirar':
            if bak.exists():
                shutil.copy(bak, f)        # data nova: o ninja recompila
                print('original restaurado:', f)
            continue
        src = f.read_bytes().decode('latin-1')
        if MARCA in src:
            print('já aplicado:', f)
            continue
        if not bak.exists():
            shutil.copy2(f, bak)
        n1 = n2 = 0
        src, n1 = RE1.subn(r'{ ctx->pc = \1; SPU_TAIL(ctx, \1, \2); }', src)
        src, n2 = RE2.subn(r'{ ctx->pc = \1; SPILL; SPU_TAIL(ctx, \1, \2); }', src)
        src = MARCA + '\n' + src
        f.write_bytes(src.encode('latin-1'))
        tot += n1 + n2
        print(f'{f}: {n1} + {n2} transferências')
    print('total', tot)


if __name__ == '__main__':
    main()
