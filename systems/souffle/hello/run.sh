#!/bin/bash
# Timings for reach-from-100-roots over 10M random edges / 2M nodes. Run ./build.sh first.
cd "$(dirname "$0")"
date -u
ms() { local a=$(date +%s%N); "$@" >/tmp/souffle_run.out 2>&1; local b=$(date +%s%N); echo "$(( (b-a)/1000000 )) ms  [$(grep -E '^[0-9]+$' /tmp/souffle_run.out | tail -1)]"; }
for j in 1 8; do
  echo "TSV load only,  compiled -j$j: $(ms ./load_c -F data -D- -j$j)"
  echo "TSV load+reach, compiled -j$j: $(ms ./reach_c -F data -D- -j$j)"
done
echo "interpreter load+reach -j8:    $(ms souffle -F data -D- -j8 reach.dl)"
for a in "1 1" "2 1" "2 8" "3 1" "4 1" "4 8"; do ./reach_emb $a; done   # binary path (mode threads)
