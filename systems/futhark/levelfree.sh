#!/bin/bash
# levelfree.sh: ATTACKS.md 3 for Futhark: the level-synchronous flood (cell 2) against the hybrid (cell 2hyb:
# data-parallel levels while the frontier is wide, a sequential worklist drain while it is narrow).
#   systems/futhark/levelfree.sh counts            levels, drains, parallel operations per flood (FUT_PROFILE)
#   systems/futhark/levelfree.sh time [ROUNDS]     ROUNDS (default 5) interleaved rounds; each round runs every
#                                                  configuration once (REPS floods in one process, min taken)
# Configurations: "name lo hi budget" (cell 2 ignores them). 1 core = the c backend, 8 = multicore (stock runtime).
set -e
cd "$(dirname "$0")"
D=${D:-/home/exedev/paradigms/data/minio}
[ -n "$NOBUILD" ] || ./build.sh p2 >/dev/null 2>&1
CFGS=${CFGS:-"orig - - -
hyb-all-par 0 0 999999999999
hyb-1k 1024 4096 999999999999
hyb-4k 4096 16384 999999999999
hyb-4k-b2k 4096 999999999 2048
hyb-16k 16384 65536 999999999999
all-seq 999999999999 1000000 999999999999"}
run() { prlimit --as=8589934592 "$@"; }
one() {   # exe threads reps name lo hi budget
	local exe=$1 th=$2 reps=$3 name=$4
	if [ "$name" = orig ]; then run $exe $D $th $reps 2 nocheck
	else P2LO=$5 P2HI=$6 P2BUDGET=$7 run $exe $D $th $reps 2hyb nocheck; fi
}
case ${1:-counts} in
counts)
	P=$(mktemp)
	echo "$CFGS" | while read name lo hi budget; do
		FUT_PROFILE=1 one build/p2_multicore.exe 8 1 $name $lo $hi $budget 2>$P | grep -E "levels|hybrid" | sed "s/^/$name: /"
		# the driver runs the flood twice (once for the counts, once timed)
		python3 -c "import json; ev=json.load(open('$P'))['events']; print('$name:', sum(1 for e in ev if '_task_' in e['name'])//2, 'parallel operations per flood')"
	done
	rm -f $P ;;
time)
	R=${2:-5}
	out=$(mktemp)
	echo "# $(date -u +%FT%TZ) load $(cut -d' ' -f1-3 /proc/loadavg)"
	for r in $(seq $R); do
		echo "$CFGS" | while read name lo hi budget; do
			for cfg in c:1:${REPS1:-5} multicore:8:${REPS8:-10}; do
				IFS=: read be th reps <<< "$cfg"
				m=$(one build/p2_$be.exe $th $reps $name $lo $hi $budget | grep -o 'min [0-9.]*' | awk '{print $2}')
				echo "$name $th $m" >> $out
			done
		done
		echo "# round $r done $(date -u +%T) load $(cut -d' ' -f1 /proc/loadavg)"
	done
	awk '{k=$1" "$2; v[k]=v[k]" "$3; if(!(k in s)){s[k]=1; o[++n]=k}}
	  END{for(i=1;i<=n;i++){k=o[i]; m=split(v[k],a," "); for(x=1;x<=m;x++)for(y=x+1;y<=m;y++)if(a[y]+0<a[x]+0){t=a[x];a[x]=a[y];a[y]=t}
	      printf "%-14s %s cores  min / median / max %7.1f / %7.1f / %7.1f ms\n", substr(k,1,index(k," ")-1), substr(k,index(k," ")+1), a[1], a[int((m+1)/2)], a[m]}}' $out
	echo "# end $(date -u +%FT%TZ) load $(cut -d' ' -f1-3 /proc/loadavg)"
	rm -f $out ;;
esac
