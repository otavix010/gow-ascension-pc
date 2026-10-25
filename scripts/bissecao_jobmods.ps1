# Bisseção dos módulos de job WWS traduzidos: acha qual(is) impede(m) o menu de aparecer.
# Cada teste registra só um subconjunto (os demais vão em GOWA_JOBLIFT_SKIP) e olha o
# título da janela aos ~100 s: "draws: N" com N > 100 = menu desenhado.
param([string[]]$Suspeitos, [string[]]$Sempre = @(), [int]$Segundos = 105)
$todos = '6EB400 6EDC00 6EF200 6FC080 710200 712C00 712F00 71EE00 727200 72C100 72DF80 73A400 73E000 73EE00 73FA80 740B80 741B00 75CC80 770F80 781D80 789F00 790F80 792E00 79C680 79DC00 7A1800 7B6680 7D8600 7F5C80 80F000 84BCC0'.Split(' ')
function Testa([string[]]$ativos) {
    $skip = ($todos | Where-Object { $ativos -notcontains $_ }) -join ','
    $log = "$(Split-Path -Parent $PSScriptRoot)\out\bis_$([guid]::NewGuid().ToString().Substring(0,6)).log"
    $j = Start-Job -ArgumentList $skip, $log, $Segundos { param($skip, $log, $seg)
        & "$(Split-Path -Parent $PSScriptRoot)\scripts\rodar_gowa.ps1" -Segundos ($seg + 5) -Log $log -Env @{ SPU_INTERP_UNLIFTED = '1'; GOWA_JOBLIFT_SKIP = $skip; GOWA_PROBE_MAX = '100000' } }
    Start-Sleep -Seconds $Segundos
    $p = Get-Process gowa -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
    $titulo = if ($p) { $p.MainWindowTitle } else { '' }
    Receive-Job $j -Wait | Out-Null; Remove-Job $j
    $draws = 0; if ($titulo -match 'draws: (\d+)') { $draws = [int]$Matches[1] }
    $shell = Select-String -Path $log -Pattern 'inited %s\] Shell' -Quiet
    Write-Host "  ativos=[$($ativos -join ',')] draws=$draws shell_inited=$shell"
    return @{ Ok = ($draws -gt 100); Shell = $shell }
}
$atual = $Suspeitos
while ($atual.Count -gt 1) {
    $meio = [int][math]::Floor($atual.Count / 2)
    $a = $atual[0..($meio - 1)]
    $b = $atual[$meio..($atual.Count - 1)]
    $r = Testa ($Sempre + $a)
    if (-not $r.Shell) { "  (abertura travou; repetindo)"; $r = Testa ($Sempre + $a) }
    if (-not $r.Ok) { $atual = $a } else { $atual = $b }
    "-> suspeitos: $($atual -join ',')"
}
"CULPADO: $atual"
