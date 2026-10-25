# Sessão conduzida de fora (máquina de teste): o jogo roda com PAD_FILE=out\pad.txt (quem está de
# fora escreve o botão: "0x4000" = X, "0x0040" = baixo, "0x0008" = START; "0x0040 4" = 4 leituras)
# e, a cada out\capt.req criado, a JANELA do jogo (só ela, PrintWindow) vai para out\tela.png.
# out\fim.req encerra antes do tempo. No fim, o FPS da fase como no teste.ps1.
# Uso: teste_manual.ps1 [-Cena cap16|inicio] [-Segundos 900] [-Limite] [-ExeNome gowa.exe]
param([string]$Cena = 'cap16', [int]$Segundos = 900, [switch]$Limite, [string]$ExeNome = 'gowa.exe')
$R = Split-Path -Parent $MyInvocation.MyCommand.Path
$exe = Join-Path (Join-Path $R 'build') $ExeNome
$elf = Join-Path $R 'jogo\GOWA.elf'
$O = Join-Path $R 'out'
New-Item -ItemType Directory -Force $O | Out-Null
New-Item -ItemType Directory -Force (Join-Path $R 'build\scratch') | Out-Null
foreach ($f in 'pad.txt', 'capt.req', 'fim.req', 'tela.png', 'tela.ok') { Remove-Item (Join-Path $O $f) -ErrorAction SilentlyContinue }
Set-Content -Path (Join-Path $O 'pad.txt') -Value '' -NoNewline
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System; using System.Runtime.InteropServices;
public class GowaWinT {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
}
"@
function Captura {
    $p = Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $exe -and $_.MainWindowHandle -ne 0 } | Select-Object -First 1
    if (-not $p) { Set-Content (Join-Path $O 'tela.ok') 'janela não encontrada'; return }
    $r = New-Object GowaWinT+RECT
    [GowaWinT]::GetWindowRect($p.MainWindowHandle, [ref]$r) | Out-Null
    $w = $r.R - $r.L; $h = $r.B - $r.T
    $bmp = New-Object System.Drawing.Bitmap $w, $h
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $hdc = $g.GetHdc()
    [GowaWinT]::PrintWindow($p.MainWindowHandle, $hdc, 2) | Out-Null
    $g.ReleaseHdc($hdc)
    $bmp.Save((Join-Path $O 'tela.png'), [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    Set-Content (Join-Path $O 'tela.ok') ("{0:N1} s, {1}x{2}" -f ((Get-Date) - $t0).TotalSeconds, $w, $h)
}
$envs = @{
    PS3_VFS_ROOT = (Join-Path $R 'vfs'); PS3_LIBSRE_IMAGE = (Join-Path $R 'fw\lle\libsre_image.bin')
    PS3_WINDOW_MONITOR = '1'; GOWA_AUDIO_MUTE = '1'; PAD_FILE = (Join-Path $O 'pad.txt')
}
if ($Cena -eq 'cap16') { $envs['PS3_SAVE_ROOT'] = (Join-Path $R 'saves\cap16') }
if (-not $Limite) { $envs['GOWA_FLIP_HZ'] = '0' }
$log = Join-Path $O ("manual_{0}_{1}.log" -f $Cena, (Get-Date -Format HHmm))
foreach ($k in $envs.Keys) { Set-Item -Path "env:$k" -Value $envs[$k] }
$t0 = Get-Date
$p = Start-Process -FilePath cmd.exe -ArgumentList '/c', "`"`"$exe`" `"$elf`" > `"$log`" 2>&1`"" `
     -WorkingDirectory (Split-Path $exe) -PassThru -WindowStyle Hidden
Set-Content (Join-Path $O 'manual_log.txt') $log
while (-not $p.HasExited -and ((Get-Date) - $t0).TotalSeconds -lt $Segundos) {
    if (Test-Path (Join-Path $O 'capt.req')) { Remove-Item (Join-Path $O 'capt.req'); try { Captura } catch { Set-Content (Join-Path $O 'tela.ok') "erro: $_" } }
    if (Test-Path (Join-Path $O 'fim.req')) { break }
    Start-Sleep -Milliseconds 400
}
Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $exe } | Stop-Process -Force
$p.WaitForExit(5000) | Out-Null
foreach ($k in $envs.Keys) { Remove-Item -Path "env:$k" -ErrorAction SilentlyContinue }
$fim = "{0:HH:mm} FIM manual {1} ({2:N0} s), log {3}" -f (Get-Date), $Cena, ((Get-Date) - $t0).TotalSeconds, $log
Add-Content -Path (Join-Path $O 'resultados.txt') -Value $fim -Encoding UTF8
Set-Content -Path (Join-Path $O 'pronto.txt') -Value $fim -Encoding UTF8
