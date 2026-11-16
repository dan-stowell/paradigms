#!/usr/bin/env bash
set -uo pipefail
export OMP_WAIT_POLICY=passive
cd /home/exedev/paradigms-w/systems/souffle
D=/home/exedev/paradigms/data/kube-apiserver
LOG=/home/exedev/paradigms-w/results/codex-kube-souffle
exec > >(tee "$LOG-raw.txt") 2>&1
date -u; uptime
build/prep_lazy "$D" build/lazyfacts || exit 1
mkdir -p out
for j in 1 8; do
for p in resolve_a resolve_b resolve_c resolve_c2 reach reach_lazy layout relocate; do
  F=$D; [[ $p = reach_lazy ]] && F=build/lazyfacts
  echo "CHECK COMMAND: build/$p -j$j -F $F -D out"
  build/$p -j$j -F "$F" -D out
  rc=$?; echo "RUN STATUS $p $j $rc"
  case $p in
    resolve_a) fs='p1_canon p1_builtin';;
    resolve_b) fs=p1_reltarget;;
    resolve_c*) fs=p1c_canon;;
    reach*) fs=p2_live;;
    layout) fs=p3_addr;;
    relocate) fs=;;
  esac
  for f in $fs; do
    echo "CHECK COMMAND: cmp out/$f.u32 $D/oracle/$f.u32"
    cmp "out/$f.u32" "$D/oracle/$f.u32"; echo "CHECK STATUS $p $j $f $?"
    python3 - "$D/oracle/$f.u32" "out/$f.u32" <<'PY'
import sys,array
w=array.array('I');g=array.array('I')
for a,p in [(w,sys.argv[1]),(g,sys.argv[2])]:
 with open(p,'rb') as f:a.frombytes(f.read())
for i,(x,y) in enumerate(zip(w,g)):
 if x!=y: print('FIRST DIFFERENCE u32 index',i,'want',x,'got',y);break
else:
 if len(w)!=len(g):print('FIRST DIFFERENCE length want',len(w),'got',len(g))
PY
  done
  if [[ $p = relocate ]]; then
    echo "CHECK COMMAND: build/check_p3 $D out/p3_image.out"
    build/check_p3 "$D" out/p3_image.out; echo "CHECK STATUS $p $j $?"
  fi
 done
done
date -u; uptime
./bench.sh 5 "$D" > "$LOG-bench.txt" 2>&1
cat "$LOG-bench.txt"
date -u; uptime
