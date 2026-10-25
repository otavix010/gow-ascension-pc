# Roda o gowa.exe por N segundos com stdout+stderr no MESMO arquivo (ordem real) e mata no fim.
# Uso: rodar_gowa.ps1 -Segundos 60 -Log out\run01.log [-Env @{ PS3_VFS_ROOT = '...' }]
param(
    [int]$Segundos = 60,
    [string]$Log = "$(Split-Path -Parent $PSScriptRoot)\out\run.log",
    [hashtable]$Env = @{},
    [string]$Elf = "$(Split-Path -Parent $PSScriptRoot)\jogo\GOWA.elf",
    [string]$Exe = "$(Split-Path -Parent $PSScriptRoot)\port\build\gowa.exe"
)
if (-not $Env.ContainsKey('PS3_VFS_ROOT')) { $Env['PS3_VFS_ROOT'] = "$(Split-Path -Parent $PSScriptRoot)\vfs" }
# janela de teste no monitor secundário, sem roubar o foco (pedido dele, 17/10); -Env @{PS3_WINDOW_MONITOR='1'} volta ao principal
if (-not $Env.ContainsKey('PS3_WINDOW_MONITOR')) { $Env['PS3_WINDOW_MONITOR'] = '2' }
# o jogo agora tem som (18/10): testes automáticos em mudo; -Env @{GOWA_AUDIO_MUTE='0'} para ouvir
if (-not $Env.ContainsKey('GOWA_AUDIO_MUTE')) { $Env['GOWA_AUDIO_MUTE'] = '1' }
# Condições de medição (24/10): sem monitor secundário a janela de teste abre no principal
# (240 Hz, o boot sobe para 200-240 FPS e o roteiro do controle desanda), e outro processo
# pesado (o wimlib-imagex dele usou ~14 núcleos) invalida a medida. Espera até 30 min por
# um monitor não principal e menos de ~2 núcleos ocupados pelos outros processos.
# -Env @{GOWA_SEM_ESPERA='1'} pula.
if (-not $Env.ContainsKey('GOWA_SEM_ESPERA')) {
    Add-Type -AssemblyName System.Windows.Forms
    $limite = (Get-Date).AddMinutes(30); $avisou = $false
    while ((Get-Date) -lt $limite) {
        $sec = @([System.Windows.Forms.Screen]::AllScreens | Where-Object { -not $_.Primary }).Count
        $a = @{}; Get-Process | ForEach-Object { if ($_.CPU) { $a[$_.Id] = $_.CPU } }
        Start-Sleep -Seconds 3
        $carga = 0.0
        Get-Process | Where-Object { $_.ProcessName -notlike 'gowa*' } | ForEach-Object { if ($_.CPU -and $a.ContainsKey($_.Id)) { $carga += $_.CPU - $a[$_.Id] } }
        $carga = $carga / 3
        if ($sec -ge 1 -and $carga -lt 2.0) { break }
        if (-not $avisou) { "{0:HH:mm} esperando condições: monitores secundários={1}, carga dos outros={2:N1} núcleos" -f (Get-Date), $sec, $carga | Out-Host; $avisou = $true }
        Start-Sleep -Seconds 27
    }
}
foreach ($k in $Env.Keys) { Set-Item -Path "env:$k" -Value $Env[$k] }
$t0 = Get-Date
$p = Start-Process -FilePath cmd.exe -ArgumentList "/c", "`"`"$Exe`" `"$Elf`" > `"$Log`" 2>&1`"" `
     -WorkingDirectory (Split-Path $Exe) -PassThru -WindowStyle Hidden
$terminou = $p.WaitForExit($Segundos * 1000)
if (-not $terminou) {
    # pelo caminho, qualquer nome (gowa.exe, gowa_base.exe... no A/B com dois executáveis)
    Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $Exe } | Stop-Process -Force
    $p.WaitForExit(5000) | Out-Null
}
foreach ($k in $Env.Keys) { Remove-Item -Path "env:$k" -ErrorAction SilentlyContinue }
$code = if ($terminou) { $p.ExitCode } else { "morto apos $Segundos s" }
"exit: {0} ({1:N1} s); log {2} ({3} linhas)" -f $code, ((Get-Date) - $t0).TotalSeconds, $Log, (Get-Content $Log | Measure-Object -Line).Lines
