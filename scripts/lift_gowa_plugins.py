"""Traduz os plugins SPU do módulo de job 0x72DF80 (w19/20/21): fragmentos de
código cru do GOWA.elf (0x7493B0..0x75CC80) que o módulo copia por DMA para
um endereço de LS que varia a cada job. Cada fragmento é uma função folha com
um pool de constantes no fim, lido por lqr.

Cada fragmento é traduzido sozinho na base BASE (0x30000, fora de onde eles
caem de verdade) e pós-processado: um spu_ls_read128/write128 com endereço
literal dentro do fragmento só pode ser lqr/stqr (relativo ao pc), e passa a
somar g_spu_hot_reloc (= LS real - LS traduzido, posto pelo interpretador ao
chamar a função em linha; ver spu_interp_hot_register_reloc).

As funções NÃO entram no registro global de funções SPU (um contexto de
imagem 0 casaria qualquer endereço); o .inc traz os ponteiros direto.

Saída: port/spu/gowapl_<EA>/spu_recomp.{c,h} e port/gowa_plugins.inc
Uso: python lift_gowa_plugins.py out\\plugins_getcode.txt
     python lift_gowa_plugins.py lista --wids 11 --inc port\\gowa_plugins_w11.inc --tabela g_gowa_plugin_fns_w11
     (outros workloads: mesma tradução, tabela e .inc separados; um EA com
     tamanhos diferentes vira um fragmento só, com o maior tamanho)
     --so 754B80,74C580: retraduz só esses (as linhas dos outros no .inc ficam)
Entradas no meio de um plugin (o job do EDGE chama alguns em +N, pulando o
prólogo; achadas com GOWA_INTERP_ENTRY_LOG, 19/10) vão em ENTRADAS_EXTRA:
viram funções próprias, registradas como as outras.
Depois de traduzir: promover_regs.py nos diretórios refeitos (mover antes o
spu_recomp.c.sem_promocao antigo deles, senão a promoção parte do antigo).
"""
import os as _os_raiz
RAIZ = _os_raiz.environ.get('GOWA_RAIZ') or _os_raiz.path.dirname(_os_raiz.path.dirname(_os_raiz.path.abspath(__file__)))  # raiz do projeto
import collections
import os
import re
import struct
import subprocess
import sys

R = RAIZ
TOOLS = os.path.join(R, 'ps3recomp', 'tools')
sys.path.insert(0, TOOLS)
from wrap_spu_elf import wrap  # noqa: E402

LIFTER = os.path.join(TOOLS, 'spu_lifter.py')
BASE = 0x30000
ELF = os.path.join(R, 'jogo', 'GOWA.elf')
OUTDIR = os.path.join(R, 'port', 'spu')
INC = os.path.join(R, 'port', 'gowa_plugins.inc')
# 0x70F630 +0x6F8 (20/10): o plugin do w11 calcula a própria base (+0x880,
# link PIC) e chama base+0x6F8 por bisl -- o meio da função 0x306D0, a rotina
# com o laço de ~27 mil instruções por job, que por isso rodava interpretada.
ENTRADAS_EXTRA = {0x754B80: [0x14], 0x755540: [0x14], 0x74C580: [0x20], 0x74EF90: [0x90], 0x70F630: [0x880, 0x6F8],
                  # 21/10: GOWA_INTERP_ENTRY_LOG no cap. 16/fase 1 com o vk22
                  0x756880: [0xC], 0x752710: [0x20], 0x75C570: [0x10],
                  # 23/10: GOWA_INTERP_ENTRY_LOG no cap. 16 com o xm1 -- a mesma rotina nos dois
                  0x75B710: [0x18], 0x75B470: [0x18],
                  # 23/10 (2): o mesmo, depois de tirar as de cima da lista do registro
                  0x75CBC0: [0xC], 0x74CEC0: [0x18]}
PROIBIDO = (r'spu_rdch|spu_wrch|spu_rchcnt|spu_indirect_branch|spu_drain_call|spu_halt|spu_stop|'
            r'spu_iret|spu_check_stack_reset|longjmp|SPU_STOP|spu_sync')


def main():
    global INC
    args = sys.argv[1:]
    wids, tabela_nome = ('19', '20', '21'), 'g_gowa_plugin_fns'
    if '--wids' in args:
        wids = tuple(args[args.index('--wids') + 1].split(','))
    if '--inc' in args:
        INC = os.path.join(R, args[args.index('--inc') + 1])
    if '--tabela' in args:
        tabela_nome = args[args.index('--tabela') + 1]
    naofolha = '--naofolha' in args
    so = None
    if '--so' in args:
        so = {int(x, 16) for x in args[args.index('--so') + 1].split(',')}
    edge = wids == ('19', '20', '21')
    d = open(ELF, 'rb').read()
    phoff = struct.unpack_from('>Q', d, 0x20)[0]
    phnum = struct.unpack_from('>H', d, 0x38)[0]
    phsz = struct.unpack_from('>H', d, 0x36)[0]
    segs = []
    for i in range(phnum):
        p = phoff + i * phsz
        t = struct.unpack_from('>I', d, p)[0]
        off, va, _pa, fsz = struct.unpack_from('>QQQQ', d, p + 8)
        if t == 1 and fsz:
            segs.append((off, va, fsz))

    def va2f(v):
        for off, va, fsz in segs:
            if va <= v < va + fsz:
                return off + (v - va)

    frag = collections.Counter()
    maior = {}
    for linha in open(args[0]):
        ea, sz, ls, wid = linha.split()
        ea, sz = int(ea, 16), int(sz, 16)
        if wid in wids and not (edge and 0x741B00 <= ea < 0x745F60):
            if edge:
                frag[(ea, sz)] += 1
            else:
                maior[ea] = max(maior.get(ea, 0), sz)
    for ea, sz in maior.items():
        frag[(ea, sz)] += 1
    tmp_elf = os.path.join(OUTDIR, '_tmp_gowapl.elf')
    tabela = []
    for (ea, sz) in sorted(frag):
        if so is not None and ea not in so:
            continue
        raw = d[va2f(ea):va2f(ea) + sz]
        nome = f'gowapl_{ea:08X}'
        with open(tmp_elf, 'wb') as f:
            f.write(wrap(raw, base=BASE, entry=BASE))
        saida = os.path.join(OUTDIR, nome)
        cmd = [sys.executable, LIFTER, '--auto-functions', tmp_elf, '--base', hex(BASE),
               '--symbol-prefix', nome + '_', '-o', saida,
               '--source-name', 'spu_recomp.c', '--header-name', 'spu_recomp.h',
               '--extra-funcs', ','.join(hex(BASE + o) for o in [0] + ENTRADAS_EXTRA.get(ea, []))]
        r = subprocess.run(cmd, capture_output=True, text=True)
        if r.returncode != 0:
            print(f'FALHOU {nome}: {r.stderr[-400:]}')
            continue
        # lqr/stqr: endereço literal dentro do fragmento -> relativo ao LS real
        c = os.path.join(saida, 'spu_recomp.c')
        src = open(c, encoding='utf-8').read()
        n_rel = 0

        def rel(m):
            nonlocal n_rel
            a = int(m.group(2), 16)
            if BASE <= a < BASE + sz:
                n_rel += 1
                return f'{m.group(1)}(ctx, (uint32_t)(0x{a:X} + g_spu_hot_reloc)'
            return m.group(0)
        src = re.sub(r'(spu_ls_read128|spu_ls_write128)\(ctx, 0x([0-9A-Fa-f]+)', rel, src)
        # --naofolha: funções que chamam outras do MESMO fragmento também
        # valem. O endereço de retorno (spu_link -- também o truque PIC do
        # plugin, brsl para a instrução seguinte) e o ponto de volta do
        # spu_drain_call passam a ser do LS real (+ g_spu_hot_reloc), como os
        # lqr: quem chama e quem retorna somam o mesmo deslocamento. Os
        # ctx->pc de transferência de cauda ficam no espaço traduzido: o
        # interpretador já soma o deslocamento quando a função volta com uma
        # (spu_hot_try_reloc) -- relocados aqui, iam duas vezes e o boot
        # travava aos 23 s.
        if naofolha:
            def rel2(m):
                nonlocal n_rel
                a = int(m.group(2), 16)
                if BASE <= a < BASE + sz:
                    n_rel += 1
                    return f'{m.group(1)}(uint32_t)(0x{a:X} + g_spu_hot_reloc){m.group(3)}'
                return m.group(0)
            src = re.sub(r'(spu_link\()0x([0-9A-Fa-f]+)(\))', rel2, src)
            src = re.sub(r'(spu_drain_call\(ctx, )0x([0-9A-Fa-f]+)(\))', rel2, src)
        if 'g_spu_hot_reloc' in src and 'extern SPU_THREAD_LOCAL int32_t g_spu_hot_reloc;' not in src:
            src = src.replace('#include "spu_recomp.h"',
                              '#include "spu_recomp.h"\n/* LS real - LS traduzido do plugin em execução (spu_interp.c) */\n'
                              'extern SPU_THREAD_LOCAL int32_t g_spu_hot_reloc;', 1)
        open(c, 'w', encoding='utf-8').write(src)

        cab = re.compile(r'^void ' + nome + r'_spu_func_([0-9A-F]{8})\(spu_context\* ctx\) \{$', re.M)
        ms = list(cab.finditer(src))
        if naofolha:   # chamadas ao próprio fragmento e o dreno delas ficam permitidos
            proib = re.compile(PROIBIDO.replace('spu_drain_call|', ''))
        else:
            proib = re.compile(PROIBIDO + '|' + nome + r'_spu_func_[0-9A-F]{8}\(ctx\)')
        fns = []
        for i, m in enumerate(ms):
            a = int(m.group(1), 16)
            fim = ms[i + 1].start() if i + 1 < len(ms) else len(src)
            if proib.search(src[m.end():fim]):
                continue
            n = (int(ms[i + 1].group(1), 16) if i + 1 < len(ms) else BASE + sz) - a
            w0 = struct.unpack_from('>I', raw, a - BASE)[0]
            fns.append((a, n, w0))
        print(f'{nome} bytes=0x{sz:04X} funcoes={len(ms)} folhas={len(fns)} lqr_relocados={n_rel}')
        tabela.append((ea, sz, nome, fns))
    if os.path.exists(tmp_elf):
        os.remove(tmp_elf)
    manter_ext, manter_lin = [], []
    if so is not None and os.path.exists(INC):
        refeitos = {f'gowapl_{ea:08X}_' for ea in so}
        for ln in open(INC, encoding='utf-8'):
            if 'gowapl_' not in ln or any(r in ln for r in refeitos):
                continue
            (manter_ext if ln.startswith('extern') else manter_lin).append(ln)
    with open(INC, 'w', encoding='utf-8') as f:
        f.write('/* GERADO por scripts/lift_gowa_plugins.py: plugins SPU (workloads %s),\n'
                ' * traduzidos na base 0x%X. Funções folha relocáveis: LS traduzido, bytes,\n'
                ' * EA dos bytes, primeira palavra, ponteiro. Fora do registro global. */\n' % ('/'.join(wids), BASE))
        f.writelines(manter_ext)
        for ea, sz, nome, fns in tabela:
            for a, n, w0 in fns:
                f.write(f'extern "C" void {nome}_spu_func_{a:08X}(void*);\n')
        f.write('static const struct { uint32_t ls, len, ea, w0; void (*fn)(void*); } %s[] = {\n' % tabela_nome)
        f.writelines(manter_lin)
        for ea, sz, nome, fns in tabela:
            for a, n, w0 in fns:
                f.write(f'    {{ 0x{a:05X}u, {n}u, 0x{ea + a - BASE:08X}u, 0x{w0:08X}u, {nome}_spu_func_{a:08X} }},\n')
        f.write('};\n')
    print(f'{len(tabela)} plugins; tabela em {INC}')


if __name__ == '__main__':
    main()
