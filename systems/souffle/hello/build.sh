#!/bin/bash
# Build everything: data generator, compiled Datalog (TSV input), load-only variant.
set -e
cd "$(dirname "$0")"
mkdir -p data
[ -x gen ] || g++ -O2 -o gen gen.cpp
[ -f data/edge.bin ] || ./gen data
souffle -o reach_c -j8 reach.dl
souffle -o load_c -j8 load.dl
if [ -f embed.cpp ]; then
  souffle -g reach_emb.cpp reach_emb.dl 2>/dev/null
  g++ -O3 -march=native -std=c++17 -fopenmp -D__EMBEDDED_SOUFFLE__ -I/usr/include/souffle \
      -o reach_emb embed.cpp -lz -lsqlite3 -lncurses -lffi -ldl
fi
