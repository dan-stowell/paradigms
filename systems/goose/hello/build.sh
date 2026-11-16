#!/bin/bash
# Build everything for the Goose hello-world. Needs the release in $TOOLS (see ../NOTES.md).
set -e
HERE="$(cd "$(dirname "$0")" && pwd)"
TOOLS=${TOOLS:-/home/exedev/tools/goose}
GOOSE=${GOOSE:-$TOOLS/rel/goose-v2026.4-linux-x64/goose}
gcc -O2 -o "$TOOLS/gen" "$HERE/gen.c"
gcc -O2 -o "$TOOLS/c_ref" "$HERE/c_ref.c"
"$GOOSE" build "$HERE/hello1.goose" -o "$TOOLS/hello1"
"$GOOSE" build "$HERE/hello8.goose" -o "$TOOLS/hello8"
[ -f "$TOOLS/u32_50M.bin" ] || "$TOOLS/gen" 50000000 "$TOOLS/u32_50M.bin" | tee "$TOOLS/oracle.txt"
