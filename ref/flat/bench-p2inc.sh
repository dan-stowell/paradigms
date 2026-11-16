#!/usr/bin/env bash
# Interleaved timings for results/c-incremental.md: the maintained flood (p2inc, DRed with derivation-order supports
# and plain DRed) vs Differential Dataflow's update vs recomputing (p2, p2par), on both real MinIO edits.
#   ref/flat/bench-p2inc.sh [ROUNDS=5]
# Needs data/minio-edit{,2}/diff (systems/ddflow: `ddflow writediff data/minio data/minio-edit`).
set -euo pipefail
here=$(cd "$(dirname "$0")/../.." && pwd)
D=/home/exedev/paradigms/data
N=${1:-5}
B=$here/build
mkdir -p "$B"
export TMPDIR="$B"
gcc -O2 -march=native -o "$B/p2inc" "$here/ref/flat/p2inc.c"
gcc -O2 -march=native -o "$B/p2" "$here/ref/flat/p2.c"
gcc -O2 -march=native -fopenmp -o "$B/p2par" "$here/ref/flat/p2par.c"
export RUSTUP_HOME=/home/exedev/tools/rust/rustup CARGO_HOME=/home/exedev/tools/rust/cargo PATH=/home/exedev/tools/rust/cargo/bin:$PATH
export CARGO_TARGET_DIR=/home/exedev/tools/rust/target-ddflow
(cd "$here/systems/ddflow" && nice -n 10 cargo build --release -q)
DD=$CARGO_TARGET_DIR/release/ddflow
export OMP_WAIT_POLICY=passive
cap="prlimit --as=8589934592"
out=$(mktemp "$B/p2inc-bench.XXXXXX"); tmp=$(mktemp "$B/p2inc-run.XXXXXX")
rec() { printf '%s\t%s\t%s\n' "$1" "$2" "$3" >> "$out"; }
echo "start $(date -u +%FT%TZ) $(uptime)"
for r in $(seq "$N"); do
  for e in minio-edit minio-edit2; do
    for mode in order plain; do
      $cap "$B/p2inc" $D/minio $D/$e 5 $mode > "$tmp"; grep -q 'P2INC OK' "$tmp"
      for dir in 'A->B' 'B->A'; do
        awk -F'\t' -v d="update $dir" '$1 == d && $3 == "rep 0" {print $2+0}' "$tmp" | while read -r v; do rec "p2inc $mode $e $dir first update" 1 "$v"; done
        awk -F'\t' -v d="update $dir" '$1 == d && $3 != "rep 0" {print $2+0}' "$tmp" | while read -r v; do rec "p2inc $mode $e $dir later updates (4 per process)" 1 "$v"; done
      done
      [ "$r" = 1 ] && grep '^update' "$tmp" | sed -n '1,2p' | cut -f1,4 | sed "s/^/  counts $mode $e: /" >&2
    done
    for dir in 'A->B' 'B->A'; do
      dest=$e; [ "$dir" = 'B->A' ] && dest=minio
      $cap "$B/p2" "$D/$dest" > "$tmp"; grep -q 'P2 OK' "$tmp"
      rec "C p2 $e $dir recompute $dest (flood)" 1 "$(sed -n 's/.*flood \([0-9.]*\) ms.*/\1/p' "$tmp")"
      for c in 1 8; do
        OMP_NUM_THREADS=$c $cap "$B/p2par" "$D/$dest" 5 > "$tmp"; grep -q 'P2 OK' "$tmp"
        rec "C p2par $e $dir recompute $dest (best of 5)" $c "$(sed -n 's/.*flood best \([0-9.]*\) ms.*/\1/p' "$tmp")"
      done
    done
    for c in 1 8; do
      $cap $DD inc $D/minio $D/$e 5 -w $c > "$tmp"; grep -q 'P2 OK' "$tmp"
      awk -F'\t' '/^update A->B/ {print $2+0}' "$tmp" | while read -r v; do rec "DD $e A->B update" $c "$v"; done
      awk -F'\t' '/^update B->A/ {print $2+0}' "$tmp" | while read -r v; do rec "DD $e B->A update" $c "$v"; done
    done
  done
done
echo "end $(date -u +%FT%TZ) $(uptime)"
python3 -c '
import sys, statistics as st, collections
d = collections.OrderedDict()
for l in open(sys.argv[1]):
    c, w, v = l.rstrip("\n").split("\t"); d.setdefault((c, w), []).append(float(v))
for (c, w), v in sorted(d.items()):
    print(f"{c}\t{w} thr\tmin {min(v):.3f}\tmed {st.median(v):.3f}\tmax {max(v):.3f}\tms (n={len(v)})")
' "$out"
rm -f "$out" "$tmp"
