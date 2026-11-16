#!/usr/bin/env bash
# Interleaved timing of the flat C references (plain, tuned 1/8 threads), then hlink: ref/flat/bench.sh
cd ~/paradigms; d=data/minio
for r in 1 2 3; do
 echo "round $r $(date -u +%T) $(cut -d' ' -f1 /proc/loadavg)"
 build/p1 $d | grep 'ms'; build/p2 $d | grep ms
 for t in 1 8; do OMP_NUM_THREADS=$t build/p1par $d | grep ms; OMP_NUM_THREADS=$t build/p2par $d | grep ms; done
done
COLD=0 ref/hlink/bench.sh minio build/hlink 3 >/dev/null 2>&1; echo "hlink $(date -u +%T) $(cut -d' ' -f1 /proc/loadavg)"; grep -E 'resolve|deadcode|process' results/minio.hlink.tsv | grep -E '^(1|8)\s'
