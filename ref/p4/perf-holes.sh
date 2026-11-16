#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
d=${1:-/home/exedev/paradigms/data/minio-p4}
perf=${PERF:-build/holes-perf/usr/lib/linux-tools-6.8.0-149/perf}
export OMP_NUM_THREADS=1 OMP_WAIT_POLICY=passive OMP_PROC_BIND=true OMP_PLACES=cores
export FLAT_HUGE=0 P4_HUGE=1 P4_REPS=${P4_REPS:-256}
unset P4_NOPATCH P4_COUNTS P4_FAULT OMP_THREAD_LIMIT
for v in B H0 H1 H2; do
    echo "PERF $v"; date -u +%FT%TZ; uptime
    args=(build/p4holes "$d" "$v")
    [[ $v != B ]] || args=(build/p4runs "$d" B)
    taskset -c 6 "$perf" stat -e cycles:u,instructions:u,branches:u,branch-misses:u,cache-misses:u \
        -- prlimit --as=12884901888 "${args[@]}"
done
