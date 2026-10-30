#!/bin/sh
# A/B de dois exes na reprodução das gravações: ab_exes.sh <pasta de gravações> <rodadas> <exe1> <exe2>  (exes em port/build_native)
cd /c/Users/otavi/gow-edge
pasta=$1; n=$2; shift 2
B='C:/Users/otavi/gow-edge'
for r in $(seq 1 $n); do
  for e in "$@"; do
    exe=$(cygpath -w "$B/port/build_native/$e"); dir=$(cygpath -w "$B/out/$pasta")
    GOWA_EDGE_NATIVO=1 GOWA_CONF_REGS_ABI=1 GOWA_CONF_PERF=1 cmd //c 'scripts_edgenfere_dir.cmd' "$exe" "$dir" 5 >/dev/null
    echo "$r $e: $(tail -1 out/confere_run.log | sed 's/.*: //')"
  done
done
