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
  Pasta com a cópia do disco BCUS98232 (a que contém PS3_GAME, ou uma pasta acima
  dela), ou o arquivo compactado com ela (.7z, .zip, .rar ou .iso decriptada): ele é
  extraído uma vez para disco\ (uns 35 GB) com o tar do Windows.
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
    [ValidateSet('auto', 'avx512', 'nativa', 'portatil')][string]$Build = 'auto',
    [int]$Jobs = 0,
    [switch]$SemOtimizar,
    [int]$RefazerDe = 0,
    [switch]$Refazer
)
$ErrorActionPreference = 'Stop'
# Reports 171, 180 e 181 (28/10): num Windows em árabe (cp1256) ou vietnamita (cp1258) o print com acento do
# unself.py dava UnicodeEncodeError depois de decriptar, e a geração parava. Os scripts Python falam UTF-8.
$env:PYTHONUTF8 = '1'
$env:PYTHONIOENCODING = 'utf-8'
$R = $PSScriptRoot
if ($R -match ' ') { throw "Coloque o projeto numa pasta sem espaços (agora: $R)" }
if (-not $Jobs) { $Jobs = [Environment]::ProcessorCount }
$SHA_GOWA = '813723ac4e091d79ea20ad2500b2e86cb0fd25f3736541431921273e7a77246d'
$FFMPEG_NOME = 'ffmpeg-n9.0-latest-win64-lgpl-shared-9.0'
$FFMPEG_URL = "https://github.com/BtbN/FFmpeg-Builds/releases/download/latest/$FFMPEG_NOME.zip"
# reports 267 e 298: a conexão com o GitHub caía no meio; primeiro a cópia no servidor do launcher (a mesma build de
# 16/10, conferida pelo sha256), o GitHub do BtbN fica de reserva
$FFMPEG_ESPELHO = "https://otavix-launcher.otavix-launcher-servidor.workers.dev/v1/arquivos/terceiros/ffmpeg/$FFMPEG_NOME.zip"
$FFMPEG_ESPELHO_SHA = 'f6affc440dd120d582e427bf2345b8c346bd1547851228cf2e7a75504663252b'
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
# Reports 470/473/474 (30/10): numa geração retomada, uma etapa marcada como feita pode ter perdido o que fez (antivírus,
# limpeza, pasta apagada) e a falha aparecia só etapas depois, sem dizer por quê. Etapa cuja saída sumiu é refeita.
foreach ($c in @(@('2_jogo', 'jogo\GOWA.elf'), @('3_firmware', 'fw\lle\libsre_image.bin'), @('3_firmware', 'port\lle_libsre\libsre_recomp.cpp'),
                 @('4_ppu', 'port\recompiled\ppu_hle_nids.cpp'), @('5_spu', 'port\spu\gowajm_0072DF80\spu_recomp.c'))) {
    $marca = Join-Path $OUT "$($c[0]).ok"
    if ((Test-Path -LiteralPath $marca) -and -not (Test-Path -LiteralPath (Join-Path $R $c[1]))) {
        Write-Host "[gerar] $($c[0]): marcada como feita, mas $($c[1]) sumiu; refazendo" -ForegroundColor Cyan
        Remove-Item -LiteralPath $marca -Force
    }
}
# Report 398 (30/10) e parecidos: um PATH sem a pasta do Windows deixa o cmd, o robocopy e o icacls sem achar
$sis = @("$env:SystemRoot\System32", $env:SystemRoot, "$env:SystemRoot\System32\Wbem", "$env:SystemRoot\System32\WindowsPowerShell\v1.0")
$noPath = @($env:PATH -split ';' | ForEach-Object { $_.Trim('"', ' ').TrimEnd('\') })
$faltam = @($sis | Where-Object { $noPath -notcontains $_ })
if ($faltam) { $env:PATH = ($faltam -join ';') + ';' + $env:PATH }

function Msg($t) { Write-Host "[gerar] $t" -ForegroundColor Cyan }
# roda um programa externo sem que o stderr dele vire exceção: no Windows PowerShell 5.1,
# com ErrorActionPreference = Stop, qualquer linha no stderr de um comando nativo lança
# (mesmo com 2>$null) antes do teste do $LASTEXITCODE. Quem chama confere o $LASTEXITCODE.
function SemExcecao([scriptblock]$bloco) {
    $eap = $ErrorActionPreference
    $ErrorActionPreference = 'Continue'
    try { & $bloco } finally { $ErrorActionPreference = $eap }
}

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
    $erroCurl = $null
    if ($curl) {
        # report 453: o Windows (antivírus ou política) não deixava o curl.exe rodar ("Acesso negado"): o PowerShell
        # parava a geração na hora. Agora isso cai no download pelo próprio PowerShell, abaixo.
        try {
            SemExcecao { & $curl.Source -L --fail --retry 3 --silent --show-error -o $part $url }
            # curl 35 (CRYPT_E_NO_REVOCATION_CHECK): a rede ou o antivírus do jogador não deixa o schannel consultar a
            # revogação do certificado (report 47 do launcher, no Dante); de novo sem essa consulta, o certificado ainda conferido
            if ($LASTEXITCODE -eq 35) { SemExcecao { & $curl.Source -L --fail --retry 3 --silent --show-error --ssl-no-revoke -o $part $url } }
            if ($LASTEXITCODE -ne 0) { $erroCurl = "curl $LASTEXITCODE" }
        } catch {
            $erroCurl = "o curl.exe não pôde rodar: $($_.Exception.Message)"
        }
    }
    if (-not $curl -or $erroCurl) {
        if ($erroCurl) { Msg "download pelo curl falhou ($erroCurl); tentando pelo PowerShell" }
        Remove-Item $part -ErrorAction SilentlyContinue
        $ProgressPreference = 'SilentlyContinue'
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
        try { Invoke-WebRequest -Uri $url -OutFile $part -UseBasicParsing }
        catch { throw "falhou o download de $url ($(if ($erroCurl) { "$erroCurl; " })PowerShell: $($_.Exception.Message))" }
    }
    Move-Item $part $destino -Force
}
# escreve no log da etapa (-Novo: do zero). O launcher lê esse arquivo a cada meio segundo para
# mostrar o progresso, e o Add-Content/Set-Content do PowerShell 5.1 falha ("sendo usado por outro
# processo") se ele estiver aberto, mesmo só para leitura: aqui ele é aberto compartilhando leitura e
# escrita e, se ainda assim estiver ocupado, a escrita é tentada de novo
function Anotar([string]$arq, [string]$texto, [switch]$Novo) {
    $b = [Text.Encoding]::UTF8.GetBytes("$texto`r`n")
    for ($i = 0; ; $i++) {
        try {
            $fs = New-Object IO.FileStream($arq, $(if ($Novo) { 'Create' } else { 'Append' }), 'Write', 'ReadWrite')
            try { $fs.Write($b, 0, $b.Length) } finally { $fs.Dispose() }
            return
        } catch [IO.IOException] {
            if ($i -ge 50) { throw }
            Start-Sleep -Milliseconds 100
        }
    }
}
# roda um comando nativo pelo cmd (stdout e stderr no log da etapa); falha = exceção com o log
function Exec([string]$log, [string]$linha) {
    Anotar $log "`r`n> $linha"
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
    Anotar $script:LOG "== $nome $(Get-Date)" -Novo
    Msg "$nome ..."
    $t0 = Get-Date
    & $corpo
    Set-Content -Path $marca -Value (Get-Date) -Encoding UTF8
    Msg ("{0}: ok ({1:N0} s)" -f $nome, ((Get-Date) - $t0).TotalSeconds)
}

# ---------------------------------------------------------------------------- ferramentas
# o "python" do PATH pode não servir: o atalho da Microsoft Store (abre a loja) ou o Python de outro programa, sem
# pip (report 85 do launcher: o do Inkscape vinha primeiro e o "pip install" parou a geração). Escolhe um Python
# 3.10+ com pip, o do lançador py primeiro e depois cada python do PATH, e põe a pasta dele na frente do PATH
# desta geração; um 3.10+ sem pip recebe o pip pelo ensurepip. Nenhum: a etapa 1_ferramentas diz o que instalar.
function Escolher-Python {
    if ($env:GOWA_TESTE_SEM_PYTHON -eq '1') { return $null }   # teste dos reports 470/473/474
    $cands = @()
    $e = SemExcecao { cmd /c 'py -3 -c "import sys; print(sys.executable)" 2>nul' }
    if ($LASTEXITCODE -eq 0 -and $e) { $cands += "$(@($e)[0])".Trim() }
    $w = SemExcecao { cmd /c 'where python 2>nul' }
    if ($w) { $cands += @($w | ForEach-Object { "$_".Trim() }) }
    $semPip = $null
    foreach ($c in ($cands | Where-Object { $_ -and $_ -notmatch '\\WindowsApps\\' } | Select-Object -Unique)) {
        if (-not (Test-Path -LiteralPath $c)) { continue }
        $v = SemExcecao { & $c -c "import sys; print(sys.version_info[0]*100+sys.version_info[1])" 2>$null }
        if (-not ("$(@($v)[0])" -match '^\d+$') -or [int]"$(@($v)[0])" -lt 310) { continue }
        $null = SemExcecao { & $c -m pip --version 2>$null }
        if ($LASTEXITCODE -eq 0) { return $c }
        if (-not $semPip) { $semPip = $c }
    }
    if ($semPip) {
        Msg "o Python em $semPip não tem pip; instalando o pip nele (ensurepip)"
        $null = SemExcecao { & $semPip -m ensurepip --user 2>$null }
        $null = SemExcecao { & $semPip -m pip --version 2>$null }
        if ($LASTEXITCODE -eq 0) { return $semPip }
        # report 336: o pip só servia para o pycryptodome, e o AES agora tem reserva no próprio Windows
        # (ps3recomp\tools\aes_cng.py): um Python sem pip também serve
        Msg "o Python em $semPip não tem pip e não deu para instalar; segue sem ele (o AES vem do Windows)"
        return $semPip
    }
    # reports 470/473/474: o Python da Microsoft Store (real, não o atalho que abre a loja) só existe em \WindowsApps\;
    # por último, ele serve se rodar e for 3.10+ (o atalho falha na conferência da versão)
    foreach ($c in ($cands | Where-Object { $_ -match '\\WindowsApps\\' } | Select-Object -Unique)) {
        $v = SemExcecao { & $c -c "import sys; print(sys.version_info[0]*100+sys.version_info[1])" 2>$null }
        if ("$(@($v)[0])" -match '^\d+$' -and [int]"$(@($v)[0])" -ge 310) { return $c }
    }
    return $null
}
$py = Escolher-Python
if ($py) {
    $env:PATH = (Split-Path -Parent $py) + ';' + $env:PATH
    Msg "Python: $py"
}
# Reports 470/473/474: a etapa 1 marcada como feita não garante que o Python e o FFmpeg ainda estejam aqui (desinstalado,
# pasta dl apagada); sem eles a geração parava etapas depois. A etapa 1 volta, confere e diz o que falta.
$marca1 = Join-Path $OUT '1_ferramentas.ok'
if ((Test-Path -LiteralPath $marca1) -and
    (-not $py -or -not (Test-Path (Join-Path $R "dl\ffmpeg9\$FFMPEG_NOME\include\libavcodec")) -or
     -not (Test-Path (Join-Path $R "dl\ffmpeg9\$FFMPEG_NOME\bin\*.dll")))) {
    Msg "1_ferramentas: $(if (-not $py) { 'nenhum Python 3.10+ achado' } else { 'o FFmpeg sumiu da pasta dl' }); conferindo de novo"
    Remove-Item -LiteralPath $marca1 -Force
}

$VS = $null
function Achar-VS {
    $vswhere = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\vswhere.exe"
    if (-not (Test-Path $vswhere)) { return $null }
    # report 350: o -latest escolhia um Visual Studio 2026 Enterprise com o componente Clang mas sem o
    # vcvars64.bat (C++ incompleto); agora só serve uma instalação com o vcvars64.bat e o clang-cl, e a 2022
    # (a testada) vem antes das outras
    $todas = @(& $vswhere -all -products * -requires Microsoft.VisualStudio.Component.VC.Llvm.Clang -format json | ConvertFrom-Json)
    $boas = @($todas | Where-Object {
        (Test-Path (Join-Path $_.installationPath 'VC\Auxiliary\Build\vcvars64.bat')) -and
        (Test-Path (Join-Path $_.installationPath 'VC\Tools\Llvm\x64\bin\clang-cl.exe')) })
    if (-not $boas.Count) { return $null }
    $boa = $boas | Sort-Object @{ Expression = { if ("$($_.installationVersion)" -like '17.*') { 0 } else { 1 } } },
                               @{ Expression = { [version]("$($_.installationVersion)" -replace '[^0-9.].*$', '') }; Descending = $true } |
           Select-Object -First 1
    return $boa.installationPath
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
    SemExcecao { & python -c "import Crypto" 2>$null }
    # o pycryptodome é só o AES mais rápido: sem ele (sem pip, ou as DLLs bloqueadas pelo Controle Inteligente de
    # Aplicativos, reports 331/334/336) as ferramentas usam o AES do Windows
    if ($LASTEXITCODE -ne 0) {
        $null = SemExcecao { cmd /c "python -m pip install --user pycryptodome >> `"$LOG`" 2>&1" }
        if ($LASTEXITCODE -ne 0) { Msg 'pycryptodome não instalado; as ferramentas usam o AES do Windows' }
    }
    $ff = Join-Path $R "dl\ffmpeg9\$FFMPEG_NOME"
    if (-not (Test-Path "$ff\include\libavcodec")) {
        Msg "baixando o FFmpeg 9 (LGPL)"
        New-Item -ItemType Directory -Force (Join-Path $R 'dl\ffmpeg9') | Out-Null
        $zip = Join-Path $R "dl\$FFMPEG_NOME.zip"
        [Net.ServicePointManager]::SecurityProtocol = [Net.SecurityProtocolType]::Tls12
        # report 53 do launcher: o zip chegou cortado ("End of Central Directory record could not be found") e a
        # geração parou; pelo Baixar (curl com novas tentativas, que acusa download incompleto) e, se o zip ainda
        # não abrir, apaga e baixa mais uma vez
        for ($vez = 1; ; $vez++) {
            $doEspelho = $false
            try {
                Baixar $FFMPEG_ESPELHO $zip
                if ((Get-FileHash -Algorithm SHA256 -LiteralPath $zip).Hash.ToLower() -ne $FFMPEG_ESPELHO_SHA) { throw 'sha256 diferente' }
                $doEspelho = $true
            } catch {
                Msg "a cópia do FFmpeg no servidor não veio ($($_.Exception.Message)); baixando do GitHub"
                Remove-Item $zip -ErrorAction SilentlyContinue
            }
            if (-not $doEspelho) { Baixar $FFMPEG_URL $zip }
            try { Expand-Archive -Path $zip -DestinationPath (Join-Path $R 'dl\ffmpeg9') -Force; break }
            catch {
                Remove-Item $zip -ErrorAction SilentlyContinue
                if ($vez -ge 2) {
                    throw ("o FFmpeg chegou incompleto ou corrompido duas vezes ($FFMPEG_URL). Confira a internet " +
                           "e se o antivírus não está bloqueando o download, e gere de novo.")
                }
                Msg 'o FFmpeg chegou incompleto; baixando de novo'
            }
        }
        Remove-Item $zip
    }
}
$VS = Achar-VS
# Reports 363, 366 e 378 (29/10): a etapa 1 estava marcada como feita, mas não havia nenhum Visual Studio com o C++
# completo (no 366 só um VS 2026 Enterprise sem as ferramentas de C++, que a checagem do launcher aceitava; no 378
# nenhum), e a geração parava pedindo "-Refazer", que o jogador não tem como dar. Agora a marca da etapa 1 sai e o
# gerador instala ele mesmo o Build Tools 2022 com C++ e Clang pelo winget (o mesmo do botão do launcher; o Windows
# pede a permissão de administrador), e procura de novo.
if (-not $VS) {
    Remove-Item -LiteralPath (Join-Path $OUT '1_ferramentas.ok') -Force -ErrorAction SilentlyContinue
    $wg = Get-Command winget -ErrorAction SilentlyContinue
    if ($wg) {
        Msg 'nenhum Visual Studio com C++ e Clang completos: instalando o Visual Studio 2022 Build Tools (alguns GB; o Windows pede permissão)'
        $null = SemExcecao { cmd /c ('winget install -e --id Microsoft.VisualStudio.2022.BuildTools --accept-package-agreements --accept-source-agreements ' +
            '--override "--passive --wait --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.VC.Llvm.Clang ' +
            "--add Microsoft.VisualStudio.Component.VC.CMake.Project --includeRecommended`" >> `"$(Join-Path $OUT 'vs_instala.log')`" 2>&1") }
        $VS = Achar-VS
    }
    if (-not $VS) {
        throw ('falta o Visual Studio 2022 Build Tools com C++ e "C++ Clang tools" (um Visual Studio 2026 ou Enterprise ' +
               'sem as ferramentas de C++ não serve). No launcher, abra o God of War: Ascension e clique em Instalar no ' +
               'item do Visual Studio, ou rode num terminal: winget install -e --id Microsoft.VisualStudio.2022.BuildTools ' +
               '--override "--passive --wait --add Microsoft.VisualStudio.Workload.VCTools --add Microsoft.VisualStudio.Component.VC.Llvm.Clang ' +
               '--add Microsoft.VisualStudio.Component.VC.CMake.Project --includeRecommended". Depois gere de novo.')
    }
    Msg "Visual Studio: $VS"
}

# O processador: o jogo é compilado no mínimo para x86-64-v3 (AVX2, FMA, BMI2). Report 40: um Xeon E3-1230 V2 (Ivy
# Bridge, só AVX) gerou o jogo inteiro, e ele fechou ao abrir com 0xC000001D (instrução ilegal). Sem essas instruções,
# o gerador para aqui, antes de minutos de geração, e diz por quê. O clang diz o que o processador tem, como na escolha
# da build (-march=native); GOWA_TESTE_MARCH troca o native nos testes. Sem resposta do clang, segue.
function Conferir-Processador {
    $march = if ($env:GOWA_TESTE_MARCH) { $env:GOWA_TESTE_MARCH } else { 'native' }
    $def = (cmd /c "echo.| `"$(Join-Path $VS 'VC\Tools\Llvm\x64\bin\clang.exe')`" -march=$march -dM -E -x c - 2>nul") -join "`n"
    if (-not $def -or ($def -match '__AVX2__' -and $def -match '__FMA__' -and $def -match '__BMI2__')) { return }
    $nome = (Get-CimInstance Win32_Processor -ErrorAction SilentlyContinue | Select-Object -First 1).Name.Trim()
    throw ("o seu processador ($nome) não tem AVX2, e o port precisa dele: Intel Core de 4ª geração (Haswell, 2013) " +
           'em diante, ou AMD Ryzen ou FX da série Excavator (2015) em diante. O jogo também pede um processador forte: ' +
           'mesmo com AVX2, os de 4 núcleos ficam abaixo de 15 fps.')
}
Conferir-Processador

# ---------------------------------------------------------------------------- jogo
# a pasta do jogo: a que contém PS3_GAME\USRDIR, ou uma até dois níveis abaixo da escolhida
function Achar-PS3Game([string]$raiz) {
    if (Test-Path -LiteralPath (Join-Path $raiz 'PS3_GAME\USRDIR') -PathType Container) { return $raiz }
    # a pasta escolhida já é o próprio PS3_GAME (com USRDIR dentro), ou o USRDIR dele: o disco é a pasta de cima
    # (um jogador escolheu "J:\JOGOS RPCS3\BCUS98232\PS3_GAME" e o gerador recusava toda vez)
    $nome = Split-Path -Leaf $raiz.TrimEnd('\')
    if ($nome -ieq 'PS3_GAME' -and (Test-Path -LiteralPath (Join-Path $raiz 'USRDIR') -PathType Container)) { return (Split-Path $raiz.TrimEnd('\')) }
    if ($nome -ieq 'USRDIR' -and (Split-Path -Leaf (Split-Path $raiz.TrimEnd('\'))) -ieq 'PS3_GAME') { return (Split-Path (Split-Path $raiz.TrimEnd('\'))) }
    # report 109: a pasta do RPCS3 (dev_hdd0, dev_bdvd, games...): o jogo de disco fica em games\<algo> ou em
    # dev_hdd0\disc\<id>, fundo demais para a busca geral; o BCUS98232 primeiro
    # report 394: o RPCS3 guarda onde está cada jogo de disco no games.yml ("BCUS98232: Y:/Jogos/GoW/"), que pode
    # ficar fora da pasta dele
    foreach ($yml in @('config\games.yml', 'games.yml')) {
        $arqYml = Join-Path $raiz $yml
        if (-not (Test-Path -LiteralPath $arqYml -PathType Leaf)) { continue }
        foreach ($l in [IO.File]::ReadAllLines($arqYml)) {
            if ($l -notmatch '^\s*BCUS98232\s*:\s*(.+?)\s*$') { continue }
            $dir = $Matches[1].Trim('"', "'") -replace '/', '\'
            if ($dir -and (Test-Path -LiteralPath (Join-Path $dir 'PS3_GAME\USRDIR') -PathType Container)) { return $dir.TrimEnd('\') }
        }
    }
    foreach ($sub in @('games', 'dev_hdd0\disc', 'dev_bdvd')) {
        $base = Join-Path $raiz $sub
        if (-not (Test-Path -LiteralPath $base -PathType Container)) { continue }
        if (Test-Path -LiteralPath (Join-Path $base 'PS3_GAME\USRDIR') -PathType Container) { return $base }
        $c = @(Get-ChildItem -LiteralPath $base -Directory -ErrorAction SilentlyContinue |
               Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'PS3_GAME\USRDIR') -PathType Container })
        $c = @($c | Sort-Object { if ($_.Name -match 'BCUS98232|Ascension') { 0 } else { 1 } })
        if ($c) { return $c[0].FullName }
    }
    $p = Get-ChildItem -LiteralPath $raiz -Directory -Recurse -Depth 3 -Filter PS3_GAME -ErrorAction SilentlyContinue |
         Where-Object { Test-Path -LiteralPath (Join-Path $_.FullName 'USRDIR') -PathType Container } | Select-Object -First 1
    if ($p) { return $p.Parent.FullName }
    return $null
}
# o disco também pode vir compactado: extrai uma vez para disco\ (o tar do Windows lê 7z, zip, rar e iso)
function Achar-Disco([string]$d) {
    if (Test-Path -LiteralPath $d -PathType Leaf) {
        if ($d -notmatch '\.(7z|zip|rar|iso)$') { throw "o disco tem que ser a pasta com PS3_GAME ou um arquivo .7z, .zip, .rar ou .iso ($d)" }
        $dx = Join-Path $R 'disco'
        $achou = $null
        if (Test-Path -LiteralPath $dx) { $achou = Achar-PS3Game $dx }
        # uma extração de outra região (ver Conferir-Regiao) não é reaproveitada: o arquivo escolhido agora é outro
        if ($achou -and (Id-Disco (Join-Path $achou 'PS3_GAME')) -notin @('', 'BCUS98232')) {
            Msg "a extração anterior em $dx é de outra versão do jogo; apagando para extrair o arquivo escolhido"
            SemExcecao { cmd /c "rmdir /s /q `"$dx`" 2>&1" } | Out-Null
            $achou = $null
        }
        if (-not $achou) {
            $precisa = [math]::Max(36GB, (Get-Item -LiteralPath $d).Length * 1.1)
            $livre = (Get-PSDrive -Name $R.Substring(0, 1)).Free
            if ($livre -lt $precisa) {
                throw ("pouco espaço para extrair o disco: {0:N0} GB livres em {1}, e o jogo extraído ocupa uns 35 GB" -f ($livre / 1GB), $R.Substring(0, 2))
            }
            Msg "extraindo $d para $dx (uns 35 GB; leva alguns minutos)"
            New-Item -ItemType Directory -Force $dx | Out-Null
            SemExcecao { & "$env:SystemRoot\System32\tar.exe" -xf $d -C $dx }
            if ($LASTEXITCODE -ne 0) { throw "falhou a extração de $d (tar $LASTEXITCODE). Extraia o arquivo numa pasta e escolha a pasta." }
            $achou = Achar-PS3Game $dx
            # um .7z/.zip com a .iso dentro: extrai a .iso também e apaga a intermediária
            if (-not $achou) {
                $iso = Get-ChildItem -LiteralPath $dx -Recurse -File -Filter *.iso -ErrorAction SilentlyContinue | Select-Object -First 1
                if ($iso) {
                    Msg "extraindo $($iso.Name)"
                    SemExcecao { & "$env:SystemRoot\System32\tar.exe" -xf $iso.FullName -C $dx }
                    if ($LASTEXITCODE -ne 0) { throw "falhou a extração de $($iso.FullName) (tar $LASTEXITCODE)" }
                    Remove-Item -LiteralPath $iso.FullName -Force
                    $achou = Achar-PS3Game $dx
                }
            }
        }
        if (-not $achou) { throw "não achei PS3_GAME\USRDIR dentro de $d (uma .iso criptografada, como as da Redump, não serve)" }
        return $achou
    }
    # reports 66/72: um .iso escolhido que não é arquivo nenhum (movido, renomeado, download por terminar, sem acesso):
    # com um só arquivo de disco na mesma pasta, é ele; senão, diz o que o Windows respondeu
    if ($d -match '\.(7z|zip|rar|iso)$' -and -not (Test-Path -LiteralPath $d -PathType Container)) {
        # report 435: um nome sem pasta deixava o Split-Path vazio e o Get-ChildItem parava com "A parameter cannot be
        # found that matches parameter name 'File'" (sem caminho ele não sabe que é uma pasta do disco)
        $pai = Split-Path $d
        if (-not $pai) { $pai = (Get-Location).ProviderPath }
        $outro = @()
        if ($pai -and [IO.Directory]::Exists($pai)) {
            $outro = @(try { [IO.Directory]::GetFiles($pai) | Where-Object { $_ -match '\.(7z|zip|rar|iso)$' } |
                             ForEach-Object { [IO.FileInfo]::new($_) } } catch { })
        }
        if ($outro.Count -eq 1) {
            Msg "o arquivo $d não existe; usando $($outro[0].FullName), o arquivo do jogo dessa pasta"
            return (Achar-Disco $outro[0].FullName)
        }
        $erro = try { [void][IO.File]::GetAttributes($d); 'sem acesso' } catch { $_.Exception.InnerException.Message }
        throw ("não consegui abrir o arquivo do jogo $d ($erro). Confira se ele ainda está nessa pasta com esse nome e " +
               'se o download terminou, e escolha-o de novo em "Escolher o disco de novo".')
    }
    $achou = Achar-PS3Game $d
    if (-not $achou) {
        # uma pasta com só o arquivo do disco dentro
        $arq = @(Get-ChildItem -LiteralPath $d -File -ErrorAction SilentlyContinue | Where-Object { $_.Name -match '\.(7z|zip|rar|iso)$' })
        if ($arq.Count -eq 1) { return (Achar-Disco $arq[0].FullName) }
        # a PS3_GAME there that is a link leading nowhere (moved, or a circle back to the port's vfs): said as it is
        $como = ''
        if (Test-Path -LiteralPath (Join-Path $d 'PS3_GAME')) { $como = " (o PS3_GAME de lá é um atalho que não leva ao jogo: $(Descrever-Atalhos (Join-Path $d 'PS3_GAME')))" }
        # report 33: a pasta escolhida era a dos ports do launcher (a que tem gow-ascension\ dentro), não a do jogo
        $cheio = [IO.Path]::GetFullPath($d).TrimEnd('\')
        $raizPort = [IO.Path]::GetFullPath($R).TrimEnd('\')
        if ($cheio -ieq $raizPort -or $cheio -ieq (Split-Path $raizPort)) {
            $como = ' (essa é a pasta onde o launcher instala os ports, não a do jogo)'
        }
        # reports 99 e 101: a pasta escolhida é o código do port (o gow-ascension-pc baixado do GitHub), não o jogo
        elseif (@(Get-ChildItem -LiteralPath $d -Recurse -Depth 1 -Force -ErrorAction SilentlyContinue |
                  Where-Object { $_.Name -in @('gerar.ps1', 'ps3recomp', 'NOVIDADES.md') }).Count -ge 2 -or
                (Split-Path -Leaf $cheio) -ieq 'ps3recomp') {
            $como = (' (essa é a pasta do CÓDIGO do port, o gow-ascension-pc baixado do GitHub, não o jogo: o launcher ' +
                     'já baixa o port sozinho. Escolha aqui a sua cópia do God of War: Ascension do PS3, a pasta com ' +
                     'PS3_GAME ou o arquivo baixado dela)')
        }
        # report 394: a pasta escolhida é a do update/DLC no RPCS3 (dev_hdd0\game\BCUS98232), não o disco: o disco
        # está na lista do RPCS3 (games.yml) ou nas pastas dele (games, dev_hdd0\disc)
        elseif ($cheio -match '^(.+?)\\dev_hdd0\\game(\\|$)') {
            $vizinho = Achar-PS3Game $Matches[1]
            if ($vizinho) {
                Msg "a pasta escolhida é a do update/DLC no RPCS3 (dev_hdd0\game), não o disco; usando o disco em $vizinho"
                return $vizinho
            }
            $como = (' (essa é a pasta do update/DLC no RPCS3, dev_hdd0\game, não o disco: o disco é o que você abre no ' +
                     'RPCS3 para jogar, a pasta com PS3_GAME ou o arquivo .iso/.7z/.zip/.rar dele)')
        }
        # reports 129 e 130: a pasta escolhida é a do update (o .pkg do 1.12), não a do disco; o disco costuma estar
        # ao lado, na pasta de cima
        elseif (@(Get-ChildItem -LiteralPath $d -Filter *.pkg -File -ErrorAction SilentlyContinue).Count -gt 0) {
            $pai = Split-Path $cheio
            $vizinho = if ($pai) { Achar-PS3Game $pai } else { $null }
            if ($vizinho) {
                Msg "a pasta escolhida tem o update do jogo (.pkg), não o disco; usando o disco em $vizinho"
                return $vizinho
            }
            $como = (' (essa pasta tem um update do jogo, o .pkg, não o disco: o update 1.04 o gerador baixa sozinho. ' +
                     'Escolha a pasta do disco, a que tem PS3_GAME, ou o arquivo .iso/.7z/.zip/.rar dele)')
        }
        # o que há lá dentro, para quem escolheu uma pasta com nome de .iso ou a pasta errada
        if (-not $como) {
            $dentro = @(Get-ChildItem -LiteralPath $d -Force -ErrorAction SilentlyContinue | Select-Object -First 6 | ForEach-Object Name)
            $como = " (dentro dela há: $(if ($dentro) { $dentro -join ', ' } else { 'nada' }))"
        }
        throw ("não achei o jogo (PS3_GAME com USRDIR) em $d$como. No launcher, clique em `"Escolher o disco de novo`" e " +
               'escolha o arquivo do jogo (.7z, .zip, .rar ou .iso) ou a pasta onde ele foi extraído.')
    }
    return $achou
}
# o jogo já preparado (etapa 2 feita) e o disco informado: se a junção para o PS3_GAME não leva mais ao disco, só ela
# é refeita, sem refazer etapa nenhuma. Quem moveu ou apagou a pasta do disco, ou o tinha num HD agora desligado,
# escolhe o disco de novo no launcher e o jogo volta a achá-lo em segundos (sem ele, o jogo abria e fechava sem dizer
# nada). Uma junção que ainda leva a um disco fica como está, e o teste é o mesmo do Achar-PS3Game (PS3_GAME\USRDIR):
# a 1ª versão exigia o PARAM.SFO e parou o "gerar de novo" de um jogador. rmdir numa junção, mesmo quebrada, apaga só
# a junção; numa pasta de verdade com arquivos, não apaga nada.
#
# A junção aponta sempre para a pasta REAL (Destino-Real segue atalhos e junções até ela) e a atual só é desfeita
# depois de o destino novo estar conferido: no PC de um jogador, o PS3_GAME da pasta do disco era ele mesmo um atalho
# para vfs\PS3_GAME, e refazer a junção para lá fechou um círculo (vfs -> disco -> vfs) e perdeu o caminho do jogo.
function Destino-Real([string]$p) {
    $vistos = @{}
    for ($i = 0; $i -lt 16; $i++) {
        $chave = [IO.Path]::GetFullPath($p).TrimEnd('\').ToLowerInvariant()
        if ($vistos[$chave]) { return $null }   # um círculo de atalhos
        $vistos[$chave] = 1
        $it = Get-Item -LiteralPath $p -Force -ErrorAction SilentlyContinue
        if (-not $it) { return $null }
        if (-not $it.LinkType) { return $it.FullName }
        $t = @($it.Target)[0]
        if (-not $t) { return $null }
        if (-not [IO.Path]::IsPathRooted($t)) { $t = Join-Path (Split-Path $p) $t }
        $p = $t
    }
    return $null
}
function Descrever-Atalhos([string]$p) {
    $partes = @()
    for ($i = 0; $i -lt 6; $i++) {
        $it = Get-Item -LiteralPath $p -Force -ErrorAction SilentlyContinue
        if (-not $it) { $partes += "$p (não existe)"; break }
        if (-not $it.LinkType) { $partes += "$p (pasta)"; break }
        $t = @($it.Target)[0]
        $partes += "$p ($($it.LinkType) para $t)"
        $p = $t
    }
    return ($partes -join ' -> ')
}
# Sem junção: o vfs\PS3_GAME vira um espelho do disco, com as pastas de verdade e cada arquivo ligado ao do disco por
# link físico (o mesmo arquivo com dois nomes: nada é copiado nem ocupa espaço a mais, mas só no mesmo drive). A marca
# diz que a pasta é um espelho do gerador: só uma pasta com ela é apagada (rmdir /s apaga os nomes do espelho; os
# arquivos do disco ficam). Uma pasta de verdade sem a marca pode ser a única cópia do jogador e nunca é apagada.
$MARCA_ESPELHO = '.otavix-espelho'
if (-not ('LinkFisico' -as [type])) { Add-Type @"
using System; using System.Runtime.InteropServices;
public static class LinkFisico {
  [DllImport("kernel32.dll", CharSet = CharSet.Unicode, SetLastError = true)]
  public static extern bool CreateHardLink(string novo, string existente, IntPtr reservado);
}
"@ }
# Report 340 (29/10): o disco em outro drive, sem junção (falta de privilégio), sem link físico (não atravessa
# drives) e sem espaço para a cópia de 34 GB. O jogo então lê o disco direto de onde ele está: vfs\disco_direto.txt
# guarda a pasta que contém o PS3_GAME (o boot do gowa.exe aponta o /dev_bdvd para lá; o update continua no vfs), e o
# vfs\PS3_GAME vira uma pasta mínima com a marca do gerador, o PARAM.SFO e os ícones (o launcher confere o PARAM.SFO).
function Arquivo-Direto { Join-Path (Join-Path $R 'vfs') 'disco_direto.txt' }
function Disco-Direto {
    $f = Arquivo-Direto
    if (-not (Test-Path -LiteralPath $f)) { return $null }
    $raiz = ([IO.File]::ReadAllText($f, [Text.Encoding]::UTF8)).Trim()
    $p = Join-Path $raiz 'PS3_GAME'
    if (Test-Path -LiteralPath (Join-Path $p 'USRDIR') -PathType Container) { return $p }
    return $null
}
# a pasta do disco que o jogo lê: a direta, se houver, senão o vfs\PS3_GAME (junção, espelho ou cópia)
function Pasta-Do-Disco([string]$pg) { $d = Disco-Direto; if ($d) { $d } else { $pg } }
function Ligar-Direto([string]$real, [string]$pg) {
    New-Item -ItemType Directory -Force $pg | Out-Null
    [IO.File]::WriteAllText((Join-Path $pg $MARCA_ESPELHO), "direto para $real`r`n")
    foreach ($a in 'PARAM.SFO', 'ICON0.PNG', 'PIC1.PNG') {
        $o = Join-Path $real $a
        if (Test-Path -LiteralPath $o) { Copy-Item -LiteralPath $o -Destination (Join-Path $pg $a) -Force }
    }
    [IO.File]::WriteAllText((Arquivo-Direto), (Split-Path -Parent $real), (New-Object Text.UTF8Encoding($false)))
    Msg "o disco fica onde está ($real): o jogo lê direto de lá, sem copiar (não mova nem desligue esse drive)"
}
function Tirar-Ligacao([string]$pg) {
    $it = Get-Item -LiteralPath $pg -Force -ErrorAction SilentlyContinue
    if (-not $it) { return }
    if ($it.LinkType) { SemExcecao { cmd /c "rmdir `"$pg`" 2>&1" } | Out-Null; return }   # só a junção
    if (Test-Path -LiteralPath (Join-Path $pg $MARCA_ESPELHO)) { SemExcecao { cmd /c "rmdir /s /q `"$pg`" 2>&1" } | Out-Null }
}
function Espelhar-Disco([string]$de, [string]$para) {
    New-Item -ItemType Directory -Force $para | Out-Null
    [IO.File]::WriteAllText((Join-Path $para $MARCA_ESPELHO), "links físicos para $de`r`n")
    $n = 0
    $pastas = New-Object System.Collections.Generic.Stack[string]
    $pastas.Push($de)
    while ($pastas.Count) {
        $d = $pastas.Pop()
        $alvo = $para + $d.Substring($de.Length)
        [IO.Directory]::CreateDirectory($alvo) | Out-Null
        foreach ($f in [IO.Directory]::GetFiles($d)) {
            if (-not [LinkFisico]::CreateHardLink((Join-Path $alvo ([IO.Path]::GetFileName($f))), $f, [IntPtr]::Zero)) {
                $cod = [Runtime.InteropServices.Marshal]::GetLastWin32Error()
                throw ("$f (erro {0}: {1})" -f $cod, (New-Object ComponentModel.Win32Exception $cod).Message)
            }
            $n++
        }
        foreach ($s in [IO.Directory]::GetDirectories($d)) {
            # uma junção dentro do disco não se atravessa num PC destes: sem ela faltariam arquivos ao jogo
            if ([IO.File]::GetAttributes($s) -band [IO.FileAttributes]::ReparsePoint) { throw "$s é um atalho dentro do disco" }
            $pastas.Push($s)
        }
    }
    return $n
}
# a ligação de $pg ao PS3_GAME do disco ($real), conferida pelo USRDIR. Primeiro a junção: às vezes o Windows deixa
# uma recém-criada que não se atravessa (o destino certo no fsutil, mas nada aparece por ela), e criá-la de novo
# resolve; a espera também cobre um antivírus que segure a junção nova. Num PC onde nenhuma junção abre (um jogador
# com o Windows 11 25H2, 26200: "Junção criada" três vezes e nada dentro dela, para uma pasta de verdade em Downloads),
# o espelho de links físicos. GOWA_TESTE_SEM_JUNCAO=1 (testes) pula a junção, como num PC desses.
function Ligar-Disco([string]$pg, [string]$real) {
    if ([IO.Path]::GetFullPath($real).TrimEnd('\') -ieq [IO.Path]::GetFullPath($pg).TrimEnd('\')) {
        if (Test-Path -LiteralPath (Join-Path $pg 'USRDIR') -PathType Container) { return }
        throw "o disco escolhido é a própria pasta do jogo ($pg). Escolha o arquivo do jogo (.7z, .zip, .rar ou .iso) ou a pasta onde ele foi extraído."
    }
    $saida = @()
    Remove-Item -LiteralPath (Arquivo-Direto) -Force -ErrorAction SilentlyContinue   # o modo direto se refaz se precisar
    $semJuncao = $env:GOWA_TESTE_SEM_JUNCAO -eq '1'
    for ($tentativa = 1; $tentativa -le 3 -and -not $semJuncao; $tentativa++) {
        Tirar-Ligacao $pg
        $saida += SemExcecao { cmd /c "mklink /J `"$pg`" `"$real`" 2>&1" }
        for ($i = 0; $i -lt 10; $i++) {
            if (Test-Path -LiteralPath (Join-Path $pg 'USRDIR') -PathType Container) { return }
            Start-Sleep -Milliseconds 300
        }
        Msg "a junção do disco não abriu (tentativa $tentativa de 3)"
    }
    if (-not $semJuncao) {
        # report 44 (Windows 11 26200 de novo, disco em D: e 18 GB a menos do que a cópia pede): o drive do disco vai
        # para o log, e um link simbólico, que o Windows resolve por outro caminho que a junção, é tentado antes dos
        # links físicos. Ele só se cria com o modo de desenvolvedor ligado ou como administrador; senão segue adiante.
        try { $di = [IO.DriveInfo]([IO.Path]::GetPathRoot([IO.Path]::GetFullPath($real))); Msg "drive do disco: $($di.Name) $($di.DriveType) $($di.DriveFormat)" } catch {}
        Tirar-Ligacao $pg
        $saida += SemExcecao { cmd /c "mklink /D `"$pg`" `"$real`" 2>&1" }
        for ($i = 0; $i -lt 10; $i++) {
            if (Test-Path -LiteralPath (Join-Path $pg 'USRDIR') -PathType Container) { Msg 'disco ligado por link simbólico'; return }
            Start-Sleep -Milliseconds 300
        }
    }
    Msg 'a junção não abre neste PC: ligando os arquivos do disco um a um, por link físico (nada é copiado)'
    Tirar-Ligacao $pg
    if (Test-Path -LiteralPath $pg) {
        throw "não deu para ligar o disco: $pg já existe e não é uma junção nem um espelho do gerador ($(Descrever-Atalhos $pg)). Apague essa pasta e gere de novo."
    }
    try { $n = Espelhar-Disco $real $pg }
    catch {
        $erro = $_.Exception.Message
        Tirar-Ligacao $pg
        # o disco em outro drive (erro 17): o link físico não atravessa drives. Report 27: o disco em J: (pelo jeito
        # um drive de rede ou virtual, onde a junção também não abre) e o port em C:. Copia o disco para o port.
        # Report 32: em H:, o link físico deu "erro 5: Acesso negado"; com o disco em outro drive, qualquer falha leva
        # à cópia (lá o link físico nunca daria certo).
        # Report 123: no mesmo drive, mas dentro do OneDrive (arquivos que podem estar só na nuvem), o link físico deu
        # "privilégios insuficientes": qualquer falha do link leva à cópia.
        # Report 340: em vez de copiar 34 GB, o jogo lê o disco de onde ele está (Ligar-Direto); a cópia fica de reserva
        try {
            Ligar-Direto $real $pg
            if (Disco-Direto) { return }
            throw 'o modo direto não ficou de pé'
        } catch {
            $erro = "$erro; o modo direto falhou: $($_.Exception.Message)"
            Tirar-Ligacao $pg
            Remove-Item -LiteralPath (Arquivo-Direto) -Force -ErrorAction SilentlyContinue
        }
        try {
            Copiar-Disco $real $pg
            if (Test-Path -LiteralPath (Join-Path $pg 'USRDIR') -PathType Container) { Msg "disco copiado de $real"; return }
            throw 'a cópia ficou sem o USRDIR'
        } catch {
            $erro = "$erro; a cópia também falhou: $($_.Exception.Message)"
            Tirar-Ligacao $pg
        }
        throw ("não deu para ligar o disco em $pg ($real): a junção não abre neste PC ($(($saida | Out-String).Trim())) " +
               "e o link físico falhou: $erro. Mova a pasta do jogo para o mesmo drive do port " +
               "($(Split-Path -Qualifier $pg)) e escolha o disco de novo no launcher.")
    }
    if (-not (Test-Path -LiteralPath (Join-Path $pg 'USRDIR') -PathType Container)) {
        throw "não deu para ligar o disco em $pg ($real): o espelho de links físicos ficou sem o USRDIR."
    }
    Msg "disco ligado por links físicos: $n arquivos de $real"
}
# Sem junção e em outro drive: uma cópia do PS3_GAME na pasta do port (~34 GB), conferido o espaço livre antes
# (o tamanho do disco mais 2 GB). A marca é a mesma do espelho: ao escolher outro disco, a cópia é apagada e refeita
# do disco escolhido, que existe (o Ligar-Disco o confere antes). robocopy: códigos de 0 a 7 são sucesso.
function Copiar-Disco([string]$de, [string]$para) {
    $tam = (Get-ChildItem -LiteralPath $de -Recurse -File -Force | Measure-Object -Property Length -Sum).Sum
    $drive = Split-Path -Qualifier $para
    $livre = (Get-PSDrive -Name $drive.TrimEnd(':')).Free
    if ($livre -lt $tam + 2GB) {
        throw ("faltam {0:N1} GB livres em {1} para copiar o disco ({2:N1} GB)" -f (($tam + 2GB - $livre) / 1GB), $drive, ($tam / 1GB))
    }
    Msg ("sem atalho possível para o drive {0}, copiando o disco ({1:N1} GB) para a pasta do port (leva alguns minutos)" -f (Split-Path -Qualifier $de), ($tam / 1GB))
    New-Item -ItemType Directory -Force $para | Out-Null
    [IO.File]::WriteAllText((Join-Path $para $MARCA_ESPELHO), "cópia de $de`r`n")
    $saida = SemExcecao { robocopy $de $para /E /R:1 /W:1 /NP /NFL /NDL /NJH /NJS 2>&1 }
    if ($LASTEXITCODE -ge 8) { throw "o robocopy falhou (código $LASTEXITCODE): $(($saida | Out-String).Trim())" }
}
# Os campos de texto (fmt 0x204) de um PARAM.SFO; vazio se o arquivo não for um SFO.
function Ler-Sfo([string]$arq) {
    $r = @{}
    try {
        $b = [IO.File]::ReadAllBytes($arq)
        if ($b.Length -lt 20 -or [Text.Encoding]::ASCII.GetString($b, 1, 3) -ne 'PSF') { return $r }
        $kt = [BitConverter]::ToUInt32($b, 8); $dt = [BitConverter]::ToUInt32($b, 12); $n = [BitConverter]::ToUInt32($b, 16)
        for ($e = 0; $e -lt $n; $e++) {
            $p = 20 + 16 * $e
            $ko = $kt + [BitConverter]::ToUInt16($b, $p); $fmt = [BitConverter]::ToUInt16($b, $p + 2)
            $ln = [BitConverter]::ToUInt32($b, $p + 4); $do = $dt + [BitConverter]::ToUInt32($b, $p + 12)
            $fim = [Array]::IndexOf($b, [byte]0, [int]$ko)
            if ($fmt -eq 0x204 -and $fim -gt $ko) {
                $r[[Text.Encoding]::ASCII.GetString($b, $ko, $fim - $ko)] = [Text.Encoding]::UTF8.GetString($b, $do, $ln).TrimEnd([char]0)
            }
        }
    } catch { }
    $r
}
# Por que a pasta não é o update 1.04 do BCUS98232 (vazio se for): o PARAM.SFO dela diz o jogo (TITLE_ID), se é update
# (CATEGORY GD; o disco é DG) e a versão (APP_VER).
function Motivo-Update([string]$pasta) {
    $sfo = Join-Path $pasta 'PARAM.SFO'
    if (-not (Test-Path -LiteralPath $sfo -PathType Leaf)) {
        if (Test-Path -LiteralPath (Join-Path $pasta 'PS3_GAME') -PathType Container) { return 'é a pasta do disco, não a do update' }
        return 'a pasta não tem o update (PARAM.SFO e USRDIR\GOWA.SELF)'
    }
    $s = Ler-Sfo $sfo
    if ($s['TITLE_ID'] -ne 'BCUS98232') { return "é de outro jogo ($($s['TITLE_ID']))" }
    if ($s['CATEGORY'] -ne 'GD') { return 'é o disco, não o update' }
    if ($s['APP_VER'] -ne '01.04') { return "é a versão $($s['APP_VER']); o port precisa da 01.04" }
    if (-not (Test-Path -LiteralPath (Join-Path $pasta 'USRDIR\GOWA.SELF') -PathType Leaf)) { return 'falta o USRDIR\GOWA.SELF' }
    ''
}
# O update escolhido no launcher (26/10, report 30: um jogador escolheu ali a mesma pasta do disco e a geração parou).
# Serve o .pkg, a pasta do update instalado e as pastas em volta onde o RPCS3 o guarda (dev_hdd0\game\BCUS98232), ou
# um arquivo de dentro dela. Devolve @{ caminho } com o .pkg ou a pasta, ou @{ motivo } quando nenhuma serve.
function Achar-Update([string]$p) {
    if (Test-Path -LiteralPath $p -PathType Leaf) {
        if ($p -match '\.pkg$') { return @{ caminho = $p } }
        $p = Split-Path -Parent $p
    }
    if (-not (Test-Path -LiteralPath $p -PathType Container)) { return @{ motivo = 'não existe mais' } }
    $motivo = ''
    foreach ($c in @($p, (Join-Path $p 'BCUS98232'), (Join-Path $p 'game\BCUS98232'), (Join-Path $p 'dev_hdd0\game\BCUS98232'),
                     (Split-Path -Parent $p))) {
        if (-not $c -or -not (Test-Path -LiteralPath $c -PathType Container)) { continue }
        $m = Motivo-Update $c
        if (-not $m) { return @{ caminho = $c } }
        if (-not $motivo) { $motivo = $m }
    }
    # um .pkg do jogo solto na pasta, o do 1.04 antes (a versão é conferida depois de extraído)
    $pkg = Get-ChildItem -LiteralPath $p -Filter '*.pkg' -File -ErrorAction SilentlyContinue |
           Where-Object { $_.Name -match 'BCUS98232' } | Sort-Object { $_.Name -notmatch '0104|1\.04' } | Select-Object -First 1
    if ($pkg) { return @{ caminho = $pkg.FullName } }
    @{ motivo = $motivo }
}
# O TITLE_ID do PARAM.SFO de um PS3_GAME ('' sem PARAM.SFO legível).
function Id-Disco([string]$ps3game) {
    $sfo = Join-Path $ps3game 'PARAM.SFO'
    if (-not (Test-Path -LiteralPath $sfo -PathType Leaf)) { return '' }
    [string](Ler-Sfo $sfo)['TITLE_ID']
}
# O disco tem que ser o americano (26/10, report 31: o europeu, BCES01741, gerou o jogo, porque o update 1.04 vem à
# parte, e o jogo lia os arquivos dele, esperava e fechava sem dizer nada). Sem PARAM.SFO legível, segue: uma cópia
# incompleta ainda serve (o jogo tira o título do update).
function Conferir-Regiao([string]$ps3game) {
    $id = Id-Disco $ps3game
    if (-not $id -or $id -eq 'BCUS98232') { return }
    $reg = switch -regex ($id) { '^B[CL]ES' { 'europeia' } '^B[CL]AS' { 'asiática' } '^B[CL]J[SM]' { 'japonesa' } default { 'de outra região' } }
    throw ("este disco é a versão $reg do God of War: Ascension ($id). O port só funciona com a versão americana, " +
           'BCUS98232, a do update 1.04: os arquivos das outras versões são diferentes e o jogo fecha ao abrir. ' +
           'No launcher, clique em "Escolher o disco de novo" e escolha uma cópia do disco americano.')
}
# Reports 62 e 64 (27/10): o jogo abria e fechava sozinho, sem erro. Faltava um arquivo do disco na cópia do jogador
# (EXTRASCIN2.PSARC num, GAME.PSARC no outro: extração ou cópia interrompida), e o jogo sai quando não acha um deles.
# A lista dados\disco_bcus98232.tsv tem o nome e o tamanho de cada arquivo que o jogo lê do disco americano (só isso,
# nada do jogo); o que faltar ou vier com outro tamanho para a geração com a lista.
function Conferir-Arquivos([string]$pg) {
    $lista = Join-Path $R 'dados\disco_bcus98232.tsv'
    if (-not (Test-Path -LiteralPath $lista)) { return }
    $ruins = @()
    foreach ($linha in [IO.File]::ReadAllLines($lista)) {
        $nome, $tam = $linha -split "`t"
        if (-not $nome) { continue }
        $alvo = Join-Path $pg ($nome -replace '/', '\')
        $f = Get-Item -LiteralPath $alvo -Force -ErrorAction SilentlyContinue
        # Reports 161, 162, 163, 166, 174 e 177 (28/10): cópia tirada do PS3 para um pendrive FAT32 (sem arquivos de
        # mais de 4 GB) vem com o EXTRASCIN2.PSARC (9,3 GB) em partes .66600, .66601, ...: junta as partes uma vez.
        if (-not $f -and (Test-Path -LiteralPath "$alvo.66600")) {
            Msg "juntando as partes de $nome (cópia de pendrive FAT32: .66600, .66601, ...)"
            $saida = [IO.File]::Create("$alvo.juntando")
            try {
                for ($k = 0; (Test-Path -LiteralPath ("{0}.666{1:D2}" -f $alvo, $k)); $k++) {
                    $parte = [IO.File]::OpenRead(("{0}.666{1:D2}" -f $alvo, $k))
                    try { $parte.CopyTo($saida, 4MB) } finally { $parte.Dispose() }
                }
            } finally { $saida.Dispose() }
            if ((Get-Item -LiteralPath "$alvo.juntando").Length -eq [int64]$tam) {
                Move-Item -LiteralPath "$alvo.juntando" -Destination $alvo
                Msg "$nome montado das partes"
            } else { Remove-Item -LiteralPath "$alvo.juntando" -Force }
            $f = Get-Item -LiteralPath $alvo -Force -ErrorAction SilentlyContinue
        }
        if (-not $f) { $ruins += "$nome (falta)" }
        elseif ($f.Length -ne [int64]$tam) { $ruins += ("{0} ({1:N0} bytes, deviam ser {2:N0})" -f $nome, $f.Length, [int64]$tam) }
    }
    if ($ruins) {
        throw ("a sua cópia do jogo está incompleta: $($ruins.Count) arquivo(s) do disco faltando ou cortados: " +
               (($ruins | Select-Object -First 6) -join '; ') + $(if ($ruins.Count -gt 6) { '; ...' }) + ". " +
               "Sem eles o jogo abre e fecha sozinho. Extraia ou copie o jogo de novo (confira o espaço livre e se o " +
               'antivírus não apagou nada) e, no launcher, clique em "Escolher o disco de novo".')
    }
    # Reports 153, 155 e 157 (27/10): o jogo abria e fechava sem desenhar nada, com todos os arquivos no tamanho certo.
    # Era uma .iso criptografada (as da Redump): os nomes e tamanhos se leem, mas o conteúdo vem cifrado, e o jogo sai
    # ao ler a lista de pacotes. USRDIR\ARCHIVES é essa lista, em texto ("19", "GAME.PSARC", ...) em todo disco americano.
    $arq = Join-Path $pg 'USRDIR\ARCHIVES'
    if (Test-Path -LiteralPath $arq) {
        $b = [IO.File]::ReadAllBytes($arq)
        $txt = [Text.Encoding]::ASCII.GetString($b)
        if ($b.Length -lt 2 -or $b[0] -lt 0x30 -or $b[0] -gt 0x39 -or $txt -notmatch 'GAME\.PSARC') {
            throw ('a sua cópia do jogo está criptografada: os arquivos estão lá, mas o conteúdo vem cifrado (é o caso ' +
                   'das .iso da Redump, feitas para gravar em disco), e com ele o jogo abre e fecha sozinho. O port precisa ' +
                   'de uma cópia decriptada: a pasta com PS3_GAME tirada do seu PS3 ou do RPCS3, ou uma .iso decriptada. ' +
                   'No launcher, clique em "Escolher o disco de novo" e escolha essa cópia.')
        }
    }
}
function Extrair-Pkg([string]$pkg) {
    $tmp = Join-Path $R 'out\update_pkg'
    Exec $LOG "python `"$R\ps3recomp\tools\pkg_extract.py`" `"$pkg`" `"$tmp`""
    $sfo = Get-ChildItem $tmp -Recurse -Filter PARAM.SFO | Select-Object -First 1
    if (-not $sfo) { throw "o PKG não trouxe PARAM.SFO ($tmp)" }
    $sfo.DirectoryName
}
if ($Disco -and (Test-Path (Join-Path $OUT '2_jogo.ok'))) {
    $pg = Join-Path (Join-Path $R 'vfs') 'PS3_GAME'
    # refeita também quando o disco ligado é de outra região (ver Conferir-Regiao): o jogador escolhe o americano
    # depois, e a ligação que abria levava ainda ao outro
    $pgReal = Pasta-Do-Disco $pg   # o modo direto (report 340): o disco fica no outro drive
    $quebrado = -not (Test-Path -LiteralPath (Join-Path $pgReal 'USRDIR') -PathType Container)
    $idLigado = if ($quebrado) { '' } else { Id-Disco $pgReal }
    # reports 379 e 389 (29/10): com o jogo gerado, "Escolher o disco de novo" também religa quando a cópia ligada
    # tem arquivo faltando ou cortado, ou quando a pasta escolhida é outra (só com junção ou modo direto: o espelho
    # de links físicos e a cópia ficam, salvo arquivo ruim). Um arquivo compactado não entra na comparação (seria
    # extraído de novo); ele religa só nos casos de antes.
    $ruim = $false; $outro = $false
    if (-not $quebrado) {
        try { Conferir-Arquivos $pgReal } catch { $ruim = $true; Msg "a cópia do disco ligada tem problema ($($_.Exception.Message)); ligando o disco escolhido" }
    }
    $itPg = Get-Item -LiteralPath $pg -Force -ErrorAction SilentlyContinue
    if (-not $quebrado -and -not $ruim -and (($itPg -and $itPg.LinkType) -or (Disco-Direto)) -and
        (Test-Path -LiteralPath $Disco -PathType Container)) {
        $f = Achar-PS3Game $Disco
        $atual = Destino-Real $pgReal
        $esc = if ($f) { Destino-Real (Join-Path $f 'PS3_GAME') } else { $null }
        if ($esc -and $atual -and [IO.Path]::GetFullPath($esc).TrimEnd('\') -ine [IO.Path]::GetFullPath($atual).TrimEnd('\')) {
            $outro = $true
            Msg "disco escolhido em outra pasta ($esc; o ligado era $atual)"
        }
    }
    if ($quebrado -or $ruim -or $outro -or ($idLigado -and $idLigado -ne 'BCUS98232')) {
        $Disco = Achar-Disco $Disco
        $alvo = Join-Path $Disco 'PS3_GAME'
        $real = Destino-Real $alvo
        $pgCheio = [IO.Path]::GetFullPath($pg).TrimEnd('\').ToLowerInvariant()
        if (-not $real -or [IO.Path]::GetFullPath($real).TrimEnd('\').ToLowerInvariant() -eq $pgCheio -or
            -not (Test-Path -LiteralPath (Join-Path $real 'USRDIR') -PathType Container)) {
            throw ("o PS3_GAME da pasta escolhida não leva ao jogo: $(Descrever-Atalhos $alvo). " +
                   'No launcher, clique em "Escolher o disco de novo" e escolha o arquivo do jogo (.7z, .zip, .rar ou .iso) ' +
                   'ou a pasta onde ele foi extraído (a que tem PS3_GAME com USRDIR dentro).')
        }
        Conferir-Regiao $real
        Ligar-Disco $pg $real
        Msg "disco ligado de novo: $real"
    }
    Conferir-Arquivos (Pasta-Do-Disco $pg)
}
Etapa '2_jogo' {
    $Disco = Achar-Disco $Disco
    Msg "disco: $Disco"
    $vfs = Join-Path $R 'vfs'
    New-Item -ItemType Directory -Force $vfs | Out-Null
    $pg = Join-Path $vfs 'PS3_GAME'
    # junção para o PS3_GAME do disco, sem copiar 34 GB (ou o espelho de links físicos, num PC onde a junção não
    # abre: ver Ligar-Disco); refeita a cada geração (o disco pode ter mudado de lugar), sempre para a pasta real e
    # conferida antes de desfazer a atual (ver Destino-Real acima). rmdir numa junção apaga só a junção.
    $real = Destino-Real (Join-Path $Disco 'PS3_GAME')
    if (-not $real -or -not (Test-Path -LiteralPath (Join-Path $real 'USRDIR') -PathType Container)) {
        throw "o PS3_GAME da pasta escolhida não leva ao jogo: $(Descrever-Atalhos (Join-Path $Disco 'PS3_GAME')). Escolha o arquivo do jogo (.7z, .zip, .rar ou .iso) ou a pasta onde ele foi extraído."
    }
    Conferir-Regiao $real
    Ligar-Disco $pg $real
    Conferir-Arquivos (Pasta-Do-Disco $pg)

    # o update escolhido (ver Achar-Update); se não for o 1.04, o gerador baixa o oficial, como no Automático
    $upd = ''
    if ($Update -ne 'auto') {
        $u = Achar-Update $Update
        if ($u.caminho -and $u.caminho -match '\.pkg$') {
            $tmp = Extrair-Pkg $u.caminho
            $m = Motivo-Update $tmp
            $u = if ($m) { @{ motivo = "o .pkg $m" } } else { @{ caminho = $tmp } }
        }
        if ($u.caminho) {
            $upd = $u.caminho
            Msg "update: $upd"
        } else {
            Msg "o update escolhido ($Update) não serve: $($u.motivo). Baixando o update 1.04 oficial no lugar dele (no launcher, deixe o update em Automático para não ver este aviso)"
            $Update = 'auto'
        }
    }
    if ($Update -eq 'auto') {
        $Update = Join-Path $R 'dl\BCUS98232_update_1.04.pkg'
        if (-not (Test-Path -LiteralPath $Update) -or (Get-FileHash -LiteralPath $Update -Algorithm SHA1).Hash.ToLower() -ne $UPDATE_SHA1) {
            Msg 'baixando o update 1.04 oficial do servidor da PlayStation (235 MB)'
            Baixar $UPDATE_URL $Update
        }
        if ((Get-FileHash -LiteralPath $Update -Algorithm SHA1).Hash.ToLower() -ne $UPDATE_SHA1) {
            Remove-Item -LiteralPath $Update -Force
            throw 'o update baixado não confere (SHA-1); rode de novo'
        }
        $upd = Extrair-Pkg $Update
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
    function Pup-Oficial {
        $pup = Join-Path $R 'dl\PS3UPDAT.PUP'
        if (-not (Test-Path -LiteralPath $pup)) {
            $lista = Join-Path $R 'dl\ps3-updatelist.txt'
            Baixar $PUP_LISTA $lista | Out-Null
            $url = [regex]::Match((Get-Content $lista -Raw), 'CDN=([^;\s]+/PS3UPDAT\.PUP)').Groups[1].Value
            if (-not $url) { throw "a lista de atualização do PS3 não trouxe o PS3UPDAT.PUP ($PUP_LISTA)" }
            Msg "baixando o firmware oficial do PS3 (PS3UPDAT.PUP, ~200 MB) de $url"
            Baixar $url $pup | Out-Null
        }
        $pup
    }
    # o libsre.prx a partir de uma pasta com o firmware instalado (RPCS3), do PS3UPDAT.PUP ou de um libsre.sprx
    function Libsre-De($origem) {
        $sprx = $origem
        if (Test-Path -LiteralPath $origem -PathType Container) {
            $sprx = Get-ChildItem -LiteralPath $origem -Recurse -Filter libsre.sprx -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty FullName
            if (-not $sprx) { throw "não achei libsre.sprx em $origem" }
        } elseif ($origem -match '\.pup$') {
            # o arquivo do site da PlayStation: o libsre.sprx sai dos pacotes do dev_flash de dentro dele
            $sprx = Join-Path $fw 'libsre.sprx'
            Exec $LOG "python `"$R\scripts\gpl\pup_libsre.py`" `"$origem`" `"$sprx`" --vault `"$ch`" --baixar"
        } elseif (-not (Test-Path -LiteralPath $origem -PathType Leaf)) {
            throw "$origem não existe"
        }
        Exec $LOG "python `"$R\scripts\gpl\unself.py`" `"$sprx`" `"$fw\libsre.prx`" --vault `"$ch`" --baixar"
        $h = (Get-FileHash "$fw\libsre.prx" -Algorithm SHA256).Hash.ToLower()
        if ($h -ne $SHA_LIBSRE) { throw "libsre.prx diferente do esperado (sha256 $h)" }
    }
    # reports 111, 113, 124 e 127 (27/10): no campo "Firmware" do launcher escolheram a pasta do jogo, a PKGDIR, um
    # PS3GAME que não existe ou um libsre de outra versão. O que foi escolhido e não serve vira só um aviso, e o
    # gerador usa o firmware oficial, que é o padrão.
    $feito = $false
    if ($Firmware -ne 'auto') {
        try { Libsre-De $Firmware; $feito = $true }
        catch { Msg "o firmware escolhido não serviu ($(($_.Exception.Message -split "`n")[0])); usando o firmware oficial do PS3" }
    }
    if (-not $feito) { Libsre-De (Pup-Oficial) }
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
    # report 128: a pasta de uma geração anterior sem permissão de leitura (criada como outro usuário ou como
    # administrador): apaga para refazer; se nem isso der, diz o que fazer
    if (Test-Path -LiteralPath $sw) {
        try { $null = Get-ChildItem -LiteralPath $sw -ErrorAction Stop }
        catch {
            Msg "a pasta $sw de uma geração anterior não abre ($($_.Exception.Message)); apagando para refazer"
            SemExcecao { cmd /c "icacls `"$sw`" /reset /t /c /q 2>&1" } | Out-Null
            SemExcecao { cmd /c "rmdir /s /q `"$sw`" 2>&1" } | Out-Null
            # reports 279 e 288: nem assim sai (dono é outro usuário/administrador): gera numa pasta nova ao lado
            # (a ferramenta só escreve aqui e o resultado é copiado para port\spu)
            $n = 2
            while ((Test-Path -LiteralPath $sw) -and
                   -not $(try { $null = Get-ChildItem -LiteralPath $sw -ErrorAction Stop; $true } catch { $false })) {
                if ($n -gt 9) {
                    throw ("a pasta $sw ficou sem permissão (foi criada por outro usuário ou como administrador). " +
                           'Apague a pasta out dentro da pasta do port pelo Explorer (ele pede a permissão) e gere de novo.')
                }
                $sw = Join-Path $R "out\spu_workloads$n"
                $n++
            }
            Msg "gerando em $sw"
        }
    }
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
# A versão da build (30/10, pedido do usuário): GOWA_BUILD=auto|portatil|nativa (a opção do launcher; o -Build da
# linha de comando, se dado, vale mais). auto = a detecção: AVX-512 -> nativa (-march=native), senão portátil
# (x86-64-v3); processador sem nome comercial -> portátil. Trocar de versão recompila (as etapas 6 e 7 voltam).
# Se a nativa não compilar, a geração cai para a portátil com aviso. O resultado vai para port\build.txt
# (escolhida/usada/pasta/exe/motivo, uma chave=valor por linha): o launcher abre esse exe e manda no report.
$llvm = Join-Path $VS 'VC\Tools\Llvm\x64\bin'
$escolha = if ($PSBoundParameters.ContainsKey('Build')) { $Build } elseif ($env:GOWA_BUILD) { $env:GOWA_BUILD.Trim().ToLower() } else { 'auto' }
if ($escolha -eq 'avx512') { $escolha = 'nativa' }
if ($escolha -notin @('auto', 'portatil', 'nativa')) { Msg "GOWA_BUILD='$escolha' não existe (auto, portatil, nativa): usando auto"; $escolha = 'auto' }
$motivo = ''
if ($escolha -eq 'auto') {
    $def = (cmd /c "echo.| `"$llvm\clang.exe`" -march=native -dM -E -x c - 2>nul") -join "`n"
    if ($def -match '__AVX512F__' -and $def -match '__AVX512BW__' -and $def -match '__AVX512VL__' -and $def -match '__AVX512DQ__') {
        $alvo = 'nativa'; $motivo = 'automático: o processador tem AVX-512'
    } else { $alvo = 'portatil'; $motivo = 'automático: sem AVX-512' }
    # reports 461/462: "Genuine Intel(R) CPU 0000 @ 2.60GHz" (amostra de engenharia, não se identifica) anuncia AVX-512
    # e o -march=native liga o que a CPUID diz; o jogo fechou com 0xC000001D (instrução ilegal) logo que as SPUs
    # começaram. Processador que não diz o nome comercial fica com a build portátil (AVX2), a que todos os outros usam.
    $nomeCpu = "$((Get-CimInstance Win32_Processor -ErrorAction SilentlyContinue | Select-Object -First 1).Name)".Trim()
    if ($alvo -eq 'nativa' -and $nomeCpu -match '^Genuine Intel\(R\) CPU|\b0000 @') {
        Msg "processador sem nome comercial ($nomeCpu, amostra de engenharia): build portátil (AVX2) em vez da nativa"
        $alvo = 'portatil'; $motivo = 'automático: processador sem nome comercial (amostra de engenharia)'
    }
} else { $alvo = $escolha; $motivo = 'escolhida pelo jogador' }
# sem AVX-512: x86-64-v3 (AVX2), a build testada nos PCs de teste, e não -march=native: tira uma variável dos
# defeitos gráficos vistos num Ryzen 5 5600X (cores de vértice e texturas erradas) enquanto a causa é investigada
function Pasta-Build([string]$a) { if ($a -eq 'nativa') { 'build_native' } else { 'build_v3rz' } }
function Args-Build([string]$a) { if ($a -eq 'nativa') { 'native' } else { 'x86-64-v3 rz' } }
$bdir = Pasta-Build $alvo
# a geração anterior: com a mesma escolha e o exe dela no lugar, fica a build usada (inclusive a portátil que
# substituiu uma nativa que não compilou); escolha diferente ou exe sumido, compila de novo
$infoBuild = Join-Path $R 'port\build.txt'
$antes = @{}
if (Test-Path $infoBuild) { foreach ($l in (Get-Content -LiteralPath $infoBuild -Encoding UTF8)) { if ($l -match '^(\w+)=(.*)$') { $antes[$matches[1]] = $matches[2] } } }
if ($antes['escolhida'] -eq $escolha -and $antes['pasta'] -and (Test-Path (Join-Path $R "port\$($antes['pasta'])\gowa.exe"))) {
    $bdir = $antes['pasta']; $alvo = if ($bdir -eq 'build_native') { 'nativa' } else { 'portatil' }
    if ($antes['motivo']) { $motivo = $antes['motivo'] }
} elseif (-not (Test-Path (Join-Path $R "port\$bdir\gowa.exe")) -or ($antes['pasta'] -and $antes['pasta'] -ne $bdir)) {
    if (Test-Path (Join-Path $OUT '6_compilar.ok')) { Msg "versão da build: $(if ($alvo -eq 'nativa') { 'nativa' } else { 'portátil' }) (outra que a anterior): compilando de novo" }
    Remove-Item -LiteralPath (Join-Path $OUT '6_compilar.ok'), (Join-Path $OUT '7_jogar.ok') -Force -ErrorAction SilentlyContinue
}
# report 508: arquivos com data no futuro (zip extraído num fuso a oeste de quem o fez, relógio acertado no meio da
# geração) deixam o build.ninja sempre mais velho que as entradas dele, e o ninja reconfigura até desistir
# ("manifest 'build.ninja' still dirty after 100 tries"). Traz essas datas para agora; devolve quantos arquivos.
function Corrigir-Datas {
    $agora = Get-Date; $n = 0
    foreach ($p in 'port', 'ps3recomp', 'scripts') {
        Get-ChildItem -LiteralPath (Join-Path $R $p) -Recurse -File -Force -ErrorAction SilentlyContinue |
            Where-Object { $_.LastWriteTime -gt $agora.AddMinutes(1) } |
            ForEach-Object { try { $_.LastWriteTime = $agora; $n++ } catch { } }
    }
    return $n
}
# reports 536/537/559/560/563: o launcher extrai a atualização com a data do commit, e um commit publicado horas
# depois fica mais velho que os .obj de uma geração feita nesse meio-tempo; o ninja não recompila o arquivo novo e o
# link junta objetos de versões diferentes (símbolo indefinido, ou pior, código velho que liga). O conteúdo de cada
# fonte do repositório fica anotado na pasta da build depois de compilar; o que mudou desde então vai para agora.
# Sem a anotação (build de antes desta correção), todos vão para agora: uma compilação completa, uma vez.
function Fontes-Repo {
    $p = Join-Path $R 'port'
    @(Get-ChildItem -LiteralPath $p -File -Force -ErrorAction SilentlyContinue) +
    @(Get-ChildItem -LiteralPath (Join-Path $p 'edge') -Recurse -File -Force -ErrorAction SilentlyContinue) +
    @(Get-ChildItem -LiteralPath (Join-Path $p 'spu') -Directory -Filter 'gowanat_*' -Force -ErrorAction SilentlyContinue |
      ForEach-Object { Get-ChildItem -LiteralPath $_.FullName -Recurse -File -Force -ErrorAction SilentlyContinue }) +
    @(Get-ChildItem -LiteralPath (Join-Path $R 'ps3recomp') -Recurse -File -Force -ErrorAction SilentlyContinue) +
    @(Get-ChildItem -LiteralPath (Join-Path $R 'scripts') -Recurse -File -Force -ErrorAction SilentlyContinue)
}
function Hashes-Fontes {
    $md5 = [Security.Cryptography.MD5]::Create(); $h = @{}
    try {
        foreach ($f in Fontes-Repo) {
            try {
                $b = [IO.File]::ReadAllBytes($f.FullName)
                $h[$f.FullName.Substring($R.Length).TrimStart('\')] = [BitConverter]::ToString($md5.ComputeHash($b)).Replace('-', '')
            } catch { }
        }
    } finally { $md5.Dispose() }
    return $h
}
function Atualizar-Datas-Fontes([string]$bd) {
    $pasta = Join-Path $R "port\$bd"
    if (-not (Test-Path (Join-Path $pasta 'build.ninja'))) { return 0 }   # build nova: o ninja compila tudo
    $anot = @{}
    $arq = Join-Path $pasta 'fontes_md5.txt'
    if (Test-Path $arq) {
        foreach ($l in [IO.File]::ReadAllLines($arq)) { if ($l -match '^([0-9A-F]{32}) (.+)$') { $anot[$matches[2]] = $matches[1] } }
    }
    $agora = Get-Date; $n = 0
    foreach ($kv in (Hashes-Fontes).GetEnumerator()) {
        if ($anot[$kv.Key] -ne $kv.Value) {
            try { (Get-Item -LiteralPath (Join-Path $R $kv.Key) -Force).LastWriteTime = $agora; $n++ } catch { }
        }
    }
    return $n
}
function Anotar-Fontes([string]$bd) {
    $linhas = foreach ($kv in (Hashes-Fontes).GetEnumerator()) { "$($kv.Value) $($kv.Key)" }
    try { [IO.File]::WriteAllLines((Join-Path $R "port\$bd\fontes_md5.txt"), [string[]]@($linhas), (New-Object Text.UTF8Encoding $false)) } catch { }
}
# report 538: um .obj zerado (o PC desligou ou travou no meio da compilação) é mais novo que o fonte e o ninja não o
# refaz; o llvm-lib para em "unknown machine: 0" e o lld-link o lê como vazio (símbolo indefinido). Um .obj de
# verdade começa com a máquina (x64) ou com a assinatura do bigobj; quatro bytes zero é arquivo perdido.
function Apagar-Obj-Zerados([string]$bd) {
    $n = 0; $b = New-Object byte[] 4
    # (o -Include não vale com -LiteralPath no PowerShell 5.1: o filtro é pela extensão)
    foreach ($f in @(Get-ChildItem -LiteralPath (Join-Path $R "port\$bd") -Recurse -File -Force -ErrorAction SilentlyContinue |
                     Where-Object { $_.Extension -eq '.obj' -or $_.Extension -eq '.lib' })) {
        $ruim = $f.Length -lt 4
        if (-not $ruim) {
            try {
                $fs = [IO.File]::OpenRead($f.FullName)
                try { $null = $fs.Read($b, 0, 4) } finally { $fs.Dispose() }
                $ruim = ($b[0] -eq 0 -and $b[1] -eq 0 -and $b[2] -eq 0 -and $b[3] -eq 0)
            } catch { }
        }
        if ($ruim) { try { Remove-Item -LiteralPath $f.FullName -Force; $n++ } catch { } }
    }
    return $n
}
# o texto apareceu no $LOG depois do byte $de (só a saída da última tentativa)
function Novo-No-Log([long]$de, [string]$texto) {
    try {
        $fs = New-Object IO.FileStream($LOG, 'Open', 'Read', 'ReadWrite')
        try { $null = $fs.Seek($de, 'Begin'); $s = (New-Object IO.StreamReader($fs)).ReadToEnd() } finally { $fs.Dispose() }
        return $s.Contains($texto)
    } catch { return $false }
}
# report 511: o "setup.exe modify ... --wait" pelo cmd voltou na hora (o setup.exe pede a elevação e devolve o
# controle) e a compilação de novo veio antes do componente. Agora o pedido de administrador é do próprio
# Start-Process, que espera; depois espera os processos do instalador terminarem (até 40 min).
function Instalar-ComponenteVS([string]$inst, [string]$comp) {
    $logVs = Join-Path $OUT 'vs_instala.log'
    $args_ = @('modify', '--installPath', "`"$VS`"", '--add', $comp, '--passive', '--norestart')
    try {
        $p = Start-Process -FilePath $inst -ArgumentList $args_ -Verb RunAs -Wait -PassThru -ErrorAction Stop
        Anotar $logVs "$(Get-Date) setup.exe $($args_ -join ' '): código $($p.ExitCode)"
    } catch {
        # o jogador recusou a permissão de administrador (ou o Windows não deixou pedir)
        Anotar $logVs "$(Get-Date) setup.exe $($args_ -join ' '): não abriu ($($_.Exception.Message))"
        Msg "o instalador do Visual Studio não abriu ($($_.Exception.Message))"
        return
    }
    $t0 = Get-Date
    while (((Get-Date) - $t0).TotalMinutes -lt 40) {
        $vivos = @(Get-CimInstance Win32_Process -ErrorAction SilentlyContinue | Where-Object {
            $_.Name -in 'vs_installer.exe', 'vs_installershell.exe', 'vs_setup_bootstrapper.exe' -or
            ($_.Name -eq 'setup.exe' -and (-not $_.ExecutablePath -or $_.ExecutablePath -like '*\Microsoft Visual Studio\Installer\*')) })
        if (-not $vivos.Count) { break }
        Start-Sleep -Seconds 5
    }
}
# o kernel32.lib de algum Windows SDK 10/11 (o que o vcvars64.bat procura)
function Tem-SdkWindows {
    return [bool](Get-ChildItem -Path "${env:ProgramFiles(x86)}\Windows Kits\10\Lib\*\um\x64\kernel32.lib" -ErrorAction SilentlyContinue)
}
# uma build (os passos de antes: SDK/CMake que faltam, -j2 se o ninja parar); devolve se o gowa.exe saiu
function Compilar-Build([string]$a) {
    $bd = Pasta-Build $a; $ba = Args-Build $a
    if ($a -eq 'nativa' -and $env:GOWA_TESTE_NATIVA_FALHA -eq '1') { Msg 'teste: a nativa "falhou" (GOWA_TESTE_NATIVA_FALHA)'; return $false }
    Msg "build: $a (código SPU com $(if ($a -eq 'portatil') { '-march=x86-64-v3, contas de float em para-zero' } else { '-march=native' }))"
    $env:GOWA_JOBS = "$Jobs"
    $exe = Join-Path $R "port\$bd\gowa.exe"
    $nf = Corrigir-Datas
    if ($nf -gt 0) { Msg "$nf arquivo(s) com data no futuro (relógio ou fuso do zip): data corrigida para agora" }
    $nm = Atualizar-Datas-Fontes $bd
    if ($nm -gt 0) { Msg "$nm arquivo(s) do código mudaram desde a última compilação: serão compilados de novo" }
    $nz = Apagar-Obj-Zerados $bd
    if ($nz -gt 0) { Msg "$nz arquivo(s) da compilação anterior estavam corrompidos (zerados): serão compilados de novo" }
    # o build_portatil.cmd sai com 0 mesmo quando o ninja falha, e o ninja não apaga o gowa.exe de antes: numa
    # atualização, um erro de compilação deixava o exe velho e a etapa dizia "ok". O exe de antes fica de lado e só
    # volta se a compilação não gerar outro (o jogo continua jogável e a etapa mostra o erro).
    $ant = Join-Path $R "port\$bd\gowa_anterior.exe"
    if (Test-Path $exe) { Move-Item -LiteralPath $exe -Destination $ant -Force }
    $l0 = (Get-Item -LiteralPath $LOG).Length
    Exec $LOG "`"$R\scripts\build_portatil.cmd`" $ba"
    # o laço do ninja mesmo assim (relógio do Windows muito errado, ou datas que não deu para corrigir): a pasta da
    # build some e configura de novo uma vez
    if (-not (Test-Path $exe) -and (Novo-No-Log $l0 'still dirty after')) {
        Msg 'o ninja não parou de reconfigurar (datas dos arquivos): apagando a pasta da build e compilando de novo'
        $null = Corrigir-Datas
        Remove-Item -LiteralPath (Join-Path $R "port\$bd") -Recurse -Force -ErrorAction SilentlyContinue
        Exec $LOG "`"$R\scripts\build_portatil.cmd`" $ba"
    }
    # report 402: Visual Studio sem o Windows SDK (kernel32.lib); report 452: sem o "C++ CMake tools" (o cmake/ninja).
    # Pede ao instalador do VS o componente que falta e tenta de novo.
    $faltaSdk = -not (Test-Path $exe) -and (Select-String -LiteralPath $LOG -Pattern 'GOWA_ERRO_SDK' -SimpleMatch -Quiet)
    $faltaCmake = -not (Test-Path $exe) -and (Select-String -LiteralPath $LOG -Pattern 'GOWA_ERRO_CMAKE' -SimpleMatch -Quiet)
    if ($faltaSdk -or $faltaCmake) {
        $inst = "${env:ProgramFiles(x86)}\Microsoft Visual Studio\Installer\setup.exe"
        if (Test-Path $inst) {
            $comp = if ($faltaSdk) { 'Microsoft.VisualStudio.Component.Windows11SDK.22621' } else { 'Microsoft.VisualStudio.Component.VC.CMake.Project' }
            Msg ("o Visual Studio está sem o {0}: instalando o componente (alguns minutos; o Windows pede permissão)" -f
                 $(if ($faltaSdk) { 'Windows SDK' } else { 'C++ CMake tools' }))
            Instalar-ComponenteVS $inst $comp
            if ($faltaSdk -and -not (Tem-SdkWindows)) {
                Msg ('o Windows SDK continua faltando. Abra o "Visual Studio Installer", clique em Modificar no Build Tools ' +
                     '2022 e marque "SDK do Windows 11" (em Componentes individuais); depois gere de novo')
            }
            Remove-Item -LiteralPath (Join-Path $R "port\$bd") -Recurse -Force -ErrorAction SilentlyContinue
            Exec $LOG "`"$R\scripts\build_portatil.cmd`" $ba"
        }
    }
    # report 415: o ninja parou no meio dos arquivos grandes da PPU (provável falta de memória com muitos clang em
    # paralelo). A compilação continua de onde parou com 2 de cada vez.
    if (-not (Test-Path $exe) -and (Test-Path (Join-Path $R "port\$bd\build.ninja"))) {
        Msg 'a compilação parou; tentando de novo com 2 arquivos por vez (menos memória)'
        $env:GOWA_JOBS = '2'
        Exec $LOG "`"$R\scripts\build_portatil.cmd`" $ba"
    }
    if (Test-Path $exe) { Remove-Item -LiteralPath $ant -Force -ErrorAction SilentlyContinue; Anotar-Fontes $bd; return $true }
    if (Test-Path $ant) { Move-Item -LiteralPath $ant -Destination $exe -Force }
    return $false
}
Etapa '6_compilar' {
    $env:GOWA_VS = $VS
    $env:FFMPEG_DIR = Join-Path $R "dl\ffmpeg9\$FFMPEG_NOME"
    # reports 398/402 (30/10): o build_portatil.cmd roda o vcvars64 com o PATH só do Windows; o Python volta por aqui
    # reports 470/473/474: sem Python escolhido ($py nulo) o Split-Path parava a etapa; o build_portatil.cmd usa então
    # o PATH do jogador, como antes do 932cbf8
    # o mesmo Python vai para o cmake (GOWA_PY_EXE), em vez do find_package(Python3) procurar outro por conta própria
    if ($py) { $env:GOWA_PY_DIR = Split-Path -Parent $py; $env:GOWA_PY_EXE = $py }
    else { Remove-Item Env:GOWA_PY_DIR, Env:GOWA_PY_EXE -ErrorAction SilentlyContinue }
    $ok = $false
    try { $ok = Compilar-Build $alvo } catch { if ($alvo -ne 'nativa') { throw }; Anotar $LOG "`r`n=== a nativa parou: $_" }
    if (-not $ok -and $alvo -eq 'nativa') {
        # a nativa não compilou: a portátil, com aviso (o jogo continua jogável)
        Msg 'AVISO: a build nativa não compilou; gerando a portátil (AVX2) no lugar'
        $script:alvo = 'portatil'; $script:bdir = Pasta-Build 'portatil'
        $script:motivo = "a nativa não compilou ($motivo); portátil no lugar"
        $ok = Compilar-Build 'portatil'
    }
    if (-not $ok) {
        # o launcher só manda o fim do log: as linhas de erro vão para o fim e para a mensagem
        $err = @(Select-String -LiteralPath $LOG -Pattern 'FAILED:|error:|fatal error|GOWA_ERRO_|LINK : |lld-link: error|reconhecido como|incorreta|CMake Error' |
                 Select-Object -First 30 | ForEach-Object { $_.Line.Substring(0, [Math]::Min(300, $_.Line.Length)) })
        Anotar $LOG ("`r`n=== erros da compilação (primeiras linhas)`r`n" + ($err -join "`r`n"))
        throw ("o gowa.exe não foi gerado (veja $LOG)" + $(if ($err) { "`r`n" + (($err | Select-Object -First 8) -join "`r`n") } else { '' }))
    }
}
# o launcher abre o primeiro exe que existir (build_native antes da build_v3rz): o exe da outra versão (de uma
# geração anterior, reports 461/462) fica renomeado, e o build.txt diz qual abrir
foreach ($o in @('build_native', 'build_v3rz')) {
    $velho = Join-Path $R "port\$o\gowa.exe"
    if ($o -ne $bdir -and (Test-Path $velho)) {
        Move-Item -LiteralPath $velho -Destination "$velho.antigo" -Force -ErrorAction SilentlyContinue
        Msg "a $o de antes ficou como gowa.exe.antigo (o jogo abre a $bdir)"
    }
}
$usada = if ($bdir -eq 'build_native') { 'nativa' } else { 'portatil' }
[IO.File]::WriteAllLines($infoBuild, [string[]]@("escolhida=$escolha", "usada=$usada", "pasta=$bdir", "exe=port/$bdir/gowa.exe", "motivo=$motivo"), (New-Object Text.UTF8Encoding $false))   # UTF-8 sem BOM
Msg "versão da build: escolhida $escolha, usada $usada ($motivo)"

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
# ---------------------------------------------------------------------------- shaders pré-compilados
# Os shaders do jogo traduzidos e compilados (o DXBC, que serve a qualquer placa), juntados dos PCs de teste por
# scripts\pacote_shaders.py, vêm de um asset da release "shaders" do repositório: sem eles, cada shader novo
# custa ~40-75 ms na primeira vez que aparece e o jogo engasga ao entrar numa área nova. Vão para o cache do jogo
# (o GOWA_CACHE_DIR do launcher, <port>\cache) sem sobrescrever o que o PC do jogador já compilou; um shader que
# não esteja no pacote é compilado na hora, como antes. Roda a cada geração, baixa só quando o pacote mudou, e
# uma falha aqui não para nada. Primeiro o servidor do launcher (Cloudflare), depois a release do GitHub.
$SHADERS_URLS = @('https://otavix-launcher.otavix-launcher-servidor.workers.dev/v1/arquivos/shaders/otavix010/gow-ascension-pc',
                  'https://github.com/otavix010/gow-ascension-pc/releases/download/shaders')
function Instalar-Shaders([string]$SHADERS_URL) {
    $cache = Join-Path $R 'cache\rsx_shader_cache_v2'
    $marca = Join-Path $R 'cache\shaders_pacote.txt'
    $dl = Join-Path $R 'dl'
    New-Item -ItemType Directory -Force $cache, $dl | Out-Null
    $shaFile = Join-Path $dl 'shaders.zip.sha256'
    Baixar "$SHADERS_URL/shaders.zip.sha256" $shaFile
    $sha = ((Get-Content -LiteralPath $shaFile -Raw) -split '\s+')[0].Trim().ToLower()
    if ($sha -notmatch '^[0-9a-f]{64}$') { throw "o sha256 do pacote de shaders veio estranho" }
    if ((Test-Path -LiteralPath $marca) -and ((Get-Content -LiteralPath $marca -Raw).Trim() -eq $sha)) {
        Msg 'shaders pré-compilados: já estão no cache'
        return
    }
    $zip = Join-Path $dl 'shaders.zip'
    Msg 'baixando os shaders pré-compilados (o jogo engasga menos ao entrar em áreas novas)'
    Baixar "$SHADERS_URL/shaders.zip" $zip
    $got = (Get-FileHash -LiteralPath $zip -Algorithm SHA256).Hash.ToLower()
    if ($got -ne $sha) { Remove-Item -LiteralPath $zip -Force; throw "o pacote de shaders chegou diferente do esperado" }
    Add-Type -AssemblyName System.IO.Compression.FileSystem
    $z = [IO.Compression.ZipFile]::OpenRead($zip)
    $novos = 0
    try {
        foreach ($e in $z.Entries) {
            # só os arquivos de shader da pasta do pacote, pelo nome: nada fora do cache
            if ($e.FullName -notmatch '^rsx_shader_cache_v2/([VP]_[0-9a-f]{16}_[0-9a-f]{8}\.bin)$') { continue }
            $dest = Join-Path $cache $Matches[1]
            if (Test-Path -LiteralPath $dest) { continue }
            [IO.Compression.ZipFileExtensions]::ExtractToFile($e, $dest)
            $novos++
        }
    } finally { $z.Dispose() }
    Remove-Item -LiteralPath $zip -Force
    Set-Content -LiteralPath $marca -Value $sha -Encoding ASCII
    Msg "shaders pré-compilados: $novos novos no cache"
}
$falhas = @()
foreach ($u in $SHADERS_URLS) {
    try { Instalar-Shaders $u; $falhas = @(); break } catch { $falhas += $_.Exception.Message }
}
if ($falhas) { Msg "shaders pré-compilados não instalados ($($falhas -join '; ')); o jogo compila cada um na primeira vez" }

Msg "pronto. Para jogar: $R\jogar.cmd  (log em out\jogo.log)"
