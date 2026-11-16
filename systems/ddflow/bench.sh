#!/usr/bin/env bash
# Rerun every ddflow timing next to the C references, interleaved round-robin in one window, N rounds (default 5):
#   systems/ddflow/bench.sh [N] [EDITS...]      (EDITS: data dirs edited from minio; default minio-edit minio-edit2 if present)
# Prints one line per (cell, workers, measure): min / median / max ms over the rounds, then `uptime`.
set -euo pipefail
here=$(cd "$(dirname "$0")/../.." && pwd)
D=/home/exedev/paradigms/data
N=${1:-5}; shift || true
EDITS=("$@"); [ ${#EDITS[@]} -gt 0 ] || for e in minio-edit minio-edit2; do [ -d $D/$e ] && EDITS+=($e); done
export RUSTUP_HOME=/home/exedev/tools/rust/rustup CARGO_HOME=/home/exedev/tools/rust/cargo PATH=/home/exedev/tools/rust/cargo/bin:$PATH
export CARGO_TARGET_DIR=/home/exedev/tools/rust/target-ddflow
(cd "$here/systems/ddflow" && nice -n 10 cargo build --release -q)
B=$CARGO_TARGET_DIR/release/ddflow
mkdir -p "$here/build"
gcc -O2 -march=native -o "$here/build/p2" "$here/ref/flat/p2.c"
gcc -O2 -march=native -fopenmp -o "$here/build/p2par" "$here/ref/flat/p2par.c"
export OMP_WAIT_POLICY=passive
cap="prlimit --as=12884901888"
out=$(mktemp)
rec() { echo "$1	$2	$3" >> "$out"; }                     # cell, workers, ms
wall() { local t0=$(date +%s%N); "$@" > "$tmp" 2>&1; echo $(( ($(date +%s%N) - t0) / 1000000 )); }
tmp=$(mktemp)
echo "start $(date -u +%FT%TZ) $(uptime)"
for r in $(seq "$N"); do
  for w in minio kube-apiserver; do
    ms=$(wall $cap "$here/build/p2" $D/$w); grep -q 'P2 OK' "$tmp"
    rec "C p2 $w flood (warm)" 1 "$(sed -n 's/.*flood \([0-9.]*\) ms.*/\1/p' "$tmp")"; rec "C p2 $w e2e" 1 "$ms"
    for c in 1 8; do
      OMP_NUM_THREADS=$c $cap "$here/build/p2par" $D/$w 5 > "$tmp"; grep -q 'P2 OK' "$tmp"
      rec "C p2par $w flood (warm, best of 5 in-process)" $c "$(sed -n 's/.*flood best \([0-9.]*\) ms.*/\1/p' "$tmp")"
    done
    for c in 1 8; do
      ms=$(wall $cap $B p2 $D/$w -w $c); grep -q 'P2 OK' "$tmp"
      rec "dd p2 $w full (warm: facts in memory)" $c "$(awk -F'\t' '/^full/ {print $2+0}' "$tmp")"; rec "dd p2 $w e2e" $c "$ms"
      $cap $B p2 $D/$w phased -w $c > "$tmp"; grep -q 'P2 OK' "$tmp"
      rec "dd p2 $w index phase (facts, no roots)" $c "$(awk -F'\t' '/^index/ {print $2+0}' "$tmp")"
      rec "dd p2 $w flood phase (roots into indexed facts)" $c "$(awk -F'\t' '/^flood/ {print $2+0}' "$tmp")"
    done
  done
  for e in "${EDITS[@]}"; do
    ms=$(wall $cap "$here/build/p2" $D/$e); grep -q 'P2 OK' "$tmp"
    rec "C p2 $e flood (recompute)" 1 "$(sed -n 's/.*flood \([0-9.]*\) ms.*/\1/p' "$tmp")"
    for c in 1 8; do
      OMP_NUM_THREADS=$c $cap "$here/build/p2par" $D/$e 5 > "$tmp"; grep -q 'P2 OK' "$tmp"
      rec "C p2par $e flood (recompute, best of 5)" $c "$(sed -n 's/.*flood best \([0-9.]*\) ms.*/\1/p' "$tmp")"
      $cap $B inc $D/minio $D/$e 5 -w $c > "$tmp"; grep -q 'P2 OK' "$tmp"
      rec "dd inc minio->$e full A" $c "$(awk -F'\t' '/^full A/ {print $2+0}' "$tmp")"
      rec "dd inc minio->$e full B from scratch" $c "$(awk -F'\t' '/^full B/ {print $2+0}' "$tmp")"
      awk -F'\t' '/^update A->B/ {print $2+0}' "$tmp" | while read -r v; do rec "dd inc minio->$e update A->B (5 per process)" $c "$v"; done
      awk -F'\t' '/^update B->A/ {print $2+0}' "$tmp" | while read -r v; do rec "dd inc minio->$e update B->A (5 per process)" $c "$v"; done
    done
  done
done
echo "end $(date -u +%FT%TZ) $(uptime)"
python3 -c '
import sys, statistics as st, collections
d = collections.OrderedDict()
for l in open(sys.argv[1]):
    c, w, v = l.rstrip("\n").split("\t"); d.setdefault((c, w), []).append(float(v))
for (c, w), v in d.items():
    print(f"{c}\t{w} cores\tmin {min(v):.1f}\tmed {st.median(v):.1f}\tmax {max(v):.1f}\tms (n={len(v)})")
' "$out"
rm -f "$out" "$tmp"
