#!/usr/bin/env bash
# levelfree.sh: ATTACKS.md 3 for Goose: the bulk-synchronous owner-partitioned flood (p2par) against the
# asynchronous one (p2async: no supersteps, termination by message counts), interleaved.
#   systems/goose/levelfree.sh [ROUNDS=5] [DATA]
# Each round runs every configuration once: one process, P2REPS floods, its best flood; prints min / median / max
# over the rounds, then each configuration's counts (supersteps, batches, ids) from its last run.
set -u
cd "$(dirname "$0")"
R=${1:-5}
D=${2:-/home/exedev/paradigms/data/minio}
G=${GOOSE:-/home/exedev/tools/goose/rel/goose-v2026.4-linux-x64/goose}
mkdir -p bin
for p in p2par p2async; do $G build $p.goose -o bin/$p >/dev/null 2>&1 || { echo "build $p failed"; exit 1; }; done
run() { systemd-run --user --scope -q -p MemoryMax=8G "$@"; }
CFGS=${CFGS:-"bsp-1 p2par 1 -
bsp-8 p2par 8 -
async-1 p2async 1 -
async-8 p2async 8 -
async-8-f4k p2async 8 4096
bsp-8-lazy p2par 8 - lazy
async-8-lazy p2async 8 - lazy"}
out=$(mktemp)
cnt=$(mktemp)
echo "# goose levelfree: $R rounds, $(date -u +%FT%TZ), load $(cut -d' ' -f1-3 /proc/loadavg)"
for r in $(seq "$R"); do
  while read name prog w flush mode; do
    if [ "$flush" = - ]; then x=$(P2REPS=${P2REPS:-20} run bin/$prog "$D" $w ${mode:-} 2>&1)
    else x=$(P2REPS=${P2REPS:-20} P2FLUSH=$flush run bin/$prog "$D" $w ${mode:-} 2>&1); fi
    echo "$x" | grep -q "P2 OK" || echo "FAIL $name: $x" >&2
    echo "$name $(echo "$x" | grep -o 'best flood [0-9.]*' | awk '{print $3}')" >>"$out"
    echo "$name: $(echo "$x" | grep -o '[0-9]* supersteps.*live' | head -1)" >>"$cnt"
  done <<< "$CFGS"
  echo "# round $r $(date -u +%T) load $(cut -d' ' -f1 /proc/loadavg)"
done
awk '{k=$1; v[k]=v[k]" "$2; if(!(k in s)){s[k]=1; o[++n]=k}}
  END{for(i=1;i<=n;i++){k=o[i]; m=split(v[k],a," "); for(x=1;x<=m;x++)for(y=x+1;y<=m;y++)if(a[y]+0<a[x]+0){t=a[x];a[x]=a[y];a[y]=t}
      printf "%-14s best flood min / median / max %6.1f / %6.1f / %6.1f ms\n", k, a[1], a[int((m+1)/2)], a[m]}}' "$out"
tail -n "$(echo "$CFGS" | wc -l)" "$cnt"
echo "# end $(date -u +%FT%TZ), load $(cut -d' ' -f1-3 /proc/loadavg)"
rm -f "$out" "$cnt"
