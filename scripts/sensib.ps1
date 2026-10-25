# Teste de caminho crítico: roda a mesma cena com atrasos artificiais em cada
# candidato a gargalo e compara com a base (intercalado: base, EDGE, RSX, base...).
# O que derruba o FPS quando atrasado está no caminho crítico; o que não muda tem folga.
# Uso: sensib.ps1 [-Cena cap16|inicio] [-Rodadas 2] [-Prefixo sens]
param([string]$Cena = 'cap16', [int]$Rodadas = 2, [string]$Prefixo = 'sens')
$casos = [ordered]@{
    base = @{}
    edge = @{ GOWA_SLOW_JOBMOD = '72DF80:10' }
    rsx  = @{ GOWA_RSX_SLOW_NS = '500' }
    ppu  = @{ GOWA_SLOW_PPU_PCT = '15' }
}
$res = @{}
foreach ($k in $casos.Keys) { $res[$k] = @() }
for ($i = 1; $i -le $Rodadas; $i++) {
    foreach ($k in $casos.Keys) {
        $o = & "$(Split-Path -Parent $PSScriptRoot)\scripts\ab_fps.ps1" -A $casos[$k] -B $casos[$k] -Pares 1 -Prefixo "${Prefixo}_${k}$i" -Cena $Cena
        $linha = ($o | Where-Object { $_ -like 'A1 :*' })
        $v = if ($linha -match 'fase ([\d,\.]+) \(') { [double]($matches[1] -replace ',', '.') } else { 0 }
        $linhaB = ($o | Where-Object { $_ -like 'B1 :*' })
        $vB = if ($linhaB -match 'fase ([\d,\.]+) \(') { [double]($matches[1] -replace ',', '.') } else { 0 }
        $res[$k] += $v; $res[$k] += $vB
        "{0:HH:mm} {1} rodada {2}: {3} / {4}" -f (Get-Date), $k, $i, $v, $vB
    }
}
foreach ($k in $casos.Keys) {
    $m = ($res[$k] | Measure-Object -Average -Minimum -Maximum)
    "{0}: média {1:N2} (mín {2:N1}, máx {3:N1})" -f $k, $m.Average, $m.Minimum, $m.Maximum
}
