#!/usr/bin/env bash
set -euo pipefail
cd "$(dirname "$0")/../.."
d=${1:-/home/exedev/paradigms/data/minio-p4}
export OMP_WAIT_POLICY=passive FLAT_HUGE=0 P4_HUGE=1 P4_REPS=${P4_REPS:-256}
unset P4_NOPATCH OMP_THREAD_LIMIT P4_COUNTS P4_FAULT
# All eight visible CPUs are required for the eight-worker comparison. Single
# worker runs use CPU 6; binding workers prevents migration, not host contention.
export OMP_PROC_BIND=true OMP_PLACES=cores
for round in 1 2 3 4 5; do
    for t in 1 8; do
        export OMP_NUM_THREADS=$t
        cpus=0-7; [[ $t == 1 ]] && cpus=6
        # Rotate order so no variant is always first in a round.
        variants=(p4par B H1 H2 H0)
        for ((i=0;i<5;i++)); do
            v=${variants[$(((i+round-1)%5))]}
            echo "ROUND $round threads $t variant $v taskset $cpus"
            date -u +%FT%TZ; uptime
            case $v in
                p4par) args=(build/p4par "$d");;
                B) args=(build/p4runs "$d" B);;
                *) args=(build/p4holes "$d" "$v");;
            esac
            taskset -c "$cpus" prlimit --as=12884901888 "${args[@]}"
        done
    done
done
date -u +%FT%TZ; uptime
