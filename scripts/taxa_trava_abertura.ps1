# Mede a taxa da trava intermitente da abertura: N execuções curtas, cada uma
# conta se o log do jogo chegou a "registered level variables module" (~17 s).
# Uso: taxa_trava_abertura.ps1 -N 6 -Segundos 30 -Prefixo ab -Env @{ VAR='x' }
param([int]$N = 6, [int]$Segundos = 30, [string]$Prefixo = 'ab', [hashtable]$Env = @{})
$ok = 0
for ($i = 1; $i -le $N; $i++) {
    $log = "$(Split-Path -Parent $PSScriptRoot)\out\${Prefixo}_$i.log"
    $e = @{ GOWA_PROBE_MAX = '100000' }
    foreach ($k in $Env.Keys) { $e[$k] = $Env[$k] }
    & "$(Split-Path -Parent $PSScriptRoot)\scripts\rodar_gowa.ps1" -Segundos $Segundos -Log $log -Env $e | Out-Null
    $passou = (Select-String -Path $log -Pattern 'registered level variables module' -Quiet)
    if ($passou) { $ok++ }
    Write-Output ("{0} {1}" -f $log, $(if ($passou) { 'passou' } else { 'TRAVOU' }))
}
Write-Output ("{0}: {1}/{2} passaram" -f $Prefixo, $ok, $N)
