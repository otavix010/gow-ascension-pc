# Roda o jogo até a fase com cada variante de leitura da profundidade como cor
# (GOWA_ZCOLOR_ORDER 0..3 e GOWA_ZCOLOR=0) e despeja o quadro apresentado em
# out\zvar\<variante>\, para comparar a imagem final.
param([int]$Quadro = 21000, [int]$Segundos = 190)
$P = '50:0x4000,85:0x4000,105:0x0008,125:0x4000,145:0x4000,165:0x4000,185:0x4000'
$vars = [ordered]@{ 'o0' = @{GOWA_ZCOLOR_ORDER='0'}; 'o1' = @{GOWA_ZCOLOR_ORDER='1'}; 'o2' = @{GOWA_ZCOLOR_ORDER='2'};
                    'o3' = @{GOWA_ZCOLOR_ORDER='3'}; 'off' = @{GOWA_ZCOLOR='0'} }
foreach ($k in $vars.Keys) {
    $d = "$(Split-Path -Parent $PSScriptRoot)\out\zvar\$k"
    New-Item -ItemType Directory -Force $d | Out-Null
    $env2 = @{ PAD_SCRIPT = $P; LD_FRAME_DUMP = $d; LD_FRAME_DUMP_FROM = "$Quadro"; LD_FRAME_DUMP_EVERY = '400' } + $vars[$k]
    & "$(Split-Path -Parent $PSScriptRoot)\scripts\rodar_gowa.ps1" -Segundos $Segundos -Log "$(Split-Path -Parent $PSScriptRoot)\out\zvar_$k.log" -Env $env2 | Out-Null
    "$k : $((Get-ChildItem $d -Filter *.ppm).Count) quadros"
}
