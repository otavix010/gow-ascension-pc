# Compila um arquivo lifted isolado com clang-cl e mede tempo e pico de memória.
# Uso: medir_compilacao.ps1 -Arquivo ppu_recomp_032.cpp -Opt /O1
param(
    [string]$Arquivo = "ppu_recomp_032.cpp",
    [string]$Opt = "/O1",
    [string]$Saida = "$(Split-Path -Parent $PSScriptRoot)\out\teste_obj"
)
$llvm = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Tools\Llvm\x64\bin"
$vc = "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat"
New-Item -ItemType Directory -Force $Saida | Out-Null
$src = "$(Split-Path -Parent $PSScriptRoot)\port\recompiled\$Arquivo"
$obj = Join-Path $Saida ($Arquivo + ($Opt -replace '[/ ]','_') + ".obj")
$inc = "-I$(Split-Path -Parent $PSScriptRoot)\port\recompiled -I$(Split-Path -Parent $PSScriptRoot)\ps3recomp\include -I$(Split-Path -Parent $PSScriptRoot)\ps3recomp\runtime\ppu -I$(Split-Path -Parent $PSScriptRoot)\ps3recomp\runtime\platform"
$cmdline = "call `"$vc`" >nul && `"$llvm\clang-cl.exe`" /nologo -TP $inc /DWIN32 /D_WINDOWS /EHsc $Opt /DNDEBUG -std:c++20 -MD -fno-strict-aliasing -fwrapv /w /bigobj /Fo`"$obj`" -c -- `"$src`""
$t0 = Get-Date
$p = Start-Process -FilePath cmd.exe -ArgumentList "/c", $cmdline -PassThru -NoNewWindow -RedirectStandardError "$obj.err"
$pico = 0
while (-not $p.HasExited) {
    $ws = (Get-Process clang-cl -ErrorAction SilentlyContinue | Where-Object { $_.StartTime -ge $t0 } | Measure-Object -Property PeakWorkingSet64 -Maximum).Maximum
    if ($ws -gt $pico) { $pico = $ws }
    Start-Sleep -Milliseconds 500
}
$dt = (Get-Date) - $t0
"{0} {1}: {2:N1} min, pico {3:N2} GB, exit {4}, obj {5:N1} MB" -f $Arquivo, $Opt, $dt.TotalMinutes, ($pico/1GB), $p.ExitCode, ((Get-Item $obj -ErrorAction SilentlyContinue).Length/1MB)
