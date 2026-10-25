# Caça o congelamento do boot (~21 s, 21/10: tela de abertura a 60 FPS e o processo inteiro
# para de escrever, nem o vigia): boots curtos repetidos com GOWA_FREEZE_DUMP (despejo das
# threads em out\freeze_<pid>.txt por WriteFile quando os flips param). Para na primeira
# rodada que gerar um freeze_*.txt e guarda o log em out\congela_<n>.log.
# Uso: caca_congela.ps1 [-Rodadas 60] [-Segundos 45] [-Exe ...] [-Env @{...}]
param([int]$Rodadas = 60, [int]$Segundos = 45, [string]$Exe = "$(Split-Path -Parent $PSScriptRoot)\port\build\gowa.exe",
      [hashtable]$Env = @{})
$R = "$(Split-Path -Parent $PSScriptRoot)"
$P = '50:0x4000,85:0x4000,95:0x0040,98:0x0040,101:0x0040,106:0x4000,115:0x4000'
$base = @{ PS3_SAVE_ROOT = "$R\out\saves_teste\cap16"; PAD_SCRIPT = $P; GOWA_FREEZE_DUMP = '12' }
foreach ($k in $Env.Keys) { $base[$k] = $Env[$k] }
for ($i = 1; $i -le $Rodadas; $i++) {
    $log = "$R\out\congela_atual.log"
    $t0 = Get-Date
    & "$R\scripts\rodar_gowa.ps1" -Segundos $Segundos -Log $log -Env $base -Exe $Exe | Out-Null
    $novos = Get-ChildItem "$R\out\freeze_*.txt" -ErrorAction SilentlyContinue | Where-Object { $_.LastWriteTime -ge $t0 }
    $ult = (Select-String -Path $log -Pattern '^\[fps\]' | Select-Object -Last 1).Line
    "{0:HH:mm} rodada {1}: {2}; último [fps]: {3}" -f (Get-Date), $i, ($(if ($novos) { "CONGELOU -> " + ($novos.Name -join ',') } else { 'ok' })), $ult
    if ($novos) {
        Copy-Item $log "$R\out\congela_$i.log"
        "log em out\congela_$i.log"
        break
    }
}
