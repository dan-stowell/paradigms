#!/bin/bash
# Rerun every Halide cell's timings: systems/halide/bench.sh [DATA]   (build first: systems/halide/cells/build.sh)
# Warm lines come from the drivers (bench.h: per-iteration min/med/max over 5 runs of >= 1 s, in-process).
# e2e lines: the whole process (start, mmap of the flat files, compute, oracle check excluded where the driver
# separates it), wall time from /usr/bin/time, min/med/max over 5 runs. Threads: HL_NUM_THREADS (Halide's pool;
# its idle workers spin briefly, then sleep on a condition variable).
set -u
D=${1:-/home/exedev/paradigms/data/minio}
B=$(cd "$(dirname "$0")" && pwd)/cells/build
echo "# $(date -u +%FT%TZ) $(uptime)"
lim="systemd-run --user --scope -q -p MemoryMax=10G"
for t in 1 8; do
    s=$([ $t = 1 ] && echo f || echo t)   # serial / parallel schedule
    HL_NUM_THREADS=$t $lim $B/p1 $D all | grep -E "^(counts|1[abc]) .*_$s |^counts" | sed "s/^/p1 threads=$t warm /"
    HL_NUM_THREADS=$t $lim $B/p2 $D all | grep -E "^2 flood_._$s " | sed "s/^/p2 threads=$t warm /"
    HL_NUM_THREADS=$t $lim $B/p3 $D all | grep -E "^3[ab] " | sed "s/^/p3 threads=$t warm /"
done
e2e() {  # e2e NAME THREADS CMD...: wall ms of the whole process, 5 runs
    local name=$1 t=$2; shift 2
    local v=()
    for i in 1 2 3 4 5; do
        v+=($( { HL_NUM_THREADS=$t /usr/bin/time -f "%e" $lim "$@" >/dev/null; } 2>&1 | tail -1))
    done
    printf "%s threads=%s e2e wall s: %s\n" "$name" "$t" "$(printf '%s\n' "${v[@]}" | sort -n | tr '\n' ' ')"
}
for t in 1 8; do
    v=$([ $t = 1 ] && echo 0 || echo 1)
    e2e p1 $t $B/p1 $D e2e $v
    e2e p2-lazy $t $B/p2 $D e2e $([ $t = 1 ] && echo 0 || echo 1)
    e2e p2-eager $t $B/p2 $D e2e $([ $t = 1 ] && echo 2 || echo 3)
    e2e p3 $t $B/p3 $D e2e 0 $([ $t = 1 ] && echo 0 || echo 3)
done
