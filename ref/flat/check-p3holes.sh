#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
mkdir -p build-relayout2/tmp
export TMPDIR="$PWD/build-relayout2/tmp"
D=${DATA_ROOT:-/home/exedev/paradigms/data}
mkdir -p build-relayout2/check-edit1 build-relayout2/check-edit2
for step in 0 1 2 3 4; do mkdir -p "build-relayout2/chain/step$step"; done
for name in p3holes p3inc_check p3compare p3bench; do
  gcc -O3 -o "build-relayout2/$name" "ref/flat/$name.c"
done
gcc -O1 -g -fsanitize=undefined -o build-relayout2/check-allocator ref/flat/check-p3holes-allocator.c
prlimit --as=8589934592 build-relayout2/check-allocator
gcc -O3 -fopenmp -o build-relayout2/p3parbench ref/flat/p3bench.c
gcc -O3 -o build-relayout2/p3 ref/flat/p3.c
gcc -O3 -fopenmp -o build-relayout2/p3par ref/flat/p3.c
for e in 1 2; do
  suffix=edit
  if [[ $e == 2 ]]; then suffix=edit2; fi
  B="$D/minio-$suffix"
  prlimit --as=8589934592 build-relayout2/p3compare "$D/minio" "$B"
  P3INC_EXPORT="build-relayout2/check-edit$e" prlimit --as=8589934592 build-relayout2/p3holes "$D/minio" "$B" 0.01
  prlimit --as=8589934592 build-relayout2/p3inc_check "$D/minio" "$B" "build-relayout2/check-edit$e"
  OMP_NUM_THREADS=1 prlimit --as=8589934592 build-relayout2/p3 "$B" --pipeline
  OMP_NUM_THREADS=1 prlimit --as=8589934592 build-relayout2/p3par "$B" --pipeline
  prlimit --as=8589934592 python3 ref/flat/check-p3holes-table.py "$D/minio" "$B" "build-relayout2/check-edit$e" --negative
  python3 ref/flat/check-p3inc-negative.py build-relayout2/p3inc_check "$D/minio" "$B" "build-relayout2/check-edit$e"
done
prlimit --as=8589934592 build-relayout2/p3 "$D/minio" --pipeline
OMP_NUM_THREADS=1 prlimit --as=8589934592 build-relayout2/p3par "$D/minio" --pipeline
P3INC_EXPORT=build-relayout2/chain prlimit --as=8589934592 build-relayout2/p3holes "$D/minio" "$D/minio-edit2" 0.01 "$D/minio-edit"
# Each chain directory exports the active state; fill the other checker slot
# from the same initial-plan pair, without replacing the chain state under test.
for step in 1 2 3 4; do
  tag=a
  if (( step % 2 == 0 )); then tag=b; fi
  for ext in addr img table; do cp "build-relayout2/chain/$tag.$ext" "build-relayout2/chain/step$step/$tag.$ext"; done
  B="$D/minio-edit2"
  if [[ $step == 3 ]]; then B="$D/minio-edit"; fi
  prlimit --as=8589934592 build-relayout2/p3inc_check "$D/minio" "$B" "build-relayout2/chain/step$step"
  prlimit --as=8589934592 python3 ref/flat/check-p3holes-table.py "$D/minio" "$B" "build-relayout2/chain/step$step"
done
