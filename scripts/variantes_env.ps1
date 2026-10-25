# Variantes de imagem na cena do Dodge, cada uma com seu conjunto de variáveis de
# ambiente (hashtable de hashtables, por exemplo @{ a_base=@{}; b_f16o2=@{GOWA_F16_ORDER='2'} }).
# Quadros em out\varimg\<nome>, mosaico (4 últimos de cada) em out\varimg\mosaico.png.
param([hashtable]$Grupos, [int]$Segundos = 205, [string]$Cada = '500')
$P = '50:0x4000,85:0x4000,105:0x0008,125:0x4000,145:0x4000,165:0x4000,185:0x4000'
$raiz = "$(Split-Path -Parent $PSScriptRoot)\out\varimg"
$nomes = @()
foreach ($v in ($Grupos.Keys | Sort-Object)) {
    $d = "$raiz\$v"
    New-Item -ItemType Directory -Force $d | Out-Null
    Get-ChildItem $d -Filter *.ppm | ForEach-Object { Remove-Item $_.FullName }
    $env2 = @{ PAD_SCRIPT = $P; LD_FRAME_DUMP = $d; LD_FRAME_DUMP_EVERY = $Cada }
    foreach ($k in $Grupos[$v].Keys) { $env2[$k] = $Grupos[$v][$k] }
    & "$(Split-Path -Parent $PSScriptRoot)\scripts\rodar_gowa.ps1" -Segundos $Segundos -Log "$raiz\$v.log" -Env $env2 | Out-Null
    "$v : $((Get-ChildItem $d -Filter *.ppm).Count) quadros"
    $nomes += $v
}
python -I "$(Split-Path -Parent $PSScriptRoot)\scripts\mosaico_variantes.py" $raiz @nomes
