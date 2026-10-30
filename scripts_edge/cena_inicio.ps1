param([string]$Nome = 'ini', [string]$Extra = '', [long]$Cpus = 0, [int]$Segundos = 250, [string]$Capturas = '', [string]$Mov = '')
# Cena "inicio" (novo jogo, fase 1) da bancada, com o controle por roteiro (o mesmo do teste do henry),
# janela fora da tela, sem som (mesmo ritmo), saÃ­da de Ã¡udio em WAV. -Extra 'CHAVE=valor;...'
$R = 'C:\Users\otavi\gow-edge'; $T = 'C:\Users\otavi\.claude\jobs\4d9c3f8a\tmp\cena'
Remove-Item -Recurse -Force "$T\saves_$Nome" -ErrorAction SilentlyContinue
New-Item -ItemType Directory -Force "$T\cache" | Out-Null
# o perfil das medições (o mesmo da cena "inicio" do ab_fps.ps1), copiado: o jogo grava nele
Copy-Item -Recurse 'C:\Users\otavi\gowa-recomp\port\build\gamedata\dev_hdd0\home\00000001\savedata' "$T\saves_$Nome"
$envs = @{
    PS3_VFS_ROOT = "$R\vfs"; PS3_LIBSRE_IMAGE = "$R\fw\lle\libsre_image.bin"; PS3_SAVE_ROOT = "$T\saves_$Nome"
    GOWA_CACHE_DIR = "$R\cache"; PS3_WINDOW_MONITOR = 'oculta'; GOWA_AUDIO_MUTE = '1'; GOWA_AUDIO_WAV = "$T\$Nome.wav"
    GOWA_AUDIO_BUFFER_MS = '100'; GOWA_FLIP_HZ = '0'
    PAD_SCRIPT = '50:0x4000,70:0x2000,85:0x4000,105:0x0008,125:0x4000,145:0x4000,165:0x4000,185:0x4000'; WATCHDOG_AT = '172,180'
}
foreach ($kv in ($Extra -split ';')) { if ($kv -match '^([^=]+)=(.*)$') { $envs[$matches[1]] = $matches[2] } }
foreach ($k in $envs.Keys) { Set-Item -Path "env:$k" -Value $envs[$k] }
$p = Start-Process -FilePath "$R\port\build_native\gowa.exe" -ArgumentList "`"$R\jogo\GOWA.elf`"" -WorkingDirectory "$R\port\build_native" `
    -RedirectStandardError "$T\$Nome.log" -RedirectStandardOutput "$T\${Nome}_out.log" -PassThru
if ($Cpus) { Start-Sleep -Milliseconds 300; try { $p.ProcessorAffinity = [IntPtr]$Cpus } catch { "afinidade falhou: $_" } }
$t0 = Get-Date
if ($Capturas) {
    Add-Type -AssemblyName System.Drawing
    if (-not ('CenaWin' -as [type])) { Add-Type 'using System; using System.Runtime.InteropServices; public class CenaWin { [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; } [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r); [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint f); }' }
    foreach ($cs in ($Capturas -split ',' | ForEach-Object { [int]$_ })) {
        $falta = $cs - ((Get-Date) - $t0).TotalSeconds
        if ($falta -gt 0) { Start-Sleep -Milliseconds ([int]($falta * 1000)) }
        $p.Refresh(); $h = $p.MainWindowHandle
        if ($h -eq 0) { "sem janela aos $cs s"; continue }
        $r = New-Object CenaWin+RECT; [void][CenaWin]::GetWindowRect($h, [ref]$r)
        $bmp = New-Object System.Drawing.Bitmap ([Math]::Max(1, $r.R - $r.L)), ([Math]::Max(1, $r.B - $r.T))
        $g = [System.Drawing.Graphics]::FromImage($bmp); $hdc = $g.GetHdc(); [void][CenaWin]::PrintWindow($h, $hdc, 2); $g.ReleaseHdc($hdc)
        $bmp.Save("$T\${Nome}_$cs.png"); $g.Dispose(); $bmp.Dispose()
    }
}
if ($Mov -match '^(\d+)-(\d+)$') {
    # movimento da imagem a cada ~100 ms (160x90, diferença média do verde para a captura anterior), com a hora
    # de parede, em <Nome>_mov.tsv -- o formato do atraso.py
    $de, $ate = [int]$matches[1], [int]$matches[2]
    Add-Type -AssemblyName System.Drawing
    if (-not ('CenaWin' -as [type])) { Add-Type 'using System; using System.Runtime.InteropServices; public class CenaWin { [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; } [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r); [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint f); }' }
    while (((Get-Date) - $t0).TotalSeconds -lt $de) { Start-Sleep -Milliseconds 200 }
    $p.Refresh(); $h = $p.MainWindowHandle
    $r = New-Object CenaWin+RECT; [void][CenaWin]::GetWindowRect($h, [ref]$r)
    $big = New-Object System.Drawing.Bitmap ([Math]::Max(1, $r.R - $r.L)), ([Math]::Max(1, $r.B - $r.T))
    $small = New-Object System.Drawing.Bitmap 160, 90
    $gb = [System.Drawing.Graphics]::FromImage($big); $gs = [System.Drawing.Graphics]::FromImage($small)
    $rect = New-Object System.Drawing.Rectangle 0, 0, 160, 90
    $prev = $null; $out = New-Object System.Collections.Generic.List[string]
    while (((Get-Date) - $t0).TotalSeconds -lt $ate) {
        $hdc = $gb.GetHdc(); [void][CenaWin]::PrintWindow($h, $hdc, 2); $gb.ReleaseHdc($hdc)
        $ms = [DateTimeOffset]::Now.ToUnixTimeMilliseconds()
        $gs.DrawImage($big, 0, 0, 160, 90)
        $bd = $small.LockBits($rect, 'ReadOnly', 'Format32bppArgb')
        $buf = New-Object byte[] ($bd.Stride * 90); [Runtime.InteropServices.Marshal]::Copy($bd.Scan0, $buf, 0, $buf.Length); $small.UnlockBits($bd)
        $dif = 0.0; $lum = 0.0
        if ($prev) { for ($i = 0; $i -lt $buf.Length; $i += 4) { $dif += [Math]::Abs([int]$buf[$i + 1] - [int]$prev[$i + 1]); $lum += $buf[$i + 1] } }
        $out.Add(("{0}`t{1}`t{2}" -f $ms, ($dif / 14400).ToString('F2', [Globalization.CultureInfo]::InvariantCulture), ($lum / 14400).ToString('F1', [Globalization.CultureInfo]::InvariantCulture)))
        $prev = $buf
        Start-Sleep -Milliseconds 60
    }
    $out | Set-Content "$T\${Nome}_mov.tsv"
}
$resta = $Segundos - ((Get-Date) - $t0).TotalSeconds
if ($resta -gt 0) { Start-Sleep -Milliseconds ([int]($resta * 1000)) }
Stop-Process -Id $p.Id -Force -ErrorAction SilentlyContinue
if (-not $p.WaitForExit(10000)) { "AVISO: o jogo (PID $($p.Id)) não fechou" }
foreach ($k in $envs.Keys) { Remove-Item "env:$k" -ErrorAction SilentlyContinue }
$go = Select-String -Path "$T\$Nome.log", "$T\${Nome}_out.log" -Pattern 'go to playing' | Select-Object -First 1
"fim $Nome; go to playing: $(if ($go) { 'sim' } else { 'NÃƒO' })"


