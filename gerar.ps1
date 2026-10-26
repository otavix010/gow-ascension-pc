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
    if ($curl) {
        SemExcecao { & $curl.Source -L --fail --retry 3 --silent --show-error -o $part $url }
        if ($LASTEXITCODE -ne 0) { throw "falhou o download de $url (curl $LASTEXITCODE)" }
    } else {
        $ProgressPreference = 'SilentlyContinue'
        Invoke-WebRequest -Uri $url -OutFile $part -UseBasicParsing
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
    SemExcecao { & python -c "import Crypto" 2>$null }
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
# a pasta do jogo: a que contém PS3_GAME\USRDIR, ou uma até dois níveis abaixo da escolhida
function Achar-PS3Game([string]$raiz) {
    if (Test-Path -LiteralPath (Join-Path $raiz 'PS3_GAME\USRDIR') -PathType Container) { return $raiz }
    # a pasta escolhida já é o próprio PS3_GAME (com USRDIR dentro), ou o USRDIR dele: o disco é a pasta de cima
    # (um jogador escolheu "J:\JOGOS RPCS3\BCUS98232\PS3_GAME" e o gerador recusava toda vez)
    $nome = Split-Path -Leaf $raiz.TrimEnd('\')
    if ($nome -ieq 'PS3_GAME' -and (Test-Path -LiteralPath (Join-Path $raiz 'USRDIR') -PathType Container)) { return (Split-Path $raiz.TrimEnd('\')) }
    if ($nome -ieq 'USRDIR' -and (Split-Path -Leaf (Split-Path $raiz.TrimEnd('\'))) -ieq 'PS3_GAME') { return (Split-Path (Split-Path $raiz.TrimEnd('\'))) }
    $p = Get-ChildItem -LiteralPath $raiz -Directory -Recurse -Depth 2 -Filter PS3_GAME -ErrorAction SilentlyContinue |
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
    $achou = Achar-PS3Game $d
    if (-not $achou) {
        # uma pasta com só o arquivo do disco dentro
        $arq = @(Get-ChildItem -LiteralPath $d -File -ErrorAction SilentlyContinue | Where-Object { $_.Name -match '\.(7z|zip|rar|iso)$' })
        if ($arq.Count -eq 1) { return (Achar-Disco $arq[0].FullName) }
        # a PS3_GAME there that is a link leading nowhere (moved, or a circle back to the port's vfs): said as it is
        $como = ''
        if (Test-Path -LiteralPath (Join-Path $d 'PS3_GAME')) { $como = " (o PS3_GAME de lá é um atalho que não leva ao jogo: $(Descrever-Atalhos (Join-Path $d 'PS3_GAME')))" }
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
        if ($erro -match 'erro 17:') {
            try {
                Copiar-Disco $real $pg
                if (Test-Path -LiteralPath (Join-Path $pg 'USRDIR') -PathType Container) { Msg "disco copiado de $real"; return }
                throw 'a cópia ficou sem o USRDIR'
            } catch {
                $erro = "$erro; a cópia também falhou: $($_.Exception.Message)"
                Tirar-Ligacao $pg
            }
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
function Extrair-Pkg([string]$pkg) {
    $tmp = Join-Path $R 'out\update_pkg'
    Exec $LOG "python `"$R\ps3recomp\tools\pkg_extract.py`" `"$pkg`" `"$tmp`""
    $sfo = Get-ChildItem $tmp -Recurse -Filter PARAM.SFO | Select-Object -First 1
    if (-not $sfo) { throw "o PKG não trouxe PARAM.SFO ($tmp)" }
    $sfo.DirectoryName
}
if ($Disco -and (Test-Path (Join-Path $OUT '2_jogo.ok'))) {
    $pg = Join-Path (Join-Path $R 'vfs') 'PS3_GAME'
    if (-not (Test-Path -LiteralPath (Join-Path $pg 'USRDIR') -PathType Container)) {
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
        Ligar-Disco $pg $real
        Msg "disco ligado de novo: $real"
    }
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
    Ligar-Disco $pg $real

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
    if ($Firmware -eq 'auto') {
        $Firmware = Join-Path $R 'dl\PS3UPDAT.PUP'
        if (-not (Test-Path -LiteralPath $Firmware)) {
            $lista = Join-Path $R 'dl\ps3-updatelist.txt'
            Baixar $PUP_LISTA $lista
            $url = [regex]::Match((Get-Content $lista -Raw), 'CDN=([^;\s]+/PS3UPDAT\.PUP)').Groups[1].Value
            if (-not $url) { throw "a lista de atualização do PS3 não trouxe o PS3UPDAT.PUP ($PUP_LISTA)" }
            Msg "baixando o firmware oficial do PS3 (PS3UPDAT.PUP, ~200 MB) de $url"
            Baixar $url $Firmware
        }
    }
    $sprx = $Firmware
    if (Test-Path -LiteralPath $Firmware -PathType Container) {
        $sprx = Get-ChildItem -LiteralPath $Firmware -Recurse -Filter libsre.sprx -ErrorAction SilentlyContinue | Select-Object -First 1 -ExpandProperty FullName
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
# sem AVX-512: x86-64-v3 (AVX2), a build testada nos PCs de teste, e não -march=native: tira uma variável dos
# defeitos gráficos vistos num Ryzen 5 5600X (cores de vértice e texturas erradas) enquanto a causa é investigada
if ($alvo -eq 'avx512') { $bargs = 'native'; $bdir = 'build_native' } else { $bargs = 'x86-64-v3 rz'; $bdir = 'build_v3rz' }
Etapa '6_compilar' {
    Msg "build: $alvo (código SPU com $(if ($alvo -eq 'portatil') { '-march=x86-64-v3, contas de float em para-zero' } else { '-march=native' }))"
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
