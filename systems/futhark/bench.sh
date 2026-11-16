#!/bin/bash
# bench.sh: rerun every Futhark cell's timings; one line per (cell, cores, warm/e2e) with min / median / max.
#   systems/futhark/bench.sh [DATADIR [P4DATADIR]]       (builds first: YIELD=1 build.sh)
#   CELLS="4 4bf" bench.sh: only those cells. Problem 4 reads P4DATADIR; after each P4 cell the C references
#   (ref/p4/p4.c 1 core, ref/p4/p4par.c 8 threads; best lay out + emit of 10 warm reps) run 5 times each, so the
#   two are interleaved in the same window. P4 at 1 thread of the multicore backend: CFGS=multicore:1:1mc.
# 1 core = the sequential `c` backend; 8 cores = the `multicore` backend, both the stock runtime ("spin": its
# join loops and scan look-backs busy-wait) and the patched one ("yield": sched_yield() in those loops, build.sh).
# warm: the entry alone, inputs resident, repeated in one process until a run is >= ~1 s (REPS below);
# e2e: the whole process (start, read and upload the flat files, one run, copy the output out), 5 runs, no check.
# HUGE=1: every large malloc of the Futhark runtime (inputs, outputs, intermediates) on 2 MB huge pages, through
#   glibc's tunable (no code change; needs vm.nr_hugepages reserved, falls back to 4 KB pages when the pool is empty).
# CFGS="c:1:1 multicore:8:8spin mcy:8:8yield" picks the configurations (backend:threads:label).
set -e
[ -n "$HUGE" ] && export GLIBC_TUNABLES=glibc.malloc.hugetlb=2
cd "$(dirname "$0")"
D=${1:-/home/exedev/paradigms/data/minio}
D4=${2:-/home/exedev/paradigms/data/minio-p4}
[ -n "$NOBUILD" ] || YIELD=1 ./build.sh >/dev/null 2>&1
run() { systemd-run --user --scope -q -p MemoryMax=10G "$@"; }
# cell program reps(1 core) reps(8 cores)
cells="1a p1 20 40
1b p1 10 40
1b_scan p1 10 40
1c p1 10 20
2 p2 8 20
2hyb p2 8 20
2seq p2 8 20
2lazy p2 6 20
3a p3 80 100
3a_loop p3 80 100
3b p3 6 20
3b_blk p3 8 20
4 p4 5 10
4g p4 5 10
4f p4 3 6
4b p4 5 10
4bf p4 5 10"
echo "# ${HUGE:+HUGE=1 }$(date -u +%FT%TZ) load $(cut -d' ' -f1-3 /proc/loadavg) data $D"
echo "$cells" | while read cell p r1 r8; do
	[ -n "$CELLS" ] && ! echo " $CELLS " | grep -q " $cell " && continue
	dd=$D; [ $p = p4 ] && dd=$D4
	for cfg in ${CFGS:-c:1:1 multicore:8:8spin mcy:8:8yield}; do
		IFS=: read be th lab <<< "$cfg"
		reps=$r8; [ $th = 1 ] && reps=$r1
		set -- $be $th $reps $lab${HUGE:++huge}
		line=$(run build/${p}_$1.exe $dd $2 $3 $cell nocheck | grep ' | min')
		printf "%-8s %-7s warm  %s\n" $cell $4 "$(echo "$line" | sed 's/.*| //')"
		ts=""
		for i in 1 2 3 4 5; do
			t0=$(date +%s%N); run build/${p}_$1.exe $dd $2 1 $cell nocheck >/dev/null; t1=$(date +%s%N)
			ts="$ts $(( (t1 - t0) / 1000000 ))"
		done
		printf "%-8s %-7s e2e   %s\n" $cell $4 "$(echo $ts | tr ' ' '\n' | sort -n | awk '{a[NR]=$1} END {printf "min %d med %d max %d ms", a[1], a[3], a[5]}')"
	done
	if [ $p = p4 ]; then
		# the C references (ref/p4/p4.c 1 core, ref/p4/p4par.c OpenMP 8 threads): best lay out + best emit of
		# P4_REPS=10 warm reps per process, 5 processes
		gcc -O2 -march=native -o build/p4ref "$(pwd)/../../ref/p4/p4.c"
		gcc -O2 -march=native -fopenmp -o build/p4parref "$(pwd)/../../ref/p4/p4par.c"
		for c in "p4ref 1" "p4parref 8"; do
			set -- $c
			ts=""
			for i in 1 2 3 4 5; do
				ts="$ts $(OMP_NUM_THREADS=$2 OMP_WAIT_POLICY=passive P4_REPS=10 run build/$1 $D4 | awk '/lay out/ {print $6 + $9}')"
			done
			printf "%-8s %-7s warm  %s\n" C-$1 $2 "$(echo $ts | tr ' ' '\n' | sort -n | awk '{a[NR]=$1} END {printf "min %.1f med %.1f max %.1f ms (lay out + emit)", a[1], a[3], a[5]}')"
		done
	fi
done
echo "# end $(date -u +%FT%TZ) load $(cut -d' ' -f1-3 /proc/loadavg)"
