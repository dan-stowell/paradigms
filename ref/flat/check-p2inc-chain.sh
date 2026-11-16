#!/usr/bin/env bash
# One maintained state per seed; every random and real step checked in a fork.
set -euo pipefail
cd "$(dirname "$0")/../.."
mkdir -p build
export TMPDIR="$PWD/build"
gcc -O2 -march=native -o build/p2inc ref/flat/p2inc.c
D=/home/exedev/paradigms/data
for seed in 4 5 6; do
  date -u +%FT%TZ
  uptime
  CHAIN=1 CHAIN_EDIT="$D/minio-edit2" FUZZ=240 SEED="$seed" \
    prlimit --as=8589934592 build/p2inc "$D/minio" "$D/minio-edit" 0
done
