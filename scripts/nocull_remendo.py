"""Experimento (21/10): EDGE sem o descarte de triângulos. Na função por desenho do 0x72DF80
(func_53B0 e a cópia em 5478/55xx), o jogo já tem o caminho que pula a chamada do descarte
(D618 -> D640: transformação D250 + compactação D92C) quando o bit 4096 da palavra de
flags (r86) está ligado -- e força o descarte quando o modo lido de r83+0x40 é 16. Este
remendo faz o teste do bit também aceitar g_gowa_edge_nocull (gowa_offline.cpp,
GOWA_EDGE_NOCULL=1): o desenho segue com a lista de índices inteira e a GPU descarta.

GOWA_EDGE_NOCULL=2: além disso, toda chamada do descarte (D618) vai com r4 = 0 -- o modo
"sem descarte" da própria D640: espera o DMA e devolve a contagem total de triângulos guardada
no contexto (+0x190), sem a transformação D250 nem a compactação D92C; as etapas seguintes
(548C..) rodam normalmente com todos os triângulos.

uso: python nocull_remendo.py aplicar|tirar     (no port/spu/gowajm_0072DF80/spu_recomp.c)"""
import os as _os_raiz
RAIZ = _os_raiz.environ.get('GOWA_RAIZ') or _os_raiz.path.dirname(_os_raiz.path.dirname(_os_raiz.path.abspath(__file__)))  # raiz do projeto
import sys
from pathlib import Path

F = Path(_os_raiz.path.join(RAIZ, r'port\spu\gowajm_0072DF80\spu_recomp.c'))
s = F.read_text(encoding='latin-1')
nl = '\r\n' if '\r\n' in s else '\n'
TESTE = 'if (ctx->gpr[84]._u32[0] != 0)'
NOVO = 'if (g_gowa_edge_nocull || ctx->gpr[84]._u32[0] != 0)'
DECL = 'extern int g_gowa_edge_nocull;   /* GOWA_EDGE_NOCULL (nocull_remendo.py) */'
ANT = 'ctx->gpr[84] = spu_and(ctx->gpr[86], ctx->gpr[31]);'
R6 = 'ctx->gpr[6] = spu_il(0);'
R4 = 'if (g_gowa_edge_nocull >= 2) ctx->gpr[4] = spu_il(0);   /* GOWA_EDGE_NOCULL=2: descarte no modo 0 */'
if sys.argv[1] == 'tirar':
    s = s.replace(NOVO, TESTE).replace(DECL + nl, '').replace('        ' + R4 + nl, '')
    F.write_text(s, encoding='latin-1')
    print('tirado')
    sys.exit(0)
if NOVO in s:
    print('já aplicado')
    sys.exit(0)
linhas = s.split(nl)
n = 0
for i, t in enumerate(linhas):
    if t.strip() == ANT:
        for j in range(i + 1, min(i + 4, len(linhas))):
            if TESTE in linhas[j]:
                linhas[j] = linhas[j].replace(TESTE, NOVO)
                n += 1
                break
m = 0
for i in range(len(linhas) - 1):
    if linhas[i].strip() == R6 and 'gowajm_0072DF80_spu_func_0000D618__b(ctx)' in linhas[i + 1]:
        linhas[i] = linhas[i] + nl + '        ' + R4
        m += 1
s = nl.join(linhas)
print(f'{m} chamadas do descarte com o modo 0 opcional')
i = s.find('#include "spu_recomp.h"')
k = s.find(nl, i) + len(nl)
s = s[:k] + DECL + nl + s[k:]
F.write_text(s, encoding='latin-1')
print(f'{n} testes remendados')
