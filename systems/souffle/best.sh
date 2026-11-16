#!/usr/bin/env bash
# best.sh <binary> <threads> [runs=5] [extra args]: run a flatio program N times on MinIO's flat files and print
# the best (minimum) load, compute and end-to-end wall time, in ms. e2e = process start to exit, as `date` sees it.
set -euo pipefail
bin=$1; j=$2; n=${3:-5}; shift 3 || shift $#
here=$(cd "$(dirname "$0")" && pwd)
data=${DATA:-/home/exedev/paradigms/data/minio}
out=${OUT:-$here/build/out}
mkdir -p "$out"
bl=1e9; bc=1e9; be=1e9
for ((i = 0; i < n; i++)); do
	t0=$(date +%s%N)
	line=$(prlimit --as=12000000000 "$bin" -j"$j" -F "$data" -D "$out" "$@" 2>&1 >/dev/null | grep '^flatio: load' | tail -1)
	t1=$(date +%s%N)
	l=$(sed -E 's/.*load ([0-9.]+) ms.*/\1/' <<<"$line")
	c=$(sed -E 's/.*compute ([0-9.]+) ms.*/\1/' <<<"$line")
	e=$(awk -v a="$t0" -v b="$t1" 'BEGIN{printf "%.1f", (b-a)/1e6}')
	bl=$(awk -v a="$bl" -v b="$l" 'BEGIN{print (b<a)?b:a}')
	bc=$(awk -v a="$bc" -v b="$c" 'BEGIN{print (b<a)?b:a}')
	be=$(awk -v a="$be" -v b="$e" 'BEGIN{print (b<a)?b:a}')
done
printf '%s -j%s best of %d: load %s ms, compute (warm) %s ms, e2e %s ms\n' "$(basename "$bin")" "$j" "$n" "$bl" "$bc" "$be"
