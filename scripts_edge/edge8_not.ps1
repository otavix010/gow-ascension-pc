# 30/10: depois do A/B de SPUs (out\nspu_not_pronto.txt), o A/B do EDGE edge7 (4 partes + F538) x edge8 (+ D250 e o
# laço local da E3C4 desligado no AVX2) pelo edge_ab_not.ps1 (conferência das duas + 2 pares no cap. 16).
$R = 'C:\gowa-teste'
while (-not (Test-Path (Join-Path $R 'out\nspu_not_pronto.txt'))) { Start-Sleep -Seconds 30 }
Copy-Item (Join-Path $R 'out\edge_ab_not.txt') (Join-Path $R 'out\edge_ab_not_e6.txt') -Force -ErrorAction SilentlyContinue
& (Join-Path $R 'edge_ab_not.ps1') -ExeA 'gowa_edge7.exe' -ExeB 'gowa_edge8.exe' -Pares 2
