# 30/10: depois do A/B do EDGE (out\edge_not_pronto.txt), uma rodada do cap. 16 com o gowa_edge9.exe (main fac4757:
# 6 SPUs por padrão com <= 8 CPUs lógicas), sem GOWA_SPURS_NSPU, para ver o "[spurs] 6 SPUs" e o fps.
$R = 'C:\gowa-teste'
Start-Sleep -Seconds 120
while (-not (Test-Path (Join-Path $R 'out\edge_not_pronto.txt'))) { Start-Sleep -Seconds 30 }
Start-Sleep -Seconds 90
& (Join-Path $R 'teste.ps1') -Cena cap16 -Roteiro lento -Segundos 320 -ExeNome 'gowa_edge9.exe' -Nome 'spu6' *> (Join-Path $R 'out\spu6_teste.txt')
$log = Get-ChildItem (Join-Path $R 'out') -Filter 'cap16_*spu6*.log' | Sort-Object LastWriteTime | Select-Object -Last 1
$l = if ($log) { (Select-String -Path $log.FullName -Pattern '\[spurs\]' | Select-Object -First 1).Line } else { 'sem log' }
"$(Get-Date -Format 'HH:mm:ss') $l | $(Get-Content (Join-Path $R 'out\resultados.txt') -Tail 1)" | Out-File -Encoding utf8 (Join-Path $R 'out\spu6_not.txt')
