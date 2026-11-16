#!/usr/bin/env bash
# Five interleaved warm runs. Build/check first with ref/p4/check.sh.
set -euo pipefail
cd "$(dirname "$0")/../.."
d=${1:-/home/exedev/paradigms/data/minio-p4}
export OMP_WAIT_POLICY=passive P4_REPS=${P4_REPS:-64}
export P4_NOPATCH=0 P4_HUGE=1 FLAT_HUGE=0
unset OMP_THREAD_LIMIT OMP_DYNAMIC
for run in 1 2 3 4 5; do
    for variant in plain par1 par8; do
        date -u +%FT%TZ
        uptime
        echo "run=$run variant=$variant reps=$P4_REPS"
        case $variant in
            plain) OMP_NUM_THREADS=1 prlimit --as=12884901888 build/p4 "$d" ;;
            par1) OMP_NUM_THREADS=1 prlimit --as=12884901888 build/p4par "$d" ;;
            par8) OMP_NUM_THREADS=8 prlimit --as=12884901888 build/p4par "$d" ;;
        esac
    done
done
# One diagnostic run per thread count, adjacent to a patched control.
for threads in 1 8; do
    for patch in 0 1; do
        date -u +%FT%TZ
        uptime
        echo "split threads=$threads P4_NOPATCH=$patch"
        OMP_NUM_THREADS=$threads P4_NOPATCH=$patch prlimit --as=12884901888 build/p4par "$d"
    done
done
