# Refaz na bancada (gow-edge) a etapa 5 do gerar.ps1 -- tradução de todos os módulos SPU e os passes de
# desempenho, como no PC do jogador -- e depois reaplica os ganchos do EDGE nativo (edge_nativo.py aplicar).
# Backup do port\spu anterior em out\bak_spu_<HHmm>. Log em out\etapa5_bancada.log.
$ErrorActionPreference = 'Stop'
$R = Split-Path -Parent $PSScriptRoot
$s = Join-Path $R 'scripts'
$LOG = Join-Path $R 'out\etapa5_bancada.log'
$py = 'C:\Users\otavi\AppData\Local\Programs\Python\Python312\python.exe'
if (-not (Test-Path $py)) { $py = (Get-Command python).Source }
function Rodar([string]$desc, [string[]]$arg) {
    $t = Get-Date
    "== $desc" | Add-Content -Encoding utf8 $LOG
    # pelo cmd: o stderr das ferramentas (avisos, contagens) não vira erro do PowerShell
    $linha = ($arg | ForEach-Object { "`"$_`"" }) -join ' '
    cmd /c "`"$py`" -X utf8 $linha >> `"$LOG`" 2>&1"
    if ($LASTEXITCODE) { throw "falhou: $desc (código $LASTEXITCODE, veja $LOG)" }
    "   {0:N0} s" -f ((Get-Date) - $t).TotalSeconds | Add-Content -Encoding utf8 $LOG
}
"etapa 5 da bancada $(Get-Date -Format 'dd/MM HH:mm:ss') ($py)" | Out-File -Encoding utf8 $LOG
$bak = Join-Path $R ("out\bak_spu_" + (Get-Date -Format 'HHmm'))
cmd /c "robocopy `"$R\port\spu`" `"$bak`" /E /NFL /NDL /NJH /NJS /NP" | Out-Null
if ($LASTEXITCODE -ge 8) { throw "falhou o backup ($LASTEXITCODE)" }
# recomeça do zero, como o gerar.ps1: os passes partem dos backups .sem_* se eles existirem (29/10: sem isto a
# tradução nova se perdeu e os passes refizeram a antiga)
Get-ChildItem (Join-Path $R 'port\spu') -Force -ErrorAction SilentlyContinue |
    Where-Object { $_.Name -ne 'gowanat_tspm' } | Remove-Item -Recurse -Force
$sw = Join-Path $R 'out\spu_workloads'
New-Item -ItemType Directory -Force $sw | Out-Null
Rodar 'build_spu_workloads' @("$R\ps3recomp\tools\build_spu_workloads.py", '--input', "$R\jogo\GOWA.elf", '--input', "$R\fw\libsre.prx",
                              '--lifted', $sw, '--out', "$sw\spu_workloads.c", '--constructor', '--allow-partial')
cmd /c "robocopy `"$sw`" `"$R\port\spu`" /E /NFL /NDL /NJH /NJS /NP" | Out-Null
if ($LASTEXITCODE -ge 8) { throw "falhou a cópia de $sw ($LASTEXITCODE)" }
Rodar 'lift_gowa_jobmods' @("$s\lift_gowa_jobmods.py")
Rodar 'lift_gowa_pm' @("$s\lift_gowa_pm.py")
Rodar 'lift_gowa_tspm' @("$s\lift_gowa_tspm.py")
Rodar 'lift_gowa_syssrv' @("$s\lift_gowa_syssrv.py")
Rodar 'lift_gowa_plugins edge' @("$s\lift_gowa_plugins.py", "$R\dados\plugins_getcode_edge_cap16.txt")
Rodar 'lift_gowa_plugins w11' @("$s\lift_gowa_plugins.py", "$R\dados\plugins_getcode_w11.txt", '--wids', '11', '--naofolha',
                                '--inc', "$R\port\gowa_plugins_w11.inc", '--tabela', 'g_gowa_plugin_fns_w11')
$hot = Join-Path $R 'port\gowa_jobmods_hot.inc'
$utf8 = New-Object System.Text.UTF8Encoding($false)
[IO.File]::WriteAllText($hot, ("/* GERADO por gerar.ps1 + scripts/folhas_spu.py: funções folha seguras dos módulos de job`n" +
    " * que ficam interpretados (GOWA_JOBLIFT_HOT). va, LS, bytes da função. */`n" +
    "static const struct { uint32_t va, ls, len; } g_gowa_hot[] = {`n"), $utf8)
Rodar 'folhas_spu' @("$s\folhas_spu.py", "$R\port\spu\gowajm_006FC080\spu_recomp.c", 'gowajm_006FC080_', '--inc', '6FC080', $hot)
[IO.File]::AppendAllText($hot, "};`n", $utf8)
Rodar 'spu_num_remap' @("$s\spu_num_remap.py", 'aplicar')
Rodar 'guarda_avx512' @("$s\guarda_avx512.py", 'aplicar')
Rodar 'otimizar' @("$s\otimizar.py")
Rodar 'edge_nativo aplicar' @("$R\scripts_edge\edge_nativo.py", 'aplicar')
"fim $(Get-Date -Format 'HH:mm:ss')" | Add-Content -Encoding utf8 $LOG
