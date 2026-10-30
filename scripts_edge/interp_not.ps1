# 30/10: no notebook, o que roda interpretado no cap. 16 (GOWA_INTERP_ENTRY_LOG=1, o resumo [interp-entry] no log),
# com o exe edge7. Copia o log para out\interp_not.log; fim em out\interp_not_pronto.txt. Tarefa gowa_interp.
param([string]$Exe = 'gowa_edge7.exe')
$R = 'C:\gowa-teste'
$fim = Join-Path $R 'out\interp_not_pronto.txt'
Remove-Item $fim -ErrorAction SilentlyContinue
Start-Sleep -Seconds 60
& (Join-Path $R 'teste.ps1') -Cena cap16 -Roteiro lento -Segundos 300 -ExeNome $Exe -Nome "interp_$Exe" -Extra 'GOWA_EDGE_NATIVO=1;GOWA_INTERP_ENTRY_LOG=1' *> (Join-Path $R 'out\interp_not_teste.txt')
Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.Path -like "$R\build\*" } | Stop-Process -Force -ErrorAction SilentlyContinue
$log = Get-ChildItem (Join-Path $R 'out') -Filter "cap16_*interp_$Exe*.log" | Sort-Object LastWriteTime | Select-Object -Last 1
if ($log) { Copy-Item $log.FullName (Join-Path $R 'out\interp_not.log') -Force }
"pronto $(Get-Date -Format 'HH:mm:ss') $(Get-Content (Join-Path $R 'out\resultados.txt') -Tail 1)" | Out-File -Encoding utf8 $fim
