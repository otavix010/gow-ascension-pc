# Uma execução de uma cena com o mesmo roteiro do ab_fps.ps1 (controle e save) e o
# resumo do FPS na fase. Para conferências (GOWA_*_CHECK) e capturas, sem A/B.
# Uso: roda_cena.ps1 -Exe ...\gowa_x.exe -Env @{ GOWA_NINHO_CHECK='1' } -Cena cap16 -Log out\x.log [-Segundos 230]
param([string]$Exe = "$(Split-Path -Parent $PSScriptRoot)\port\build\gowa.exe", [hashtable]$Env = @{},
      [string]$Cena = 'cap16', [int]$Segundos = 0, [string]$Log = "$(Split-Path -Parent $PSScriptRoot)\out\cena.log")
$P = '50:0x4000,85:0x4000,105:0x0008,125:0x4000,145:0x4000,165:0x4000,185:0x4000'
$extra = @{}
if ($Cena -eq 'cap16') {
    $P = '50:0x4000,85:0x4000,95:0x0040,98:0x0040,101:0x0040,106:0x4000,115:0x4000'
    $extra = @{ PS3_SAVE_ROOT = "$(Split-Path -Parent $PSScriptRoot)\out\saves_teste\cap16" }
}
if (-not $Segundos) { $Segundos = if ($Cena -eq 'cap16') { 230 } else { 260 } }
$env2 = @{ PAD_SCRIPT = $P; WATCHDOG_AT = '172,180' } + $extra
foreach ($k in $Env.Keys) { $env2[$k] = $Env[$k] }
& "$(Split-Path -Parent $PSScriptRoot)\scripts\rodar_gowa.ps1" -Segundos $Segundos -Log $Log -Env $env2 -Exe $Exe
& "$(Split-Path -Parent $PSScriptRoot)\scripts\fps_fase.ps1" -Log $Log
