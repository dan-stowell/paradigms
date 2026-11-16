#!/usr/bin/env bash
# Build the plain C references and check each problem against its oracle:  ref/flat/check.sh [data/minio]
set -euo pipefail
here=$(cd "$(dirname "$0")/../.." && pwd); d=${1:-$here/data/minio}
mkdir -p "$here/build"
for p in p1 p2 p3; do gcc -O2 -march=native -o "$here/build/$p" "$here/ref/flat/$p.c"; done
for p in p1par p2par p2pipe p2lazy; do gcc -O2 -march=native -fopenmp -o "$here/build/$p" "$here/ref/flat/$p.c"; done
gcc -O2 -march=native -fopenmp -o "$here/build/p3par" "$here/ref/flat/p3.c"
ok=0
for p in p1 p2 p3 p1par p2par p2pipe p2lazy p3par; do "$here/build/$p" "$d" || ok=1; done
"$here/build/p3" "$d" --pipeline | tail -1 || ok=1
exit $ok
