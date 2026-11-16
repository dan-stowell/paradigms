#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
D=${1:-/home/exedev/paradigms/data/minio-p4}
PERF_BIN=${PERF_BIN:-build/perf-tools/usr/lib/linux-tools-6.8.0-149/perf}
export P4_REPS=32 OMP_WAIT_POLICY=passive
unset P4_CP4_GENERIC P4_CP4_COVERED P4_NOPATCH
for variant in p4 p4par p4cp4; do
    echo "PERF $variant 1"
    date -u +%FT%TZ
    uptime
    OMP_NUM_THREADS=1 "$PERF_BIN" stat -e instructions,branch-misses \
        prlimit --as=12884901888 taskset -c 5 "build/$variant" "$D"
done
for variant in p4par p4cp4; do
    echo "PERF $variant 8"
    date -u +%FT%TZ
    uptime
    OMP_NUM_THREADS=8 "$PERF_BIN" stat -e instructions,branch-misses \
        prlimit --as=12884901888 "build/$variant" "$D"
done
