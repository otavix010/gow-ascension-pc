param([string]$Nome = 'a', [string]$Espera = '1', [long]$Cpus = 0, [int]$Segundos = 90)
# Roda o GoW da bancada no monitor 2, sem som (mesmo ritmo), gravando a saÃ­da de Ã¡udio em WAV.
$R = 'C:\Users\otavi\gow-edge'; $T = 'C:\Users\otavi\.claude\jobs\4d9c3f8a\tmp\audio'
New-Item -ItemType Directory -Force "$T\saves", "$T\cache" | Out-Null
$env:PS3_VFS_ROOT = "$R\vfs"; $env:PS3_LIBSRE_IMAGE = "$R\fw\lle\libsre_image.bin"
$env:PS3_SAVE_ROOT = "$T\saves"; $env:GOWA_CACHE_DIR = "$T\cache"; $env:PS3_WINDOW_MONITOR = 'oculta'
$env:GOWA_AUDIO_MUTE = '1'; $env:GOWA_AUDIO_WAV = "$T\$Nome.wav"; $env:GOWA_AUDIO_ESPERA = $Espera; $env:AUDIO_RATE = '1'; if ($env:TESTE_BUF) { $env:GOWA_AUDIO_BUFFER_MS = $env:TESTE_BUF }
$p = Start-Process -FilePath "$R\port\build_native\gowa.exe" -ArgumentList "`"$R\jogo\GOWA.elf`"" -WorkingDirectory "$R\port\build_native" `
    -RedirectStandardError "$T\$Nome.log" -RedirectStandardOutput "$T\${Nome}_out.log" -PassThru
if ($Cpus) { Start-Sleep -Milliseconds 300; try { $p.ProcessorAffinity = [IntPtr]$Cpus } catch { "afinidade falhou: $_" } }
Start-Sleep -Seconds $Segundos
Get-Process gowa -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq "$R\port\build_native\gowa.exe" } | Stop-Process -Force
"fim $Nome"

