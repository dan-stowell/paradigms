#!/bin/bash
# usage: run.sh [datadir]   (data generated once: 50M-element idx + 16M-element table)
set -euo pipefail
cd "$(dirname "$0")"; B=$PWD/build
D=${1:-/home/exedev/tools/halide/data}; mkdir -p "$D"
[ -f "$D/idx.u32" ] || "$B/mkdata" "$D"
date -u +"%Y-%m-%dT%H:%M:%SZ"
for t in 1 8; do HL_NUM_THREADS=$t "$B/driver" "$D/idx.u32" "$D/table.u32"; echo; done
