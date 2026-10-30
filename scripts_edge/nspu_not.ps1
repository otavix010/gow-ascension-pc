# 30/10: no notebook (4 núcleos / 8 lógicas), a 7a SPU dos tasksets (GOWA_SPURS_NSPU=7, padrão desde 20/10, medido
# só no 9800X3D) x 6. Espera out\interp_not_pronto.txt; cap. 16 roteiro lento, 3 pares alternados, 90 s parado antes
# de cada rodada. Resultado em out\nspu_not.txt; fim em out\nspu_not_pronto.txt. Tarefa gowa_nspu.
param([string]$Exe = 'gowa_edge7.exe', [int]$Pares = 3)
$R = 'C:\gowa-teste'
$fim = Join-Path $R 'out\nspu_not_pronto.txt'
Remove-Item $fim -ErrorAction SilentlyContinue
while (-not (Test-Path (Join-Path $R 'out\interp_not_pronto.txt'))) { Start-Sleep -Seconds 30 }
$res = Join-Path $R 'out\nspu_not.txt'
"inicio $(Get-Date -Format 'HH:mm:ss') exe=$Exe" | Out-File -Encoding utf8 $res
foreach ($par in 1..$Pares) {
    $ordem = if ($par % 2) { @('7', '6') } else { @('6', '7') }
    foreach ($n in $ordem) {
        Start-Sleep -Seconds 90
        $mhz = (Get-CimInstance Win32_Processor).CurrentClockSpeed
        & (Join-Path $R 'teste.ps1') -Cena cap16 -Roteiro lento -Segundos 320 -ExeNome $Exe -Nome "nspu${n}_$par" -Extra "GOWA_EDGE_NATIVO=1;GOWA_SPURS_NSPU=$n" *> (Join-Path $R 'out\nspu_not_teste.txt')
        "cap16 nspu=$n par ${par} (clock $mhz): $(Get-Content (Join-Path $R 'out\resultados.txt') -Tail 1)" | Add-Content -Encoding utf8 $res
        Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.Path -like "$R\build\*" } | Stop-Process -Force -ErrorAction SilentlyContinue
    }
}
"fim $(Get-Date -Format 'HH:mm:ss')" | Add-Content -Encoding utf8 $res
"pronto" | Out-File -Encoding utf8 $fim
