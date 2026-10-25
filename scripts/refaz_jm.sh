#!/bin/bash
# Retraduz módulos de job comuns (não o 0x72DF80, que tem núcleos vk/ninho, nem o
# 0x727200, que tem fusão/promoção de chamadas/núcleo vc) e refaz por cima a
# cadeia de passes que eles tinham (23/10), na ordem em que foi aplicada:
#   tradução -> promover_regs (folhas, a partir do .sem_promocao novo)
#   -> spu_cc_interno -> spu_num_remap -> ls_const_fold -> flush_preguicoso
#   -> encadeia todas -> encadeia todas --spill
# Antes de tudo copia o diretório inteiro para out/bak_<módulo>_<HHMM>/ e tira os
# backups antigos dos passes do caminho (sem_antigos/), para cada passe gravar o
# seu a partir da tradução nova. As entradas extras ficam no ENTRADAS_EXTRA do
# lift_gowa_jobmods.py.
#
# uso: bash scripts/refaz_jm.sh 789F00 781D80 ...
set -e
R="${GOWA_RAIZ:-$(cd "$(dirname "$0")/.." && pwd)}"
cd $R
HM=$(date +%H%M)
cp -p port/gowa_jobmods.inc out/bak_gowa_jobmods_inc_$HM
for QUAL in "$@"; do
    MOD=gowajm_00${QUAL}
    D=port/spu/$MOD
    B=out/bak_${MOD}_${HM}
    mkdir -p $B/sem_antigos
    cp -p $D/* $B/
    echo "== $MOD (backup em $B)"
    python scripts/lift_gowa_jobmods.py --so $QUAL
    for f in spu_recomp.c.sem_promocao spu_recomp.c.sem_pn spu_recomp.c.sem_kc spu_recomp.c.sem_const spu_recomp.c.sem_flushpreg spu_recomp.c.antes_sel spu_recomp.c.sem_tail spu_recomp.c.sem_encadeia spu_recomp.c.sem_gancho; do
        [ -f $D/$f ] && mv $D/$f $B/sem_antigos/
    done
    cp $D/spu_recomp.c $D/spu_recomp.c.sem_promocao
    python scripts/promover_regs.py $D | tail -1
    python scripts/spu_cc_interno.py aplicar $D | tail -1
done
python scripts/spu_num_remap.py aplicar | tail -2
python scripts/ls_const_fold.py aplicar | tail -2
for QUAL in "$@"; do
    D=port/spu/gowajm_00${QUAL}
    python scripts/flush_preguicoso.py $D --aplicar | tail -1
    python scripts/encadeia.py $D todas | tail -1
    python scripts/encadeia.py $D todas --spill | tail -1
done
echo "pronto (compilar com scripts/gowa_ninja.cmd)"
