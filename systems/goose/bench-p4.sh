#!/usr/bin/env bash
# systems/goose/bench-p4.sh [rounds=5] [data dir]: problem 4, Goose interleaved with the C references (ref/p4/p4.c,
# p4par.c). Builds everything, then per round runs each variant once and records its numbers; prints one line per
# (cell, cores, kind): min / median / max over the rounds, plus the load seen.
#   warm: in-process reps (P4_REPS=20 after one warm-up); value = min rep of layout + emit (C: its "lay out" +
#         "emit"; Goose: the slowest worker's layout + emit). "goose-wall" is main's go-to-last-done per rep.
#   e2e:  wall of one process, one rep: start, read the flat files, compute, output, self-check against the oracle
#         (C sorts its relocations with qsort; Goose reads its output files back and sorts with std sort).
#         "e2e-nocheck": Goose without the check (its output is the two files written by the workers).
#   K variants: P4_K=k keeps the commonest k templates, the rest becomes glue runs (made at load, untimed).
set -u
cd "$(dirname "$0")"
R=${1:-5}
D=${2:-/home/exedev/paradigms/data/minio-p4}
G=${GOOSE:-/home/exedev/tools/goose/rel/goose-v2026.4-linux-x64/goose}
B=../../build
mkdir -p bin "$B"
$G build p4.goose -o bin/p4 >/dev/null 2>&1 || { echo "build p4 failed"; exit 1; }
gcc -O2 -march=native -o "$B/p4" ../../ref/p4/p4.c || exit 1
gcc -O2 -march=native -fopenmp -o "$B/p4par" ../../ref/p4/p4par.c || exit 1
export OMP_WAIT_POLICY=passive
run() { systemd-run --user --scope -q -p MemoryMax=10G "$@"; }     # Goose reserves 2 GB/stack: no prlimit --as
out=$(mktemp)
wall() { local t0 t1; t0=$(date +%s%N); "$@" >/dev/null 2>&1 || echo "FAIL $*" >&2; t1=$(date +%s%N); echo "scale=1; ($t1-$t0)/1000000" | bc; }
cw() { awk '{for(i=1;i<=NF;i++){if($i=="out"){l=$(i+1)} if($i=="emit"){e=$(i+1)}}} END{printf "%.1f\n", l+e}'; }    # C: lay out + emit
gw() { tr ' ' '\n' | awk '/^layout$/{getline; split($0,a,"/"); l=a[1]} /^emit$/{getline; split($0,a,"/"); e=a[1]} END{printf "%.1f\n", l+e}'; }
gwall() { grep -o 'rep wall [0-9.]*' | awk '{print $3}'; }
echo "goose p4 bench: $R rounds, $(date -u +%FT%TZ), load $(cut -d' ' -f1-3 /proc/loadavg)"
for r in $(seq "$R"); do
  l=$(cut -d' ' -f1 /proc/loadavg); echo "round $r $(date -u +%T) load $l" >&2; echo "load - - $l" >>"$out"
  echo "c-p4 1 warm $(P4_REPS=20 "$B/p4" "$D" | cw)" >>"$out"
  x=$(P4_REPS=20 run bin/p4 "$D" 1); echo "goose 1 warm $(echo "$x" | gw)" >>"$out"; echo "goose-wall 1 warm $(echo "$x" | gwall)" >>"$out"
  echo "c-p4par 8 warm $(OMP_NUM_THREADS=8 P4_REPS=20 "$B/p4par" "$D" | cw)" >>"$out"
  x=$(P4_REPS=20 run bin/p4 "$D" 8); echo "goose 8 warm $(echo "$x" | gw)" >>"$out"; echo "goose-wall 8 warm $(echo "$x" | gwall)" >>"$out"
  echo "c-p4par 1 warm $(OMP_NUM_THREADS=1 P4_REPS=20 "$B/p4par" "$D" | cw)" >>"$out"
  for k in 0 256 4096; do
    for w in 1 8; do echo "goose-K$k $w warm $(P4_K=$k P4_REPS=20 run bin/p4 "$D" $w | gw)" >>"$out"; done
  done
  echo "c-p4 1 e2e $(wall "$B/p4" "$D")" >>"$out"
  echo "goose 1 e2e $(wall run bin/p4 "$D" 1)" >>"$out"
  echo "goose 1 e2e-nocheck $(P4_NOCHECK=1 wall run bin/p4 "$D" 1)" >>"$out"
  echo "c-p4par 8 e2e $(OMP_NUM_THREADS=8 wall "$B/p4par" "$D")" >>"$out"
  echo "goose 8 e2e $(wall run bin/p4 "$D" 8)" >>"$out"
  echo "goose 8 e2e-nocheck $(P4_NOCHECK=1 wall run bin/p4 "$D" 8)" >>"$out"
done
echo "cell cores kind   min / median / max ms"
awk '{k=$1" "$2" "$3; v[k]=v[k]" "$4; if(!(k in seen)){seen[k]=1; order[++n]=k}}
  END{for(i=1;i<=n;i++){k=order[i]; m=split(v[k],a," "); for(x=1;x<=m;x++)for(y=x+1;y<=m;y++)if(a[y]+0<a[x]+0){t=a[x];a[x]=a[y];a[y]=t}
      printf "%-22s %s / %s / %s\n", k, a[1], a[int((m+1)/2)], a[m]}}' "$out"
rm -f "$out" /tmp/goose_p4_code.bin /tmp/goose_p4_rel.bin
