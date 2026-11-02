#!/bin/bash
# Bancada sintética dos plugins do EDGE (02/11): o plugin traduzido SEM ganchos x o nativo sobre as mesmas LS.
# Prepara as cópias sem ganchos (port/spu/gowapl_*/limpo_banc.c, fora do CMake e do git) e as imagens dos plugins.
# Depois, dentro de scripts_edge/banc: cmd //c "$(cygpath -w $PWD/cc.cmd) banc_<ID>" (portátil, MXCSR em DAZ + RZ)
# ou cc_nat.cmd (-march=native, só DAZ; gera banc_<ID>_nat.exe), e ./banc_<ID>.exe <casos>. BANC_V=1 lista todas as
# diferenças.
set -e
G=$(cd "$(dirname "$0")/../.." && pwd)
cd "$G"
python -I scripts_edge/edge_nativo.py tirar >/dev/null
for d in port/spu/gowapl_*; do cp "$d/spu_recomp.c" "$d/limpo_banc.c"; done
python -I scripts_edge/edge_nativo.py aplicar >/dev/null
cd scripts_edge/banc
for f in banc_*.c; do id=${f#banc_}; id=${id%.c}; python -I img.py "$id" 1000 "pl_$id.bin" >/dev/null; done
echo "pronto: $(ls pl_*.bin | wc -l) imagens"
