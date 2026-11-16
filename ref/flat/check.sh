#!/usr/bin/env bash
# Build the plain C references and check each problem against its oracle:  ref/flat/check.sh [data/minio]
set -euo pipefail
here=$(cd "$(dirname "$0")/../.." && pwd); d=${1:-$here/data/minio}
build=${BUILD_DIR:-$here/build-g}
mkdir -p "$build"
for p in p1 p2 p3; do gcc -O2 -march=native -o "$build/$p" "$here/ref/flat/$p.c"; done
for p in p1par p2par p2pipe p2lazy; do gcc -O2 -march=native -fopenmp -o "$build/$p" "$here/ref/flat/$p.c"; done
gcc -O2 -march=native -pthread -o "$build/p2async" "$here/ref/flat/p2async.c"
gcc -O2 -march=native -fopenmp -o "$build/p3par" "$here/ref/flat/p3.c"
ok=0
export OMP_WAIT_POLICY=${OMP_WAIT_POLICY:-passive} # spinning barriers collapse ~100x when the 8 cores are shared (JOURNAL 16:20Z)
for p in p1 p2 p3 p1par p2par p2async p2pipe p2lazy p3par; do "$build/$p" "$d" || ok=1; done
"$build/p3" "$d" --pipeline | tail -1 || ok=1
exit $ok
