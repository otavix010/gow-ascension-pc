#!/bin/bash
# Otavix: gerador do God of War Ascension no Linux (SteamOS e outras distros x86-64). Faz o mesmo que o gerar.ps1:
# o gowa.exe sai igual ao do Windows e roda pelo Proton. Os programas da engine (.exe) rodam pelo wine do Proton;
# os scripts, no python3 do sistema (num venv com o pycryptodome); a compilação, no clang-cl/lld-link com o SDK
# do Windows baixado pelo xwin.
#
# Uso: scripts/linux/gerar.sh --disco <pasta com PS3_GAME | .iso | .7z | .zip | .rar> [--update <.pkg>]
#                             [--firmware <PS3UPDAT.PUP>] [--jobs N] [--de <etapa>]
# Ferramentas em GOWA_FERR (padrão ~/.local/share/otavix/ferr; baixadas na primeira vez). O wine: OTX_WINE, senão o
# Proton mais novo da Steam (compatibilitytools.d ou steamapps/common/Proton*), senão o wine do sistema.
set -u -o pipefail
R=$(cd "$(dirname "$0")/../.." && pwd)
OUT=$R/out/gerar_linux; mkdir -p "$OUT" "$R/dl"
LOG=$OUT/gerar.log
DISCO=''; UPDATE=''; FIRMWARE=''; JOBS=$(nproc); DE=''
while [ $# -gt 0 ]; do
    case "$1" in
        --disco) DISCO=$2; shift 2 ;;
        --update) UPDATE=$2; shift 2 ;;
        --firmware) FIRMWARE=$2; shift 2 ;;
        --jobs) JOBS=$2; shift 2 ;;
        --de) DE=$2; shift 2 ;;
        *) echo "opção desconhecida: $1"; exit 2 ;;
    esac
done

# as mesmas constantes do gerar.ps1
SHA_GOWA=813723ac4e091d79ea20ad2500b2e86cb0fd25f3736541431921273e7a77246d
SHA_LIBSRE=9e7ee6edc99b1d372686cf927b883b18f19ed4d32d1faf35e214cca4ae0737b6
UPDATE_URL='http://b0.ww.np.dl.playstation.net/tppkg/np/BCUS98232/BCUS98232_T76/5525fbb7b6b5456d/UP9000-BCUS98232_00-GOWABDPATCH00001-A0104-V0100-PE.pkg'
PUP_LISTA='http://fus01.ps3.update.playstation.net/update/ps3/list/us/ps3-updatelist.txt'
SERVIDOR='https://otavix-launcher.otavix-launcher-servidor.workers.dev/v1/arquivos'
ENGINE_VERSAO=$(sed -n "s/^\$ENGINE_VERSAO = '\(.*\)'.*/\1/p" "$R/gerar.ps1")
ENGINE_SHA=$(sed -n "s/^\$ENGINE_SHA = '\(.*\)'.*/\1/p" "$R/gerar.ps1")
UPDATE_SHA1=$(sed -n "s/^\$UPDATE_SHA1 = '\(.*\)'.*/\1/p" "$R/gerar.ps1")
FFMPEG_NOME=$(sed -n "s/^\$FFMPEG_NOME = '\(.*\)'.*/\1/p" "$R/gerar.ps1")
FFMPEG_SHA=$(sed -n "s/^\$FFMPEG_ESPELHO_SHA = '\(.*\)'.*/\1/p" "$R/gerar.ps1")
ENGINE_NOME=otavix-engine-ps3-$ENGINE_VERSAO

msg() { echo "[gerar] $*" | tee -a "$LOG"; }
falha() { echo "[gerar] ERRO: $*" | tee -a "$LOG"; exit 1; }
ex() { echo "> $*" >> "$LOG"; "$@" >> "$LOG" 2>&1 || falha "$(basename "$1") falhou (veja $LOG)"; }
baixar() {   # url destino: retoma quedas (como o Baixar do gerar.ps1)
    local i
    for i in 1 2 3 4; do
        curl -L --fail --retry 5 --retry-all-errors --retry-delay 3 -C - -o "$2" "$1" >> "$LOG" 2>&1 && return 0
        [ $? -eq 33 ] && rm -f "$2"
        msg "o download caiu; tentando de novo ($i)"
    done
    return 1
}
confere() { [ -f "$2" ] && [ "$(sha256sum "$2" | cut -d' ' -f1)" = "$1" ]; }
etapa() {   # nome função: pula as etapas feitas (marcadas), a não ser a partir de --de
    if [ -n "$DE" ] && [ "$1" \> "$DE" -o "$1" = "$DE" ]; then rm -f "$OUT/$1.ok"; fi
    if [ -f "$OUT/$1.ok" ]; then msg "$1: já feita"; return; fi
    msg "$1"; "$2"; touch "$OUT/$1.ok"
}

# ---------------------------------------------------------------------------- pré-requisitos
# o que o gerador usa do sistema (o resto ele baixa): falha logo no começo, com a lista do que falta
prerequisitos() {
    local falta='' c
    for c in curl python3 tar xz sha256sum sha1sum find; do command -v "$c" >/dev/null 2>&1 || falta="$falta $c"; done
    python3 -c 'import venv, ensurepip' >/dev/null 2>&1 || falta="$falta python3-venv"
    [ "$(uname -m)" = x86_64 ] || falha "o port precisa de um PC x86-64 (este é $(uname -m))"
    grep -qw avx2 /proc/cpuinfo || falha 'o processador não tem AVX2 (o port precisa dele; os Intel desde 2013 e os AMD desde 2015 têm)'
    [ -z "$falta" ] || falha "faltam programas do sistema:$falta (no SteamOS já vêm; em outras distros, instale pelo gerenciador de pacotes)"
    if [ -n "$DISCO" ] && [ -f "$DISCO" ] && ! command -v bsdtar >/dev/null 2>&1; then
        falha 'para abrir o arquivo do jogo (.iso/.7z/.zip/.rar) é preciso o bsdtar (pacote libarchive); ou extraia o arquivo e escolha a pasta'
    fi
    local livre; livre=$(df -Pk "$R" | awk 'NR==2{print int($4/1048576)}')
    [ "${livre:-99}" -ge 15 ] || falha "pouco espaço em $(df -P "$R" | awk 'NR==2{print $6}'): ${livre} GB livres, e a geração precisa de uns 15 GB"
}
prerequisitos

# ---------------------------------------------------------------------------- ferramentas
F=${GOWA_FERR:-$HOME/.local/share/otavix/ferr}
export GOWA_FERR=$F
achar_wine() {
    [ -n "${OTX_WINE:-}" ] && [ -x "$OTX_WINE" ] && { echo "$OTX_WINE"; return; }
    local w
    for w in $(ls -dt "$HOME"/.local/share/Steam/compatibilitytools.d/*/files/bin/wine \
                      "$HOME"/.steam/steam/compatibilitytools.d/*/files/bin/wine \
                      "$HOME"/.local/share/Steam/steamapps/common/Proton*/files/bin/wine \
                      "$HOME"/.steam/steam/steamapps/common/Proton*/files/bin/wine 2>/dev/null); do
        [ -x "$w" ] && { echo "$w"; return; }
    done
    command -v wine
}
ferramentas() {
    mkdir -p "$F"; cd "$F" || falha "não deu para criar $F"
    [ -x llvm/bin/clang-cl ] || { msg 'baixando o LLVM 19.1.5 (1,5 GB)'; baixar https://github.com/llvm/llvm-project/releases/download/llvmorg-19.1.5/LLVM-19.1.5-Linux-X64.tar.xz llvm.tar.xz && mkdir -p llvm && tar -C llvm --strip-components=1 -xf llvm.tar.xz && rm llvm.tar.xz || falha 'o LLVM não baixou'; }
    [ -x cmake/bin/cmake ] || { msg 'baixando o CMake'; baixar https://github.com/Kitware/CMake/releases/download/v3.31.6/cmake-3.31.6-linux-x86_64.tar.gz cmake.tgz && mkdir -p cmake && tar -C cmake --strip-components=1 -xzf cmake.tgz && rm cmake.tgz || falha 'o CMake não baixou'; }
    [ -x ninja ] || { msg 'baixando o Ninja'; baixar https://github.com/ninja-build/ninja/releases/download/v1.12.1/ninja-linux.zip ninja.zip && python3 -c "import zipfile;zipfile.ZipFile('ninja.zip').extractall('.')" && chmod +x ninja && rm ninja.zip || falha 'o Ninja não baixou'; }
    [ -x xwin ] || { msg 'baixando o xwin'; baixar https://github.com/Jake-Shadle/xwin/releases/download/0.6.5/xwin-0.6.5-x86_64-unknown-linux-musl.tar.gz xwin.tgz && tar --strip-components=1 -xzf xwin.tgz && rm xwin.tgz || falha 'o xwin não baixou'; }
    [ -d winsdk/crt/include ] || { msg 'baixando o CRT e o Windows SDK da Microsoft (licença da Microsoft aceita, como no Visual Studio)'; ./xwin --accept-license --arch x86_64 splat --output winsdk >> "$LOG" 2>&1 || falha 'o xwin não baixou o SDK'; }
    # o lld-link/llvm-mt querem libxml2.so.2 (ICU 67), que distros novas não têm: a cópia do runtime da Steam
    mkdir -p libextra
    if ! LD_LIBRARY_PATH=libextra ldd llvm/bin/lld >/dev/null 2>&1 || LD_LIBRARY_PATH=libextra ldd llvm/bin/lld | grep -q 'not found'; then
        local rt
        rt=$(ls -d "$HOME"/.local/share/Steam/steamrt64/steam-runtime-steamrt/steamrt3c_platform_*/files/lib/x86_64-linux-gnu \
                   "$HOME"/.local/share/umu/steamrt3/sniper_platform_*/files/lib/x86_64-linux-gnu \
                   "$HOME"/.local/share/Steam/steamapps/common/SteamLinuxRuntime_sniper/sniper_platform_*/files/lib/x86_64-linux-gnu 2>/dev/null | head -1)
        for l in libxml2.so.2 libicuuc.so.67 libicudata.so.67; do
            [ -e libextra/$l ] && continue
            local s; s=$(ls "$rt"/$l* /usr/lib/$l* /usr/lib64/$l* /usr/lib/x86_64-linux-gnu/$l* 2>/dev/null | head -1)
            [ -n "$s" ] && cp -L "$s" libextra/$l
        done
        LD_LIBRARY_PATH=libextra ldd llvm/bin/lld | grep -q 'not found' && falha "o lld-link precisa de: $(LD_LIBRARY_PATH=libextra ldd llvm/bin/lld | grep 'not found' | awk '{print $1}' | tr '\n' ' ')(instale o libxml2 2.9 da sua distro)"
    fi
    [ -x venv/bin/python ] || python3 -m venv venv || falha 'python3 -m venv falhou (instale o python3 com venv)'
    venv/bin/python -c 'import Crypto' 2>/dev/null || venv/bin/pip install -q pycryptodome >> "$LOG" 2>&1 || falha 'o pycryptodome não instalou'
    cd "$R"
}
WINE=$(achar_wine)
[ -n "$WINE" ] || falha 'nenhum Proton achado: na Steam, instale o "Proton Experimental" ou o GE-Proton (ou defina OTX_WINE com o caminho de um wine)'
etapa 1_ferramentas ferramentas
PY=$F/venv/bin/python
export OTX_WINE=$WINE WINEDEBUG=-all WINEPREFIX=${GOWA_WINEPREFIX:-$R/out/wineprefix}
mkdir -p "$WINEPREFIX"
msg "wine: $WINE"
# um .exe da engine pelo wine (os caminhos /home/... chegam como Z:\home\...); uma falha para o gerador
eng() { local p=$1; shift; ( cd "$R/engine" && ex "$WINE" "bin/$p" "$@" ) || exit 1; }
# um zip feito no Windows pode ter os nomes com \ (o pacote da engine): vira pasta; nada sai da pasta de destino
extrair_zip() {
    python3 - "$1" "$2" <<'EOF'
import os, sys, zipfile
dest = os.path.realpath(sys.argv[2])
with zipfile.ZipFile(sys.argv[1]) as z:
    for i in z.infolist():
        nome = i.filename.replace('\\', '/')
        alvo = os.path.realpath(os.path.join(dest, nome))
        if not (alvo == dest or alvo.startswith(dest + os.sep)):
            sys.exit(f'caminho fora da pasta no zip: {i.filename}')
        if nome.endswith('/'):
            os.makedirs(alvo, exist_ok=True); continue
        os.makedirs(os.path.dirname(alvo), exist_ok=True)
        with z.open(i) as f, open(alvo, 'wb') as g:
            g.write(f.read())
EOF
}

# ---------------------------------------------------------------------------- engine e FFmpeg
engine_ffmpeg() {
    local zip=$R/dl/$ENGINE_NOME.zip
    if ! confere "$ENGINE_SHA" "$zip"; then
        msg "baixando a Otavix Engine PS3 $ENGINE_VERSAO"
        baixar "$SERVIDOR/engine/ps3/$ENGINE_NOME.zip" "$zip" || falha 'a engine não baixou'
        confere "$ENGINE_SHA" "$zip" || { rm -f "$zip"; falha 'a engine baixada não confere (sha256)'; }
    fi
    rm -rf "$R/engine.novo"; mkdir -p "$R/engine.novo"
    extrair_zip "$zip" "$R/engine.novo" || falha 'o zip da engine não abriu'
    rm -rf "$R/engine"; mv "$R/engine.novo" "$R/engine"
    local ff=$R/dl/ffmpeg9/$FFMPEG_NOME
    if [ ! -d "$ff/include/libavcodec" ]; then
        msg 'baixando o FFmpeg 9 (LGPL)'
        baixar "$SERVIDOR/terceiros/ffmpeg/$FFMPEG_NOME.zip" "$R/dl/ffmpeg.zip" || falha 'o FFmpeg não baixou'
        confere "$FFMPEG_SHA" "$R/dl/ffmpeg.zip" || { rm -f "$R/dl/ffmpeg.zip"; falha 'o FFmpeg baixado não confere'; }
        mkdir -p "$R/dl/ffmpeg9"
        extrair_zip "$R/dl/ffmpeg.zip" "$R/dl/ffmpeg9" && rm -f "$R/dl/ffmpeg.zip"
    fi
}
etapa 1_engine engine_ffmpeg

# ---------------------------------------------------------------------------- disco, update e o executável do jogo
# a pasta do disco (a que tem PS3_GAME/USRDIR) na escolhida, nela mesma (PS3_GAME ou USRDIR escolhidos) ou até 4 níveis abaixo
achar_ps3game() {
    local r=${1%/} b p
    [ -d "$r" ] || return 1
    [ -d "$r/PS3_GAME/USRDIR" ] && { echo "$r"; return 0; }
    b=$(basename "$r")
    [ "${b^^}" = PS3_GAME ] && [ -d "$r/USRDIR" ] && { dirname "$r"; return 0; }
    [ "${b^^}" = USRDIR ] && [ "$(basename "$(dirname "$r")")" = PS3_GAME ] && { dirname "$(dirname "$r")"; return 0; }
    # vários discos (uma pasta de ROMs com outros jogos): o do God of War: Ascension primeiro, pelo PARAM.SFO
    local todos; todos=$(find "$r" -mindepth 1 -maxdepth 4 -type d -name PS3_GAME 2>/dev/null | while read -r x; do [ -d "$x/USRDIR" ] && echo "$x"; done)
    p=$(echo "$todos" | while read -r x; do [ -n "$x" ] && grep -aq 'BCUS98232' "$x/PARAM.SFO" 2>/dev/null && { echo "$x"; break; }; done)
    [ -z "$p" ] && p=$(echo "$todos" | head -1)
    [ -n "$p" ] && { dirname "$p"; return 0; }
    return 1
}
# o arquivo do disco (.iso/.7z/.zip/.rar, > 100 MB) na pasta ou até $2 níveis abaixo: um só, ou com vários a única .iso;
# as partes 2+ de um .rar dividido não contam
arquivo_do_disco() {
    local l; l=$(find "$1" -mindepth 1 -maxdepth $((${2:-3} + 1)) -type f -size +100M \( -iname '*.iso' -o -iname '*.7z' -o -iname '*.zip' -o -iname '*.rar' \) 2>/dev/null |
                 grep -viE '\.part0*([2-9]|[1-9][0-9]+)\.rar$' | grep -v "^$R/")
    [ -z "$l" ] && return 1
    [ "$(echo "$l" | wc -l)" -eq 1 ] && { echo "$l"; return 0; }
    local iso; iso=$(echo "$l" | grep -iE '\.iso$')
    [ -n "$iso" ] && [ "$(echo "$iso" | wc -l)" -eq 1 ] && { echo "$iso"; return 0; }
    return 1
}
achar_disco() {
    local d=${1%/} a pai
    if [ -f "$d" ]; then   # um arquivo: extrai uma vez em disco/ (bsdtar abre iso, 7z, zip e rar)
        local x=$R/disco
        if ! achar_ps3game "$x" >/dev/null; then
            command -v bsdtar >/dev/null || falha 'para abrir o arquivo do jogo é preciso o bsdtar (pacote libarchive); ou extraia o arquivo e escolha a pasta'
            msg "extraindo $d (uns 35 GB; leva alguns minutos)"; rm -rf "$x"; mkdir -p "$x"
            if ! bsdtar -xf "$d" -C "$x" >> "$LOG" 2>&1; then
                case "$d" in *.part1.rar|*.part01.rar|*.r00) falha "$d é a 1a parte de um arquivo dividido: extraia-o com o 7-Zip/unrar numa pasta e escolha a pasta" ;; esac
                falha "falhou a extração de $d: extraia o arquivo numa pasta e escolha a pasta"
            fi
            # um .7z/.zip com a .iso dentro: extrai a .iso também
            if ! achar_ps3game "$x" >/dev/null; then
                local iso; iso=$(find "$x" -type f -iname '*.iso' | head -1)
                [ -n "$iso" ] && { msg "extraindo $(basename "$iso")"; ex bsdtar -xf "$iso" -C "$x"; rm -f "$iso"; }
            fi
        fi
        ACHADO=$(achar_ps3game "$x") || falha "não achei PS3_GAME/USRDIR dentro de $d (uma .iso criptografada, como as da Redump, não serve)"
        return 0
    fi
    [ -d "$d" ] || falha "a pasta do jogo $d não existe (foi movida ou renomeada?)"
    ACHADO=$(achar_ps3game "$d") && return 0
    a=$(arquivo_do_disco "$d" 3) && { msg "usando o arquivo do jogo $a"; achar_disco "$a"; return 0; }
    pai=$(dirname "$d")
    # a pasta PS3_UPDATE do disco (a atualização do sistema do PS3) não é o jogo: o disco é a pasta de cima
    if [ "$(basename "$d")" = PS3_UPDATE ] && achar_ps3game "$pai" >/dev/null; then msg 'a pasta escolhida é a PS3_UPDATE do disco; usando o disco da pasta de cima'; ACHADO=$(achar_ps3game "$pai"); return 0; fi
    # só textos (o README que veio com o arquivo): o jogo ou o arquivo dele costuma estar na pasta de cima
    if [ -z "$(find "$d" -mindepth 1 -maxdepth 1 ! -iregex '.*\.\(txt\|nfo\|url\|diz\|sfv\|md5\|jpg\|png\|ini\)' | head -1)" ]; then
        achar_ps3game "$pai" >/dev/null && { msg 'a pasta escolhida só tem textos (README); usando o jogo da pasta de cima'; ACHADO=$(achar_ps3game "$pai"); return 0; }
        a=$(arquivo_do_disco "$pai" 0) && { msg "a pasta escolhida só tem textos (README); usando o arquivo $a"; achar_disco "$a"; return 0; }
    fi
    local partes; partes=$(find "$d" -maxdepth 4 -type f \( -iname '*.part1.rar' -o -iname '*.part01.rar' -o -iname '*.r00' \) | head -1)
    [ -n "$partes" ] && falha "lá dentro há um arquivo dividido em partes ($(basename "$partes")): extraia-o com o 7-Zip/unrar numa pasta e escolha essa pasta"
    falha "não achei o jogo (PS3_GAME com USRDIR) em $d (dentro dela há: $(ls -A "$d" 2>/dev/null | head -6 | tr '\n' ',' | sed 's/,$//; s/,/, /g')). Escolha o arquivo do jogo (.iso, .7z, .zip ou .rar) ou a pasta onde ele foi extraído."
}
jogo() {
    [ -n "$DISCO" ] || falha 'falta --disco (a pasta com PS3_GAME, ou o .iso/.7z/.zip/.rar do jogo)'
    ACHADO=''; achar_disco "$DISCO"; local d=$ACHADO
    [ -d "$d/PS3_GAME/USRDIR" ] || falha "não achei PS3_GAME/USRDIR em $d"
    # o port é da versão americana (BCUS98232): a europeia (BCES01741) e as outras não servem
    local id; id=$(grep -aoE 'B[CL][EUAJ][SM][0-9]{5}' "$d/PS3_GAME/PARAM.SFO" 2>/dev/null | head -1)
    case "$id" in
        ''|BCUS98232) ;;
        B?ES*) falha "este disco é a versão europeia do God of War: Ascension ($id). O port só funciona com a versão americana (BCUS98232)." ;;
        *) falha "este disco é de outra versão do God of War: Ascension ($id). O port só funciona com a versão americana (BCUS98232)." ;;
    esac
    msg "disco: $d"
    mkdir -p "$R/vfs"
    rm -f "$R/vfs/PS3_GAME"; [ -e "$R/vfs/PS3_GAME" ] && mv "$R/vfs/PS3_GAME" "$R/vfs/PS3_GAME.antigo-$(date +%Y%m%d%H%M%S)"
    ln -s "$(cd "$d/PS3_GAME" && pwd)" "$R/vfs/PS3_GAME"   # o vfs do jogo vê o disco sem copiar 34 GB
    local pkg=$UPDATE
    if [ -z "$pkg" ]; then
        pkg=$R/dl/BCUS98232_update_1.04.pkg
        if [ "$(sha1sum "$pkg" 2>/dev/null | cut -d' ' -f1)" != "$UPDATE_SHA1" ]; then
            msg 'baixando o update 1.04 oficial do servidor da PlayStation (235 MB)'
            baixar "$UPDATE_URL" "$pkg" || falha "o update 1.04 não baixou; baixe pelo navegador ($UPDATE_URL) e passe --update <arquivo>"
            [ "$(sha1sum "$pkg" | cut -d' ' -f1)" = "$UPDATE_SHA1" ] || { rm -f "$pkg"; falha 'o update baixado não confere (SHA-1)'; }
        fi
    fi
    local t=$R/out/update_pkg; rm -rf "$t"
    eng pkg.exe "$pkg" "$t"
    local upd; upd=$(dirname "$(find "$t" -name PARAM.SFO | head -1)")
    [ -f "$upd/USRDIR/GOWA.SELF" ] || falha "não achei USRDIR/GOWA.SELF no update ($upd)"
    mkdir -p "$R/vfs/game/BCUS98232"; cp -r "$upd/." "$R/vfs/game/BCUS98232/"
    mkdir -p "$R/jogo"; cp "$upd/USRDIR/EBOOT.BIN" "$upd/USRDIR/GOWA.SELF" "$R/jogo/"
    local ch=$R/dl/rpcs3_chaves
    ex "$PY" "$R/scripts/gpl/unself.py" "$R/jogo/EBOOT.BIN" "$R/jogo/EBOOT.elf" --vault "$ch" --baixar
    ex "$PY" "$R/scripts/gpl/unself.py" "$R/jogo/GOWA.SELF" "$R/jogo/GOWA.elf" --vault "$ch" --klic-de "$R/jogo/EBOOT.elf"
    confere "$SHA_GOWA" "$R/jogo/GOWA.elf" || falha 'GOWA.elf diferente do esperado: o port só serve para BCUS98232 com o update 1.04'
}
etapa 2_jogo jogo

# ---------------------------------------------------------------------------- firmware (libsre por LLE)
firmware() {
    local fw=$R/fw lle=$R/fw/lle ch=$R/dl/rpcs3_chaves pup=$FIRMWARE
    mkdir -p "$lle"
    if [ -z "$pup" ]; then
        pup=$R/dl/PS3UPDAT.PUP
        if [ ! -f "$pup" ]; then
            baixar "$PUP_LISTA" "$R/dl/ps3-updatelist.txt" || falha 'a lista de atualização do PS3 não baixou'
            local url; url=$(grep -o 'CDN=[^;[:space:]]*/PS3UPDAT\.PUP' "$R/dl/ps3-updatelist.txt" | head -1 | cut -d= -f2)
            [ -n "$url" ] || falha 'a lista de atualização do PS3 não trouxe o PS3UPDAT.PUP'
            msg "baixando o firmware oficial do PS3 (~200 MB)"; baixar "$url" "$pup" || falha 'o firmware não baixou'
        fi
    fi
    ex "$PY" "$R/scripts/gpl/pup_libsre.py" "$pup" "$fw/libsre.sprx" --vault "$ch" --baixar
    ex "$PY" "$R/scripts/gpl/unself.py" "$fw/libsre.sprx" "$fw/libsre.prx" --vault "$ch" --baixar
    confere "$SHA_LIBSRE" "$fw/libsre.prx" || falha 'libsre.prx diferente do esperado'
    eng ppu_prx.exe "$fw/libsre.prx" --base 0x30000000 --output "$lle"
    eng ppu_prx.exe "$fw/libsre.prx" --symbol libsre --out "$lle/libsre_exports.c" --json "$lle/libsre_exports_c.json"
    ex "$PY" "$R/scripts/libsre_stubs_json.py" "$lle/libsre_imports.json" "$lle/libsre_hle_stubs.json"
    eng ppu_traduz.exe "$lle/libsre_image.bin" --raw --base 0x30000000 --toc 0x30039AB0 --code-end 0x3001DB18 --symbol-prefix libsre_ --functions "$lle/libsre_functions.json" --hle-stubs "$lle/libsre_hle_stubs.json" -o "$lle/recomp"
    mkdir -p "$R/port/lle_libsre"
    cp "$lle/recomp/ppu_recomp_000.cpp" "$R/port/lle_libsre/libsre_recomp.cpp"
    cp "$lle/recomp/ppu_recomp.h" "$lle/libsre_exports.c" "$R/port/lle_libsre/"
}
etapa 3_firmware firmware

# ---------------------------------------------------------------------------- PPU
ppu() {
    local o=$R/out
    eng ppu_carrega.exe "$R/jogo/GOWA.elf" -o "$o"
    eng ppu_traduz.exe "$R/jogo/GOWA.elf" --functions "$o/GOWA.functions.json" --hle-stubs "$o/GOWA.imports.json" --code-end 0x661C14 --chunks 48 --output "$R/port/recompiled" -j "$JOBS"
    eng ppu_nids.exe --all --out "$R/port/recompiled/ppu_hle_nids.cpp"
    cd "$R"
    ex "$PY" scripts/ppu_preambulo.py
    ex "$PY" scripts/ppu_restrict.py aplicar
    ex "$PY" scripts/remendos_ppu.py aplicar
    ex "$PY" scripts/sonda.py por func_001C05A4 loc_0039C710 loc_00415914 func_00415120
    ex "$PY" scripts/gerar_imports_inc.py
}
etapa 4_ppu ppu

# ---------------------------------------------------------------------------- SPU
spu() {
    cd "$R"
    find port/spu -mindepth 1 -maxdepth 1 ! -name gowanat_tspm -exec rm -rf {} + 2>/dev/null
    local sw=$R/out/spu_workloads
    eng spu_workloads.exe --input "$R/jogo/GOWA.elf" --input "$R/fw/libsre.prx" --lifted "$sw" --out "$sw/spu_workloads.c" --constructor --allow-partial
    mkdir -p port/spu; cp -r "$sw/." port/spu/
    local s=scripts
    ex "$PY" $s/lift_gowa_jobmods.py
    ex "$PY" $s/lift_gowa_pm.py
    ex "$PY" $s/lift_gowa_tspm.py
    ex "$PY" $s/lift_gowa_syssrv.py
    ex "$PY" $s/lift_gowa_plugins.py "$R/dados/plugins_getcode_edge_cap16.txt"
    ex "$PY" $s/lift_gowa_plugins.py "$R/dados/plugins_getcode_w11.txt" --wids 11 --naofolha --inc "$R/port/gowa_plugins_w11.inc" --tabela g_gowa_plugin_fns_w11
    local hot=$R/port/gowa_jobmods_hot.inc
    printf '/* GERADO por gerar.sh + scripts/folhas_spu.py: funções folha seguras dos módulos de job\n * que ficam interpretados (GOWA_JOBLIFT_HOT). va, LS, bytes da função. */\nstatic const struct { uint32_t va, ls, len; } g_gowa_hot[] = {\n' > "$hot"
    ex "$PY" $s/folhas_spu.py "$R/port/spu/gowajm_006FC080/spu_recomp.c" gowajm_006FC080_ --inc 6FC080 "$hot"
    printf '};\n' >> "$hot"
    ex "$PY" $s/spu_num_remap.py aplicar     # obrigatório (GOWA_SPU_NUM_REMAP=3 no boot)
    ex "$PY" $s/guarda_avx512.py aplicar
    ex "$PY" $s/otimizar.py
}
etapa 5_spu spu

# ---------------------------------------------------------------------------- compilar (portátil: AVX2, para-zero)
compilar() {
    export FFMPEG_DIR=$R/dl/ffmpeg9/$FFMPEG_NOME GOWA_PY_EXE=$PY GOWA_JOBS=$JOBS
    ex bash "$R/scripts/linux/build_linux.sh" x86-64-v3 rz   # pelo bash: o zip do port pode chegar sem a permissão de execução
    [ -f "$R/port/build_v3rz/gowa.exe" ] || falha 'o gowa.exe não foi gerado (veja o log)'
}
etapa 6_compilar compilar
# o mesmo port/build.txt do gerar.ps1 (o launcher manda nos reports qual build o jogador usa): no Linux é sempre a
# portátil (AVX2), a que roda pelo Proton
printf 'escolhida=portatil\nusada=portatil\npasta=build_v3rz\nexe=port/build_v3rz/gowa.exe\nmotivo=Linux (%s, Proton %s): build portátil (AVX2)\n' \
    "$(. /etc/os-release 2>/dev/null; echo "${PRETTY_NAME:-Linux}")" "$(basename "$(dirname "$(dirname "$(dirname "$WINE")")")")" > "$R/port/build.txt"
msg "versão da build: portátil (AVX2), pelo Proton"

# ---------------------------------------------------------------------------- jogar (pelo Proton)
jogar() {
    local b=$R/port/build_v3rz
    cp "$R/dl/ffmpeg9/$FFMPEG_NOME/bin/"*.dll "$b/"
    # o compilador de shaders do wine (vkd3d) falha em alguns shaders do jogo; o da Microsoft (redistribuível) vai
    # junto da engine quando houver: engine/bin/d3dcompiler_47.dll
    [ -f "$R/engine/bin/d3dcompiler_47.dll" ] && cp "$R/engine/bin/d3dcompiler_47.dll" "$b/"
    cat > "$R/jogar.sh" <<EOF
#!/bin/bash
# Gerado pelo scripts/linux/gerar.sh: roda o gowa.exe pelo Proton/wine. Variáveis opcionais: GOWA_FLIP_HZ, GOWA_RES_SCALE.
# O prefixo é criado uma vez pelo script do Proton (que instala o vkd3d-proton e o DXVK); o jogo roda pelo wine
# dele com esse prefixo (pelo script do Proton a saída do jogo se perde).
R=\$(cd "\$(dirname "\$0")" && pwd)
W=\${OTX_WINE:-$WINE}
P=\$(dirname "\$(dirname "\$(dirname "\$W")")")/proton
z() { echo "Z:\$(echo "\$1" | tr / '\\\\')"; }
export STEAM_COMPAT_DATA_PATH=\${GOWA_COMPAT:-\$R/out/proton} STEAM_COMPAT_CLIENT_INSTALL_PATH=\${STEAM_COMPAT_CLIENT_INSTALL_PATH:-\$HOME/.local/share/Steam}
mkdir -p "\$STEAM_COMPAT_DATA_PATH"
if [ -x "\$P" ] && [ ! -d "\$STEAM_COMPAT_DATA_PATH/pfx/drive_c" ]; then "\$P" run cmd.exe /c exit >/dev/null 2>&1; fi
if [ -d "\$STEAM_COMPAT_DATA_PATH/pfx/drive_c" ]; then export WINEPREFIX=\$STEAM_COMPAT_DATA_PATH/pfx; else export WINEPREFIX=\$R/out/wineprefix; fi
export WINEDEBUG=-all WINEDLLOVERRIDES="d3d12,d3d12core,dxgi=n,b;d3dcompiler_47=n"
export PS3_VFS_ROOT=\$(z "\$R/vfs") PS3_LIBSRE_IMAGE=\$(z "\$R/fw/lle/libsre_image.bin")
export PS3_SAVE_ROOT=\${PS3_SAVE_ROOT:-\$(z "\$R/saves")} GOWA_CACHE_DIR=\${GOWA_CACHE_DIR:-\$(z "\$R/cache")}
mkdir -p "\$R/saves" "\$R/cache" "\$R/out"
# idioma: o wine informa ao jogo o idioma do LANG do processo, que muda conforme quem abre o jogo (modo jogo da Steam,
# launcher, terminal) -- e o jogo grava no save o idioma que recebeu. Sem PS3_IDIOMA escolhido (no ambiente ou no
# gowa_opcoes.txt do launcher; "auto" não conta), decide aqui: no SteamOS, o idioma da Steam; fora dele, o
# LC_ALL/LC_MESSAGES/LANG e, se vazios, o da Steam
idioma_de() { case "\$1" in pt*|brazilian|portuguese) echo pt-br ;; fr*|french) echo fr ;; es*|spanish|latam) echo es ;; en*|english) echo en ;; esac; }
idioma_steam() { sed -n 's/.*"language"[[:space:]]*"\([a-z]*\)".*/\1/p' "\$HOME/.steam/registry.vdf" 2>/dev/null | head -1; }
OP=("\$R/port/build_v3rz/gowa_opcoes.txt" "\$R/port/gowa_opcoes.txt" "\$R/gowa_opcoes.txt")
if { [ -z "\${PS3_IDIOMA:-}" ] || [ "\$PS3_IDIOMA" = auto ]; } && ! grep -hs '^PS3_IDIOMA=' "\${OP[@]}" | tr -d '\r' | grep -qvE '=(auto)?\$'; then
    L=\${LC_ALL:-\${LC_MESSAGES:-\${LANG:-}}}
    case "\$L" in C|C.*|POSIX|POSIX.*) L= ;; esac
    I=
    if grep -qs '^ID=steamos' /etc/os-release || [ -z "\$L" ]; then I=\$(idioma_de "\$(idioma_steam)"); fi
    [ -z "\$I" ] && I=\$(idioma_de "\$L")
    [ -n "\$I" ] && export PS3_IDIOMA=\$I
fi
cd "\$R/port/build_v3rz" && exec "\$W" gowa.exe "\$(z "\$R/jogo/GOWA.elf")" > "\$R/out/jogo.log" 2>&1
EOF
    chmod +x "$R/jogar.sh"
}
etapa 7_jogar jogar

# ---------------------------------------------------------------------------- shaders pré-compilados (opcional)
shaders() {
    local u=$SERVIDOR/shaders/otavix010/gow-ascension-pc cache=$R/cache/rsx_shader_cache_v2 marca=$R/cache/shaders_pacote.txt
    mkdir -p "$cache"
    baixar "$u/shaders.zip.sha256" "$R/dl/shaders.zip.sha256" || return 1
    local sha; sha=$(awk '{print tolower($1)}' "$R/dl/shaders.zip.sha256")
    [ -f "$marca" ] && [ "$(cat "$marca")" = "$sha" ] && { msg 'shaders pré-compilados: já estão no cache'; return 0; }
    baixar "$u/shaders.zip" "$R/dl/shaders.zip" && confere "$sha" "$R/dl/shaders.zip" || return 1
    python3 - "$R/dl/shaders.zip" "$cache" <<'EOF'
import re, sys, zipfile, os
z = zipfile.ZipFile(sys.argv[1]); n = 0
for e in z.namelist():
    m = re.match(r'^rsx_shader_cache_v2/([VP]_[0-9a-f]{16}_[0-9a-f]{8}\.bin)$', e.replace('\\', '/'), re.I)   # o -match do PowerShell ignora caixa
    if not m: continue
    d = os.path.join(sys.argv[2], m.group(1))
    if os.path.exists(d): continue
    open(d, 'wb').write(z.read(e)); n += 1
print(f'{n} novos')
EOF
    rm -f "$R/dl/shaders.zip"; echo "$sha" > "$marca"
}
shaders >> "$LOG" 2>&1 || msg 'shaders pré-compilados não instalados; o jogo compila cada um na primeira vez'
msg "pronto. Para jogar: $R/jogar.sh  (log em out/jogo.log)"
