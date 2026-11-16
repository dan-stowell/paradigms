#!/usr/bin/env bash
# Problem 4's flat files and oracle for MinIO: build ref/fc's instrumented copy of fc, build MinIO with it
# while it dumps every function body it emits (FC_P4DUMP), check that the dump did not change the program
# (the same binary with and without it), and turn the dump into data/minio-p4/.
#   ref/p4/extract.sh [out dir (data/minio-p4 in the main checkout)]
#   kube-apiserver: TARGET=/home/exedev/targets/kubernetes PKG=./cmd/kube-apiserver GOCACHE=<dir> ref/p4/extract.sh data/kube-p4
set -euo pipefail
here=$(cd "$(dirname "$0")/../.." && pwd)
d=${1:-/home/exedev/paradigms/data/minio-p4}
mkdir -p "$d" "$here/build"
bash "$here/ref/fc/build.sh"
tmp=$(mktemp -d "$d/run.XXXXXX")
bash "$here/ref/fc/run.sh" "$tmp/plain"
P4DUMP="$d/dump.bin" bash "$here/ref/fc/run.sh" "$tmp/dump"
cmp "$tmp/plain/bin" "$tmp/dump/bin" && echo "the program built with the dump on is byte-identical to the one without"
"$tmp/dump/bin" --version | head -1
rm -rf "$tmp"
(cd "$here/ref/p4/mkflat" && GOTOOLCHAIN=local go build -o "$here/build/p4mkflat" .)
prlimit --as=12884901888 "$here/build/p4mkflat" "$d/dump.bin" "$d" | tee "$d/stats.txt"
