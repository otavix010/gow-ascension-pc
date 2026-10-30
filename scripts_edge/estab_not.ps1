# 30/10: estabilidade do EDGE nativo no notebook: 12 aberturas do cap. 16 com o gowa_edge10.exe (nativo 1, 1, 0, ...),
# 260 s cada (o "go to playing" sai aos ~190 s), contando quantas chegam ao jogo. A rodada 2 do A/B nat01 travou aos
# 24 s ("initialize local transport", tela de carregamento) com o nativo ligado. Resultado em out\estab_not.txt.
param([string]$Exe = 'gowa_edge10.exe', [int]$N = 12, [string]$Ate = '23:59')
$R = 'C:\gowa-teste'
$res = Join-Path $R 'out\estab_not.txt'
"inicio $(Get-Date -Format 'HH:mm:ss') exe=$Exe" | Out-File -Encoding utf8 $res
for ($i = 1; $i -le $N; $i++) {
    if ((Get-Date -Format 'HH:mm') -ge $Ate) { break }
    $m = if ($i % 3 -eq 0) { '0' } else { '1' }
    Start-Sleep -Seconds 30
    & (Join-Path $R 'teste.ps1') -Cena cap16 -Roteiro lento -Segundos 260 -ExeNome $Exe -Nome "estab${m}_$i" -Extra "GOWA_EDGE_NATIVO=$m" *> (Join-Path $R 'out\estab_teste.txt')
    Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.Path -like "$R\build\*" } | Stop-Process -Force -ErrorAction SilentlyContinue
    $log = Get-ChildItem (Join-Path $R 'out') -Filter "cap16_*estab${m}_$i*.log" | Sort-Object LastWriteTime | Select-Object -Last 1
    $go = if ($log) { Select-String -Path $log.FullName -Pattern 'go to playing' -SimpleMatch -Quiet } else { $false }
    $ult = if ($log) { (Select-String -Path $log.FullName -Pattern '\[jogo-log\]' | Select-Object -Last 1).Line } else { 'sem log' }
    $esp = if ($log -and -not $go) { (Select-String -Path $log.FullName -Pattern 'espera 0x415914' | Select-Object -Last 1).Line } else { '' }
    "rodada $i nativo=$m jogo=$(if ($go) { 'sim' } else { 'NAO' }) | $(if ($ult) { $ult.Substring(0, [Math]::Min(80, $ult.Length)) }) $esp" | Add-Content -Encoding utf8 $res
}
"fim $(Get-Date -Format 'HH:mm:ss')" | Add-Content -Encoding utf8 $res
