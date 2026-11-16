#!/bin/bash
# Rerun every GraphBLAS cell's timings next to the tuned C references, interleaved in one window.
#   systems/graphblas/bench.sh [DATA]     R=5 rounds, REPS=10 in-process repetitions per warm run
# warm: per process, the best of REPS in-process solves (compute only, inputs already in GraphBLAS objects / memory);
#       then min / median / max of those over R processes. e2e: wall time of one whole process (start, mapping the
#       flat files, building the GraphBLAS objects, one solve, writing out/*.u32, the oracle check), min/median/max
#       over R processes. Threads: GB_THREADS (GxB_NTHREADS) for GraphBLAS, OMP_NUM_THREADS for C; passive waits.
cd "$(dirname "$0")"
D=${1:-/home/exedev/paradigms/data/minio}
R=${R:-5}
REPS=${REPS:-10}
C=${C:-/home/exedev/paradigms/build}
T=$(mktemp -d)
export OMP_WAIT_POLICY=passive
run() { systemd-run --user --scope -q -p MemoryMax=12G "$@"; }
wall() { local t0=$EPOCHREALTIME; "$@" >/dev/null; echo "$EPOCHREALTIME - $t0" | bc -l | awk '{printf "%.1f\n", $1*1000}'; }
date -u +%Y-%m-%dT%H:%M:%SZ; uptime
for r in $(seq "$R"); do
	for t in 1 8; do
		export GB_THREADS=$t OMP_NUM_THREADS=$t
		run ./p2gb "$D" "$REPS" | sed -n "s/.*flood best \([0-9.]*\) ms.*with rule joins (AT, S) best \([0-9.]*\) ms.*/P2-flood $t warm \1\nP2-flood+rulejoins $t warm \2/p" >>"$T/x"
		run "$C/p2par" "$D" "$REPS" | sed -n "s/.*flood best \([0-9.]*\) ms.*/C-p2par $t warm \1/p" >>"$T/x"
		run "$C/p2lazy" "$D" "$REPS" | sed -n "s/.*flood best \([0-9.]*\) ms.*/C-p2lazy $t warm \1/p" >>"$T/x"
		run ./p1gb "$D" "$REPS" | sed -n "s/.*1a \([0-9.]*\) ms, 1b \([0-9.]*\) ms, 1c \([0-9.]*\) ms, all \([0-9.]*\) ms.*/P1a $t warm \1\nP1b $t warm \2\nP1c $t warm \3\nP1-all $t warm \4/p" >>"$T/x"
		run "$C/p1par" "$D" "$REPS" | sed -n "s/.*best [0-9.]* ms = join by name \([0-9.]*\) + relocation targets \([0-9.]*\).*/C-p1par-1a $t warm \1\nC-p1par-1b $t warm \2/p; s/.*content-hash join \([0-9.]*\) ms.*/C-p1par-1c $t warm \1/p" >>"$T/x"
		GB_NOGUARD=1 run ./p1gb "$D" "$REPS" | sed -n "s/.*1a \([0-9.]*\) ms.*1c \([0-9.]*\) ms, all \([0-9.]*\) ms.*/P1a-noguard $t warm \1\nP1c-noguard $t warm \2/p" >>"$T/x"
		echo "P2 $t e2e $(wall run ./p2gb "$D" 1)" >>"$T/x"
		echo "P1-all $t e2e $(wall run ./p1gb "$D" 1)" >>"$T/x"
		echo "C-p2par $t e2e $(wall run "$C/p2par" "$D" 1)" >>"$T/x"
	done
done
awk '{k=$1" "$2" "$3; v[k]=v[k]" "$4} END {for (k in v) print k, v[k]}' "$T/x" | sort -k1,1 -k2n | while read -r c t m vals; do
	echo "$vals" | tr ' ' '\n' | grep . | sort -n | awk -v c="$c" -v t="$t" -v m="$m" \
		'{x[NR]=$1} END {printf "%-22s %d threads %-4s min %8.1f median %8.1f max %8.1f ms (%d runs)\n", c, t, m, x[1], x[int((NR+1)/2)], x[NR], NR}'
done
date -u +%Y-%m-%dT%H:%M:%SZ; uptime
rm -rf "$T"
