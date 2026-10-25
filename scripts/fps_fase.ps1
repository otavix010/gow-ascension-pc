# Resume uma execução: quando entrou na fase ("go to playing") e o FPS da
# jogabilidade estável. 18/10: a média a partir de +20 s incluía 2-3 amostras
# de uma transição a 46-54 FPS logo depois da entrada, em número variável por
# execução -- era a maior parte do "ruído" de +-1 FPS. Agora: só amostras a
# partir de +40 s (média e mediana).
# Uso: fps_fase.ps1 -Log out\run453.log [-Desde 40]
param([string]$Log, [int]$Desde = 40, [switch]$Apresentacoes)
$tp = $null
$p = Select-String -Path $Log -Pattern 'go to playing' | Select-Object -First 1
if ($p -and $p.Line -match 't=(\d+)ms') { $tp = [int]$matches[1] / 1000.0 }
# as linhas [fps] saem a cada 5 s; a i-ésima (a partir de 1) cobre até ~5*i s
$fps = New-Object System.Collections.Generic.List[double]
$i = 0
# 20/10: o primeiro número do [fps] conta APRESENTAÇÕES, que chegaram a ~1,9
# por quadro do jogo (cada flip apresentado duas vezes). O campo jogo= (flips
# pedidos pelo jogo por segundo) é o FPS de verdade e é o usado quando existe;
# -Apresentacoes usa o número antigo.
foreach ($l in (Select-String -Path $Log -Pattern '^\[fps\] ' | ForEach-Object Line)) {
    if ($l -match '^\[fps\] ([\d.]+)') {
        $i++
        $v = $matches[1]
        if (-not $Apresentacoes -and $l -match 'jogo=([\d.]+)') { $v = $matches[1] }
        if ($tp -and (5 * $i) -ge ($tp + $Desde)) { $fps.Add([double]::Parse($v, [Globalization.CultureInfo]::InvariantCulture)) }
    }
}
$m = if ($fps.Count) { [math]::Round(($fps | Measure-Object -Average).Average, 1) } else { '-' }
$med = '-'
if ($fps.Count) { $o = $fps | Sort-Object; $med = [math]::Round($o[[int][math]::Floor(($o.Count - 1) / 2)], 1) }
"{0}: fase aos {1} s, FPS medio na fase {2} (mediana {3}, {4} amostras)" -f (Split-Path $Log -Leaf), $tp, $m, $med, $fps.Count
