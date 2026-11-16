#!/bin/sh
# build dk (the C API host) against libduckdb v1.5.5 from the GitHub release (see NOTES.md, install)
set -e
D=${DUCKDB_LIB:-$HOME/tools/duckdb}
cd "$(dirname "$0")"
gcc -O2 -Wall -Wno-format-truncation -o dk dk.c -I"$D" -L"$D" -lduckdb -Wl,-rpath,"$D"
