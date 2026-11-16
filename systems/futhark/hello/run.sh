#!/bin/bash
# usage: run.sh [N=50000000]   (builds if needed; data goes in build/, gitignored)
set -e
cd "$(dirname "$0")"
N=${1:-50000000}
[ -x build/gen ] || ./build.sh
F=build/rand_$N.u32
[ -f $F ] || build/gen $N 1 $F
date -u
echo "== c backend (sequential)"
build/driver_c $F
for t in 1 8; do
  echo "== multicore, $t thread(s)"
  build/driver_multicore $F $t
done
