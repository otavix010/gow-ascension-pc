# EDGE nativo no notebook (i5-10300H, AVX2, exes portáteis v3rz). 1) conferência das 300 gravações da fase 1 com o
# exe B; 2) A/B no capítulo 16 (roteiro lento do notebook) entre dois exes, alternados, com o nativo ligado nos dois.
# 30/10 00h10: A = gowa_edge4.exe (plugins 0x74C580 e 0x752710, D640, E550/EC08), B = gowa_edge6.exe (o mesmo + 58 laços com registradores locais; antes, edge5 = + vk
# da DCC0/E3C4). Roda pela tarefa gowa_edge (sessão interativa). Resultado em out\edge_ab_not.txt; fim em
# out\edge_not_pronto.txt.
param([string]$ExeA = 'gowa_edge4.exe', [string]$ExeB = 'gowa_edge6.exe', [int]$Pares = 3)
$R = 'C:\gowa-teste'
$fim = Join-Path $R 'out\edge_not_pronto.txt'
Remove-Item $fim -ErrorAction SilentlyContinue
$res = Join-Path $R 'out\edge_ab_not.txt'
"inicio $(Get-Date -Format 'HH:mm:ss') A=$ExeA B=$ExeB clock=$((Get-CimInstance Win32_Processor).CurrentClockSpeed)" | Out-File -Encoding utf8 $res

# 1) conferência (sem janela)
$env:PS3_VFS_ROOT = "$R\vfs"; $env:PS3_LIBSRE_IMAGE = "$R\fw\lle\libsre_image.bin"; $env:GOWA_AUDIO_MUTE = '1'
$env:GOWA_CONF_REPLAY = "$R\out\confere_f1"; $env:GOWA_CONF_REPS = '5'; $env:GOWA_EDGE_NATIVO = '1'; $env:GOWA_CONF_REGS_ABI = '1'
foreach ($e in $ExeA, $ExeB) {
    $p = Start-Process -FilePath "$R\build\$e" -ArgumentList "$R\jogo\GOWA.elf" -WorkingDirectory "$R\build" -PassThru -WindowStyle Hidden `
         -RedirectStandardError "$R\out\edge_conf_err.txt" -RedirectStandardOutput "$R\out\edge_conf_out.txt"
    if (-not $p.WaitForExit(900000)) { Stop-Process -Id $p.Id -Force; "conferencia ${e}: TRAVOU (15 min)" | Add-Content -Encoding utf8 $res; continue }
    "conferencia ${e}: " + (Get-Content "$R\out\confere_f1\conferencia.txt" -Tail 1) | Add-Content -Encoding utf8 $res
}
foreach ($k in 'PS3_VFS_ROOT', 'PS3_LIBSRE_IMAGE', 'GOWA_AUDIO_MUTE', 'GOWA_CONF_REPLAY', 'GOWA_CONF_REPS', 'GOWA_EDGE_NATIVO', 'GOWA_CONF_REGS_ABI') {
    Remove-Item "env:$k" -ErrorAction SilentlyContinue
}

# 2) A/B no capítulo 16: o notebook esquenta, então 90 s parado antes de cada rodada e a ordem alterna a cada par;
# 320 s para a janela de medição (40 s depois do "go to playing", que aqui sai aos ~190 s) caber
foreach ($par in 1..$Pares) {
    $ordem = if ($par % 2) { @($ExeA, $ExeB) } else { @($ExeB, $ExeA) }
    foreach ($e in $ordem) {
        Start-Sleep -Seconds 90
        $mhz = (Get-CimInstance Win32_Processor).CurrentClockSpeed
        & (Join-Path $R 'teste.ps1') -Cena cap16 -Roteiro lento -Segundos 320 -ExeNome $e -Nome "ab_${e}_$par" -Extra 'GOWA_EDGE_NATIVO=1' *> (Join-Path $R 'out\edge_not_teste.txt')
        $l = Get-Content (Join-Path $R 'out\resultados.txt') -Tail 1
        "cap16 $e par ${par} (clock $mhz): $l" | Add-Content -Encoding utf8 $res
        Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.Path -like "$R\build\*" } | Stop-Process -Force -ErrorAction SilentlyContinue
    }
}
"fim $(Get-Date -Format 'HH:mm:ss')" | Add-Content -Encoding utf8 $res
"pronto" | Out-File -Encoding utf8 $fim
