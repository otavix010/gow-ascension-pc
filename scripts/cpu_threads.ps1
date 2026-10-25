# Uso de CPU por thread do gowa.exe numa janela: duas leituras do
# TotalProcessorTime com -Janela segundos de intervalo, a partir de -Inicio
# segundos depois do início do processo. Mostra as threads mais ocupadas
# (% de um núcleo) e a soma.
param([int]$Inicio = 200, [int]$Janela = 20, [int]$Top = 16)
$p = $null
for ($i = 0; $i -lt 600 -and -not $p; $i++) { $p = Get-Process gowa -ErrorAction SilentlyContinue | Select-Object -First 1; if (-not $p) { Start-Sleep -Milliseconds 500 } }
if (-not $p) { 'gowa.exe não apareceu'; exit 1 }
while (((Get-Date) - $p.StartTime).TotalSeconds -lt $Inicio) { Start-Sleep -Milliseconds 500 }
$p.Refresh()
$t0 = @{}; foreach ($t in $p.Threads) { try { $t0[$t.Id] = $t.TotalProcessorTime.TotalMilliseconds } catch {} }
$w0 = Get-Date
Start-Sleep -Seconds $Janela
$p.Refresh()
$dt = ((Get-Date) - $w0).TotalMilliseconds
$rows = foreach ($t in $p.Threads) {
    try { $ms = $t.TotalProcessorTime.TotalMilliseconds } catch { continue }
    $base = if ($t0.ContainsKey($t.Id)) { $t0[$t.Id] } else { 0 }
    [pscustomobject]@{ Id = $t.Id; Pct = [math]::Round(100 * ($ms - $base) / $dt, 1); Prio = $t.CurrentPriority }
}
$rows | Sort-Object Pct -Descending | Select-Object -First $Top | Format-Table -AutoSize | Out-String
"soma: {0:N1}% de um núcleo ({1} threads)" -f (($rows | Measure-Object Pct -Sum).Sum), $rows.Count
