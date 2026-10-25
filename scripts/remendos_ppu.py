"""Remendos persistentes no código lifted do GOWA (port/recompiled/ppu_recomp_NNN.cpp).

Uso:
  python remendos_ppu.py aplicar     (idempotente)
  python remendos_ppu.py tirar       (remove todas as linhas marcadas)
  python remendos_ppu.py ver         (lista o que está aplicado)

Diferente das sondas (scripts/sonda.py), estes ficam: corrigem o comportamento do
jogo. Cada linha inserida termina com o marcador '/*REMENDO*/'. Antes da primeira
alteração de um arquivo, grava <arquivo>.sem_remendo (cópia do original) se ainda
não existir.

Remendos:
  texstream -- espera real pelo job SPU "UpdateTexStreaming" (contador u16 em
  0x9AA4A4) nas esperas do próprio jogo, que leem 0x9AA4A8 (nunca escrito), e na
  entrada de quem grava a tabela de estado do heap de VRAM. Detalhes em
  gowa_texstream_wait (port/gowa_offline.cpp) e no PROGRESSO.md (17/10).
"""
import os as _os_raiz
RAIZ = _os_raiz.environ.get('GOWA_RAIZ') or _os_raiz.path.dirname(_os_raiz.path.dirname(_os_raiz.path.abspath(__file__)))  # raiz do projeto
import re
import shutil
import sys
from pathlib import Path

DIR = Path(_os_raiz.path.join(RAIZ, r'port\recompiled'))
MARCA = '/*REMENDO*/'
DECL = 'extern "C" void gowa_texstream_wait(void); ' + MARCA

# (função, linha a inserir logo após a abertura)
ENTRADAS = [
    ('func_0038DA00', 'gowa_texstream_wait(); ' + MARCA),   # antes da manutenção do heap e do próximo disparo
    ('func_0038D9A0', 'gowa_texstream_wait(); ' + MARCA),   # espera oficial no meio do frame (0x41676C)
    ('func_003973C8', 'gowa_texstream_wait(); ' + MARCA),   # escritores da tabela (carga de recurso,
    ('func_0039490C', 'gowa_texstream_wait(); ' + MARCA),   #  streaming) -- vistos com GOWA_TSRACE e
    ('func_00395AE8', 'gowa_texstream_wait(); ' + MARCA),   #  GOWA_WGUARD com o job em voo
]


# pausa -- PAUSE do x86 nos laços de espera ativa da thread principal: a dica
# "or 31,31,31" do PPU (atrasa o despacho, cede o núcleo ao outro thread de
# hardware) e as voltas das esperas por listas de job WWS (func_00445720 e
# func_00445320, que chamam func_00445048 sem parar). Sem ela a espera roda a
# toda velocidade e tira recursos do thread de SPU que divide o núcleo (8
# núcleos, 9 threads ocupadas). GOWA_PPU_PAUSE=0 desliga em tempo de execução.
# 24/10: modo 2 (gowa_ppu_espera, gowa_offline.cpp) cede a fatia depois de 256
# voltas -- padrão com menos de 12 CPUs lógicas. As linhas antigas são trocadas.
PAUSA_DECL = 'extern "C" int g_gowa_ppu_pause; extern "C" void gowa_ppu_espera(void); ' + MARCA
PAUSA = ('if (g_gowa_ppu_pause) { if (g_gowa_ppu_pause == 1) __builtin_ia32_pause(); '
         'else gowa_ppu_espera(); } ' + MARCA)
PAUSA_DECL_V1 = 'extern "C" int g_gowa_ppu_pause; ' + MARCA
PAUSA_V1 = 'if (g_gowa_ppu_pause) __builtin_ia32_pause(); ' + MARCA
PAUSA_ROTULOS = ('loc_00445754:', 'loc_00445364:')
PAUSA_LINHA = 'ctx->gpr[31] = ctx->gpr[31] | ctx->gpr[31];'


def aplicar_pausa():
    for p in arquivos():
        s = ler(p)
        if PAUSA_V1 in s:   # remendo de antes de 24/10: só PAUSE
            n1 = s.count(PAUSA_V1)
            s = s.replace(PAUSA_V1, PAUSA).replace(PAUSA_DECL_V1, PAUSA_DECL)
            gravar(p, s)
            print('pausa atualizada em', n1, 'pontos ->', p.name)
        if PAUSA_LINHA not in s and not any(r in s for r in PAUSA_ROTULOS):
            continue
        eol = '\r\n' if '\r\n' in s[:4000] else '\n'
        linhas = s.split(eol)
        out, n = [], 0
        for i, ln in enumerate(linhas):
            out.append(ln)
            t = ln.strip()
            if t in PAUSA_ROTULOS or t == PAUSA_LINHA:
                prox = linhas[i + 1] if i + 1 < len(linhas) else ''
                if MARCA not in prox:
                    out.append('        ' + PAUSA)
                    n += 1
        if not n:
            print('pausa já aplicada:', p.name)
            continue
        s = eol.join(out)
        if PAUSA_DECL not in s:
            i = s.find('#include')
            j = s.find('\n', i) + 1
            s = s[:j] + PAUSA_DECL + eol + s[j:]
        gravar(p, s)
        print('pausa em', n, 'pontos ->', p.name)


def arquivos():
    return sorted(DIR.glob('ppu_recomp_[0-9][0-9][0-9].cpp'))


def ler(p):
    return p.read_bytes().decode('latin-1')


def gravar(p, s):
    bak = p.with_name(p.name + '.sem_remendo')
    if not bak.exists():
        shutil.copy2(p, bak)
    p.write_bytes(s.encode('latin-1'))


def aplicar():
    pend = {f: l for f, l in ENTRADAS}
    for p in arquivos():
        s = ler(p)
        mudou = False
        for fn, linha in list(pend.items()):
            m = re.search(r'^void ' + fn + r'\(ppu_context\* (?:__restrict )?ctx\) \{\r?\n', s, re.M)
            if not m:
                continue
            del pend[fn]
            if s[m.end():m.end() + 400].find(MARCA) >= 0:
                print('já aplicado:', fn, '->', p.name)
                continue
            eol = '\r\n' if s[m.end() - 2:m.end()] == '\r\n' else '\n'
            s = s[:m.end()] + '        ' + linha + eol + s[m.end():]
            mudou = True
            print('remendo em', fn, '->', p.name)
        if mudou:
            if DECL not in s:
                eol = '\r\n' if '\r\n' in s[:4000] else '\n'
                i = s.find('#include')
                j = s.find('\n', i) + 1
                s = s[:j] + DECL + eol + s[j:]
            gravar(p, s)
    for fn in pend:
        print('NÃO ACHEI', fn)
    aplicar_pausa()


def tirar():
    for p in arquivos():
        s = ler(p)
        if MARCA not in s:
            continue
        s2 = re.sub(r'^[^\n]*' + re.escape(MARCA) + r'[^\n]*\n', '', s, flags=re.M)
        p.write_bytes(s2.encode('latin-1'))
        print('removido de', p.name)


def ver():
    for p in arquivos():
        s = ler(p)
        if MARCA not in s:
            continue
        for m in re.finditer(r'^void (func_[0-9A-F]{8})\(ppu_context\* (?:__restrict )?ctx\) \{\r?\n[^\n]*' + re.escape(MARCA), s, re.M):
            print(p.name, m.group(1))


if __name__ == '__main__':
    {'aplicar': aplicar, 'tirar': tirar, 'ver': ver}[sys.argv[1] if len(sys.argv) > 1 else 'ver']()
