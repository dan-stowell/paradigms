#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
D=${1:-/home/exedev/paradigms/data/minio-p4}
export OMP_WAIT_POLICY=passive P4_REPS=${P4_REPS:-32}
unset P4_CP4_GENERIC P4_CP4_COVERED P4_NOPATCH
run() {
    local label=$1 threads=$2 exe=$3
    shift 3
    echo "RUN $round $label"
    date -u +%FT%TZ
    uptime
    if [[ $threads == 1 ]]; then
        env OMP_NUM_THREADS=1 "$@" prlimit --as=12884901888 taskset -c 5 "$exe" "$D"
    else
        env OMP_NUM_THREADS="$threads" "$@" prlimit --as=12884901888 "$exe" "$D"
    fi
}
for round in 1 2 3 4 5; do
    run p4 1 build/p4
    run cp1 1 build/p4cp1
    run par1 1 build/p4par
    run cp4_1 1 build/p4cp4 P4_CP4_COVERED=1
    run generic1 1 build/p4cp4 P4_CP4_COVERED=1 P4_CP4_GENERIC=1
    run par8 8 build/p4par
    run cp4_8 8 build/p4cp4
 done
