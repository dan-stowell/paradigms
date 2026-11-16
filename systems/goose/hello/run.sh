#!/bin/bash
# Run the oracle, C reference, Goose 1-thread and Goose N-thread, 3 times each. Timings go to stderr.
TOOLS=${TOOLS:-/home/exedev/tools/goose}
F=$TOOLS/u32_50M.bin
date -u
cat "$TOOLS/oracle.txt"
for i in 1 2 3; do "$TOOLS/c_ref" "$F"; done
for i in 1 2 3; do "$TOOLS/hello1" "$F"; done
for t in 1 2 4 8; do for i in 1 2 3; do "$TOOLS/hello8" "$F" $t; done; done
