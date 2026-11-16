#!/bin/bash
# build.sh: the converter, then p2.gt under every schedule in sched/*.gt -> build/p2_<schedule>
#   systems/graphit/build.sh [DATADIR...]   (also converts each DATADIR to build/<name>.wsg if missing)
# GraphIt is at $GRAPHIT (default /home/exedev/tools/graphit, built per NOTES.md). The generated C++ is compiled
# with OpenMP (g++ 13 has no Cilk Plus), so OMP_NUM_THREADS picks the threads. NUMA schedules need graphit.patch
# applied to GraphIt and libnuma-dev. SCHEDS="sched/push.gt" limits it.
set -e
cd "$(dirname "$0")"
G=${GRAPHIT:-/home/exedev/tools/graphit}
mkdir -p build/gen
gcc -O2 -march=native -o build/towsg towsg.c
for d in "$@"; do
	w=build/$(basename "$d").wsg
	[ -s $w ] || build/towsg "$d" $w
done
for s in ${SCHEDS:-sched/*.gt}; do
	n=$(basename $s .gt)
	(cd build/gen && python3 $G/build/bin/graphitc.py -a ../../p2.gt -f ../../$s -o $n.cpp >/dev/null)
	numa=; case $n in *numa*) numa="-DNUMA -lnuma" ;; esac # needs libnuma-dev; run with OMP_PLACES=sockets
	g++ -std=c++14 -O3 -march=native -DOPENMP -fopenmp -w -I $G/src/runtime_lib build/gen/$n.cpp host.cpp $numa -o build/p2_$n
done
