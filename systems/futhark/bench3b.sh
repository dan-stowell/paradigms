#!/bin/bash
# bench3b.sh: 3b (copy as a scatter of 64-byte chunks) vs 3b_blk (copy in 64 blocks, a memcpy loop in each),
# interleaved (c backend = c1; multicore at 1 and 8 threads): ROUNDS rounds, each one process per (cell, config) with REPS warm reps; prints each process's
# min and median, then per (cell, config) min / median / max of the per-process medians. 2 MB pages as in bench.sh.
#   systems/futhark/bench3b.sh [DATADIR]     (ROUNDS=5 REPS=8; NOBUILD=1 skips ./build.sh p3)
set -e
cd "$(dirname "$0")"
export GLIBC_TUNABLES=glibc.malloc.hugetlb=2
D=${1:-/home/exedev/paradigms/data/minio}
[ -n "$NOBUILD" ] || ./build.sh p3 >/dev/null 2>&1
out=$(mktemp)
echo "# $(date -u +%FT%TZ) load $(cut -d' ' -f1-3 /proc/loadavg)"
for r in $(seq ${ROUNDS:-5}); do
	for cfg in c:1 multicore:1 multicore:8; do
		IFS=: read be th <<< "$cfg"
		for cell in 3b 3b_blk 3b_copy 3b_blkcopy; do
			l=$(prlimit --as=6442450944 build/p3_$be.exe $D $th ${REPS:-8} $cell nocheck | grep ' | min')
			mn=$(echo "$l" | sed 's/.*min \([0-9.]*\) med \([0-9.]*\).*/\1 \2/')
			lab=$th; [ $be = c ] && lab=c1
			echo "$cell $lab $mn" | tee -a $out
		done
	done
done
echo "# end $(date -u +%FT%TZ) load $(cut -d' ' -f1-3 /proc/loadavg)"
echo "# cell threads: best min | per-process medians min / median / max (ms)"
for cell in 3b 3b_blk 3b_copy 3b_blkcopy; do for th in c1 1 8; do
	grep "^$cell $th " $out | awk -v c=$cell -v t=$th '{m[NR]=$4; if (NR==1||$3<b) b=$3} END {n=asort(m); printf "%-10s %s: %.1f | %.1f / %.1f / %.1f\n", c, t, b, m[1], m[int((n+1)/2)], m[n]}'
done; done
rm -f $out
