#!/usr/bin/env bash
# bench.sh [rounds=5] [data dir] (ONLY='3a|3a-rank' runs just those cells; ONLY='4|4-tuned|p4c' just problem 4): rebuild nothing; rerun every Soufflé cell, 1 and 8 threads, interleaved, and print
# one line per (cell, threads, warm|e2e) with min/median/max ms over the rounds, after checking each output
# against its oracle. warm = flatio's "compute" (rules only: inputs already bulk-loaded into the B-trees, output
# writing excluded); e2e = process start to exit (reading the flat files, building the indexes, rules, writing).
# 8 threads run with OMP_WAIT_POLICY=passive (spinning barriers lose ~100x on a loaded machine; RESUME.md).
# Needs: ./build.sh for each .dl below, gcc -O2 -o build/check_p3 check_p3.c, build/prep_lazy (prep_lazy.c) run once:
#   build/prep_lazy /home/exedev/paradigms/data/minio build/lazyfacts
# Problem 4 (data P4=/home/exedev/paradigms/data/minio-p4): ./build.sh emit.dl; ./build.sh emit2.dl;
#   gcc -O2 -o build/check_p4 check_p4.c; and ref/p4/check.sh (builds the C references build/p4, build/p4par at the
#   repo root). The pseudo-cell p4c runs those C references in the same rounds: warm = their "lay out" + "emit" (best
#   of P4_REPS=5 in the process), e2e = the process (it also sorts and checks the relocations, so it overstates).
set -uo pipefail
here=$(cd "$(dirname "$0")" && pwd)
D=${2:-${DATA:-/home/exedev/paradigms/data/minio}}
P4=${P4:-/home/exedev/paradigms/data/minio-p4}
root=$(cd "$here/../.." && pwd)
O=$here/build/bench
mkdir -p "$O"
n=${1:-5}
export OMP_WAIT_POLICY=passive
# cell  binary  factdir  check
cells=(
	"1a resolve_a $D cmp -s $O/p1_canon.u32 $D/oracle/p1_canon.u32 && cmp -s $O/p1_builtin.u32 $D/oracle/p1_builtin.u32"
	"1b resolve_b $D cmp -s $O/p1_reltarget.u32 $D/oracle/p1_reltarget.u32"
	"1c resolve_c $D cmp -s $O/p1c_canon.u32 $D/oracle/p1c_canon.u32"
	"1c-slot resolve_c2 $D cmp -s $O/p1c_canon.u32 $D/oracle/p1c_canon.u32"
	"2 reach $D cmp -s $O/p2_live.u32 $D/oracle/p2_live.u32"
	"2-lazy reach_lazy $here/build/lazyfacts cmp -s $O/p2_live.u32 $D/oracle/p2_live.u32"
	"3a layout $D $here/build/check_p3 $D $D/oracle/p3_image.bin $O/p3_addr.u32"
	"3a-rank layout_rank $D $here/build/check_p3 $D $D/oracle/p3_image.bin $O/p3_addr.u32"
	"3b relocate $D $here/build/check_p3 $D $O/p3_image.out"
	"4 emit $P4 $here/build/check_p4 $P4 $O"
	"4-tuned emit2 $P4 $here/build/check_p4 $P4 $O"
)
tmp=$(mktemp -d)
echo "# $(date -u +%FT%TZ) $(uptime)"
for ((r = 0; r < n; r++)); do
	for c in "${cells[@]}"; do
		read -r cell bin fd check <<<"$c"
		[[ -n "${ONLY:-}" && ! "$cell" =~ ^(${ONLY})$ ]] && continue
		for j in 1 8; do
			t0=$(date +%s%N)
			line=$(systemd-run --user --scope -q -p MemoryMax=10G "$here/build/$bin" -j$j -F "$fd" -D "$O" 2>&1 >/dev/null | grep '^flatio:')
			t1=$(date +%s%N)
			if ! eval "${check}" >/dev/null; then echo "$cell -j$j: CHECK FAILED" >&2; fi
			sed -nE 's/.*compute ([0-9.]+) ms.*/\1/p' <<<"$line" >>"$tmp/$cell.$j.warm"
			awk -v a="$t0" -v b="$t1" 'BEGIN{printf "%.1f\n", (b-a)/1e6}' >>"$tmp/$cell.$j.e2e"
			sed -nE 's/.*assemble min ([0-9.]+) .*/\1/p' <<<"$line" >>"$tmp/$cell.$j.assemble"
		done
	done
	# problem 4's C references, in the same round
	if [[ -z "${ONLY:-}" || "p4c" =~ ^(${ONLY})$ ]]; then
		for v in "p4c-plain 1 $root/build/p4" "p4c-par 1 $root/build/p4par" "p4c-par 8 $root/build/p4par"; do
			read -r cell j bin <<<"$v"
			t0=$(date +%s%N)
			line=$(OMP_NUM_THREADS=$j P4_REPS=5 prlimit --as=12884901888 "$bin" "$P4" 2>&1)
			t1=$(date +%s%N)
			grep -q '^P4 OK' <<<"$line" || echo "$cell -j$j: CHECK FAILED" >&2
			sed -nE 's/.*lay out ([0-9.]+) ms, emit ([0-9.]+) ms.*/\1 \2/p' <<<"$line" | awk '{printf "%.1f\n", $1 + $2}' >>"$tmp/$cell.$j.warm"
			awk -v a="$t0" -v b="$t1" 'BEGIN{printf "%.1f\n", (b-a)/1e6}' >>"$tmp/$cell.$j.e2e"
		done
	fi
	echo "# round $r done $(date -u +%FT%TZ) $(uptime)"
done
cells+=("p4c-plain" "p4c-par")
for c in "${cells[@]}"; do
	read -r cell _ <<<"$c"
	for j in 1 8; do
		for k in warm e2e assemble; do
			f=$tmp/$cell.$j.$k
			[ -s "$f" ] || continue
			sort -g "$f" | awk -v c="$cell" -v j="$j" -v k="$k" '{v[NR]=$1} END{printf "%-8s -j%s %-8s min %8.1f  median %8.1f  max %8.1f ms  (%d runs)\n", c, j, k, v[1], v[int((NR+1)/2)], v[NR], NR}'
		done
	done
done
rm -rf "$tmp"
