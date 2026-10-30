# 30/10: o efeito do EDGE nativo no jogo, no notebook: o mesmo exe (gowa_edge10.exe: todos os nativos, 6 SPUs) com
# GOWA_EDGE_NATIVO 0 x 1 alternado, cap. 16 roteiro lento, 3 pares. Espera a rodada das 6 SPUs (out\spu6_not.txt).
# Conferência das 300 gravações da fase 1 com o nativo antes. Resultado em out\nat01_not.txt.
param([string]$Exe = 'gowa_edge10.exe', [int]$Pares = 3)
$R = 'C:\gowa-teste'
Start-Sleep -Seconds 60
while (-not (Test-Path (Join-Path $R 'out\spu6_not.txt'))) { Start-Sleep -Seconds 30 }
$res = Join-Path $R 'out\nat01_not.txt'
"inicio $(Get-Date -Format 'HH:mm:ss') exe=$Exe" | Out-File -Encoding utf8 $res
$env:PS3_VFS_ROOT = "$R\vfs"; $env:PS3_LIBSRE_IMAGE = "$R\fw\lle\libsre_image.bin"; $env:GOWA_AUDIO_MUTE = '1'
$env:GOWA_CONF_REPLAY = "$R\out\confere_f1"; $env:GOWA_CONF_REPS = '5'; $env:GOWA_CONF_REGS_ABI = '1'
$p = Start-Process -FilePath "$R\build\$Exe" -ArgumentList "$R\jogo\GOWA.elf" -WorkingDirectory "$R\build" -PassThru -WindowStyle Hidden `
     -RedirectStandardError "$R\out\nat01_conf_err.txt" -RedirectStandardOutput "$R\out\nat01_conf_out.txt"
if (-not $p.WaitForExit(900000)) { Stop-Process -Id $p.Id -Force; "conferencia: TRAVOU" | Add-Content -Encoding utf8 $res }
else { "conferencia: " + (Get-Content "$R\out\confere_f1\conferencia.txt" -Tail 1) | Add-Content -Encoding utf8 $res }
foreach ($k in 'PS3_VFS_ROOT', 'PS3_LIBSRE_IMAGE', 'GOWA_AUDIO_MUTE', 'GOWA_CONF_REPLAY', 'GOWA_CONF_REPS', 'GOWA_CONF_REGS_ABI') { Remove-Item "env:$k" -ErrorAction SilentlyContinue }
foreach ($par in 1..$Pares) {
    $ordem = if ($par % 2) { @('1', '0') } else { @('0', '1') }
    foreach ($m in $ordem) {
        Start-Sleep -Seconds 90
        & (Join-Path $R 'teste.ps1') -Cena cap16 -Roteiro lento -Segundos 320 -ExeNome $Exe -Nome "nat${m}_$par" -Extra "GOWA_EDGE_NATIVO=$m" *> (Join-Path $R 'out\nat01_teste.txt')
        "cap16 nativo=$m par ${par}: $(Get-Content (Join-Path $R 'out\resultados.txt') -Tail 1)" | Add-Content -Encoding utf8 $res
        Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.Path -like "$R\build\*" } | Stop-Process -Force -ErrorAction SilentlyContinue
    }
}
"fim $(Get-Date -Format 'HH:mm:ss')" | Add-Content -Encoding utf8 $res
