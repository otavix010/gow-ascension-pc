# 29/10: EDGE nativo no PC de teste 2 (i5-11400F + RX 9060 XT), exe portátil (AVX2, para-zero) gowa_edge.exe.
# 1) conferência das 300 gravações com o nativo ligado; 2) A/B no jogo, mesmo exe, GOWA_EDGE_NATIVO 0 x 1,
# cap. 16 e fase 1, alternando. Roda pela tarefa gowa_pele (sessão interativa). Fim: out\edge_pronto.txt
$R = 'C:\OtavixPorts\gowa-teste'
$fim = Join-Path $R 'out\edge_pronto.txt'
Remove-Item $fim -ErrorAction SilentlyContinue
$res = Join-Path $R 'out\edge_ab.txt'
"inicio $(Get-Date -Format 'HH:mm:ss')" | Out-File -Encoding utf8 $res

# 1) conferência (sem janela)
$env:PS3_VFS_ROOT = "$R\vfs"; $env:PS3_LIBSRE_IMAGE = "$R\fw\lle\libsre_image.bin"; $env:GOWA_AUDIO_MUTE = '1'
$env:GOWA_CONF_REPLAY = "$R\out\confere"; $env:GOWA_CONF_REPS = '5'; $env:GOWA_EDGE_NATIVO = '1'; $env:GOWA_CONF_REGS_ABI = '1'
$p = Start-Process -FilePath "$R\build\gowa_edge.exe" -ArgumentList "$R\jogo\GOWA.elf" -WorkingDirectory "$R\build" -PassThru -WindowStyle Hidden `
     -RedirectStandardError "$R\out\edge_conf_err.txt" -RedirectStandardOutput "$R\out\edge_conf_out.txt"
$p.WaitForExit()
Copy-Item "$R\out\confere\conferencia.txt" "$R\out\conf_edge_nativo.txt" -Force
"conferencia nativo: " + (Get-Content "$R\out\confere\conferencia.txt" -Tail 1) | Add-Content -Encoding utf8 $res
foreach ($k in 'PS3_VFS_ROOT', 'PS3_LIBSRE_IMAGE', 'GOWA_AUDIO_MUTE', 'GOWA_CONF_REPLAY', 'GOWA_CONF_REPS', 'GOWA_EDGE_NATIVO', 'GOWA_CONF_REGS_ABI') {
    Remove-Item "env:$k" -ErrorAction SilentlyContinue
}

# 2) A/B no jogo
foreach ($cena in 'cap16', 'inicio') {
    foreach ($rodada in 1, 2) {
        foreach ($modo in '0', '1') {
            & (Join-Path $R 'teste.ps1') -Cena $cena -ExeNome gowa_edge.exe -Nome "edge${modo}_$rodada" -Extra "GOWA_EDGE_NATIVO=$modo" *> (Join-Path $R "out\edge_teste.txt")
            $l = Get-Content (Join-Path $R 'out\resultados.txt') -Tail 1
            "$cena nativo=$modo rodada ${rodada}: $l" | Add-Content -Encoding utf8 $res
            Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.Path -like "$R\build\*" } | Stop-Process -Force -ErrorAction SilentlyContinue
        }
    }
}
"fim $(Get-Date -Format 'HH:mm:ss')" | Add-Content -Encoding utf8 $res
"pronto" | Out-File -Encoding utf8 $fim
