# Compara a imagem da cena do Dodge entre configurações: cada variante roda até
# a fase, despeja quadros a cada 500 e os últimos 4 viram uma linha do mosaico
# out\varimg\mosaico.png (uma linha por variante, na ordem de -Variantes).
param([string[]]$Variantes = @('base', 'mmsync', 'cast0', 'pipe0'), [int]$Segundos = 205)
$P = '50:0x4000,85:0x4000,105:0x0008,125:0x4000,145:0x4000,165:0x4000,185:0x4000'
$cfg = @{
    'base'   = @{}
    'mmsync' = @{ GOWA_MM_ASYNC = '0' }
    'cast0'  = @{ GOWA_CAST_Y16 = '0' }
    'pipe0'  = @{ GOWA_LD_PIPE = '0' }
    'f16o0'  = @{ GOWA_F16_ORDER = '0' }
    'gam0'   = @{ GOWA_TEX_GAMMA = '0' }
    'zc0'    = @{ GOWA_ZCOLOR = '0' }
    'f32rt0' = @{ GOWA_F32_RT = '0' }
}
$raiz = "$(Split-Path -Parent $PSScriptRoot)\out\varimg"
New-Item -ItemType Directory -Force $raiz | Out-Null
foreach ($v in $Variantes) {
    $d = "$raiz\$v"
    New-Item -ItemType Directory -Force $d | Out-Null
    Get-ChildItem $d -Filter *.ppm | ForEach-Object { Remove-Item $_.FullName }
    $env2 = @{ PAD_SCRIPT = $P; GOWA_MLAA_OUT = '0'; GOWA_F16_ORDER = '2'; LD_FRAME_DUMP = $d; LD_FRAME_DUMP_EVERY = '500' } + $cfg[$v]
    & "$(Split-Path -Parent $PSScriptRoot)\scripts\rodar_gowa.ps1" -Segundos $Segundos -Log "$raiz\$v.log" -Env $env2 | Out-Null
    "$v : $((Get-ChildItem $d -Filter *.ppm).Count) quadros"
}
python -I "$(Split-Path -Parent $PSScriptRoot)\scripts\mosaico_variantes.py" $raiz @Variantes
