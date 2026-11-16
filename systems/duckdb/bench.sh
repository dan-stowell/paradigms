#!/bin/sh
# Rerun every DuckDB cell's timings: one line per (cell, threads, warm / e2e).
#   systems/duckdb/bench.sh [DATA] ; R=5 rounds (interleaved 1 and 8 threads), REPS=7 in-process reps per warm run
#   ONLY=p2: just the two P2 shapes (2prep + 2loop, the level-at-a-time loop; 2rprep + 2rec, the recursive CTEs)
# warm: the cell's statements only (tables already loaded), min/median/max over REPS reps in one process, then the
#       min/median/max of those medians over R rounds. e2e: the whole process (start, Arrow views over the flat files,
#       CREATE TABLE, compute, writing the output arrays), min/median/max over R rounds.
# DuckDB's scheduler has no passive/spin knob (its workers park on a semaphore after a short spin): see NOTES.md.
cd "$(dirname "$0")"
D=${1:-/home/exedev/paradigms/data/minio}
R=${R:-5}
REPS=${REPS:-7}
T=$(mktemp -d)
mkdir -p out
dk() { systemd-run --user --scope -q -p MemoryMax=10G ./dk "$@"; }
date -u +%Y-%m-%dT%H:%M:%SZ; uptime
for r in $(seq "$R"); do
	for t in 1 8; do
		export DK_QUIET=1 DK_REPS=$REPS
		[ "${ONLY:-}" = p2 ] || dk "$D" $t sql/load1.sql -- sql/p1.sql | grep '^cell' >>"$T/warm.$t"
		dk "$D" $t sql/load2.sql -- sql/p2prep.sql sql/p2loop.sql | grep '^cell' >>"$T/warm.$t"
		dk "$D" $t sql/load2.sql -- sql/p2rec.sql | grep '^cell' >>"$T/warm.$t"
		if [ "${ONLY:-}" = p2 ]; then
			export DK_REPS=1
			for p in "P2:sql/load2.sql sql/p2prep.sql sql/p2loop.sql" "P2rec:sql/load2.sql sql/p2rec.sql"; do
				# shellcheck disable=SC2086
				dk "$D" $t ${p#*:} | sed -n "s/^total process: \([0-9.]*\) ms/${p%%:*} \1/p" >>"$T/e2e.$t"
			done
			continue
		fi
		dk "$D" $t sql/load1.sql sql/p1.sql sql/load2.sql sql/p2prep.sql sql/p2aux.sql -- sql/p2eager.sql sql/p2loop.sql |
			grep '^cell' | sed 's/^cell 2loop/cell 2loop-eager/' | grep -v 'cell 1\|2prep\|2aux' >>"$T/warm.$t"
		dk "$D" $t sql/load1.sql sql/p1.sql sql/load2.sql sql/p2prep.sql sql/p2aux.sql -- sql/p2lazy.sql sql/p2loop.sql |
			grep '^cell' | sed 's/^cell 2loop/cell 2loop-lazy/' | grep -v 'cell 1\|2prep\|2lazy' >>"$T/warm.$t"
		dk "$D" $t sql/load3.sql -- sql/p3a.sql | grep '^cell' >>"$T/warm.$t"
		dk "$D" $t sql/load3b.sql sql/load3b_iso.sql -- sql/p3b.sql | grep '^cell' >>"$T/warm.$t"
		DK_THP=1 dk "$D" $t sql/load3b.sql sql/load3b_iso.sql -- sql/p3b.sql | grep '^cell' | sed 's/^cell 3b/cell 3b-hugepages/' >>"$T/warm.$t"
		export DK_REPS=1
		for p in "P1:sql/load1.sql sql/p1.sql" "P2:sql/load2.sql sql/p2prep.sql sql/p2loop.sql" "P2rec:sql/load2.sql sql/p2rec.sql" "P3a:sql/load3.sql sql/p3a.sql" \
			"P3b:sql/load3b.sql sql/load3b_iso.sql sql/p3b.sql" \
			"P1-P3:sql/load1.sql sql/p1.sql sql/load3.sql sql/p3a.sql sql/load3b.sql sql/p3link.sql sql/p3b.sql"; do
			# shellcheck disable=SC2086
			dk "$D" $t ${p#*:} | sed -n "s/^total process: \([0-9.]*\) ms/${p%%:*} \1/p" >>"$T/e2e.$t"
		done
	done
done
for t in 1 8; do
	# warm: per cell, the medians of each round
	sed -n 's/^cell \([^:]*\): min [0-9.]* median \([0-9.]*\) .*/\1 \2/p' "$T/warm.$t" |
		awk -v t=$t '{v[$1] = v[$1] " " $2} END {for (c in v) print c, t, v[c]}' | sort |
		while read -r c th vals; do
			echo "$vals" | tr ' ' '\n' | grep . | sort -n | awk -v c="$c" -v t="$th" '{x[NR]=$1} END {printf "%-16s %d threads warm  min %8.1f median %8.1f max %8.1f ms (%d rounds)\n", c, t, x[1], x[int((NR+1)/2)], x[NR], NR}'
		done
	awk '{v[$1] = v[$1] " " $2} END {for (c in v) print c, v[c]}' "$T/e2e.$t" | sort |
		while read -r c vals; do
			echo "$vals" | tr ' ' '\n' | grep . | sort -n | awk -v c="$c" -v t=$t '{x[NR]=$1} END {printf "%-16s %d threads e2e   min %8.1f median %8.1f max %8.1f ms (%d rounds)\n", c, t, x[1], x[int((NR+1)/2)], x[NR], NR}'
		done
done
grep -h 'minor faults' "$T"/warm.* | grep '^cell 3b' | sort | uniq | head -4
rm -rf "$T"
