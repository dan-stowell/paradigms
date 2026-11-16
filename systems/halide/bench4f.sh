#!/bin/bash
# bench4f.sh: P4 emit over the flattened stream in Halide (p4f: D and H1 streams) against the C references and
# Futhark's 4D, interleaved: ROUNDS rounds, in each one process per (program, threads). Prints one line per
# (round, program, threads) with its best emit ms, then min / median / max over the rounds.
#   systems/halide/bench4f.sh [ROUNDS [P4DATADIR]]
# Build first: systems/halide/cells/build.sh p4f; systems/futhark/build.sh p4 (optional: skipped if absent).
# Halide: wall time, bench.h min over 5 runs of >= 1 s; output on 2 MB pages. C: P4_REPS=32, best of; passive
# OpenMP wait; p4par's number is lay out + emit (its emit starts from items, the others from a prepared stream).
# Futhark: multicore backend, 10 reps, its min. Memory: systemd-run MemoryMax=12G.
set -u
here=$(cd "$(dirname "$0")" && pwd)
R=${1:-5}
D=${2:-/home/exedev/paradigms/data/minio-p4}
B=$here/cells/build
F=$here/../futhark/build/p4_multicore.exe
T=$(mktemp -d)
for p in p4par p4runs p4holes; do gcc -O2 -march=native -fopenmp -o $T/$p $here/../../ref/p4/$p.c; done
lim="systemd-run --user --scope -q -p MemoryMax=12G"
out=$T/runs
echo "# $(date -u +%FT%TZ) $(uptime) data $D"
for r in $(seq 1 $R); do
    echo "# round $r $(date -u +%FT%TZ) load $(cut -d' ' -f1-3 /proc/loadavg)"
    for t in 1 8; do
        s=$([ $t = 1 ] && echo f || echo t)
        HL_NUM_THREADS=$t $lim $B/p4f $D emitf_$s | awk -v t=$t '/^4f/ {print "halide-"$2, t, $5} /P4 FAIL/ {print "FAIL", t, 0}' | tee -a $out
        HL_NUM_THREADS=$t $lim $B/p4f $D emitf_${s}_r32 | awk -v t=$t '/^4f/ {print "halide-"$2"-r32", t, $5} /P4 FAIL/ {print "FAIL", t, 0}' | tee -a $out
        c="env OMP_NUM_THREADS=$t OMP_WAIT_POLICY=passive P4_REPS=32 $lim"
        $c $T/p4par $D | awk -v t=$t '/^p4par/ {gsub(",", ""); print "p4par(lay+emit)", t, $6 + $9} /P4 FAIL/ {print "FAIL", t, 0}' | tee -a $out
        for v in D B; do $c $T/p4runs $D $v | awk -v t=$t -v v=$v '/^p4runs/ {print "p4runs-"v, t, $6} /P4 FAIL/ {print "FAIL", t, 0}' | tee -a $out; done
        $c $T/p4holes $D H1 | awk -v t=$t '/^p4holes/ {print "p4holes-H1", t, $6} /P4 FAIL/ {print "FAIL", t, 0}' | tee -a $out
        [ -x $F ] && GLIBC_TUNABLES=glibc.malloc.hugetlb=2 $lim $F $D $t 10 4D | awk -v t=$t '/^4D / {for (i = 1; i <= NF; i++) if ($i == "min") print "futhark-4D", t, $(i+1)} /P4D FAIL/ {print "FAIL", t, 0}' | tee -a $out
    done
done
echo "# end $(date -u +%FT%TZ) load $(cut -d' ' -f1-3 /proc/loadavg)"
echo "# program threads: min / median / max over rounds (ms)"
sort -k1,1 -k2,2n $out | awk '{k=$1" "$2; if (k != last && last != "") flush(); last=k; v[++n]=$3}
    function flush() {asort(v); printf "%-22s %s / %s / %s\n", last, v[1], v[int((n+1)/2)], v[n]; delete v; n=0}
    END {flush()}'
rm -rf $T
