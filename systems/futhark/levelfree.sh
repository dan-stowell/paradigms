#!/bin/bash
# levelfree.sh: ATTACKS.md 3 for Futhark: the level-synchronous flood (cell 2) against the hybrid (cell 2hyb:
# data-parallel levels while the frontier is wide, a sequential worklist drain while it is narrow).
#   systems/futhark/levelfree.sh counts            levels, drains, parallel operations per flood (FUT_PROFILE)
#   systems/futhark/levelfree.sh time [ROUNDS]     ROUNDS (default 5) interleaved rounds; each round runs every
#                                                  configuration once (REPS floods in one process, min taken)
# Configurations: "name cell [VAR=value ...]" (the driver's knobs, p2.c). 1 core = the c backend, 8 = multicore
# (stock runtime).
set -e
cd "$(dirname "$0")"
D=${D:-/home/exedev/paradigms/data/minio}
[ -n "$NOBUILD" ] || ./build.sh p2 >/dev/null 2>&1
CFGS=${CFGS:-"orig 2
hyb-all-par 2hyb P2LO=0 P2HI=0
hyb-1k 2hyb P2LO=1024 P2HI=4096
hyb-4k 2hyb P2LO=4096 P2HI=16384
hyb-4k-b2k 2hyb P2LO=4096 P2HI=16384 P2BUDGET=2048
hyb-16k 2hyb P2LO=16384 P2HI=65536
all-seq 2hyb P2LO=999999999999 P2HI=1000000
bat-64 2bat P2C=64 P2B=1024 P2CAP=4096 P2H=2048
bat-8 2bat P2C=8 P2B=4096 P2CAP=16384 P2H=8192"}
one() {   # exe threads reps name cell [VAR=value ...]
	local exe=$1 th=$2 reps=$3 name=$4 cell=$5
	shift 5
	env "$@" prlimit --as=8589934592 $exe $D $th $reps $cell ${CHECK:-nocheck}
}
case ${1:-counts} in
counts)
	P=$(mktemp)
	echo "$CFGS" | while read name cell vars; do
		FUT_PROFILE=1 CHECK=check one build/p2_multicore.exe 8 1 $name $cell $vars 2>$P | grep -E "levels|hybrid|batch|P2" | sed "s/^/$name: /"
		# the driver runs the flood twice (once for the counts, once timed)
		python3 -c "import json; ev=json.load(open('$P'))['events']; print('$name:', sum(1 for e in ev if '_task_' in e['name'])//2, 'parallel operations per flood')"
	done
	rm -f $P ;;
time)
	R=${2:-5}
	out=$(mktemp)
	echo "# $(date -u +%FT%TZ) load $(cut -d' ' -f1-3 /proc/loadavg)"
	for r in $(seq $R); do
		echo "$CFGS" | while read name cell vars; do
			for cfg in c:1:${REPS1:-5} multicore:8:${REPS8:-10}; do
				IFS=: read be th reps <<< "$cfg"
				m=$(one build/p2_$be.exe $th $reps $name $cell $vars | grep -o 'min [0-9.]*' | awk '{print $2}')
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
