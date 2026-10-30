# 30/10: no notebook (i5-10300H, AVX2), depois do A/B do EDGE (out\edge_not_pronto.txt): 1) conferência das 300
# gravações da fase 1 com o exe (o caminho AVX2 dos nativos novos); 2) cap. 16 com GOWA_PERF_JOGO (perfil do jogo de
# verdade: amostras das threads da SPU/PPU por 60 s depois do "go to playing"). O exe grava em ..\..\out (C:\out);
# os perfis vão para out\pj_<exe>_*.txt. Fim em out\perf_not_pronto.txt. Tarefa gowa_perf (sessão interativa).
param([string]$Exe = 'gowa_edge7.exe')
$R = 'C:\gowa-teste'
$fim = Join-Path $R 'out\perf_not_pronto.txt'
Remove-Item $fim -ErrorAction SilentlyContinue
while (-not (Test-Path (Join-Path $R 'out\edge_not_pronto.txt'))) { Start-Sleep -Seconds 30 }
$res = Join-Path $R 'out\perf_not.txt'
"inicio $(Get-Date -Format 'HH:mm:ss') exe=$Exe clock=$((Get-CimInstance Win32_Processor).CurrentClockSpeed)" | Out-File -Encoding utf8 $res

$env:PS3_VFS_ROOT = "$R\vfs"; $env:PS3_LIBSRE_IMAGE = "$R\fw\lle\libsre_image.bin"; $env:GOWA_AUDIO_MUTE = '1'
$env:GOWA_CONF_REPLAY = "$R\out\confere_f1"; $env:GOWA_CONF_REPS = '5'; $env:GOWA_EDGE_NATIVO = '1'; $env:GOWA_CONF_REGS_ABI = '1'
foreach ($m in '1', '0') {
    $env:GOWA_EDGE_NATIVO = $m
    $p = Start-Process -FilePath "$R\build\$Exe" -ArgumentList "$R\jogo\GOWA.elf" -WorkingDirectory "$R\build" -PassThru -WindowStyle Hidden `
         -RedirectStandardError "$R\out\perf_conf_err.txt" -RedirectStandardOutput "$R\out\perf_conf_out.txt"
    if (-not $p.WaitForExit(900000)) { Stop-Process -Id $p.Id -Force; "conferencia nativo=${m}: TRAVOU (15 min)" | Add-Content -Encoding utf8 $res; continue }
    "conferencia nativo=${m}: " + (Get-Content "$R\out\confere_f1\conferencia.txt" -Tail 1) | Add-Content -Encoding utf8 $res
}
foreach ($k in 'PS3_VFS_ROOT', 'PS3_LIBSRE_IMAGE', 'GOWA_AUDIO_MUTE', 'GOWA_CONF_REPLAY', 'GOWA_CONF_REPS', 'GOWA_EDGE_NATIVO', 'GOWA_CONF_REGS_ABI') {
    Remove-Item "env:$k" -ErrorAction SilentlyContinue
}

New-Item -ItemType Directory -Force 'C:\out' | Out-Null
Remove-Item 'C:\out\perfil_jogo_*.txt' -ErrorAction SilentlyContinue
Start-Sleep -Seconds 90
& (Join-Path $R 'teste.ps1') -Cena cap16 -Roteiro lento -Segundos 330 -ExeNome $Exe -Nome "perf_$Exe" -Extra 'GOWA_EDGE_NATIVO=1;GOWA_PERF_JOGO=240,60' *> (Join-Path $R 'out\perf_not_teste.txt')
"cap16 com perfil: " + (Get-Content (Join-Path $R 'out\resultados.txt') -Tail 1) | Add-Content -Encoding utf8 $res
Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.Path -like "$R\build\*" } | Stop-Process -Force -ErrorAction SilentlyContinue
foreach ($f in Get-ChildItem 'C:\out\perfil_jogo_*.txt' -ErrorAction SilentlyContinue) {
    Copy-Item $f.FullName (Join-Path $R ("out\pj_" + [IO.Path]::GetFileNameWithoutExtension($Exe) + "_" + $f.Name.Substring(12)))
}
$log = Get-ChildItem (Join-Path $R 'out') -Filter "cap16_*perf_$Exe*.log" | Sort-Object LastWriteTime | Select-Object -Last 1
if ($log) { Select-String -Path $log.FullName -Pattern '\[perf-jogo\]' | ForEach-Object { $_.Line } | Add-Content -Encoding utf8 $res }
"fim $(Get-Date -Format 'HH:mm:ss')" | Add-Content -Encoding utf8 $res
"pronto" | Out-File -Encoding utf8 $fim
