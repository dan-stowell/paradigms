#!/bin/sh
# Build the GraphBLAS cells against SuiteSparse:GraphBLAS in /home/exedev/tools/graphblas (NOTES.md: install).
set -e
cd "$(dirname "$0")"
GB=${GB:-/home/exedev/tools/graphblas}
for p in p1gb p2gb; do
	gcc -O2 -march=native -Wall -Wno-unused-result -Wno-unused-function -I"$GB/include/suitesparse" -o $p $p.c \
		-L"$GB/lib" -Wl,-rpath,"$GB/lib" -lgraphblas -lm
done
