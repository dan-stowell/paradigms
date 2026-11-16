#!/usr/bin/env bash
# Build the plain and OpenMP C references for problem 4 and check it against the oracle:  ref/p4/check.sh [data/minio-p4]
set -euo pipefail
here=$(cd "$(dirname "$0")/../.." && pwd); d=${1:-/home/exedev/paradigms/data/minio-p4}
mkdir -p "$here/build"
gcc -O2 -march=native -o "$here/build/p4" "$here/ref/p4/p4.c"
prlimit --as=12884901888 "$here/build/p4" "$d"
gcc -O2 -march=native -fopenmp -o "$here/build/p4par" "$here/ref/p4/p4par.c"
for threads in 1 8; do
    OMP_NUM_THREADS=$threads OMP_WAIT_POLICY=passive P4_NOPATCH=0 prlimit --as=12884901888 "$here/build/p4par" "$d"
done
