#!/bin/bash
# Fotos (LS + registradores na entrada e na saída) das primeiras chamadas de um plugin do EDGE nas gravações da
# conferência: aplica a sonda (sonda_fotos.py), compila o objeto e o gowa_dev, roda confere e confere_f1 e tira a
# sonda de novo (o código gerado volta ao que era; recompila). Fotos em out/pl<ID>/<gravação>/.
# uso: bash fotos_plugin.sh <ID, ex. 00751450> <função de entrada> <função do laço com o SPU_RET> [N fotos]
set -e
ID=$1; ENT=$2; LACO=$3; N=${4:-12}
G=/c/Users/otavi/gow-edge
REP=/c/Users/otavi/.claude/jobs/4d9c3f8a/tmp/rep
OBJ="$(cygpath -w /c/Users/otavi/AppData/Local/Temp/claude/C--Users-otavi/3ef7356c-f80d-4a76-86ce-c34e8213ad8d/scratchpad/obj.cmd)"
F=$G/port/spu/gowapl_$ID/spu_recomp.c
cp "$F" "$F.antes_sonda"
python -I $G/scripts_edge/sonda_fotos.py gowapl_$ID $ENT $LACO
cmd //c "$OBJ CMakeFiles/gowa.dir/spu/gowapl_$ID/spu_recomp.c.obj" | grep -E " error|exit=" || true
cmd //c "$(cygpath -w $REP/dev.cmd)" | grep " error" || true
for p in confere confere_f1; do
    mkdir -p $G/out/pl$ID/$p; rm -f $G/out/pl$ID/$p/*
    (cd $REP && GOWA_PL_FOTO=$N GOWA_PL_FOTO_DIR="$(cygpath -w $G/out/pl$ID/$p)" bash rodar.sh gowa_dev.exe $p 1 1 | grep -o "[0-9]* iguais" | head -1)
    echo "$p: $(ls $G/out/pl$ID/$p | wc -l) fotos"
done
mv -f "$F.antes_sonda" "$F"
touch "$F"
cmd //c "$OBJ CMakeFiles/gowa.dir/spu/gowapl_$ID/spu_recomp.c.obj" | grep -E " error|exit=" || true
cmd //c "$(cygpath -w $REP/dev.cmd)" | grep " error" || true
grep -c "pl_foto" "$F" || true
