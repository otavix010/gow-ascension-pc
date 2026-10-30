# 29/10 noite: EDGE nativo (4 partes: plugins 0x74C580 e 0x752710, D640, E550/EC08) no notebook (i5-10300H, AVX2),
# exe portátil gowa_edge4.exe. 1) conferência das 300 gravações da fase 1 com o nativo ligado; 2) A/B no capítulo 16
# (roteiro lento do notebook), mesmo exe, GOWA_EDGE_NATIVO 0 x 1 alternado. Roda pela tarefa gowa_edge (sessão
# interativa). Resultado em out\edge_ab_not.txt; fim em out\edge_not_pronto.txt.
param([string]$Exe = 'gowa_edge4.exe', [int]$Pares = 3)
$R = 'C:\gowa-teste'
$fim = Join-Path $R 'out\edge_not_pronto.txt'
Remove-Item $fim -ErrorAction SilentlyContinue
$res = Join-Path $R 'out\edge_ab_not.txt'
"inicio $(Get-Date -Format 'HH:mm:ss') exe=$Exe" | Out-File -Encoding utf8 $res

# 1) conferência (sem janela)
$env:PS3_VFS_ROOT = "$R\vfs"; $env:PS3_LIBSRE_IMAGE = "$R\fw\lle\libsre_image.bin"; $env:GOWA_AUDIO_MUTE = '1'
$env:GOWA_CONF_REPLAY = "$R\out\confere_f1"; $env:GOWA_CONF_REPS = '5'; $env:GOWA_EDGE_NATIVO = '1'; $env:GOWA_CONF_REGS_ABI = '1'
$p = Start-Process -FilePath "$R\build\$Exe" -ArgumentList "$R\jogo\GOWA.elf" -WorkingDirectory "$R\build" -PassThru -WindowStyle Hidden `
     -RedirectStandardError "$R\out\edge_conf_err.txt" -RedirectStandardOutput "$R\out\edge_conf_out.txt"
$p.WaitForExit()
"conferencia nativo: " + (Get-Content "$R\out\confere_f1\conferencia.txt" -Tail 1) | Add-Content -Encoding utf8 $res
foreach ($k in 'PS3_VFS_ROOT', 'PS3_LIBSRE_IMAGE', 'GOWA_AUDIO_MUTE', 'GOWA_CONF_REPLAY', 'GOWA_CONF_REPS', 'GOWA_EDGE_NATIVO', 'GOWA_CONF_REGS_ABI') {
    Remove-Item "env:$k" -ErrorAction SilentlyContinue
}

# 2) A/B no capítulo 16
foreach ($par in 1..$Pares) {
    foreach ($modo in '0', '1') {
        # o notebook esquenta: 90 s parado antes de cada rodada (na 1ª tentativa o fps caiu de 10,4 para 6-7 já
        # na 2ª rodada, com o nativo ligado ou não); 320 s para a janela de medição (40 s depois do "go to playing",
        # que aqui sai aos ~190 s) caber
        Start-Sleep -Seconds 90
        & (Join-Path $R 'teste.ps1') -Cena cap16 -Roteiro lento -Segundos 320 -ExeNome $Exe -Nome "edge4_${modo}_$par" -Extra "GOWA_EDGE_NATIVO=$modo" *> (Join-Path $R "out\edge_not_teste.txt")
        $l = Get-Content (Join-Path $R 'out\resultados.txt') -Tail 1
        "cap16 nativo=$modo par ${par}: $l" | Add-Content -Encoding utf8 $res
        Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.Path -like "$R\build\*" } | Stop-Process -Force -ErrorAction SilentlyContinue
    }
}
"fim $(Get-Date -Format 'HH:mm:ss')" | Add-Content -Encoding utf8 $res
"pronto" | Out-File -Encoding utf8 $fim
