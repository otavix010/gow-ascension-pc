# 29/10 noite: áudio num PC lento de verdade (notebook i5-10300H, cap. 16 a ~6-10 fps). Espera o A/B do EDGE
# (out\edge_not_pronto.txt) e roda o cap. 16 com GOWA_AUDIO_SPU_PRIO 0 x 1 (a SPU que mixa o som em HIGHEST até o
# PUT no anel), 2 pares, 90 s parado antes de cada rodada. Conta, das linhas [audio] do log, os blocos atrasados
# e as vezes que o dispositivo ficou vazio depois do "go to playing". Resultado em out\audio_prio_not.txt.
param([string]$Exe = 'gowa_edge4.exe', [int]$Pares = 2)
$R = 'C:\gowa-teste'
while (-not (Test-Path (Join-Path $R 'out\edge_not_pronto.txt'))) { Start-Sleep -Seconds 30 }
$res = Join-Path $R 'out\audio_prio_not.txt'
"inicio $(Get-Date -Format 'HH:mm:ss') exe=$Exe" | Out-File -Encoding utf8 $res
foreach ($par in 1..$Pares) {
    foreach ($modo in '0', '1') {
        Start-Sleep -Seconds 90
        $nome = "aprio${modo}_$par"
        & (Join-Path $R 'teste.ps1') -Cena cap16 -Roteiro lento -Segundos 320 -ExeNome $Exe -Nome $nome -Extra "GOWA_EDGE_NATIVO=1;GOWA_AUDIO_SPU_PRIO=$modo" *> (Join-Path $R 'out\audio_prio_teste.txt')
        $fps = Get-Content (Join-Path $R 'out\resultados.txt') -Tail 1
        $log = Get-ChildItem (Join-Path $R 'out') -Filter "cap16_*$nome*.log" | Sort-Object LastWriteTime | Select-Object -Last 1
        $late = 0; $dry = 0; $n = 0
        if ($log) {
            $go = Select-String -Path $log.FullName -Pattern 'go to playing' | Select-Object -First 1
            $de = if ($go) { $go.LineNumber } else { 0 }
            Select-String -Path $log.FullName -Pattern '^\[audio\] [0-9]+ s: [0-9]+ blocos.*?, ([0-9]+) atrasados.*?([0-9]+) vezes o dispositivo' |
                Where-Object { $_.LineNumber -gt $de } | ForEach-Object { $late += [int]$_.Matches[0].Groups[1].Value; $dry += [int]$_.Matches[0].Groups[2].Value; $n++ }
        }
        "cap16 prio=$modo par ${par}: $fps | depois do go to playing: $late blocos atrasados, $dry vezes vazio, em $n linhas [audio]" | Add-Content -Encoding utf8 $res
        Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.Path -like "$R\build\*" } | Stop-Process -Force -ErrorAction SilentlyContinue
    }
}
"fim $(Get-Date -Format 'HH:mm:ss')" | Add-Content -Encoding utf8 $res
