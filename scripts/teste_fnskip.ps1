# Um passo da bissecção de função do módulo de job 0x6FC080 (ver gancho_fnskip.py).
# Uso: teste_fnskip.ps1 -Faixa "4000-18200" [-Seg 75] [-Tag x]
# Imprime BOM se o jogo chega ao Shell ("ready to accept XMB invites") no tempo, RUIM se não.
param([string]$Faixa = "", [int]$Seg = 75, [string]$Tag = "fns", [string]$Skip = "7B6680", [hashtable]$Extra = @{})
$log = "$(Split-Path -Parent $PSScriptRoot)\out\$Tag.log"
$envs = @{ GOWA_PROBE_MAX = '100000'; GOWA_JOBLIFT_SKIP = $Skip }
if ($Faixa) { $envs['GOWA_FNSKIP'] = $Faixa }
foreach ($k in $Extra.Keys) { $envs[$k] = $Extra[$k] }
& "$(Split-Path -Parent $PSScriptRoot)\scripts\rodar_gowa.ps1" -Segundos $Seg -Log $log -Env $envs | Out-Null
$ok = Select-String -Path $log -Pattern 'ready to accept XMB invites' -Quiet
$t = (Select-String -Path $log -Pattern 'ready to accept XMB invites' | Select -First 1).Line
if ($ok) { "BOM  faixa=$Faixa  $t" } else { "RUIM faixa=$Faixa" }
