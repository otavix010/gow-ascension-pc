#!/bin/bash
# Retraduz um módulo SPU e refaz, na ordem, os passes que já estavam aplicados por
# cima da tradução (21/10). Antes de tudo copia o diretório inteiro do módulo para
# out/bak_<módulo>_<HHMM>/ e afasta os backups antigos dos passes (sem_antigos/),
# para cada passe gravar o seu a partir da tradução nova.
#
# uso: bash scripts/refaz_modulo.sh 72DF80   (módulo de job 0x72DF80: com os 14 núcleos vk)
#      bash scripts/refaz_modulo.sh pm       (policy module 0x79E500)
# 21/10: depois dos núcleos vk, o ninho da 73E0 (só 72DF80) e o encadeia.py todas.
# As entradas extras ficam nos scripts de tradução (ENTRADAS_EXTRA em
# lift_gowa_jobmods.py; --extra-funcs em lift_gowa_pm.py).
set -e
R="${GOWA_RAIZ:-$(cd "$(dirname "$0")/.." && pwd)}"
cd $R
export GOWA_RAIZ_PY="$(cygpath -w "$R" 2>/dev/null || echo "$R")"
QUAL=$1
HM=$(date +%H%M)
if [ "$QUAL" = "pm" ]; then
    MOD=gowapm_0079E500
else
    MOD=gowajm_00${QUAL}
fi
D=port/spu/$MOD
B=out/bak_${MOD}_${HM}
mkdir -p $B/sem_antigos
cp -p $D/* $B/
cp -p port/gowa_jobmods.inc $B/ 2>/dev/null || true
cp -p port/gowa_pm_hot.inc $B/ 2>/dev/null || true
echo "backup em $B"

# 1) tradução
if [ "$QUAL" = "pm" ]; then
    python scripts/lift_gowa_pm.py
else
    python scripts/lift_gowa_jobmods.py --so $QUAL
fi
# 2) backups antigos dos passes saem do caminho; .sem_promocao = a tradução nova
for f in spu_recomp.c.sem_pn spu_recomp.c.sem_kc spu_recomp.c.sem_const spu_recomp.c.sem_flushpreg spu_recomp.c.antes_sel spu_recomp.c.sem_tail spu_recomp.c.sem_encadeia; do
    [ -f $D/$f ] && mv $D/$f $B/sem_antigos/
done
cp $D/spu_recomp.c $D/spu_recomp.c.sem_promocao
# 3) passes, na ordem em que foram aplicados
python scripts/spu_cc_interno.py aplicar $D
python scripts/spu_num_remap.py aplicar
if [ "$QUAL" != "pm" ]; then
    python scripts/ls_const_fold.py aplicar
fi
if [ "$QUAL" = "72DF80" ]; then
    # gancho manual do GOWA_SLOW_JOBMOD (teste de caminho crítico, scripts/sensib.ps1)
    python - <<'PYEOF'
import re
p = __import__('os').environ['GOWA_RAIZ_PY'] + r'\port\spu\gowajm_0072DF80\spu_recomp.c'
s = open(p, encoding='latin-1', newline='').read()
m = re.search(r'/\* spu_cc_interno\.py[^\r\n]*\r?\n', s)
nl = '\r\n' if m.group(0).endswith('\r\n') else '\n'
s = s.replace(m.group(0), m.group(0) + 'void gowa_slow_jobmod(unsigned ea);   /* gowa_offline.cpp */' + nl
              + 'void gowa_job_inicio(void* ctx);       /* gowa_offline.cpp, GOWA_JOBDMA */' + nl, 1)
b = 'static SPU_PN void gowajm_0072DF80_spu_func_0000AD98__b(spu_context* ctx) {' + nl
assert s.count(b) == 1
s = s.replace(b, b + '        gowa_slow_jobmod(0x0072DF80u);   /* GOWA_SLOW_JOBMOD (experimento) */' + nl
              + '        gowa_job_inicio(ctx);   /* GOWA_JOBDMA (mapa de E/S do job) */' + nl, 1)
open(p, 'w', encoding='latin-1', newline='').write(s)
PYEOF
fi
python scripts/flush_preguicoso.py $D --aplicar
# 4) núcleos vetoriais (os laços que ganham, medidos laço a laço em 21/10)
if [ "$QUAL" = "72DF80" ]; then
    for p in 73E0:7BB4 73E0:7ED8 F538:F620 A880:A950 D250:D4DC E550:EC08 E550:EE28 E550:F24C E550:E804 DCC0:E210 E550:EA7C E550:F034 E550:F104 DCC0:DFF0; do
        python scripts/vetorizar_laco.py $D ${p%%:*} ${p##*:} --fonte $D/spu_recomp.c.sem_flushpreg --gancho > /dev/null
    done
    echo "ganchos vk: $(grep -c '/\*VK ' $D/spu_recomp.c)"
fi
# 5) ninho da 73E0 em AVX-512 (21/10; vetorizar_ninho.py lê o .sem_flushpreg)
if [ "$QUAL" = "72DF80" ]; then
    python scripts/vetorizar_ninho.py $D 73E0 79B0 81C0 --gancho
fi
# 6) por último: saltos diretos enxutos (SPU_TAILX) em toda transferência estática (21/10)
python scripts/encadeia.py $D todas
python scripts/encadeia.py $D todas --spill   # 22/10 02:01: também as transferências com SPILL
echo "pronto: $MOD (compilar com scripts/gowa_ninja.cmd)"
