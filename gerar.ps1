<#
.SYNOPSIS
  Gera e compila o God of War: Ascension a partir da SUA cópia do jogo.

.DESCRIPTION
  Faz tudo em ordem, e cada etapa pula se já foi feita (use -Refazer para refazer):
    1. confere as ferramentas (Visual Studio Build Tools com Clang, Python) e baixa o FFmpeg;
    2. prepara os arquivos do jogo: disco, update 1.04 e decriptação do executável;
    3. prepara a libsre a partir do firmware oficial;
    4. traduz o código da PPU;
    5. traduz o código das SPUs;
    6. compila (escolhe sozinho a build com ou sem AVX-512 para a sua CPU);
    7. deixa o jogar.cmd pronto.
  Os logs de cada etapa ficam em out\gerar\.

.PARAMETER Disco
  Pasta com a cópia do disco BCUS98232 (a que contém PS3_GAME).
.PARAMETER Update
  O update 1.04: o arquivo .pkg, ou a pasta BCUS98232 já instalada no RPCS3
  (RPCS3\dev_hdd0\game\BCUS98232). Sem ele, o gerador baixa o update oficial
  do servidor da PlayStation (gratuito, 235 MB) e confere o SHA-1.
.PARAMETER Firmware
  O arquivo PS3UPDAT.PUP do site da PlayStation, a pasta do RPCS3 com o firmware
  instalado (File > Install Firmware) ou o arquivo libsre.sprx. Sem ele, o gerador
  baixa o PS3UPDAT.PUP oficial do servidor de atualização do PS3 (gratuito, ~200 MB).
.PARAMETER ChavesRpcs3
  Pasta com key_vault.cpp e key_vault.h do código-fonte do RPCS3. Se não for dada,
  os dois arquivos são baixados do repositório do RPCS3 para dl\rpcs3_chaves.

.PARAMETER RefazerDe
  Refaz da etapa N em diante. Depois de atualizar o port, -RefazerDe 4 retraduz e
  recompila sem repetir o disco, o update e o firmware.
.PARAMETER SemOtimizar
  Pula os passes de desempenho do código SPU (scripts\otimizar.py).
.EXAMPLE
  gerar.cmd     (clique duas vezes: pede as pastas em janelas)
.EXAMPLE
  .\gerar.ps1 -Disco D:\GOWA -Update C:\RPCS3\dev_hdd0\game\BCUS98232 -Firmware C:\RPCS3
.EXAMPLE
  .\gerar.ps1 -Disco D:\GOWA     (baixa o update 1.04 e o firmware oficiais sozinho)
#>
param(
    [string]$Disco = '',
    [string]$Update = '',
    [string]$Firmware = '',
    [string]$ChavesRpcs3 = '',
    [ValidateSet('auto', 'avx512', 'portatil')][string]$Build = 'auto',
    [int]$Jobs = 0,
    [switch]$SemOtimizar,
    [int]$RefazerDe = 0,
    [switch]$Refazer
)
$ErrorActionPreference = 'Stop'
$R = $PSScriptRoot
if ($R -match ' ') { throw "Coloque o projeto numa pasta sem espaços (agora: $R)" }
if (-not $Jobs) { $Jobs = [Environment]::ProcessorCount }
$SHA_GOWA = '813723ac4e091d79ea20ad2500b2e86cb0fd25f3736541431921273e7a77246d'
$FFMPEG_NOME = 'ffmpeg-n9.0-latest-win64-lgpl-shared-9.0'
$FFMPEG_URL = "https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/$FFMPEG_NOME.zip"
# Arquivos gratuitos da Sony, baixados dos servidores oficiais dela quando o jogador não
# indica os seus (nada deles vem neste repositório)
$UPDATE_URL = 'http://b0.ww.np.dl.playstation.net/tppkg/np/BCUS98232/BCUS98232_T76/5525fbb7b6b5456d/UP9000-BCUS98232_00-GOWABDPATCH00001-A0104-V0100-PE.pkg'
$UPDATE_SHA1 = '5a94c17aa07cb5181a505d3429c88ea3d9f4e4d0'   # do arquivo inteiro
$PUP_LISTA = 'http://fus01.ps3.update.playstation.net/update/ps3/list/us/ps3-updatelist.txt'
# libsre.prx decriptado: o mesmo nos firmwares 4.x até o 4.93; a etapa 3 usa endereços fixos dele
$SHA_LIBSRE = '9e7ee6edc99b1d372686cf927b883b18f19ed4d32d1faf35e214cca4ae0737b6'
$OUT = Join-Path $R 'out\gerar'
New-Item -ItemType Directory -Force $OUT | Out-Null
# -RefazerDe N: refaz da etapa N em diante (ex.: 4 depois de atualizar o port; o jogo e o
# firmware preparados nas etapas 2 e 3 continuam valendo)
if ($RefazerDe -gt 0) {
    Get-ChildItem $OUT -Filter '*.ok' | Where-Object { [int]($_.Name.Split('_')[0]) -ge $RefazerDe } | Remove-Item -Force
}

function Msg($t) { Write-Host "[gerar] $t" -ForegroundColor Cyan }

# ---------------------------------------------------------------------------- entradas (janelas se faltarem)
function Escolher-Pasta([string]$texto) {
    try {
        Add-Type -AssemblyName System.Windows.Forms
        $d = New-Object System.Windows.Forms.FolderBrowserDialog
        $d.Description = $texto
        $d.ShowNewFolderButton = $false
        if ($d.ShowDialog() -eq 'OK') { return $d.SelectedPath }
        return ''
    } catch { return (Read-Host $texto) }
}
$marcaJogo = Join-Path $OUT '2_jogo.ok'
if (-not $Disco -and ($Refazer -or -not (Test-Path $marcaJogo))) {
    $Disco = Escolher-Pasta 'Escolha a pasta com a cópia do DISCO do God of War: Ascension (BCUS98232) -- a que contém PS3_GAME'
    if (-not $Disco) { throw 'sem a pasta do disco' }
}
# sem -Update / -Firmware: baixa os oficiais (etapas 2 e 3)
if (-not $Update) { $Update = 'auto' }
if (-not $Firmware) { $Firmware = 'auto' }
# baixa um arquivo grande (curl.exe do Windows; senão Invoke-WebRequest), por um .part
function Baixar([string]$url, [string]$destino) {
    New-Item -ItemType Directory -Force (Split-Path $destino) | Out-Null
    $part = "$destino.part"
    Remove-Item $part -ErrorAction SilentlyContinue
    $curl = Get-Command curl.exe -ErrorAction SilentlyContinue
    if ($curl) {
        & $curl.Source -L --fail --retry 3 --silent --show-error -o $part $url
        if ($LASTEXITCODE -ne 0) { throw "falhou o download de $url (curl $LASTEXITCODE)" }
    } else {
        $ProgressPreference = 'SilentlyContinue'
        Invoke-WebRequest -Uri $url -OutFile $part -UseBasicParsing
    }
    Move-Item $part $destino -Force
}
# roda um comando nativo pelo cmd (stdout e stderr no log da etapa); falha = exceção com o log
function Exec([string]$log, [string]$linha) {
    Add-Content -Path $log -Value "`r`n> $linha" -Encoding UTF8
    cmd /c "$linha >> `"$log`" 2>&1"
    if ($LASTEXITCODE -ne 0) {
        Write-Host (Get-Content $log -Tail 25 | Out-String) -ForegroundColor DarkGray
        throw "falhou (código $LASTEXITCODE): $linha`r`nlog completo: $log"
    }
}
function Etapa([string]$nome, [scriptblock]$corpo) {
    $marca = Join-Path $OUT "$nome.ok"
    if ((Test-Path $marca) -and -not $Refazer) { Msg "${nome}: já feita (pulando)"; return }
    $script:LOG = Join-Path $OUT "$nome.log"
    Set-Content -Path $script:LOG -Value "== $nome $(Get-Date)" -Encoding UTF8
    Msg "$nome ..."
    $t0 = Get-Date
    & $corpo
    Set-Content -Path $marca -Value (Get-Date) -Encoding UTF8
    Msg ("{0}: ok ({1:N0} s)" -f $nome, ((Get-Date) - $t0).TotalSeconds)
}

# ---------------------------------------------------------------------------- ferramentas
$VS = $null
function Achar-VS {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) { return $null }
    $p = & $vswhere -latest -products * -requires Microsoft.VisualStudio.Component.VC.Llvm.Clang -property installationPath
    if ($p) { return ($p | Select-Object -First 1) }
    return $null
}
Etapa '1_ferramentas' {
    $falta = @()
    # "python" pode ser o atalho da Microsoft Store (abre a loja, não roda nada)
    $v = ''
    try { $v = (cmd /c 'python -c "import sys; print(sys.version_info[0]*100+sys.version_info[1])" 2>nul') } catch { }
    if (-not ($v -match '^\d+$')) { $falta += 'Python 3 (winget install Python.Python.3.12) -- e feche/abra o terminal depois' }
    elseif ([int]$v -lt 310) { $falta += 'Python 3.10 ou mais novo' }
    if (-not (Achar-VS)) {
        $falta += 'Visual Studio 2022 Build Tools com C++, "C++ Clang tools" e "C++ CMake tools":' +
                  "`r`n    winget install Microsoft.VisualStudio.2022.BuildTools --override `"--quiet --wait --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.VC.Llvm.Clang --add Microsoft.VisualStudio.Component.VC.CMake.Project --includeRecommended`""
    }
    if ($falta) { throw ("Falta instalar:`r`n  - " + ($falta -join "`r`n  - ")) }
    & python -c "import Crypto" 2>$null
    if ($LASTEXITCODE -ne 0) { Exec $LOG 'python -m pip install --user pycryptodome' }
    $ff = Join-Path $R "dl\ffmpeg9\$FFMPEG_NOME"
    if (-not (Test-Path "$ff\include\libavcodec")) {
        Msg "baixando o FFmpeg 9 (LGPL) de $FFMPEG_URL"
        New-Item -ItemType Directory -Force (Join-Path $R 'dl\ffmpeg9') | Out-Null
        $zip = Join-Path $R "dl\$FFMPEG_NOME.zip"
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
        Invoke-WebRequest -Uri $FFMPEG_URL -OutFile $zip -UseBasicParsing
        Expand-Archive -Path $zip -DestinationPath (Join-Path $R 'dl\ffmpeg9') -Force
        Remove-Item $zip
    }
}
$VS = Achar-VS
if (-not $VS) { throw 'Visual Studio Build Tools com Clang não encontrado (rode de novo a etapa 1: -Refazer)' }

# ---------------------------------------------------------------------------- jogo
Etapa '2_jogo' {
    if (-not (Test-Path (Join-Path $Disco 'PS3_GAME\USRDIR'))) { throw "não achei PS3_GAME\USRDIR em $Disco" }
    $vfs = Join-Path $R 'vfs'
    New-Item -ItemType Directory -Force $vfs | Out-Null
    $pg = Join-Path $vfs 'PS3_GAME'
    if (-not (Test-Path $pg)) { Exec $LOG "mklink /J `"$pg`" `"$(Join-Path $Disco 'PS3_GAME')`"" }   # sem copiar 34 GB

    if ($Update -eq 'auto') {
        $Update = Join-Path $R 'dl\BCUS98232_update_1.04.pkg'
        if (-not (Test-Path $Update) -or (Get-FileHash $Update -Algorithm SHA1).Hash.ToLower() -ne $UPDATE_SHA1) {
            Msg 'baixando o update 1.04 oficial do servidor da PlayStation (235 MB)'
            Baixar $UPDATE_URL $Update
        }
        if ((Get-FileHash $Update -Algorithm SHA1).Hash.ToLower() -ne $UPDATE_SHA1) {
            Remove-Item $Update -Force
            throw 'o update baixado não confere (SHA-1); rode de novo'
        }
    }
    $upd = $Update
    if ((Test-Path $Update -PathType Leaf) -and $Update -match '\.pkg$') {
        $tmp = Join-Path $R 'out\update_pkg'
        Exec $LOG "python `"$R\ps3recomp\tools\pkg_extract.py`" `"$Update`" `"$tmp`""
        $sfo = Get-ChildItem $tmp -Recurse -Filter PARAM.SFO | Select-Object -First 1
        if (-not $sfo) { throw "o PKG não trouxe PARAM.SFO ($tmp)" }
        $upd = $sfo.DirectoryName
    }
    if (-not (Test-Path (Join-Path $upd 'USRDIR\GOWA.SELF'))) { throw "não achei USRDIR\GOWA.SELF no update ($upd)" }
    $dst = Join-Path $vfs 'game\BCUS98232'
    cmd /c "robocopy `"$upd`" `"$dst`" /E /NFL /NDL /NJH /NJS /NP >> `"$LOG`" 2>&1"
    if ($LASTEXITCODE -ge 8) { throw "falhou a cópia do update (robocopy $LASTEXITCODE)" }

    $jogo = Join-Path $R 'jogo'
    New-Item -ItemType Directory -Force $jogo | Out-Null
    Copy-Item (Join-Path $upd 'USRDIR\EBOOT.BIN'), (Join-Path $upd 'USRDIR\GOWA.SELF') $jogo -Force
    $ch = $ChavesRpcs3
    $baixar = ''
    if (-not $ch) { $ch = Join-Path $R 'dl\rpcs3_chaves'; $baixar = '--baixar' }
    $un = Join-Path $R 'scripts\gpl\unself.py'
    Exec $LOG "python `"$un`" `"$jogo\EBOOT.BIN`" `"$jogo\EBOOT.elf`" --vault `"$ch`" $baixar"
    Exec $LOG "python `"$un`" `"$jogo\GOWA.SELF`" `"$jogo\GOWA.elf`" --vault `"$ch`" --klic-de `"$jogo\EBOOT.elf`""
    $h = (Get-FileHash "$jogo\GOWA.elf" -Algorithm SHA256).Hash.ToLower()
    if ($h -ne $SHA_GOWA) { throw "GOWA.elf diferente do esperado (sha256 $h). O port só serve para BCUS98232 com o update 1.04." }
}

# ---------------------------------------------------------------------------- firmware (libsre por LLE)
Etapa '3_firmware' {
    $fw = Join-Path $R 'fw'; $lle = Join-Path $fw 'lle'
    New-Item -ItemType Directory -Force $lle | Out-Null
    $ch = $ChavesRpcs3; if (-not $ch) { $ch = Join-Path $R 'dl\rpcs3_chaves' }
    if ($Firmware -eq 'auto') {
        $Firmware = Join-Path $R 'dl\PS3UPDAT.PUP'
        if (-not (Test-Path $Firmware)) {
            $lista = Join-Path $R 'dl\ps3-updatelist.txt'
            Baixar $PUP_LISTA $lista
            $url = [regex]::Match((Get-Content $lista -Raw), 'CDN=([^;\s]+/PS3UPDAT\.PUP)').Groups[1].Value
            if (-not $url) { throw "a lista de atualização do PS3 não trouxe o PS3UPDAT.PUP ($PUP_LISTA)" }
            Msg "baixando o firmware oficial do PS3 (PS3UPDAT.PUP, ~200 MB) de $url"
            Baixar $url $Firmware
        }
    }
    $sprx = $Firmware
    if (Test-Path $Firmware -PathType Container) {
        $sprx = Get-ChildItem $Firmware -Recurse -Filter libsre.sprx -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty FullName
        if (-not $sprx) { throw "não achei libsre.sprx em $Firmware (instale o firmware no RPCS3: File > Install Firmware)" }
    } elseif ($Firmware -match '\.pup$') {
        # o arquivo do site da PlayStation: o libsre.sprx sai dos pacotes do dev_flash de dentro dele
        $sprx = Join-Path $fw 'libsre.sprx'
        Exec $LOG "python `"$R\scripts\gpl\pup_libsre.py`" `"$Firmware`" `"$sprx`" --vault `"$ch`" --baixar"
    }
    Exec $LOG "python `"$R\scripts\gpl\unself.py`" `"$sprx`" `"$fw\libsre.prx`" --vault `"$ch`" --baixar"
    $h = (Get-FileHash "$fw\libsre.prx" -Algorithm SHA256).Hash.ToLower()
    if ($h -ne $SHA_LIBSRE) { throw "libsre.prx diferente do esperado (sha256 $h). Rode sem -Firmware para o gerador baixar o firmware oficial." }
    Push-Location (Join-Path $R 'ps3recomp')
    try {
        Exec $LOG "python tools\lift_prx.py `"$fw\libsre.prx`" --base 0x30000000 --output `"$lle`""
        Exec $LOG "python tools\prx_exports.py `"$fw\libsre.prx`" --symbol libsre --out `"$lle\libsre_exports.c`" --json `"$lle\libsre_exports_c.json`""
        Exec $LOG "python `"$R\scripts\libsre_stubs_json.py`" `"$lle\libsre_imports.json`" `"$lle\libsre_hle_stubs.json`""
        Exec $LOG "python tools\ppu_lifter.py `"$lle\libsre_image.bin`" --raw --base 0x30000000 --toc 0x30039AB0 --code-end 0x3001DB18 --symbol-prefix libsre_ --functions `"$lle\libsre_functions.json`" --hle-stubs `"$lle\libsre_hle_stubs.json`" -o `"$lle\recomp`""
    } finally { Pop-Location }
    $pl = Join-Path $R 'port\lle_libsre'
    New-Item -ItemType Directory -Force $pl | Out-Null
    Copy-Item "$lle\recomp\ppu_recomp_000.cpp" "$pl\libsre_recomp.cpp" -Force
    Copy-Item "$lle\recomp\ppu_recomp.h", "$lle\libsre_exports.c" $pl -Force
}

# ---------------------------------------------------------------------------- PPU
Etapa '4_ppu' {
    $o = Join-Path $R 'out'
    Push-Location (Join-Path $R 'ps3recomp')
    try {
        Exec $LOG "python tools\ppu_loader.py `"$R\jogo\GOWA.elf`" -o `"$o`""
        Exec $LOG "python tools\ppu_lifter.py `"$R\jogo\GOWA.elf`" --functions `"$o\GOWA.functions.json`" --hle-stubs `"$o\GOWA.imports.json`" --code-end 0x661C14 --chunks 48 --output `"$R\port\recompiled`" -j $Jobs"
        Exec $LOG "python tools\gen_hle_nids.py --all --out `"$R\port\recompiled\ppu_hle_nids.cpp`""
    } finally { Pop-Location }
    Exec $LOG "python `"$R\scripts\ppu_preambulo.py`""
    Exec $LOG "python `"$R\scripts\ppu_restrict.py`" aplicar"
    Exec $LOG "python `"$R\scripts\remendos_ppu.py`" aplicar"
    Exec $LOG "python `"$R\scripts\sonda.py`" por func_001C05A4 loc_0039C710 loc_00415914 func_00415120"
    Exec $LOG "python `"$R\scripts\gerar_imports_inc.py`""
}

# ---------------------------------------------------------------------------- SPU
Etapa '5_spu' {
    $s = Join-Path $R 'scripts'
    # recomeça do zero: os passes de desempenho guardam backups .sem_* e não podem rodar
    # por cima de código já otimizado (o gowanat_tspm é código nosso, do repositório)
    Get-ChildItem (Join-Path $R 'port\spu') -Force -ErrorAction SilentlyContinue |
        Where-Object { $_.Name -ne 'gowanat_tspm' } | Remove-Item -Recurse -Force
    # a ferramenta é dona da pasta de saída inteira (substitui a cada execução): gera à parte e copia
    $sw = Join-Path $R 'out\spu_workloads'
    Exec $LOG "python `"$R\ps3recomp\tools\build_spu_workloads.py`" --input `"$R\jogo\GOWA.elf`" --input `"$R\fw\libsre.prx`" --lifted `"$sw`" --out `"$sw\spu_workloads.c`" --constructor --allow-partial"
    cmd /c "robocopy `"$sw`" `"$R\port\spu`" /E /NFL /NDL /NJH /NJS /NP >> `"$LOG`" 2>&1"
    if ($LASTEXITCODE -ge 8) { throw "falhou a cópia de $sw (robocopy $LASTEXITCODE)" }
    Exec $LOG "python `"$s\lift_gowa_jobmods.py`""
    Exec $LOG "python `"$s\lift_gowa_pm.py`""
    Exec $LOG "python `"$s\lift_gowa_tspm.py`""
    Exec $LOG "python `"$s\lift_gowa_syssrv.py`""
    Exec $LOG "python `"$s\lift_gowa_plugins.py`" `"$R\dados\plugins_getcode_edge_cap16.txt`""
    Exec $LOG "python `"$s\lift_gowa_plugins.py`" `"$R\dados\plugins_getcode_w11.txt`" --wids 11 --naofolha --inc `"$R\port\gowa_plugins_w11.inc`" --tabela g_gowa_plugin_fns_w11"
    $hot = Join-Path $R 'port\gowa_jobmods_hot.inc'
    $utf8 = New-Object System.Text.UTF8Encoding($false)   # sem BOM: o .inc entra no meio de um .cpp
    [IO.File]::WriteAllText($hot, ("/* GERADO por gerar.ps1 + scripts/folhas_spu.py: funções folha seguras dos módulos de job`n" +
        " * que ficam interpretados (GOWA_JOBLIFT_HOT). va, LS, bytes da função. */`n" +
        "static const struct { uint32_t va, ls, len; } g_gowa_hot[] = {`n"), $utf8)
    Exec $LOG "python `"$s\folhas_spu.py`" `"$R\port\spu\gowajm_006FC080\spu_recomp.c`" gowajm_006FC080_ --inc 6FC080 `"$hot`""
    [IO.File]::AppendAllText($hot, "};`n", $utf8)
    # obrigatório: o boot liga GOWA_SPU_NUM_REMAP=3 (a 7ª SPU se vê como outra nas listas de
    # job do WWS); sem este passe os números colidem e o jogo para esperando um job
    Exec $LOG "python `"$s\spu_num_remap.py`" aplicar"
    Exec $LOG "python `"$s\guarda_avx512.py`" aplicar"
    # passes de desempenho (núcleos vetoriais, luz do EDGE na GPU, saltos diretos...)
    if (-not $SemOtimizar) { Exec $LOG "python `"$s\otimizar.py`"" }
}

# ---------------------------------------------------------------------------- compilar
$llvm = Join-Path $VS 'VC\Tools\Llvm\x64\bin'
$alvo = $Build
if ($alvo -eq 'auto') {
    $def = (cmd /c "echo.| `"$llvm\clang.exe`" -march=native -dM -E -x c - 2>nul") -join "`n"
    if ($def -match '__AVX512F__' -and $def -match '__AVX512BW__' -and $def -match '__AVX512VL__' -and $def -match '__AVX512DQ__') { $alvo = 'avx512' }
    else { $alvo = 'portatil' }
}
if ($alvo -eq 'avx512') { $bargs = 'native'; $bdir = 'build_native' } else { $bargs = 'native rz'; $bdir = 'build_nativerz' }
Etapa '6_compilar' {
    Msg "build: $alvo (código SPU com -march=native$(if ($alvo -eq 'portatil') { ', contas de float em para-zero' }))"
    $env:GOWA_VS = $VS
    $env:FFMPEG_DIR = Join-Path $R "dl\ffmpeg9\$FFMPEG_NOME"
    $env:GOWA_JOBS = "$Jobs"
    Exec $LOG "`"$R\scripts\build_portatil.cmd`" $bargs"
    if (-not (Test-Path (Join-Path $R "port\$bdir\gowa.exe"))) { throw "o gowa.exe não foi gerado (veja $LOG)" }
}

# ---------------------------------------------------------------------------- jogar.cmd
Etapa '7_jogar' {
    $b = Join-Path $R "port\$bdir"
    Copy-Item (Join-Path $R "dl\ffmpeg9\$FFMPEG_NOME\bin\*.dll") $b -Force
    $j = Join-Path $R 'jogar.cmd'
    Set-Content -Path $j -Encoding ASCII -Value @(
        '@echo off',
        'rem Gerado pelo gerar.ps1. Variaveis opcionais: GOWA_FLIP_HZ (0 = sem limitador), GOWA_RES_SCALE (1-4).',
        'setlocal',
        'set "RAIZ=%~dp0"',
        'set "PS3_VFS_ROOT=%RAIZ%vfs"',
        'set "PS3_LIBSRE_IMAGE=%RAIZ%fw\lle\libsre_image.bin"',
        'if not defined PS3_SAVE_ROOT set "PS3_SAVE_ROOT=%RAIZ%saves"',
        'if not exist "%PS3_SAVE_ROOT%" mkdir "%PS3_SAVE_ROOT%"',
        'if not exist "%RAIZ%out" mkdir "%RAIZ%out"',
        "cd /d `"%RAIZ%port\$bdir`"",
        'gowa.exe "%RAIZ%jogo\GOWA.elf" > "%RAIZ%out\jogo.log" 2>&1')
}
Msg "pronto. Para jogar: $R\jogar.cmd  (log em out\jogo.log)"
