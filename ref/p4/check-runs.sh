#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
d=${1:-/home/exedev/paradigms/data/minio-p4}
mkdir -p build
export OMP_WAIT_POLICY=passive FLAT_HUGE=0 P4_REPS=1
unset P4_NOPATCH
for src in p4 p4par p4runs; do
    flags=(); [[ $src == p4 ]] || flags=(-fopenmp)
    prlimit --as=12884901888 gcc -O2 -march=native "${flags[@]}" -o "build/$src" "ref/p4/$src.c"
done
prlimit --as=12884901888 build/p4 "$d"
for t in 1 8; do
    export OMP_NUM_THREADS=$t
    prlimit --as=12884901888 build/p4par "$d"
    for variant in D A B C; do
        prlimit --as=12884901888 build/p4runs "$d" "$variant"
    done
done
for variant in A B C; do
    OMP_NUM_THREADS=8 OMP_THREAD_LIMIT=2 P4_HUGE=0 prlimit --as=12884901888 build/p4runs "$d" "$variant"
done
