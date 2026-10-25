# Teste de sensibilidade: FPS na fase com o lado SPU ou o lado PPU N% mais lento
# (GOWA_SLOW_SPU_PCT / GOWA_SLOW_PPU_PCT), intercalado com a configuração normal.
param([int]$Pct = 25, [int]$Rodadas = 1, [string]$Prefixo = 'sens')
$P = '50:0x4000,85:0x4000,105:0x0008,125:0x4000,145:0x4000,165:0x4000,185:0x4000'
$cfgs = [ordered]@{ 'base' = @{}; 'spu' = @{GOWA_SLOW_SPU_PCT="$Pct"; GOWA_SLOW_FROM="165"}; 'ppu' = @{GOWA_SLOW_PPU_PCT="$Pct"; GOWA_SLOW_FROM="165"}; 'rsx' = @{GOWA_SLOW_RSX_PCT="$Pct"; GOWA_SLOW_FROM="165"} }
for ($r = 1; $r -le $Rodadas; $r++) {
    foreach ($k in $cfgs.Keys) {
        $log = "$(Split-Path -Parent $PSScriptRoot)\out\${Prefixo}_${k}$r.log"
        & "$(Split-Path -Parent $PSScriptRoot)\scripts\rodar_gowa.ps1" -Segundos 260 -Log $log -Env (@{ PAD_SCRIPT = $P } + $cfgs[$k]) | Out-Null
        "$k$r : $(& $(Split-Path -Parent $PSScriptRoot)\scripts\fps_fase.ps1 -Log $log)"
    }
}
