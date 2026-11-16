#!/bin/bash
# Build every problem's Futhark library with the multicore and the sequential c backend, and its driver.
#   build.sh [p1 p2 p3]     FUTFLAGS=--unsafe build.sh   (bounds checks off)
#   YIELD=1 build.sh: the multicore runtime's spin-waits (the join loop of a parallel op, idle workers stealing,
#   the scan's look-back on its predecessor's flag) call sched_yield() each time round, so a descheduled thread
#   is not waited on for a whole time slice on a shared machine (Futhark has no wait-policy knob). -> build/*_mcy.exe
set -e
cd "$(dirname "$0")"
mkdir -p build
progs=${@:-p1 p2 p3}
for p in $progs; do
	for be in multicore c; do
		futhark $be $FUTFLAGS --library -o build/${p}_$be $p.fut 2>&1 | grep -v "^$" | grep -v "Size expression\|error-index\|For more information" || true
		mkdir -p build/$be
		cp build/${p}_$be.h build/$be/$p.h
		def=""; [ $be = multicore ] && def="-DMULTICORE"
		gcc -O3 -march=native $def -Ibuild/$be -I. -o build/${p}_$be.exe $p.c build/${p}_$be.c -lm -lpthread
		if [ $be = multicore ] && [ -n "$YIELD" ]; then
			sed -e 's/^\( *\)while (!(old_flag_\(.*\)) {$/&\n\1    sched_yield();/' \
			    -e 's/^\( *\)while (__atomic_load_n(&join_counter, __ATOMIC_ACQUIRE) != 0) {$/&\n\1    sched_yield();/' \
			    -e 's/^\( *\)while (!is_finished(worker) \&\& scheduler->active_work) {$/&\n\1    sched_yield();/' \
			    build/${p}_$be.c > build/${p}_mcy.c
			echo "$p: $(grep -c sched_yield build/${p}_mcy.c) yields"
			gcc -O3 -march=native $def -D_GNU_SOURCE -include sched.h -Ibuild/$be -I. -o build/${p}_mcy.exe $p.c build/${p}_mcy.c -lm -lpthread
		fi
	done
done
