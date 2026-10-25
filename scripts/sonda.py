"""Insere/remove sondas de log no código lifted (port/recompiled/ppu_recomp_NNN.cpp).

Uso:
  python sonda.py por   func_0063FEC4 loc_0063FF40 ...   (acha o arquivo sozinho)
  python sonda.py tirar                                   (remove todas as sondas)

Cada sonda vira  gowa_probe(0xADDR, ctx);  logo após a abertura da função ou o
rótulo. gowa_probe (port/gowa_offline.cpp) imprime as 8 primeiras passagens por
endereço com r3..r6 e lr. As linhas inseridas terminam com o marcador
'/*SONDA*/', que é o que 'tirar' procura.
"""
import os as _os_raiz
RAIZ = _os_raiz.environ.get('GOWA_RAIZ') or _os_raiz.path.dirname(_os_raiz.path.dirname(_os_raiz.path.abspath(__file__)))  # raiz do projeto
import re
import sys
from pathlib import Path

DIR = Path(_os_raiz.path.join(RAIZ, r'port\recompiled'))
MARCA = '/*SONDA*/'
DECL = 'extern "C" void gowa_probe(unsigned int addr, ppu_context* ctx); ' + MARCA + '\n'


def arquivos():
    return sorted(DIR.glob('ppu_recomp_[0-9][0-9][0-9].cpp'))


def ler(p):
    return p.read_bytes().decode('latin-1')


def gravar(p, s):
    p.write_bytes(s.encode('latin-1'))


def por(nomes):
    pendentes = set(nomes)
    for p in arquivos():
        txt = ler(p)
        alvo = [n for n in pendentes if (('void ' + n + '(') in txt or ('\n' + n + ':') in txt)]
        if not alvo:
            continue
        eol = '\r\n' if '\r\n' in txt else '\n'
        out = []
        for ln in txt.split(eol):
            out.append(ln)
            for n in alvo:
                addr = n.split('_')[1]
                if (ln.startswith('void ' + n + '(ppu_context* ') and ln.rstrip().endswith('{')) or ln == n + ':':
                    out.append(f'        gowa_probe(0x{addr}u, ctx); {MARCA}')
        novo = eol.join(out)
        if 'void gowa_probe(unsigned' not in novo:
            k = '#include "ppu_recomp.h"' + eol
            i = novo.index(k) + len(k)
            novo = novo[:i] + DECL.replace('\n', eol) + novo[i:]
        gravar(p, novo)
        for n in alvo:
            pendentes.discard(n)
            print(f'sonda em {n} -> {p.name}')
    for n in pendentes:
        print(f'NÃO ACHEI {n}')


def tirar():
    for p in arquivos():
        txt = ler(p)
        if MARCA not in txt:
            continue
        eol = '\r\n' if '\r\n' in txt else '\n'
        novo = eol.join(l for l in txt.split(eol) if MARCA not in l)
        gravar(p, novo)
        print(f'sondas removidas de {p.name}')


DECL_CRON = ('extern "C" void gowa_tm_begin(void); extern "C" void gowa_tm_end(unsigned int lr); '
             + MARCA + '\n')


def cron(lrs):
    """Cronômetro em volta das chamadas cujo retorno é lr (o 'ctx->lr = 0x...;'
    da linha da chamada): gowa_tm_begin() antes, gowa_tm_end(lr) depois.
    gowa_tm_end (port/gowa_offline.cpp) soma o tempo por ponto e imprime a
    cada ~5 s: chamadas, ms no total, média e máximo em µs."""
    alvos = {int(x, 16) for x in lrs}
    for p in arquivos():
        txt = ler(p)
        eol = '\r\n' if '\r\n' in txt else '\n'
        out, n = [], 0
        for ln in txt.split(eol):
            m = re.match(r'^(\s*)ctx->lr = 0x([0-9A-F]{8}); func_[0-9A-F]{8}\(ctx\);', ln)
            if m and int(m.group(2), 16) in alvos:
                out.append(f'{m.group(1)}gowa_tm_begin(); {MARCA}')
                out.append(ln)
                out.append(f'{m.group(1)}gowa_tm_end(0x{m.group(2)}u); {MARCA}')
                n += 1
                continue
            out.append(ln)
        if not n:
            continue
        novo = eol.join(out)
        if 'gowa_tm_end(unsigned' not in novo:
            k = '#include "ppu_recomp.h"' + eol
            i = novo.index(k) + len(k)
            novo = novo[:i] + DECL_CRON.replace('\n', eol) + novo[i:]
        gravar(p, novo)
        print(f'{n} cronômetros -> {p.name}')


if __name__ == '__main__':
    if sys.argv[1] == 'por':
        por(sys.argv[2:])
    elif sys.argv[1] == 'cron':
        cron(sys.argv[2:])
    elif sys.argv[1] == 'tirar_cron':   # só os cronômetros; as sondas ficam
        for p in arquivos():
            txt = ler(p)
            if 'gowa_tm_' not in txt:
                continue
            eol = '\r\n' if '\r\n' in txt else '\n'
            gravar(p, eol.join(l for l in txt.split(eol) if not ('gowa_tm_' in l and MARCA in l)))
            print(f'cronômetros removidos de {p.name}')
    else:
        # 'tirar' remove TODAS, inclusive as sondas fixas (0x1C05A4 é o log do
        # jogo, de onde scripts/fps_fase.ps1 tira o "go to playing")
        tirar()
