# A/B intercalado de FPS na fase: alterna as configurações A e B (ABAB...) com o
# mesmo executável, para que a deriva da máquina (temperatura, clock, carga)
# pese igual nos dois lados. Cada rodada é uma execução completa até a fase.
# Uso: ab_fps.ps1 -A @{} -B @{ GOWA_X='0' } -Pares 2 -Prefixo ab1 [-Segundos 260]
# -ExeA/-ExeB: um executável por lado (mudanças de compilação; copiar o gowa.exe
# para outro nome na mesma pasta, para achar as mesmas DLLs).
param([hashtable]$A = @{}, [hashtable]$B = @{}, [int]$Pares = 2, [string]$Prefixo = 'ab', [int]$Segundos = 260,
      [string]$ExeA = "$(Split-Path -Parent $PSScriptRoot)\port\build\gowa.exe",
      [string]$ExeB = "$(Split-Path -Parent $PSScriptRoot)\port\build\gowa.exe",
      [string]$Cena = 'inicio')
# -Cena inicio: a fase 1 (abertura), com o perfil das medições (padrão).
# -Cena cap16: o save de teste: Capítulo 16, "A prisão
# dos condenados", uma das partes mais pesadas do jogo -- Perfil 4 em
# out\saves_teste\cap16, "Continuar jogo"; fase aos ~129 s.
$P = '50:0x4000,85:0x4000,105:0x0008,125:0x4000,145:0x4000,165:0x4000,185:0x4000'
$extra = @{}
if ($Cena -eq 'cap16') {
    $P = '50:0x4000,85:0x4000,95:0x0040,98:0x0040,101:0x0040,106:0x4000,115:0x4000'
    $extra = @{ PS3_SAVE_ROOT = "$(Split-Path -Parent $PSScriptRoot)\out\saves_teste\cap16" }
    if (-not $PSBoundParameters.ContainsKey('Segundos')) { $Segundos = 230 }
}
$res = @{ A = @(); B = @() }
for ($i = 1; $i -le $Pares; $i++) {
    foreach ($lado in 'A', 'B') {
        $cfg = if ($lado -eq 'A') { $A } else { $B }
        $exe = if ($lado -eq 'A') { $ExeA } else { $ExeB }
        # WATCHDOG_AT: despejo das threads aos 172/180 s (fase a ~150 s), antes da
        # janela de medição: se o carregamento travar (run523, run527), mostra onde
        $env2 = @{ PAD_SCRIPT = $P; WATCHDOG_AT = '172,180' } + $extra + $cfg
        $log = "$(Split-Path -Parent $PSScriptRoot)\out\${Prefixo}_${lado}$i.log"
        & "$(Split-Path -Parent $PSScriptRoot)\scripts\rodar_gowa.ps1" -Segundos $Segundos -Log $log -Env $env2 -Exe $exe | Out-Null
        $linha = & "$(Split-Path -Parent $PSScriptRoot)\scripts\fps_fase.ps1" -Log $log
        $v = if ($linha -match 'fase ([\d,\.]+) \(') { [double]($matches[1] -replace ',', '.') } else { 0 }
        $res[$lado] += $v
        "$lado$i : $linha"
    }
}
foreach ($lado in 'A', 'B') {
    $m = ($res[$lado] | Measure-Object -Average -Minimum -Maximum)
    "{0}: média {1:N2} (mín {2:N1}, máx {3:N1})" -f $lado, $m.Average, $m.Minimum, $m.Maximum
}
