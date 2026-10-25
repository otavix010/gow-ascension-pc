# Rajada de capturas da janela do gowa.exe (o que aparece na tela, composto
# pelo DWM): espera o processo ter <Segundos> de vida e grava <N> PNGs a cada
# <IntervaloMs>. Para ver piscar/cintilar na apresentação, que o despejo de
# quadros (LD_FRAME_DUMP) não mostra.
# Uso: captura_rajada.ps1 -Segundos 70 -N 40 -IntervaloMs 40 -Pasta out\rajada
param([double]$Segundos = 70, [int]$N = 40, [int]$IntervaloMs = 40,
      [string]$Pasta = "$(Split-Path -Parent $PSScriptRoot)\out\rajada")
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System; using System.Runtime.InteropServices;
public class GowaWin2 {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
}
"@
New-Item -ItemType Directory -Force $Pasta | Out-Null
Get-ChildItem $Pasta -Filter *.png | ForEach-Object { Remove-Item $_.FullName }
$t0 = Get-Date
$p = $null
while (-not $p -and ((Get-Date) - $t0).TotalSeconds -lt 120) {
    $p = Get-Process gowa -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
    if (-not $p) { Start-Sleep -Milliseconds 500 }
}
if (-not $p) { "janela do gowa não encontrada"; exit 1 }
while (((Get-Date) - $p.StartTime).TotalSeconds -lt $Segundos) { Start-Sleep -Milliseconds 100 }
$r = New-Object GowaWin2+RECT
[GowaWin2]::GetWindowRect($p.MainWindowHandle, [ref]$r) | Out-Null
$w = $r.R - $r.L; $h = $r.B - $r.T
for ($i = 0; $i -lt $N; $i++) {
    $bmp = New-Object System.Drawing.Bitmap $w, $h
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    # Só PrintWindow (o conteúdo da própria janela). NÃO usar CopyFromScreen:
    # ele copia o que estiver na tela naquela área, inclusive outras janelas
    # por cima do jogo (19/10: pegou um terminal dele).
    $hdc = $g.GetHdc()
    [GowaWin2]::PrintWindow($p.MainWindowHandle, $hdc, 2) | Out-Null
    $g.ReleaseHdc($hdc)
    $bmp.Save((Join-Path $Pasta ('cap_{0:D3}.png' -f $i)), [System.Drawing.Imaging.ImageFormat]::Png)
    $g.Dispose(); $bmp.Dispose()
    Start-Sleep -Milliseconds $IntervaloMs
}
"$N capturas ${w}x${h} em $Pasta"
