#!/usr/bin/env bash
# Large-data checks, optional count experiment, chained state, and tiny cycle cases.
set -euo pipefail
cd "$(dirname "$0")/../.."
mkdir -p build
export TMPDIR="$PWD/build"
unset LIFO COUNTS FRESH_B CHAIN CHAIN_EDIT EVICT FUZZ
D=/home/exedev/paradigms/data
date -u +%FT%TZ
uptime
gcc -O2 -march=native -o build/p2inc ref/flat/p2inc.c
for edit in minio-edit minio-edit2; do
  for mode in normal counts; do
    if test "$mode" = counts; then export COUNTS=1; else unset COUNTS; fi
    echo "FUZZ edit=$edit mode=$mode"
    FUZZ=300 prlimit --as=12884901888 build/p2inc "$D/minio" "$D/$edit" 1
  done
done
unset COUNTS
bash ref/flat/check-p2inc-chain.sh
gcc -O2 -o build/test-p2inc-storage ref/flat/test-p2inc-storage.c
prlimit --as=12884901888 build/test-p2inc-storage hash
prlimit --as=12884901888 build/test-p2inc-storage pool
gcc -O2 -o build/test-p2inc-witness ref/flat/test-p2inc-witness.c
for traversal in bfs lifo; do
  for count in no yes; do
    if test "$traversal" = lifo; then export LIFO=1; else unset LIFO; fi
    if test "$count" = yes; then export COUNTS=1; else unset COUNTS; fi
    echo "fixtures traversal=$traversal counts=$count"
    for case in cycle overlay method flags iface condition-overlay; do
      prlimit --as=12884901888 build/test-p2inc-witness "$case"
    done
  done
done
date -u +%FT%TZ
uptime
