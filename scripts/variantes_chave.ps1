# Variantes que pulam programas inteiros (GOWA_SKIP_KEY, chave pso= do log [tgt])
# na cena do Dodge. Mosaico em out\varimg\mosaico.png; quadros em out\varimg\<nome>.
param([hashtable]$Grupos, [int]$Segundos = 205, [string]$Cada = '500')
$P = '50:0x4000,85:0x4000,105:0x0008,125:0x4000,145:0x4000,165:0x4000,185:0x4000'
$raiz = "$(Split-Path -Parent $PSScriptRoot)\out\varimg"
$nomes = @()
foreach ($v in ($Grupos.Keys | Sort-Object)) {
    $d = "$raiz\$v"
    New-Item -ItemType Directory -Force $d | Out-Null
    Get-ChildItem $d -Filter *.ppm | ForEach-Object { Remove-Item $_.FullName }
    $env2 = @{ PAD_SCRIPT = $P; LD_FRAME_DUMP = $d; LD_FRAME_DUMP_EVERY = $Cada }
    if ($Grupos[$v]) { $env2['GOWA_SKIP_KEY'] = $Grupos[$v] }
    & "$(Split-Path -Parent $PSScriptRoot)\scripts\rodar_gowa.ps1" -Segundos $Segundos -Log "$raiz\$v.log" -Env $env2 | Out-Null
    "$v : $((Get-ChildItem $d -Filter *.ppm).Count) quadros"
    $nomes += $v
}
python -I "$(Split-Path -Parent $PSScriptRoot)\scripts\mosaico_variantes.py" $raiz @nomes
