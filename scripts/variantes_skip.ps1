# Variantes que pulam grupos de PSO (LD_SKIP_PSO) na cena do Dodge, para achar
# o desenho que estoura o céu. Mosaico em out\varimg\mosaico.png.
param([hashtable]$Grupos, [int]$Segundos = 205)
$P = '50:0x4000,85:0x4000,105:0x0008,125:0x4000,145:0x4000,165:0x4000,185:0x4000'
$raiz = "$(Split-Path -Parent $PSScriptRoot)\out\varimg"
$nomes = @()
foreach ($v in ($Grupos.Keys | Sort-Object)) {
    $d = "$raiz\$v"
    New-Item -ItemType Directory -Force $d | Out-Null
    Get-ChildItem $d -Filter *.ppm | ForEach-Object { Remove-Item $_.FullName }
    $env2 = @{ PAD_SCRIPT = $P; GOWA_MLAA_OUT = '0'; LD_FRAME_DUMP = $d; LD_FRAME_DUMP_EVERY = '500' }
    if ($Grupos[$v]) { $env2['LD_SKIP_PSO'] = $Grupos[$v] }
    & "$(Split-Path -Parent $PSScriptRoot)\scripts\rodar_gowa.ps1" -Segundos $Segundos -Log "$raiz\$v.log" -Env $env2 | Out-Null
    "$v : $((Get-ChildItem $d -Filter *.ppm).Count) quadros"
    $nomes += $v
}
python -I "$(Split-Path -Parent $PSScriptRoot)\scripts\mosaico_variantes.py" $raiz @nomes
