#!/usr/bin/env bash
# Compile a Soufflé program with binary flat-file IO:  build.sh prog.dl [souffle flags...]  -> build/prog
# souffle -g emits C++; one sed points its readers/writers at flatio.h (which bulk-loads the B-tree indexes);
# the generated main is kept, so build/prog takes Soufflé's usual -j N, -F factdir, -D outdir.
set -euo pipefail
here=$(cd "$(dirname "$0")" && pwd)
dl=$1; shift
name=${OUTNAME:-$(basename "$dl" .dl)}
mkdir -p "$here/build"
cd "$here/build"
souffle -j8 "$@" -g "$name.gen.cpp" "$here/$dl"
sed -e 's|#include <any>|#include <any>\n#include "flatio.h"|' \
    -e 's|IOSystem::getInstance().getReader(directiveMap, symTable, recordTable)->readAll(\*\([a-zA-Z0-9_]*\))|flatio::readAll(directiveMap, symTable, recordTable, *\1)|' \
    -e 's|IOSystem::getInstance().getWriter(directiveMap, symTable, recordTable)->writeAll(\*\([a-zA-Z0-9_]*\))|flatio::writeAll(directiveMap, symTable, recordTable, *\1)|' \
    "$name.gen.cpp" > "$name.cpp"
grep -q 'flatio::readAll\|flatio::writeAll' "$name.cpp" || { echo "build.sh: sed found no IO calls" >&2; exit 1; }
g++ -O3 -march=native -std=c++17 -fopenmp -DRAM_DOMAIN_SIZE=${DOMAIN:-64} -DUSE_NCURSES -DUSE_LIBZ -DUSE_SQLITE -I"$here" -I/usr/include/souffle -w \
    -o "$name" "$name.cpp" -lz -lsqlite3 -lncurses -lffi -ldl
echo "built $here/build/$name"
