# Teste de desempenho do GoW Ascension (port nativo) numa máquina de teste.
# Autônomo: tudo relativo à pasta deste script (build\, jogo\, fw\, vfs\, saves\, out\).
# Cenas iguais às do PC principal (scripts\ab_fps.ps1): "inicio" = fase 1 (abertura, novo jogo),
# "cap16" = Capítulo 16 do save de teste (saves\cap16, Perfil 4, "Continuar jogo").
# FPS = média do campo jogo= das linhas [fps] a partir de 40 s depois de "go to playing"
# (scripts\fps_fase.ps1). Uso:
#   teste.ps1 -Cena cap16 -Rodadas 2 [-Limite] [-Nome x] [-Segundos 230] [-Atraso 0] [-ExeNome gowa.exe]
# -Limite: com o limitador de 60 (uso normal); sem ele, GOWA_FLIP_HZ=0 (como nos A/B).
# -Atraso: segundos somados a cada tempo do roteiro do controle (máquinas que carregam mais devagar).
param([string]$Cena = 'cap16', [int]$Rodadas = 1, [switch]$Limite, [string]$Nome = '',
      [int]$Segundos = 0, [int]$Atraso = 0, [string]$ExeNome = 'gowa.exe', [string]$Roteiro = '', [string]$Extra = '', [int]$Captura = 0)
# -Captura <s>: PNG só da janela do jogo (PrintWindow) nesse segundo, ao lado do log
# -Extra: variáveis de ambiente a mais, 'CHAVE=valor;CHAVE2=valor2'
# -Roteiro: roteiro do controle inteiro ("<s>:<máscara>,..."), no lugar do da cena (máquinas lentas:
# os menus andam por quadro e precisam de mais tempo entre os toques)
$R = Split-Path -Parent $MyInvocation.MyCommand.Path
$exe = Join-Path (Join-Path $R 'build') $ExeNome
$elf = Join-Path $R 'jogo\GOWA.elf'
New-Item -ItemType Directory -Force (Join-Path $R 'out') | Out-Null
New-Item -ItemType Directory -Force (Join-Path $R 'build\scratch') | Out-Null
if ($Cena -eq 'cap16') {
    $tempos = @(50, 85, 95, 98, 101, 106, 115); $botoes = @('0x4000', '0x4000', '0x0040', '0x0040', '0x0040', '0x4000', '0x4000')
    if (-not $Segundos) { $Segundos = 230 }
} else {
    $tempos = @(50, 85, 105, 125, 145, 165, 185); $botoes = @('0x4000', '0x4000', '0x0008', '0x4000', '0x4000', '0x4000', '0x4000')
    if (-not $Segundos) { $Segundos = 260 }
}
$Segundos += $Atraso
$pad = (0..($tempos.Count - 1) | ForEach-Object { "$($tempos[$_] + $Atraso):$($botoes[$_])" }) -join ','
# 'lento': o roteiro do cap. 16 medido no notebook (i5-10300H, menus a 6-20 FPS, 24/10)
if ($Roteiro -eq 'lento') { $Roteiro = '60:0x4000,92:0x4000,112:0x0040,118:0x0040,124:0x0040,140:0x4000,165:0x4000' }
if ($Roteiro) { $pad = $Roteiro }
$envs = @{
    PS3_VFS_ROOT = (Join-Path $R 'vfs'); PS3_LIBSRE_IMAGE = (Join-Path $R 'fw\lle\libsre_image.bin')
    PS3_WINDOW_MONITOR = '1'; GOWA_AUDIO_MUTE = '1'; PAD_SCRIPT = $pad; WATCHDOG_AT = "$(172 + $Atraso),$(180 + $Atraso)"
}
if ($Cena -eq 'cap16') { $envs['PS3_SAVE_ROOT'] = (Join-Path $R 'saves\cap16') }
if (-not $Limite) { $envs['GOWA_FLIP_HZ'] = '0' }
foreach ($kv in ($Extra -split ';')) { if ($kv -match '^([^=]+)=(.*)$') { $envs[$matches[1]] = $matches[2] } }
$res = @()
for ($i = 1; $i -le $Rodadas; $i++) {
    $log = Join-Path $R ("out\{0}_{1}{2}_{3}.log" -f $Cena, $(if ($Limite) { 'lim' } else { 'sem' }), $Nome, $i)
    foreach ($k in $envs.Keys) { Set-Item -Path "env:$k" -Value $envs[$k] }
    $t0 = Get-Date
    $p = Start-Process -FilePath cmd.exe -ArgumentList '/c', "`"`"$exe`" `"$elf`" > `"$log`" 2>&1`"" `
         -WorkingDirectory (Split-Path $exe) -PassThru -WindowStyle Hidden
    if ($Captura -gt 0 -and $Captura -lt $Segundos) {
        # só a JANELA do jogo (PrintWindow), no segundo pedido
        if (-not $p.WaitForExit($Captura * 1000)) {
            try {
                Add-Type -AssemblyName System.Drawing
                if (-not ('GowaWinC' -as [type])) {
                    Add-Type 'using System; using System.Runtime.InteropServices; public class GowaWinC { [StructLayout(LayoutKind.Sequential)] public struct RECT { public int L, T, R, B; } [DllImport("user32.dll")] public static extern bool GetWindowRect(IntPtr h, out RECT r); [DllImport("user32.dll")] public static extern bool PrintWindow(IntPtr h, IntPtr hdc, uint f); }'
                }
                $gw = Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $exe -and $_.MainWindowHandle -ne 0 } | Select-Object -First 1
                if ($gw) {
                    $rc = New-Object GowaWinC+RECT; [GowaWinC]::GetWindowRect($gw.MainWindowHandle, [ref]$rc) | Out-Null
                    $bmp = New-Object System.Drawing.Bitmap ($rc.R - $rc.L), ($rc.B - $rc.T)
                    $g = [System.Drawing.Graphics]::FromImage($bmp); $hdc = $g.GetHdc()
                    [GowaWinC]::PrintWindow($gw.MainWindowHandle, $hdc, 2) | Out-Null; $g.ReleaseHdc($hdc)
                    $bmp.Save(($log -replace '\.log$', '.png'), [System.Drawing.Imaging.ImageFormat]::Png); $g.Dispose(); $bmp.Dispose()
                }
            } catch { }
        }
    }
    if (-not $p.WaitForExit([math]::Max(1, $Segundos - $(if ($Captura -gt 0 -and $Captura -lt $Segundos) { $Captura } else { 0 })) * 1000)) {
        Get-Process -ErrorAction SilentlyContinue | Where-Object { $_.Path -eq $exe } | Stop-Process -Force
        $p.WaitForExit(5000) | Out-Null
    }
    foreach ($k in $envs.Keys) { Remove-Item -Path "env:$k" -ErrorAction SilentlyContinue }
    # FPS como o fps_fase.ps1
    $tp = $null
    $m = Select-String -Path $log -Pattern 'go to playing' | Select-Object -First 1
    if ($m -and $m.Line -match 't=(\d+)ms') { $tp = [int]$matches[1] / 1000.0 }
    $fps = New-Object System.Collections.Generic.List[double]; $j = 0; $todos = @()
    foreach ($l in (Select-String -Path $log -Pattern '^\[fps\] ' | ForEach-Object Line)) {
        if ($l -match '^\[fps\] ([\d.]+)') {
            $j++; $v = $matches[1]
            if ($l -match 'jogo=([\d.]+)') { $v = $matches[1] }
            $todos += $v
            if ($tp -and (5 * $j) -ge ($tp + 40)) { $fps.Add([double]::Parse($v, [Globalization.CultureInfo]::InvariantCulture)) }
        }
    }
    $media = if ($fps.Count) { [math]::Round(($fps | Measure-Object -Average).Average, 1) } else { 0 }
    $mn = if ($fps.Count) { [math]::Round(($fps | Measure-Object -Minimum).Minimum, 1) } else { 0 }
    $linha = "{0:HH:mm} {1} {2} rodada {3}: fase aos {4} s, FPS medio {5} (min {6}, {7} amostras; {8:N0} s) | todos: {9}" -f (Get-Date), $Cena,
             $(if ($Limite) { 'com limitador' } else { 'sem limitador' }), $i, $tp, $media, $mn, $fps.Count, ((Get-Date) - $t0).TotalSeconds, ($todos -join ' ')
    Add-Content -Path (Join-Path $R 'out\resultados.txt') -Value $linha -Encoding UTF8
    $res += $media
}
$fim = "{0:HH:mm} FIM {1} {2}: {3}" -f (Get-Date), $Cena, $(if ($Limite) { 'com limitador' } else { 'sem limitador' }), ($res -join ' / ')
Add-Content -Path (Join-Path $R 'out\resultados.txt') -Value $fim -Encoding UTF8
Set-Content -Path (Join-Path $R 'out\pronto.txt') -Value $fim -Encoding UTF8
