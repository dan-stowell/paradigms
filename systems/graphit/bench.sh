#!/bin/bash
# bench.sh: GraphIt P2 schedules against the C references (ref/flat/p2.c 1 core, p2par.c 1 and 8 threads),
# interleaved: ROUNDS rounds (default 5), each runs every configuration once, in the same order.
#   systems/graphit/bench.sh [DATADIR]          (default /home/exedev/paradigms/data/minio; needs build.sh DATADIR)
# warm: one process per round runs the flood REPS times on resident data (>= ~1 s); per process we take the min
#   and the median of reps 2..REPS (rep 1 pays the first-touch faults). Reported over the rounds: min of the
#   process mins / median of the process medians / max of the process medians. p2par prints best and mean per
#   process (it does not print each rep): its best and mean stand in for min and median. p2 floods once per
#   process: 3 processes per round, all pooled (min / median / max).
# e2e: the whole process (start, read the input, one flood, check), one per round, min / median / max. GraphIt
#   reads the converted .wsg (build/<name>.wsg); the conversion from the flat files (towsg) is timed apart.
# Every process runs under prlimit --as=8G with OMP_WAIT_POLICY=passive (spinning OpenMP collapses when the 8
# cores are shared: JOURNAL 16:20Z).
set -e
cd "$(dirname "$0")"
D=${1:-/home/exedev/paradigms/data/minio}
W=build/$(basename "$D").wsg
ROUNDS=${ROUNDS:-5}
REPO=$(cd ../.. && pwd)
gcc -O2 -march=native -o build/c_p2 $REPO/ref/flat/p2.c
gcc -O2 -march=native -fopenmp -o build/c_p2par $REPO/ref/flat/p2par.c
export OMP_WAIT_POLICY=passive
lim() { prlimit --as=8589934592 "$@"; }
# label binary threads reps
CFGS=${CFGS:-"gi-serial p2_serial 1 25
gi-push p2_push 1 25
gi-hybrid_bv p2_hybrid_bv 1 25
gi-push p2_push 8 50
gi-push_sliding p2_push_sliding 8 50
gi-hybrid_bv p2_hybrid_bv 8 50
gi-densepush p2_densepush 8 50
C-p2par c_p2par 1 30
C-p2par c_p2par 8 60
C-p2 c_p2 1 0"}
T=$(mktemp -d)
echo "# start $(date -u +%FT%TZ) load $(cut -d' ' -f1-3 /proc/loadavg) data $D rounds $ROUNDS"
for r in $(seq $ROUNDS); do
	echo "# round $r $(date -u +%T) load $(cut -d' ' -f1-3 /proc/loadavg)"
	echo "$CFGS" | while read lab bin th reps; do
		k=$T/$lab.$th
		case $bin in
		c_p2)
			for i in 1 2 3; do lim build/c_p2 $D | sed -n 's/.*flood \([0-9.]*\) ms.*/\1/p' >> $k.min; done
			cp $k.min $k.med ;;
		c_p2par)
			out=$(OMP_NUM_THREADS=$th lim build/c_p2par $D $reps)
			echo "$out" | sed -n 's/.*flood best \([0-9.]*\) ms.*/\1/p' >> $k.min
			echo "$out" | sed -n 's/^mean \([0-9.]*\) ms.*/\1/p' >> $k.med ;;
		*)
			OMP_NUM_THREADS=$th lim build/$bin $W $D $reps | grep -E '^[0-9.]+$' | tail -n +2 | sort -n > $k.reps
			head -1 $k.reps >> $k.min
			awk '{a[NR]=$1} END {print a[int((NR+1)/2)]}' $k.reps >> $k.med ;;
		esac
		t0=$(date +%s%N)
		case $bin in
		c_p2) lim build/c_p2 $D > /dev/null ;;
		c_p2par) OMP_NUM_THREADS=$th lim build/c_p2par $D 1 > /dev/null ;;
		*) OMP_NUM_THREADS=$th lim build/$bin $W $D 1 > /dev/null ;;
		esac
		t1=$(date +%s%N)
		echo $(( (t1 - t0) / 1000000 )) >> $k.e2e
	done
done
stat() { sort -n "$1" | awk -v f="$2" '{a[NR]=$1} END {printf f, a[1], a[int((NR+1)/2)], a[NR]}'; }
echo "$CFGS" | while read lab bin th reps; do
	k=$T/$lab.$th
	printf "%-16s %s thr  warm min %s / med %s / max %s ms   e2e %s\n" $lab $th \
		"$(sort -n $k.min | head -1)" "$(stat $k.med '%s %s %s' | cut -d' ' -f2)" "$(sort -n $k.med | tail -1)" \
		"$(stat $k.e2e 'min %s / med %s / max %s ms')"
done
echo "# end $(date -u +%FT%TZ) load $(cut -d' ' -f1-3 /proc/loadavg)"
rm -rf $T
