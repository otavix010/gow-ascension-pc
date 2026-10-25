# Comparação intercalada de N configurações (executável + ambiente), como o
# ab_fps.ps1, mas com mais de dois lados: roda C1, C2, ..., Cn, C1, C2, ... para
# que a deriva da máquina pese igual em todos.
# Uso: ab_n.ps1 -Casos @( @{Nome='base'; Exe='...gowa_xl.exe'; Env=@{}}, @{Nome='x'; Exe='...'; Env=@{GOWA_X='0'}} )
#               -Rodadas 3 -Prefixo abn -Cena cap16|inicio
param([object[]]$Casos, [int]$Rodadas = 3, [string]$Prefixo = 'abn', [string]$Cena = 'cap16')
$res = @{}
foreach ($c in $Casos) { $res[$c.Nome] = @() }
for ($i = 1; $i -le $Rodadas; $i++) {
    foreach ($c in $Casos) {
        $o = & "$(Split-Path -Parent $PSScriptRoot)\scripts\ab_fps.ps1" -ExeA $c.Exe -ExeB $c.Exe -A $c.Env -B $c.Env -Pares 1 -Prefixo "${Prefixo}_$($c.Nome)$i" -Cena $Cena 2>&1
        # ab_fps com -Pares 1 roda A e B (duas execuções do mesmo caso)
        foreach ($lado in 'A1', 'B1') {
            $linha = ($o | Where-Object { "$_" -like "$lado :*" })
            $v = if ("$linha" -match 'fase ([\d,\.]+) \(') { [double]($matches[1] -replace ',', '.') } else { 0 }
            $res[$c.Nome] += $v
        }
        "{0:HH:mm} {1} rodada {2}: {3}" -f (Get-Date), $c.Nome, $i, (($res[$c.Nome] | Select-Object -Last 2) -join ' / ')
    }
}
foreach ($c in $Casos) {
    $vals = $res[$c.Nome] | Where-Object { $_ -gt 0 }
    $m = ($vals | Measure-Object -Average -Minimum -Maximum)
    "{0}: média {1:N2} (mín {2:N1}, máx {3:N1}; {4} válidas de {5})" -f $c.Nome, $m.Average, $m.Minimum, $m.Maximum, $m.Count, $res[$c.Nome].Count
}
