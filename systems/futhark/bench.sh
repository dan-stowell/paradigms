#!/bin/bash
# bench.sh: rerun every Futhark cell's timings; one line per (cell, cores, warm/e2e) with min / median / max.
#   systems/futhark/bench.sh [DATADIR]       (builds first: YIELD=1 build.sh)
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
[ -n "$NOBUILD" ] || YIELD=1 ./build.sh >/dev/null 2>&1
run() { systemd-run --user --scope -q -p MemoryMax=10G "$@"; }
# cell program reps(1 core) reps(8 cores)
cells="1a p1 20 40
1b p1 10 40
1b_scan p1 10 40
1c p1 10 20
2 p2 8 20
2lazy p2 6 20
3a p3 80 100
3a_loop p3 80 100
3b p3 6 20"
echo "# ${HUGE:+HUGE=1 }$(date -u +%FT%TZ) load $(cut -d' ' -f1-3 /proc/loadavg) data $D"
echo "$cells" | while read cell p r1 r8; do
	for cfg in ${CFGS:-c:1:1 multicore:8:8spin mcy:8:8yield}; do
		IFS=: read be th lab <<< "$cfg"
		reps=$r8; [ $th = 1 ] && reps=$r1
		set -- $be $th $reps $lab${HUGE:++huge}
		line=$(run build/${p}_$1.exe $D $2 $3 $cell nocheck | grep ' | min')
		printf "%-8s %-7s warm  %s\n" $cell $4 "$(echo "$line" | sed 's/.*| //')"
		ts=""
		for i in 1 2 3 4 5; do
			t0=$(date +%s%N); run build/${p}_$1.exe $D $2 1 $cell nocheck >/dev/null; t1=$(date +%s%N)
			ts="$ts $(( (t1 - t0) / 1000000 ))"
		done
		printf "%-8s %-7s e2e   %s\n" $cell $4 "$(echo $ts | tr ' ' '\n' | sort -n | awk '{a[NR]=$1} END {printf "min %d med %d max %d ms", a[1], a[3], a[5]}')"
	done
done
echo "# end $(date -u +%FT%TZ) load $(cut -d' ' -f1-3 /proc/loadavg)"
