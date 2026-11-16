#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
d=${1:-/home/exedev/paradigms/data/minio-p4}
mkdir -p build
export OMP_WAIT_POLICY=passive FLAT_HUGE=0 P4_REPS=1
unset P4_NOPATCH P4_COUNTS P4_FAULT
for src in p4holes p4runs p4par; do
    prlimit --as=12884901888 gcc -O2 -march=native -fopenmp -o "build/$src" "ref/p4/$src.c"
done
for t in 1 8; do
    for variant in H0 H1 H2; do
        OMP_NUM_THREADS=$t prlimit --as=12884901888 build/p4holes "$d" "$variant"
    done
done
for variant in H0 H1 H2; do
    OMP_NUM_THREADS=8 OMP_THREAD_LIMIT=2 P4_HUGE=0 prlimit --as=12884901888 build/p4holes "$d" "$variant"
done
for fault in byte offset relocation; do
    log="build/holes-fault-$fault.log"
    if OMP_NUM_THREADS=1 P4_FAULT=$fault build/p4holes "$d" H2 > "$log"; then
        echo "ERROR: planted $fault fault passed"; exit 1
    fi
    grep -q 'P4 FAIL' "$log"
    echo "Caught planted $fault fault"; cat "$log"
done
