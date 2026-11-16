#!/usr/bin/env bash
# systems/goose/bench.sh [rounds=5] [data dir]: rebuilds the Goose cells, then runs every cell ROUNDS times,
# interleaved, and prints one line per (cell, cores, warm/e2e): min / median / max ms over the rounds.
#   warm: each program repeats its kernel in-process (P1_REPS, P2REPS, P3_REPS) and reports its best (or min) rep;
#         the round's value is that.   e2e: wall time of one process with one rep (start, read flat files, compute,
#         write nothing; includes the self-check against the oracles).
# Goose's queues block on a pthread condition variable (no spinning wait), so there is no wait policy to set.
# Memory: Goose reserves 2 GB of address space per data stack, so `prlimit --as` kills it; the cap is a cgroup.
set -u
cd "$(dirname "$0")"
R=${1:-5}
D=${2:-/home/exedev/paradigms/data/minio}
G=${GOOSE:-/home/exedev/tools/goose/rel/goose-v2026.4-linux-x64/goose}
mkdir -p bin
for p in p1 p1par p2 p2lazy p2par p3 p3par; do $G build $p.goose -o bin/$p >/dev/null 2>&1 || { echo "build $p failed"; exit 1; }; done
run() { systemd-run --user --scope -q -p MemoryMax=8G "$@"; }
out=$(mktemp)
wall() { local t0 t1; t0=$(date +%s%N); "$@" >/dev/null 2>&1 || echo "FAIL $*" >&2; t1=$(date +%s%N); echo "scale=1; ($t1-$t0)/1000000" | bc; }
num() { grep -o "$1 [0-9.]*" | head -1 | awk '{print $NF}'; }      # the number after a label
first() { grep -o "$1 [0-9.]*/" | head -1 | tr -d / | awk '{print $NF}'; }   # min of a min/median/max triple
echo "goose bench: $R rounds, $(date -u +%FT%TZ), load $(cut -d' ' -f1-3 /proc/loadavg)"
for r in $(seq "$R"); do
  echo "round $r $(date -u +%T) load $(cut -d' ' -f1 /proc/loadavg)" >&2
  x=$(P1_REPS=5 run bin/p1 "$D")
  echo "p1a 1 warm $(echo "$x" | first '1a join by name')" >>"$out"
  echo "p1b 1 warm $(echo "$x" | first '1b relocation targets')" >>"$out"
  echo "p1c 1 warm $(echo "$x" | first '1c content-hash join')" >>"$out"
  echo "p1 1 e2e $(wall run bin/p1 "$D")" >>"$out"
  x=$(P1_REPS=5 run bin/p1par "$D" 8)
  echo "p1a 8 warm $(echo "$x" | first ' 1a')" >>"$out"
  echo "p1b 8 warm $(echo "$x" | first ' 1b')" >>"$out"
  echo "p1c 8 warm $(echo "$x" | first ' 1c')" >>"$out"
  echo "p1 8 e2e $(wall run bin/p1par "$D" 8)" >>"$out"
  echo "p2-csr 1 warm $(P2REPS=10 run bin/p2par "$D" 1 | num 'best flood')" >>"$out"
  echo "p2-csr 1 e2e $(wall run bin/p2 "$D")" >>"$out"
  echo "p2-lazy 1 warm $(run bin/p2lazy "$D" | num 'best of 5')" >>"$out"
  echo "p2-lazy 1 e2e $(wall run bin/p2lazy "$D")" >>"$out"
  echo "p2-csr 8 warm $(P2REPS=20 run bin/p2par "$D" 8 | num 'best flood')" >>"$out"
  echo "p2-csr 8 e2e $(P2REPS=1 wall run bin/p2par "$D" 8)" >>"$out"
  echo "p2-lazy 8 warm $(P2REPS=20 run bin/p2par "$D" 8 lazy | num 'best flood')" >>"$out"
  echo "p2-lazy 8 e2e $(P2REPS=1 wall run bin/p2par "$D" 8 lazy)" >>"$out"
  x=$(run bin/p3 "$D")
  echo "p3a 1 warm $(echo "$x" | num '3a layout')" >>"$out"
  echo "p3b-4k 1 warm $(P3_REPS=20 run bin/p3 "$D" | grep -o 'x20) [0-9.]*' | awk '{print $2}')" >>"$out"
  echo "p3b-2m 1 warm $(P3_REPS=20 P3_THP=1 run bin/p3 "$D" | grep -o 'x20) [0-9.]*' | awk '{print $2}')" >>"$out"
  echo "p3 1 e2e $(wall run bin/p3 "$D")" >>"$out"
  echo "p3 1 e2e-pipeline $(wall run bin/p3 "$D" --pipeline)" >>"$out"
  x=$(run bin/p3par "$D" 8)
  echo "p3a 8 warm $(echo "$x" | num ' 3a')" >>"$out"
  echo "p3b-4k 8 warm $(P3_REPS=20 run bin/p3par "$D" 8 | grep -o 'x20) [0-9.]*' | awk '{print $2}')" >>"$out"
  echo "p3b-2m 8 warm $(P3_REPS=20 P3_THP=1 run bin/p3par "$D" 8 | grep -o 'x20) [0-9.]*' | awk '{print $2}')" >>"$out"
  echo "p3b-gather 8 warm $(echo "$x" | num 'segments into it')" >>"$out"
  echo "p3 8 e2e $(wall run bin/p3par "$D" 8)" >>"$out"
done
echo "cell cores kind   min / median / max ms"
awk '{k=$1" "$2" "$3; v[k]=v[k]" "$4; if(!(k in seen)){seen[k]=1; order[++n]=k}}
  END{for(i=1;i<=n;i++){k=order[i]; m=split(v[k],a," "); for(x=1;x<=m;x++)for(y=x+1;y<=m;y++)if(a[y]+0<a[x]+0){t=a[x];a[x]=a[y];a[y]=t}
      printf "%-24s %8.1f / %8.1f / %8.1f\n", k, a[1], a[int((m+1)/2)], a[m]}}' "$out"
echo "done $(date -u +%FT%TZ), load $(cut -d' ' -f1-3 /proc/loadavg)"
rm -f "$out"
