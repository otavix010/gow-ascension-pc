# Roda uma cena (roteiro do ab_fps.ps1) com o port\build\gowa.exe e captura a JANELA do jogo
# (PrintWindow, captura_janela.ps1) nos tempos pedidos -- para comparar a imagem entre duas
# configurações. Uso: captura_cena.ps1 -Env @{ GOWA_X='1' } -Tempos 160,190 -Prefixo nc1 [-Cena cap16] [-Segundos 200]
param([hashtable]$Env = @{}, [int[]]$Tempos = @(160, 190), [string]$Prefixo = 'cap', [string]$Cena = 'cap16',
      [int]$Segundos = 0)
$R = "$(Split-Path -Parent $PSScriptRoot)"
$Exe = "$R\port\build\gowa.exe"
$P = '50:0x4000,85:0x4000,105:0x0008,125:0x4000,145:0x4000,165:0x4000,185:0x4000'
$e2 = @{ PS3_VFS_ROOT = "$R\vfs"; PS3_WINDOW_MONITOR = '2'; GOWA_AUDIO_MUTE = '1' }
if ($Cena -eq 'cap16') {
    $P = '50:0x4000,85:0x4000,95:0x0040,98:0x0040,101:0x0040,106:0x4000,115:0x4000'
    $e2['PS3_SAVE_ROOT'] = "$R\out\saves_teste\cap16"
}
$e2['PAD_SCRIPT'] = $P
foreach ($k in $Env.Keys) { $e2[$k] = $Env[$k] }
if (-not $Segundos) { $Segundos = ($Tempos | Measure-Object -Maximum).Maximum + 10 }
foreach ($k in $e2.Keys) { Set-Item -Path "env:$k" -Value $e2[$k] }
$log = "$R\out\${Prefixo}_captura.log"
$t0 = Get-Date
$p = Start-Process -FilePath cmd.exe -ArgumentList "/c", "`"`"$Exe`" `"$R\jogo\GOWA.elf`" > `"$log`" 2>&1`"" `
     -WorkingDirectory (Split-Path $Exe) -PassThru -WindowStyle Hidden
foreach ($t in ($Tempos | Sort-Object)) {
    $falta = $t - ((Get-Date) - $t0).TotalSeconds
    if ($falta -gt 0) { Start-Sleep -Seconds ([int][math]::Ceiling($falta)) }
    & "$R\scripts\captura_janela.ps1" -Saida "$R\out\${Prefixo}_t$t.png"
}
$falta = $Segundos - ((Get-Date) - $t0).TotalSeconds
if ($falta -gt 0) { Start-Sleep -Seconds ([int][math]::Ceiling($falta)) }
Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $Exe } | Stop-Process -Force
$p.WaitForExit(5000) | Out-Null
foreach ($k in $e2.Keys) { Remove-Item -Path "env:$k" -ErrorAction SilentlyContinue }
& "$R\scripts\fps_fase.ps1" -Log $log
