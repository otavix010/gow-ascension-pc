"""Passes de desempenho sobre o código SPU traduzido (etapa 5b do gerar.ps1).

Refaz, na ordem da build de referência, o que os módulos tiveram por cima da
tradução (a ordem e a lista de cada módulo saíram dos backups .sem_* da árvore de
desenvolvimento; a tradução de partida é idêntica à dela):

  módulos de job (menos 0x72DF80): promover_regs -> spu_cc_interno
  0x72DF80 (EDGE) e PM 0x79E500:   spu_cc_interno (sem promoção: no EDGE ela perde ~10%)
  tarefas do SPURS (spu_<sha>):    spu_cc_interno
  plugins (menos os 3 do w11):     promover_regs (sem convenção interna e sem saltos diretos)
  todos:                           spu_num_remap -> ls_const_fold (módulos de job) -> flush_preguicoso
  0x72DF80:                        14 núcleos vetoriais (vetorizar_laco) -> gancho LUZGPU
                                   (luz por vértice na GPU) -> ninho 0x73E0 (vetorizar_ninho)
  0x727200 (General6):             fundir_cadeia -> promover_chamadas -> vetorizar_chamadas
                                   -> gancho_vc
  plugins 0x74EF90 / 0x755540:     núcleo vetorial
  job, PM e tarefas:               encadeia todas (+ --spill, menos no 0x727200)
  por último:                      divide_pic (19 módulos; só reconhece a forma promovida)
  por último:                      guarda_avx512 (os .inc novos também compilam em AVX2)

Cada passe deixa o backup .sem_* dele; o script marca port/spu/.otimizado no fim e não
roda de novo sobre código já otimizado (para refazer, rode a etapa 5 de novo).

uso: python scripts/otimizar.py"""
import os
import re
import subprocess
import sys
from pathlib import Path

RAIZ = Path(os.environ.get('GOWA_RAIZ') or Path(__file__).resolve().parent.parent)
S = RAIZ / 'scripts'
SPU = RAIZ / 'port' / 'spu'
MARCA = SPU / '.otimizado'

EDGE = 'gowajm_0072DF80'
GEN6 = 'gowajm_00727200'
PM = 'gowapm_0079E500'
PL_SEM_PROMO = {'gowapl_006E5830', 'gowapl_006E8130', 'gowapl_0070F630'}   # os do w11 (--naofolha)
VK_EDGE = ['73E0:7BB4', '73E0:7ED8', 'F538:F620', 'A880:A950', 'D250:D4DC', 'E550:EC08', 'E550:EE28',
           'E550:F24C', 'E550:E804', 'DCC0:E210', 'E550:EA7C', 'E550:F034', 'E550:F104', 'DCC0:DFF0']
VK_PL = {'gowapl_0074EF90': '304A0:304A0', 'gowapl_00755540': '30310:30310'}
LUZGPU = ('/*LUZGPU*/ { extern int g_gowa_luz_gpu; extern int gowa_luz_gpu(spu_context*); '
          'if (g_gowa_luz_gpu && gowa_luz_gpu(ctx)) goto loc_00008210; }')


if hasattr(sys.stdout, 'reconfigure'):
    sys.stdout.reconfigure(encoding='utf-8', errors='replace')
AMB = dict(os.environ, PYTHONIOENCODING='utf-8')


def py(*args):
    cmd = [sys.executable, str(S / args[0])] + [str(a) for a in args[1:]]
    print('>', ' '.join(str(a) for a in args), flush=True)
    r = subprocess.run(cmd, cwd=str(RAIZ), capture_output=True, text=True, encoding='utf-8', errors='replace', env=AMB)
    saida = (r.stdout + r.stderr).strip().split('\n')
    for ln in saida[-2:]:
        if ln.strip():
            print('   ' + ln[:200], flush=True)
    if r.returncode != 0:
        print('\n'.join(saida[-30:]))
        sys.exit(f'falhou: {args[0]} (código {r.returncode})')


def gancho_luzgpu(d):
    """A luz por vértice do EDGE (0x73E0) passa para a GPU (rsx_edge_luz.h): a linha entra logo
    antes do rótulo do laço externo 0x79B0; com GOWA_LUZ_GPU=1 (padrão) e o desenho marcado, a
    SPU pula para o fim da função (0x8210)."""
    c = d / 'spu_recomp.c'
    raw = c.read_bytes().decode('latin-1')
    nl = '\r\n' if '\r\n' in raw[:4000] else '\n'
    if '/*LUZGPU*/' in raw:
        print('   LUZGPU já aplicado')
        return
    if raw.count('\nloc_000079B0:') != 1 or 'loc_00008210:' not in raw:
        sys.exit('LUZGPU: não achei loc_000079B0/loc_00008210 no 0x72DF80')
    raw = raw.replace(nl + 'loc_000079B0:', nl + '        ' + LUZGPU + nl + 'loc_000079B0:', 1)
    c.write_bytes(raw.encode('latin-1'))
    print('   LUZGPU aplicado')


def main():
    if MARCA.exists():
        sys.exit(f'{MARCA} existe: o código já foi otimizado (rode de novo a geração das SPUs para refazer)')
    jm = sorted(p.name for p in SPU.glob('gowajm_*') if (p / 'spu_recomp.c').exists())
    pl = sorted(p.name for p in SPU.glob('gowapl_*') if (p / 'spu_recomp.c').exists())
    if EDGE not in jm or GEN6 not in jm or not (SPU / PM / 'spu_recomp.c').exists():
        sys.exit('faltam módulos traduzidos (rode antes a etapa das SPUs)')

    # as imagens de tarefa do SPURS geradas pelo build_spu_workloads (MeshProc, kernels...)
    tarefas = sorted(p.name for p in SPU.glob('spu_*') if (p / 'spu_recomp.c').exists() and (p / 'image.elf').exists())

    promover = [SPU / m for m in jm if m != EDGE] + [SPU / m for m in pl if m not in PL_SEM_PROMO]
    py('promover_regs.py', *promover)
    # os plugins ficam sem a convenção interna e sem saltos diretos (como na build de referência)
    py('spu_cc_interno.py', 'aplicar', *([SPU / m for m in jm] + [SPU / PM] + [SPU / t for t in tarefas]))
    py('spu_num_remap.py', 'aplicar')
    py('ls_const_fold.py', 'aplicar')   # nas tarefas (spu_<sha>) não: a build de referência ficou sem
    for m in jm + [PM] + pl + ['gowatspm_30023680'] + tarefas:
        if (SPU / m / 'spu_recomp.c').exists():
            py('flush_preguicoso.py', SPU / m, '--aplicar')

    d = SPU / EDGE
    for p in VK_EDGE:
        f, r = p.split(':')
        py('vetorizar_laco.py', d, f, r, '--fonte', d / 'spu_recomp.c.sem_flushpreg', '--gancho')
    gancho_luzgpu(d)
    py('vetorizar_ninho.py', d, '73E0', '79B0', '81C0', '--gancho')

    g = SPU / GEN6
    py('fundir_cadeia.py', g, '4930,4AB0,4E90,4E9C,4F00')
    py('promover_chamadas.py', g, '63B4')
    py('vetorizar_chamadas.py', g, '4930')
    py('gancho_vc.py', g, '63B4', '4930')

    for m, p in VK_PL.items():
        if (SPU / m / 'spu_recomp.c').exists():
            # os plugins são promovidos: o núcleo sai da tradução original (.sem_promocao) e o
            # gancho entra na função promovida
            f, r = p.split(':')
            py('vetorizar_laco.py', SPU / m, f, r, '--fonte', SPU / m / 'spu_recomp.c.sem_promocao', '--gancho')

    for m in jm + [PM] + ['gowatspm_30023680'] + tarefas:
        if not (SPU / m / 'spu_recomp.c').exists():
            continue
        py('encadeia.py', SPU / m, 'todas')
        if m != GEN6:
            py('encadeia.py', SPU / m, 'todas', '--spill')
    # divide as funções no alvo do idioma "entra em seção crítica" (brsl/ai+8/bihz): o padrão
    # só é reconhecido na forma já promovida, por isso vem por último (19 módulos)
    for m in jm + pl + [PM]:
        py('divide_pic.py', SPU / m, '--aplicar')
    py('guarda_avx512.py', 'aplicar')
    MARCA.write_text('otimizado\n', encoding='utf-8')
    print('pronto')


if __name__ == '__main__':
    main()
