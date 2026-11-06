# Uma execução de uma cena com o mesmo roteiro do ab_fps.ps1 (controle e save) e o
# resumo do FPS na fase. Para conferências (GOWA_*_CHECK) e capturas, sem A/B.
# Uso: roda_cena.ps1 -Exe ...\gowa_x.exe -Env @{ GOWA_NINHO_CHECK='1' } -Cena cap16 -Log out\x.log [-Segundos 230]
param([string]$Exe = "$(Split-Path -Parent $PSScriptRoot)\port\build\gowa.exe", [hashtable]$Env = @{},
      [string]$Cena = 'cap16', [int]$Segundos = 0, [string]$Log = "$(Split-Path -Parent $PSScriptRoot)\out\cena.log")
$P = '50:0x4000,85:0x4000,105:0x0008,125:0x4000,145:0x4000,165:0x4000,185:0x4000'
$extra = @{}
if ($Cena -eq 'cap16') {
    # 05/11: o save antigo do cap16 parava no menu desde 04/11 (as rodadas só cobriam boot, menu e vídeo);
    # agora o save legion3 (perfis 1 e 3) com o roteiro de botões dele, que carrega o jogo
    $P = '60:0x4000,92:0x4000,112:0x0040,118:0x0040,124:0x0040,140:0x4000,165:0x4000'
    # 05/11: o jogo regrava o SYSDATA (opções) a cada rodada, e uma rodada em que o roteiro de botões caiu
    # na tela errada deixou opções trocadas no save de referência: daí em diante o cap16 parava no menu.
    # Cada rodada agora usa uma cópia nova do save; o de referência não é mais tocado.
    $orig = "$(Split-Path -Parent $PSScriptRoot)\out\saves_teste\cap16"
    $copia = "$(Split-Path -Parent $PSScriptRoot)\out\saves_teste_rodada\cap16"
    Remove-Item $copia -Recurse -Force -ErrorAction SilentlyContinue
    New-Item -ItemType Directory -Force (Split-Path $copia) | Out-Null
    Copy-Item $orig $copia -Recurse -Force
    $extra = @{ PS3_SAVE_ROOT = $copia }
}
if (-not $Segundos) { $Segundos = if ($Cena -eq 'cap16') { 230 } else { 260 } }
$env2 = @{ PAD_SCRIPT = $P; WATCHDOG_AT = '172,180' } + $extra
foreach ($k in $Env.Keys) { $env2[$k] = $Env[$k] }
& "$(Split-Path -Parent $PSScriptRoot)\scripts\rodar_gowa.ps1" -Segundos $Segundos -Log $Log -Env $env2 -Exe $Exe
& "$(Split-Path -Parent $PSScriptRoot)\scripts\fps_fase.ps1" -Log $Log
