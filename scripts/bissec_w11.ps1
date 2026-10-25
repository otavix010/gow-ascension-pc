# Bissecção das funções de plugin do w11 que travam o boot aos ~23 s.
# Cada teste: GOWA_PLUGINS_W11=1 com só a faixa [a,b) da tabela; passou se o
# jogo chega ao "[go to game shell]" em 45 s.
param([int]$N = 137)
function Testa([int]$a, [int]$b) {
    $log = "$(Split-Path -Parent $PSScriptRoot)\out\bis_w11_${a}_${b}.log"
    & "$(Split-Path -Parent $PSScriptRoot)\scripts\rodar_gowa.ps1" -Segundos 45 -Log $log -Env @{GOWA_PLUGINS_W11='1'; GOWA_PLUGINS_W11_FAIXA="$a-$b"} | Out-Null
    $ok = [bool](Select-String -Path $log -Pattern 'go to game shell' -Quiet)
    "faixa [$a,$b): " + $(if ($ok) { 'passou' } else { 'TRAVOU' })
    return $ok
}
$lo = 0; $hi = $N
# confirma que a faixa toda trava
$r = Testa $lo $hi; if ($r[-1]) { 'a faixa toda passou -- trava intermitente?'; exit }
while ($hi - $lo -gt 1) {
    $mid = [int](($lo + $hi) / 2)
    $r = Testa $lo $mid
    if (-not $r[-1]) { $hi = $mid; continue }
    $r = Testa $mid $hi
    if (-not $r[-1]) { $lo = $mid; continue }
    "as duas metades passam sozinhas: [$lo,$mid) e [$mid,$hi) -- interação"; exit
}
"culpada: entrada $lo"
