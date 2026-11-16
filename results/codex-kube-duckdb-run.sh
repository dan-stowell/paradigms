#!/bin/bash
set -uo pipefail
export OMP_WAIT_POLICY=passive
cd /home/exedev/paradigms-w/systems/duckdb
D=/home/exedev/paradigms/data/kube-apiserver
LOG=/home/exedev/paradigms-w/results/codex-kube-duckdb
exec > >(tee "$LOG-raw.txt") 2>&1
check() {
 for f in "$@"; do
  echo "CHECK COMMAND: cmp out/$f.u32 $D/oracle/$f.u32"
  cmp "out/$f.u32" "$D/oracle/$f.u32"; echo "CHECK STATUS $cell $t $f $?"
  python3 - "$D/oracle/$f.u32" "out/$f.u32" <<'PY'
import sys,array
w=array.array('I',open(sys.argv[1],'rb').read());g=array.array('I',open(sys.argv[2],'rb').read())
for i,(x,y) in enumerate(zip(w,g)):
 if x!=y: print('FIRST DIFFERENCE u32 index',i,'want',x,'got',y);break
else:
 if len(w)!=len(g):print('FIRST DIFFERENCE length want',len(w),'got',len(g))
PY
 done
}
run() { echo "RUN COMMAND: ./dk $D $t $*"; ./dk "$D" "$t" "$@"; echo "RUN STATUS $cell $t $?"; }
date -u; uptime
for t in 1 8; do
 cell=P1; run sql/load1.sql sql/p1.sql; check p1_canon p1_builtin p1_reltarget p1c_canon
 cell=P2; run sql/load2.sql sql/p2prep.sql sql/p2loop.sql; check p2_live
 for c in eager lazy; do
  cell=P2-$c; run sql/load1.sql sql/p1.sql sql/load2.sql sql/p2prep.sql sql/p2aux.sql sql/p2$c.sql sql/p2loop.sql; check p2_live
 done
 cell=3a; run sql/load3.sql sql/p3a.sql; check p3_addr
 for c in isolated pipeline; do
  cell=3b-$c
  if [[ $c = isolated ]]; then run sql/load3b.sql sql/load3b_iso.sql sql/p3b.sql
  else run sql/load1.sql sql/p1.sql sql/load3.sql sql/p3a.sql sql/load3b.sql sql/p3link.sql sql/p3b.sql; fi
  echo "CHECK COMMAND: python3 -I check3b.py $D out/p3_image.bin"
  python3 -I check3b.py "$D" out/p3_image.bin; echo "CHECK STATUS $cell $t $?"
 done
done
date -u; uptime
R=5 REPS=5 ./bench.sh "$D" > "$LOG-bench.txt" 2>&1
cat "$LOG-bench.txt"
date -u; uptime
