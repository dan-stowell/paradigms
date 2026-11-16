#!/usr/bin/env bash
set -uo pipefail
export OMP_WAIT_POLICY=passive
D=/home/exedev/paradigms/data/kube-apiserver
run() { echo "+ $*"; systemd-run --user --scope -q -p MemoryMax=10G "$@"; echo "exit=$?"; }
uptime
for p in p1 p2 p2lazy p3; do run systems/goose/bin/$p "$D"; done
for p in p1par p2par p3par; do
 for t in 1 8; do run systems/goose/bin/$p "$D" "$t"; done
done
run systems/goose/bin/p2par "$D" 8 lazy
run systems/goose/bin/p3 "$D" --pipeline
uptime
