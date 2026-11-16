#!/bin/bash
# ATTACKS 3 in Halide (results/halide-levelfree.md): the level-per-call flood against k levels per call, interleaved.
#   systems/halide/levelfree.sh [ROUNDS [DATA]]     (build first: systems/halide/cells/build.sh p2)
#   systems/halide/levelfree.sh counts [DATA]       calls, per-call cost, empty-call cost per variant
# Each round runs one process per configuration (bench.h: min over 5 runs of >= 1 s, in-process); prints one line
# per (round, configuration) and then min / median / max over rounds. ref/flat/p2par (OpenMP, passive wait) runs
# in the same rounds. Memory capped with systemd-run (prlimit --as makes the Halide drivers segfault).
set -u
here=$(cd "$(dirname "$0")" && pwd)
B=$here/cells/build
if [ "${1:-}" = counts ]; then
    D=${2:-/home/exedev/paradigms/data/minio}
    echo "# $(date -u +%FT%TZ) $(uptime)"
    for cfg in "f_f 1 0 1024" "f_f_e1 1 0 1024" "f_f_e2 1 0 4096" "f_f_e4 1 0 4096" "f_f_e8 1 0 4096" "f_f_e4 1 1 256" "f_f_e8 1 1 256" \
               "f_t 8 0 1024" "f_t_e1 8 0 1024" "f_t_e4 8 0 4096" "f_t_e8 8 0 4096" "f_t_e4 8 1 256" "t_f 1 0 1024" "t_t 8 0 1024"; do
        set -- $cfg
        n=$(grep -n "\"flood_$1\"" $B/p2_variants.h | head -1 | cut -d: -f1); v=$((n - $(grep -n '"flood_t_f"' $B/p2_variants.h | cut -d: -f1)))
        echo "== flood_$1 threads=$2 hyb=$3 cap=$4"
        HL_NUM_THREADS=$2 P2_HYB=$3 P2_CAP=$4 systemd-run --user --scope -q -p MemoryMax=12G $B/p2 $D count $v | grep -vE "^non"
    done
    exit 0
fi
R=${1:-5}
D=${2:-/home/exedev/paradigms/data/minio}
T=$(mktemp -d)
gcc -O2 -march=native -fopenmp -o $T/p2par $here/../../ref/flat/p2par.c
# name threads hyb cap
cfgs=("f_f 1 0 1024" "f_f_e1 1 0 1024" "f_f_e2 1 0 4096" "f_f_e4 1 1 256" "f_f_e8 1 0 4096" "t_f 1 0 1024" "t_f_e4 1 1 256"
      "f_t 8 0 1024" "f_t_e1 8 0 1024" "f_t_e2 8 0 4096" "f_t_e4 8 1 256" "f_t_e8 8 0 4096" "t_t 8 0 1024" "t_t_e4 8 1 256")
out=$T/runs
for r in $(seq 1 $R); do
    echo "# round $r $(date -u +%FT%TZ) $(uptime)"
    for cfg in "${cfgs[@]}"; do
        set -- $cfg
        ms=$(HL_NUM_THREADS=$2 P2_HYB=$3 P2_CAP=$4 systemd-run --user --scope -q -p MemoryMax=12G $B/p2 $D flood_$1 | awk '/^2 flood/ {print $4} /P2 FAIL/ {print "FAIL"}' | tr '\n' ' ')
        echo "flood_$1 threads=$2 hyb=$3 cap=$4 $ms" | tee -a $out
    done
    for t in 1 8; do
        ms=$(OMP_NUM_THREADS=$t OMP_WAIT_POLICY=passive $T/p2par $D 20 | awk '/flood best/ {print $6}')
        echo "p2par threads=$t hyb=- cap=- $ms" | tee -a $out
    done
done
echo "# summary (ms, min / median / max over $R rounds)"
awk '{k=$1" "$2" "$3" "$4; v[k]=v[k]" "$5} END {for (k in v) print k, v[k]}' $out | sort | while read a b c d rest; do
    s=$(echo $rest | tr ' ' '\n' | sort -g | tr '\n' ' ')
    n=$(echo $s | wc -w); set -- $s
    eval "md=\${$(( (n + 1) / 2 ))}"; eval "mx=\${$n}"
    echo "$a $b $c $d  $1 / $md / $mx"
done
rm -rf $T
