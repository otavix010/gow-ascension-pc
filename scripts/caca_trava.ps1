# Caça a trava intermitente do carregamento ("waiting for load save game"):
# roda o Capítulo 16 (save de teste) até a fase, várias vezes, com despejos
# das threads aos 130/140/150 s (WATCHDOG_AT). Para na primeira rodada em que
# a mensagem se repete (>= 3 vezes) e guarda o registro em out\trava_<n>.log.
# Uso: caca_trava.ps1 [-Rodadas 30] [-Segundos 155] [-Env @{...}]
param([int]$Rodadas = 30, [int]$Segundos = 155, [hashtable]$Env = @{})
$R = "$(Split-Path -Parent $PSScriptRoot)"
$P = '50:0x4000,85:0x4000,95:0x0040,98:0x0040,101:0x0040,106:0x4000,115:0x4000'
$base = @{ PS3_SAVE_ROOT = "$R\out\saves_teste\cap16"; PAD_SCRIPT = $P; WATCHDOG_AT = '130,140,150' } + $Env
for ($i = 1; $i -le $Rodadas; $i++) {
    $log = "$R\out\caca_$i.log"
    & "$R\scripts\rodar_gowa.ps1" -Segundos $Segundos -Log $log -Env $base | Out-Null
    $c = (Select-String -Path $log -Pattern 'waiting for load save game').Count
    "{0:HH:mm} rodada {1}: 'waiting for load save game' x{2}" -f (Get-Date), $i, $c
    if ($c -ge 3) {
        Copy-Item $log "$R\out\trava_$i.log"
        "TRAVA na rodada $i -> out\trava_$i.log"
        break
    }
    Remove-Item $log -ErrorAction SilentlyContinue
}
