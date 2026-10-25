# Roda o Capítulo 16 (save de teste) e conta as piscadas: 80 capturas da
# janela (PrintWindow) a cada 150 ms a partir de 170 s.
# Uso: piscadas_cap16.ps1 -Nome x [-Env @{...}] [-Segundos 200]
param([string]$Nome = 'p', [hashtable]$Env = @{}, [int]$Segundos = 200, [int]$N = 80)
$R = "$(Split-Path -Parent $PSScriptRoot)"
$pasta = "$R\out\pisc_$Nome"
$e = @{ PS3_SAVE_ROOT = "$R\out\saves_teste\cap16"; PAD_SCRIPT = '50:0x4000,85:0x4000,95:0x0040,98:0x0040,101:0x0040,106:0x4000,115:0x4000' } + $Env
$cap = Start-Process -FilePath powershell.exe -ArgumentList '-NoProfile','-ExecutionPolicy','Bypass','-File',"$R\scripts\captura_rajada.ps1",'-Segundos','170','-N',"$N",'-IntervaloMs','150','-Pasta',$pasta -WindowStyle Hidden -PassThru
& "$R\scripts\rodar_gowa.ps1" -Segundos $Segundos -Log "$R\out\pisc_$Nome.log" -Env $e | Out-Null
$cap.WaitForExit(60000) | Out-Null
python -I "$R\scripts\conta_piscadas.py" $pasta
