#!/bin/bash
# Rerun every Halide cell's timings: systems/halide/bench.sh [DATA [DATA_P4]]   (build first: systems/halide/cells/build.sh)
# P4=only: just problem 4.
# Warm lines come from the drivers (bench.h: per-iteration min/med/max over 5 runs of >= 1 s, in-process).
# e2e lines: the whole process (start, mmap of the flat files, compute, oracle check excluded where the driver
# separates it), wall time from /usr/bin/time, min/med/max over 5 runs. Threads: HL_NUM_THREADS (Halide's pool;
# its idle workers spin briefly, then sleep on a condition variable).
set -u
D=${1:-/home/exedev/paradigms/data/minio}
D4=${2:-/home/exedev/paradigms/data/minio-p4}
B=$(cd "$(dirname "$0")" && pwd)/cells/build
echo "# $(date -u +%FT%TZ) $(uptime)"
lim="prlimit --as=6000000000"
[ "${P4:-}" = only ] || for t in 1 8; do
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
[ "${P4:-}" = only ] || for t in 1 8; do
    v=$([ $t = 1 ] && echo 0 || echo 1)
    e2e p1 $t $B/p1 $D e2e $v
    e2e p2-lazy $t $B/p2 $D e2e $([ $t = 1 ] && echo 0 || echo 1)
    e2e p2-eager $t $B/p2 $D e2e $([ $t = 1 ] && echo 2 || echo 3)
    e2e p3 $t $B/p3 $D e2e 0 $([ $t = 1 ] && echo 0 || echo 3)
done

# ---- problem 4 (code generation by shape). Warm: 1 thread by process CPU time (BENCH_CPU=1, the machine is shared),
# 8 threads by wall time; the best schedule per thread count plus the gather form. e2e: whole process wall (s),
# output on 2 MB pages (P4_HUGE=1) and Halide's own allocations too (glibc.malloc.hugetlb=2), no oracle check.
# References, same window: ref/p4/p4.c (1 core) and ref/p4/p4par.c (OpenMP), built here into a temp dir.
R=$(cd "$(dirname "$0")/../.." && pwd)/ref/p4; T=$(mktemp -d)
gcc -O2 -march=native -o $T/p4 $R/p4.c && gcc -O2 -march=native -fopenmp -o $T/p4par $R/p4par.c 2>/dev/null
P4BIN="$B/p4 $D4"
export GLIBC_TUNABLES=glibc.malloc.hugetlb=2
for t in 1 8; do
    s=$([ $t = 1 ] && echo f || echo t); c=$([ $t = 1 ] && echo 1 || echo 0)
    BENCH_CPU=$c HL_NUM_THREADS=$t $lim $P4BIN lay_${s}_lf | grep -E "^4 (templates|lay)" | cut -c1-80 | sed "s/^/p4 threads=$t warm /"
    for e in emit_${s}_i2_v32_p1 emit_${s}_i2_g_p0; do
        BENCH_CPU=$c HL_NUM_THREADS=$t $lim $P4BIN $e | grep -E "^4 emit" | cut -c1-80 | sed "s/^/p4 threads=$t warm /"
    done
    [ -x $T/p4par ] && OMP_NUM_THREADS=$t OMP_WAIT_POLICY=passive P4_REPS=20 $T/p4par $D4 | grep "^p4par" | cut -c1-90 | sed "s/^/p4 threads=$t ref    /"
done
$T/p4 $D4 | head -1 | sed "s/^/p4 threads=1 ref    /"
e2e4() {  # e2e4 THREADS LAYOUT EMIT: wall of the whole process, 5 runs
    local t=$1 v=()
    for i in 1 2 3 4 5; do
        v+=($( { P4_NOCHECK=1 P4_HUGE=1 HL_NUM_THREADS=$t /usr/bin/time -f "%e" $lim $P4BIN e2e $2 $3 >/dev/null; } 2>&1 | tail -1))
    done
    printf "p4 threads=%s e2e wall s (%s %s): %s\n" "$t" "$2" "$3" "$(printf '%s\n' "${v[@]}" | sort -n | tr '\n' ' ')"
}
e2e4 1 lay_f_lf emit_f_i2_v32_p1
e2e4 8 lay_t_lf emit_t_i2_v32_p1
rm -rf $T
