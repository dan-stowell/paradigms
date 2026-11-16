#!/bin/sh
# Build the GraphBLAS cells against SuiteSparse:GraphBLAS 10.5.2 (NOTES.md: install): p1gb/p2gb against the JIT-off
# build in /home/exedev/tools/graphblas, p1gb-jit/p2gb-jit against the JIT-on build in /home/exedev/tools/graphblas-jit
# (kernels compiled on first use and cached in ~/.SuiteSparse).
set -e
cd "$(dirname "$0")"
for v in "" -jit; do
	GB=/home/exedev/tools/graphblas$v
	for p in p1gb p2gb; do
		gcc -O2 -march=native -Wall -Wno-unused-result -Wno-unused-function -I"$GB/include/suitesparse" -o $p$v $p.c \
			-L"$GB/lib" -Wl,-rpath,"$GB/lib" -lgraphblas -lm
	done
done
