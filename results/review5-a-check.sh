#!/bin/bash
set -euo pipefail
cd /home/exedev/paradigms-c
D=/home/exedev/paradigms/data/minio
B=systems/halide/cells/build/p3
for t in 1 8; do
  date -u; uptime
  systemd-run --user --scope -p MemoryMax=12G env HL_NUM_THREADS=$t "$B" "$D" 3a
  date -u; uptime
  systemd-run --user --scope -p MemoryMax=12G env HL_NUM_THREADS=$t "$B" "$D" e2e 7 3
  set +e
  systemd-run --user --scope -p MemoryMax=12G env HL_NUM_THREADS=$t P3_SKIP_UNIT=0 "$B" "$D" e2e 7 3
  rc=$?
  set -e
  echo "omitted unit exit=$rc (expected 1)"
  test "$rc" = 1
done
date -u; uptime
set +e
systemd-run --user --scope -p MemoryMax=12G env HL_NUM_THREADS=8 P3_SKIP_UNIT=0 "$B" "$D" layout_c3_w1
rc=$?
set -e
echo "omitted unit warm exit=$rc (expected 1)"
test "$rc" = 1
