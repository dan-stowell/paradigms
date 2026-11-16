#!/bin/bash
# bench4D.sh: P4 emit from the flattened stream (4D: prepare apart, then emit_flat) vs the template emit (4bf) vs the
# C references (ref/p4/p4.c; ref/p4/p4runs.c D = the same flattened stream in C), interleaved: ROUNDS rounds, in each
# one process per (program, threads). Futhark: 2 MB pages (glibc tunable), REPS warm reps; C: P4_REPS=32 (best of).
# Prints per process min and median (Futhark) or best (C), then per (program, threads) the best min and
# min / median / max of the per-process medians (C: of the per-process bests).
#   systems/futhark/bench4D.sh [P4DATADIR]     (build first: ./build.sh p4 and ref/p4/check-runs.sh)
set -e
cd "$(dirname "$0")"
D=${1:-/home/exedev/paradigms/data/minio-p4}
B=$(pwd)/../../build
out=$(mktemp)
run() { prlimit --as=12884901888 "$@"; }
fut() { # cell backend threads label
	GLIBC_TUNABLES=glibc.malloc.hugetlb=2 run build/p4_$2.exe $D $3 ${REPS:-10} $1 nocheck > $out.f
	for w in prepare $1; do
		l=$(grep "^$w " $out.f | grep ' | min') || continue
		echo "$w $4 $(echo "$l" | sed 's/.*min \([0-9.]*\) med \([0-9.]*\).*/\1 \2/')" | tee -a $out
	done
}
cref() { # binary threads label [variant]
	t=$(OMP_NUM_THREADS=$2 OMP_WAIT_POLICY=passive FLAT_HUGE=0 P4_REPS=32 run $B/$1 $D $4 | awk '/^p4 \(1 core\)/ {print $6 + $9} /^p4runs/ {print $6} /^prep D/ {print "prep", $10}')
	echo "$t" | grep -q prep && echo "prepD $3 $(echo "$t" | awk '/prep/ {print $2, $2}')" | tee -a $out
	v=$(echo "$t" | grep -v prep)
	echo "$1$4 $3 $v $v" | tee -a $out
}
echo "# $(date -u +%FT%TZ) load $(cut -d' ' -f1-3 /proc/loadavg)"
for r in $(seq ${ROUNDS:-5}); do
	fut 4bf multicore 1 mc1; fut 4D multicore 1 mc1; fut 4bf c 1 c1; fut 4D c 1 c1
	cref p4 1 1; cref p4runs 1 1 D
	fut 4bf multicore 8 8; fut 4D multicore 8 8; cref p4runs 8 8 D
done
echo "# end $(date -u +%FT%TZ) load $(cut -d' ' -f1-3 /proc/loadavg)"
echo "# program threads: best | per-process medians min / median / max (ms)"
awk '{k=$1" "$2; n[k]++; m[k, n[k]]=$4; if (!(k in b) || $3 < b[k]) b[k]=$3; if (!(k in seen)) {seen[k]=1; ord[++no]=k}}
     END {for (i=1;i<=no;i++) {k=ord[i]; c=n[k]; for (x=1;x<=c;x++) a[x]=m[k,x]; asort(a);
          printf "%-14s %s: %.1f | %.1f / %.1f / %.1f\n", k, "", b[k], a[1], a[int((c+1)/2)], a[c]; delete a}}' $out
rm -f $out $out.f
