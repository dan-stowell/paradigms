#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
d=${1:-/home/exedev/paradigms/data/minio-p4}
export OMP_WAIT_POLICY=passive FLAT_HUGE=0 P4_HUGE=1 P4_REPS=${P4_REPS:-128}
unset P4_NOPATCH OMP_THREAD_LIMIT
for round in 1 2 3 4 5; do
    echo "ROUND $round"
    date -u +%FT%TZ; uptime
    prlimit --as=12884901888 build/p4 "$d"
    for t in 1 8; do
        export OMP_NUM_THREADS=$t
        date -u +%FT%TZ; uptime
        prlimit --as=12884901888 build/p4par "$d"
        for variant in D A B C; do
            date -u +%FT%TZ; uptime
            prlimit --as=12884901888 build/p4runs "$d" "$variant"
        done
    done
done
date -u +%FT%TZ; uptime
