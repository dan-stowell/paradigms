#!/usr/bin/env bash
# Build-only generated artifacts; input data is never modified.
set -euo pipefail
cd "$(dirname "$0")/../.."
D=${1:-/home/exedev/paradigms/data/minio-p4}
mkdir -p build
# Install locally once: python3 -m pip install --target build/cpdeps --no-cache-dir capstone==5.0.6
export PYTHONPATH="$PWD/build/cpdeps${PYTHONPATH:+:$PYTHONPATH}"
prlimit --as=12884901888 python3 ref/p4/gen_cp1.py "$D" build/cp1
prlimit --as=12884901888 python3 ref/p4/gen_cp4.py "$D"
prlimit --as=12884901888 gcc -O2 -march=native -o build/p4 ref/p4/p4.c
prlimit --as=12884901888 gcc -O2 -march=native -fopenmp -o build/p4par ref/p4/p4par.c
prlimit --as=12884901888 gcc -O2 -march=native -o build/p4cp1 ref/p4/p4cp1.c
prlimit --as=12884901888 gcc -O2 -march=native -fopenmp -o build/p4cp4 ref/p4/p4cp4.c
prlimit --as=12884901888 python3 results/cp-review-check.py
P4_REPS=1 prlimit --as=12884901888 build/p4cp1 "$D"
for n in 1 8; do
    OMP_NUM_THREADS=$n OMP_WAIT_POLICY=passive P4_REPS=1 \
        prlimit --as=12884901888 build/p4cp4 "$D"
done
