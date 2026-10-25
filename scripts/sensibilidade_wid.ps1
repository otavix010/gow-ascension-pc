# Teste de sensibilidade por workload: FPS na fase com só alguns workloads SPU
# N% mais lentos (GOWA_SLOW_SPU_PCT + GOWA_SLOW_WIDS = máscara hex de wid), a
# partir de 165 s. Mostra qual workload está no caminho crítico do quadro.
param([int]$Pct = 25, [string]$Prefixo = 'sensw', [string[]]$Mascaras = @('380000', '20080', '8'))
$P = '50:0x4000,85:0x4000,105:0x0008,125:0x4000,145:0x4000,165:0x4000,185:0x4000'
$cfgs = [ordered]@{ 'base' = @{} }
foreach ($m in $Mascaras) { $cfgs["w$m"] = @{ GOWA_SLOW_SPU_PCT = "$Pct"; GOWA_SLOW_FROM = '165'; GOWA_SLOW_WIDS = $m } }
foreach ($k in $cfgs.Keys) {
    $log = "$(Split-Path -Parent $PSScriptRoot)\out\${Prefixo}_${k}.log"
    & "$(Split-Path -Parent $PSScriptRoot)\scripts\rodar_gowa.ps1" -Segundos 260 -Log $log -Env (@{ PAD_SCRIPT = $P; GOWA_LD_PIPE = '1' } + $cfgs[$k]) | Out-Null
    "$k : $(& $(Split-Path -Parent $PSScriptRoot)\scripts\fps_fase.ps1 -Log $log)"
}
