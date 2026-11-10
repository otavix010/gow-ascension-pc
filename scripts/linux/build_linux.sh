#!/bin/bash
# Otavix: build portátil do gowa.exe no Linux (equivalente ao scripts/build_portatil.cmd).
# O exe é de Windows e roda pelo Proton/Wine. Uso: scripts/linux/build_linux.sh [march] [rz]
# Ferramentas em GOWA_FERR (padrão ~/.local/share/otavix/ferr): llvm/, cmake/, ninja, winsdk/, libextra/.
set -u
RAIZ=$(cd "$(dirname "$0")/../.." && pwd)
F=${GOWA_FERR:-$HOME/.local/share/otavix/ferr}
export GOWA_FERR=$F
# o lld-link/llvm-mt do LLVM oficial querem libxml2.so.2 (ICU 67), que distros novas (SteamOS) não têm
export LD_LIBRARY_PATH="$F/libextra${LD_LIBRARY_PATH:+:$LD_LIBRARY_PATH}"
MARCH=${1:-x86-64-v3}
DIR="$RAIZ/port/build_v3"
[ "$MARCH" != "x86-64-v3" ] && DIR="$RAIZ/port/build_$MARCH"
RZ=OFF
if [ "${2:-}" = "rz" ]; then RZ=ON; DIR="${DIR}rz"; fi
PGO_MODO=-DGOWA_PGO=; PGO_PERFIL=-DGOWA_PGO_PROFILE=
if [ -f "$RAIZ/port/pgo/gowa.profdata" ]; then
    PGO_MODO=-DGOWA_PGO=use; PGO_PERFIL="-DGOWA_PGO_PROFILE=$RAIZ/port/pgo/gowa.profdata"
fi
FFMPEG_DIR=${FFMPEG_DIR:-$RAIZ/dl/ffmpeg9/ffmpeg-n9.0-latest-win64-lgpl-shared-9.0}
export FFMPEG_DIR
PY=${GOWA_PY_EXE:-$(command -v python3)}
for x in "$F/llvm/bin/clang-cl" "$F/llvm/bin/lld-link" "$F/cmake/bin/cmake" "$F/ninja" "$F/winsdk/crt/include"; do
    [ -e "$x" ] || { echo "GOWA_ERRO_FERR: falta $x"; echo "exit=3"; exit 3; }
done
if [ ! -f "$DIR/build.ninja" ]; then
    "$F/cmake/bin/cmake" -S "$RAIZ/port" -B "$DIR" -G Ninja \
      "-DCMAKE_MAKE_PROGRAM=$F/ninja" \
      "-DCMAKE_TOOLCHAIN_FILE=$RAIZ/scripts/linux/windows-xwin.cmake" \
      -DCMAKE_BUILD_TYPE=Release \
      "$PGO_MODO" "$PGO_PERFIL" "-DPython3_EXECUTABLE=$PY" \
      -DGOWA_PPU_MARCH=x86-64-v3 -DGOWA_SPU_X86_V3=ON "-DGOWA_SPU_MARCH=$MARCH" -DGOWA_HOST_FMA=ON \
      -DGOWA_RUNTIME_FAST=ON -DGOWA_SPU_DAZ=ON "-DGOWA_SPU_RZ=$RZ" \
      "-DENGINE_PACOTE_DIR=$RAIZ/engine" || { echo "exit=1"; exit 1; }
fi
cd "$DIR" || exit 1
"$F/ninja" ${GOWA_JOBS:+-j $GOWA_JOBS} gowa.exe
e=$?
echo "exit=$e"
exit $e
