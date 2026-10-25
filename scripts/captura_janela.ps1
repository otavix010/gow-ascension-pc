# Captura a janela do gowa.exe (ou a tela inteira, se não achar) em PNG.
# Uso: captura_janela.ps1 -Saida out\tela.png
param([string]$Saida = "$(Split-Path -Parent $PSScriptRoot)\out\tela.png")
Add-Type -AssemblyName System.Drawing
Add-Type @"
using System; using System.Runtime.InteropServices;
public class GowaWin {
  [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; }
  [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r);
  [DllImport("user32.dll")] public static extern bool SetForegroundWindow(IntPtr h);
  [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint flags);
}
"@
$p = Get-Process gowa -ErrorAction SilentlyContinue | Where-Object { $_.MainWindowHandle -ne 0 } | Select-Object -First 1
if ($p) {
    $r = New-Object GowaWin+RECT
    [GowaWin]::GetWindowRect($p.MainWindowHandle, [ref]$r) | Out-Null
    $w = $r.R - $r.L; $h = $r.B - $r.T
    $bmp = New-Object System.Drawing.Bitmap $w, $h
    $g = [System.Drawing.Graphics]::FromImage($bmp)
    $hdc = $g.GetHdc()
    [GowaWin]::PrintWindow($p.MainWindowHandle, $hdc, 2) | Out-Null   # PW_RENDERFULLCONTENT
    $g.ReleaseHdc($hdc)
    "janela '$($p.MainWindowTitle)' ${w}x${h}"
} else {
    # sem fallback para a tela inteira: ela pode mostrar o que o usuário está fazendo (17/10)
    "janela do gowa não encontrada; nada capturado"
    exit 1
}
$bmp.Save($Saida, [System.Drawing.Imaging.ImageFormat]::Png)
$g.Dispose(); $bmp.Dispose()
"salvo em $Saida"
