#!/bin/bash
# Build every problem's Futhark library with the multicore and the sequential c backend, and its driver.
#   build.sh [p1 p2 p3]     FUTFLAGS=--unsafe build.sh   (bounds checks off)
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
	done
done
