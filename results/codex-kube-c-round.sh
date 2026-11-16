#!/usr/bin/env bash
# One comparison round; call between paradigm runs. All output stays in this checkout.
set -uo pipefail
cd /home/exedev/paradigms-w
D=/home/exedev/paradigms/data/kube-apiserver
export OMP_WAIT_POLICY=passive
label=${1:?round label required}
exec >>results/codex-kube-c-raw.log 2>&1
echo "ROUND $label $(date -u +%FT%TZ)"
uptime
run() {
  printf 'COMMAND'; printf ' %q' "$@"; printf '\n'
  systemd-run --user --scope -q -p MemoryMax=10G "$@"
  echo "STATUS $?"
}
for t in 1 8; do
  export OMP_NUM_THREADS=$t
  echo "THREADS $t"
  run build/p1par "$D" 10
  run build/p2par "$D" 30
  run build/p2lazy "$D" 20
  run build/p3par "$D" --layout
  P3_REPS=30 run build/p3par "$D"
  P3_REPS=30 P3_THP=1 run build/p3par "$D"
done
uptime
echo "ENDROUND $label $(date -u +%FT%TZ)"
