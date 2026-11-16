#!/usr/bin/env bash
# Stable identity in C (ref/flat/unify.c): check against ddflow's idmap/strmap for both MinIO edits, then time it.
#   ref/flat/bench-unify.sh [ROUNDS] > results/c-unify-bench.txt;  ref/flat/summarize-unify.py results/c-unify-bench.txt
# Each run prints end-to-end (map the columns, then one unify) and warm (inputs mapped, unify repeated to >= 1 s,
# per iteration). Modes:
#   c1      1 core: memcmp every object's key columns (the fast path), slow path on the objects that differ
#   c1fp    1 core, per-object fingerprints given (as an extractor could write them; computed outside the timing)
#   c1lazy  as c1fp, fingerprints read from a file and the columns mapped lazily (no MAP_POPULATE), so end-to-end
#           reads only the pages the run touches: the realistic end-to-end of the fingerprint variant
#   omp8, omp8fp  OpenMP 8 threads (only the fast path is parallel)
# Rounds are interleaved (every mode and edit once per round); `uptime` is printed before each run.
set -euo pipefail
here=$(cd "$(dirname "$0")/../.." && pwd)
data=${DATA:-/home/exedev/paradigms/data}
build=${BUILD_DIR:-$here/build}
rounds=${1:-5}
fpdir=${FPDIR:-$build/unify-fp}
mkdir -p "$build" "$fpdir"
gcc -O3 -march=native -Wall -Wno-unknown-pragmas -o "$build/unify" "$here/ref/flat/unify.c"
gcc -O3 -march=native -Wall -fopenmp -o "$build/unify_omp" "$here/ref/flat/unify.c"
export OMP_WAIT_POLICY=${OMP_WAIT_POLICY:-passive}
lim=(prlimit --as=8589934592)
run() { # mode edit args...
	local mode=$1 e=$2
	shift 2
	case $mode in
	c1) "${lim[@]}" "$build/unify" "$data/minio" "$data/$e" "$@" ;;
	c1fp) "${lim[@]}" "$build/unify" "$data/minio" "$data/$e" --fp "$@" ;;
	c1lazy) "${lim[@]}" "$build/unify" "$data/minio" "$data/$e" --fpdir "$fpdir" "$@" ;;
	exact) "${lim[@]}" "$build/unify" "$data/minio" "$data/$e" --exact "$@" ;;
	omp8) "${lim[@]}" env OMP_NUM_THREADS=8 "$build/unify_omp" "$data/minio" "$data/$e" "$@" ;;
	omp8fp) "${lim[@]}" env OMP_NUM_THREADS=8 "$build/unify_omp" "$data/minio" "$data/$e" --fp "$@" ;;
	esac
}
for e in minio-edit minio-edit2; do
	run c1lazy "$e" --reps 0 >/dev/null # writes the fingerprint files once
	for m in c1 c1fp c1lazy exact omp8 omp8fp; do
		echo "== check $e $m"
		run $m "$e" --check --reps 0 | grep -E 'objects differ|byte-identical|differ from|CHECK'
	done
done
for r in $(seq 1 "$rounds"); do
	for e in minio-edit minio-edit2; do
		for m in c1 c1fp c1lazy omp8 omp8fp; do
			echo "== round $r $e $m | $(uptime)"
			run $m "$e" --reps 3
		done
	done
done
