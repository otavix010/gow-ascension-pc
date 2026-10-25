# Roda o ninja do port com N jobs e registra o pico de memoria (soma dos clang-cl/lld-link) e o tempo.
# Uso: build_medido.ps1 -Jobs 4 [-Alvo gowa.exe]
param([int]$Jobs = 4, [string]$Alvo = "", [string]$Log = "$(Split-Path -Parent $PSScriptRoot)\out\build_port.log")
$t0 = Get-Date
$args_ninja = "-j $Jobs $Alvo".Trim()
$p = Start-Process -FilePath cmd.exe -ArgumentList "/c", "$(Split-Path -Parent $PSScriptRoot)\scripts\gowa_ninja.cmd $args_ninja > `"$Log`" 2>&1" -PassThru -NoNewWindow
$pico = 0; $livreMin = [double]::MaxValue
while (-not $p.HasExited) {
    $soma = (Get-Process clang-cl, lld-link, link -ErrorAction SilentlyContinue | Measure-Object -Property WorkingSet64 -Sum).Sum
    if ($soma -gt $pico) { $pico = $soma }
    $livre = (Get-CimInstance Win32_OperatingSystem).FreePhysicalMemory * 1KB
    if ($livre -lt $livreMin) { $livreMin = $livre }
    Start-Sleep -Seconds 2
}
$dt = (Get-Date) - $t0
"build -j {0} {1}: {2:N1} min, pico somado compiladores {3:N2} GB, RAM livre minima {4:N2} GB" -f $Jobs, $Alvo, $dt.TotalMinutes, ($pico/1GB), ($livreMin/1GB)
$falhou = Select-String -Path $Log -Pattern "build stopped|exit=1" -Quiet
if ($falhou) { "*** BUILD FALHOU ***"; Select-String -Path $Log -Pattern " error" | Select-Object -First 10 | ForEach-Object Line } else { Get-Content $Log -Tail 2 }
