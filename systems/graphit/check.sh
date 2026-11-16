#!/bin/bash
# check.sh: build, convert, and check every schedule's live set against oracle/p2_live.u32 (exact, element-wise)
#   systems/graphit/check.sh [DATADIR...]   (default: data/minio and data/kube-apiserver)
cd "$(dirname "$0")"
set -- ${@:-/home/exedev/paradigms/data/minio /home/exedev/paradigms/data/kube-apiserver}
./build.sh "$@" || exit 1
export OMP_WAIT_POLICY=passive OMP_PLACES=sockets # (the NUMA schedule divides by the number of places)
ok=0
for d in "$@"; do
	for b in build/p2_*; do
		for t in 1 8; do
			r=$(OMP_NUM_THREADS=$t prlimit --as=8589934592 $b build/$(basename $d).wsg $d 1 | tail -2 | tr '\n' ' ')
			echo "$(basename $d) $(basename $b) $t thr: $r"
			case "$r" in *"P2 OK"*) ;; *) ok=1 ;; esac
		done
	done
done
[ $ok = 0 ] && echo "all P2 OK" || echo "P2 FAIL"
exit $ok
