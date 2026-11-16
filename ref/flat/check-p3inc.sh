#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
D=${DATA_ROOT:-/home/exedev/paradigms/data}
mkdir -p build-relayout/check-edit1 build-relayout/check-edit2
for name in p3inc p3inc_check p3compare p3bench; do
  gcc -O3 -o "build-relayout/$name" "ref/flat/$name.c"
done
gcc -O3 -fopenmp -o build-relayout/p3parbench ref/flat/p3bench.c
gcc -O3 -o build-relayout/p3 ref/flat/p3.c
gcc -O3 -fopenmp -o build-relayout/p3par ref/flat/p3.c
for e in 1 2; do
  suffix=edit
  if [[ $e == 2 ]]; then suffix=edit2; fi
  B="$D/minio-$suffix"
  prlimit --as=8589934592 build-relayout/p3compare "$D/minio" "$B"
  P3INC_EXPORT="build-relayout/check-edit$e" prlimit --as=8589934592 build-relayout/p3inc "$D/minio" "$B" 0.01 5
  prlimit --as=8589934592 build-relayout/p3inc_check "$D/minio" "$B" "build-relayout/check-edit$e"
  OMP_NUM_THREADS=1 prlimit --as=8589934592 build-relayout/p3 "$B" --pipeline
  OMP_NUM_THREADS=1 prlimit --as=8589934592 build-relayout/p3par "$B" --pipeline
done
prlimit --as=8589934592 build-relayout/p3 "$D/minio" --pipeline
OMP_NUM_THREADS=1 prlimit --as=8589934592 build-relayout/p3par "$D/minio" --pipeline
python3 ref/flat/check-p3inc-negative.py build-relayout/p3inc_check "$D/minio" "$D/minio-edit" build-relayout/check-edit1
