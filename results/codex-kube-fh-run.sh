#!/usr/bin/env bash
set -uo pipefail
cd /home/exedev/paradigms-w
export OMP_WAIT_POLICY=passive
D=/home/exedev/paradigms/data/kube-apiserver
run() { echo "COMMAND: $*"; uptime; systemd-run --user --scope -q -p MemoryMax=10G "$@"; echo "EXIT: $?"; uptime; }
for cfg in c:1 multicore:8 mcy:8; do
  be=${cfg%:*}; t=${cfg#*:}
  for spec in 'p1 1a' 'p1 1b' 'p1 1b_scan' 'p1 1c' 'p2 2' 'p2 2lazy' 'p3 3a' 'p3 3a_loop' 'p3 3b'; do
    read -r p cell <<<"$spec"
    run systems/futhark/build/${p}_${be}.exe "$D" "$t" 1 "$cell"
  done
done > results/codex-kube-futhark-check.log 2>&1
for t in 1 8; do
  for v in 0 1; do run env HL_NUM_THREADS=$t systems/halide/cells/build/p1 "$D" e2e "$v"; done
  for v in 0 1 2 3; do run env HL_NUM_THREADS=$t systems/halide/cells/build/p2 "$D" e2e "$v"; done
  run env HL_NUM_THREADS=$t systems/halide/cells/build/p3 "$D" e2e 0 2
done > results/codex-kube-halide-check.log 2>&1
run systems/halide/bench.sh "$D" > results/codex-kube-halide-bench.log 2>&1
bash results/codex-kube-c-round.sh between-halide-futhark
for r in 1 2 3 4 5; do
  echo "ROUND $r"
  run env NOBUILD=1 HUGE=1 systems/futhark/bench.sh "$D"
done > results/codex-kube-futhark-bench.log 2>&1
