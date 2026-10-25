"""Promove os registradores SPU (ctx->gpr[N]) a variáveis locais nas funções traduzidas.

Uso:
  python promover_regs.py port/spu/gowajm_0072DF80 [outros dirs...]   (aplica; idempotente)
  python promover_regs.py --tirar port/spu/gowajm_0072DF80 ...       (volta ao original)

Por quê: no código traduzido cada registrador vive em ctx->gpr[], acessado por
união, e as escritas na LS são memcpy por uint8_t*. Para o compilador, qualquer
escrita pode alterar ctx->gpr, então cada operação relê e regrava os
registradores na memória (visto no assembly da função quente 0x4F00 do módulo
0x727200). Com locais (_rN), ficam em registradores do host entre as fronteiras.

Fronteiras (sincronização com ctx->gpr):
  - chamadas que podem ler ou escrever registradores (funções traduzidas,
    spu_drain_call, spu_indirect_branch, SPU_RET, SPU_RET_REG, spu_stop,
    spu_halt, gowa_fnskip, spu_check_stack_reset): grava antes (SPILL) e
    relê depois (RELOAD);
  - spu_wrch / spu_rdch / spu_rchcnt: não mexem nos registradores, só gravam
    antes (podem parar a SPU);
  - todo `return` e o fim da função: grava antes.
spu_ls_read128/write128 e spu_img_restore não tocam em registradores.

Funções em que uma chamada de sincronização total aparece dentro de uma
expressão (não como comando) ficam como estão. Antes da primeira alteração de
um arquivo, grava <arquivo>.sem_promocao; as aplicações seguintes partem dele.
"""
import os
import re
import shutil
import sys
from pathlib import Path

MARCA = '/* promover_regs.py */'
FULL = {'spu_drain_call', 'spu_indirect_branch', 'SPU_RET', 'SPU_RET_REG', 'spu_stop',
        'spu_halt', 'gowa_fnskip', 'spu_check_stack_reset'}
HALF = {'spu_wrch', 'spu_rdch', 'spu_rchcnt'}
SAFE = {'spu_ls_read128', 'spu_ls_write128', 'spu_img_restore'}

RE_HDR = re.compile(r'^void (\w+)\(spu_context\* ctx\) \{[ \t]*$')
RE_GPR = re.compile(r'ctx->gpr\[(\d+)\]')
RE_CALL = re.compile(r'\b([A-Za-z_]\w*)\(ctx[,)]')


def fim_chamada(s, i):
    """i aponta para o '(' da chamada; devolve o índice logo após o ')' casado."""
    d = 0
    while i < len(s):
        c = s[i]
        if c == '(':
            d += 1
        elif c == ')':
            d -= 1
            if d == 0:
                return i + 1
        i += 1
    return -1


def inicio_comando(s, i):
    """Índice do início do comando que contém s[i] (logo após ; { } ou o começo)."""
    j = i - 1
    while j >= 0 and s[j] not in ';{}':
        j -= 1
    j += 1
    while j < i and s[j] in ' \t':
        j += 1
    return j


def processar_linha(lin, eh_lifted):
    """Devolve (linha nova, ok). Marca SPILL/RELOAD em volta das chamadas."""
    out = []
    pos = 0
    ins = []                     # (índice, texto) a inserir
    for m in RE_CALL.finditer(lin):
        nome = m.group(1)
        if nome in SAFE:
            continue
        full = nome in FULL or eh_lifted(nome)
        half = nome in HALF
        if not full and not half:
            return None, False   # chamada desconhecida com ctx: não mexe na função
        abre = lin.index('(', m.start())   # o '(' da chamada
        fecha = fim_chamada(lin, abre)
        if fecha < 0:
            return None, False
        ini = inicio_comando(lin, m.start())
        resto = lin[fecha:].lstrip()
        comando = (ini == m.start()) and resto.startswith(';')
        if full:
            if not comando:
                return None, False
            fim = lin.index(';', fecha) + 1
            ins.append((ini, 'SPILL; '))
            ins.append((fim, ' RELOAD;'))
        else:
            ins.append((ini, 'SPILL; '))
    if not ins:
        return lin, True
    ins.sort(key=lambda t: t[0])
    for i, t in ins:
        out.append(lin[pos:i])
        out.append(t)
        pos = i
    out.append(lin[pos:])
    novo = ''.join(out)
    # RELOAD; seguido de SPILL; sem nada entre eles: os dois sobram
    novo = re.sub(r' RELOAD;\s*SPILL; ', ' ', novo)
    return novo, True


def processar(src):
    linhas = src.split('\n')
    nomes = set()
    for l in linhas:
        m = RE_HDR.match(l.rstrip('\r'))
        if m:
            nomes.add(m.group(1))
    prefixos = {n.rsplit('_spu_func_', 1)[0] + '_spu_func_' for n in nomes if '_spu_func_' in n}

    def eh_lifted(nome):
        return any(nome.startswith(p) for p in prefixos) or nome in nomes

    res = []
    i = 0
    nf = npromo = 0
    while i < len(linhas):
        l = linhas[i]
        m = RE_HDR.match(l.rstrip('\r'))
        if not m:
            res.append(l)
            i += 1
            continue
        j = i + 1
        while j < len(linhas) and linhas[j].rstrip('\r') != '}':
            j += 1
        corpo = linhas[i + 1:j]
        nf += 1
        texto = '\n'.join(corpo)
        regs = sorted({int(x) for x in RE_GPR.findall(texto)})
        # Só funções sem chamada que volte (função traduzida, drain, desvio
        # indireto): nelas a recarga de todos os registradores depois de cada
        # chamada custava mais do que ganhava (A/B 18/10: -4%). SPU_RET, stop,
        # halt e return são saídas: só gravam.
        tem_chamada = any(eh_lifted(mm.group(1)) or mm.group(1) in ('spu_drain_call', 'spu_indirect_branch',
                                                                    'gowa_fnskip', 'spu_check_stack_reset')
                          for mm in RE_CALL.finditer(texto))
        # PROMOVER_LACOS=1: também funções com laço (goto para um rótulo anterior),
        # com SPILL/RELOAD em volta das chamadas. Desligado por padrão: no
        # 0x72DF80 mesclado mediu pior (22,60 x 23,25, 18/10). A regra só de
        # folhas nos outros módulos mediu +8% (24,35 x 22,55).
        rotulos = {}
        laco = False
        for k, c in enumerate(corpo):
            mm = re.match(r'^(loc_[0-9A-F]+):', c.strip())
            if mm:
                rotulos[mm.group(1)] = k
            for g in re.findall(r'goto (loc_[0-9A-F]+);', c):
                if g in rotulos and rotulos[g] <= k:
                    laco = True
        if tem_chamada and not (laco and os.environ.get('PROMOVER_LACOS', '0') == '1'):
            regs = []
        # registradores escritos: só estes voltam para ctx->gpr nas saídas
        escritos = sorted({int(x) for x in re.findall(r'ctx->gpr\[(\d+)\](?:\._u\d+\[\d+\])? = (?!=)', texto)})
        cr = '\r' if l.endswith('\r') else ''
        novo = []
        ok = bool(regs)
        for c in corpo:
            if not ok:
                break
            c2, ok = processar_linha(c, eh_lifted)
            if ok:
                c2 = RE_GPR.sub(lambda mm: '_r' + mm.group(1), c2)
                c2 = re.sub(r'\breturn;', '{ SPILL; return; }', c2)
                novo.append(c2)
        if not ok:
            res.extend(linhas[i:j + 1])
            i = j + 1
            continue
        npromo += 1
        spill = ' '.join(f'ctx->gpr[{r}] = _r{r};' for r in escritos) or '(void)0;'
        reload = ' '.join(f'_r{r} = ctx->gpr[{r}];' for r in regs)
        res.append(f'#define SPILL do {{ {spill} }} while (0) {MARCA}{cr}')
        res.append(f'#define RELOAD do {{ {reload} }} while (0){cr}')
        res.append(l)
        res.append('        u128 ' + ', '.join(f'_r{r}' for r in regs) + ';' + cr)
        res.append('        RELOAD;' + cr)
        res.extend(novo)
        res.append('        SPILL;' + cr)
        res.append(linhas[j])
        res.append('#undef SPILL' + cr)
        res.append('#undef RELOAD' + cr)
        i = j + 1
    return '\n'.join(res), nf, npromo


def main():
    args = sys.argv[1:]
    tirar = '--tirar' in args
    for d in [a for a in args if not a.startswith('--')]:
        f = Path(d) / 'spu_recomp.c'
        bak = f.with_name(f.name + '.sem_promocao')
        if tirar:
            if bak.exists():
                shutil.copy2(bak, f)
                print('original restaurado:', f)
            continue
        if not bak.exists():
            shutil.copy2(f, bak)
        src = bak.read_bytes().decode('latin-1')
        novo, nf, np_ = processar(src)
        f.write_bytes(novo.encode('latin-1'))
        print(f'{f}: {np_} de {nf} funções promovidas')


if __name__ == '__main__':
    main()
