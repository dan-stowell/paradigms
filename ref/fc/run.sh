#!/usr/bin/env bash
# Build MinIO with build/fcp4 (ref/fc/build.sh). With P4DUMP=<file> the problem-4 dump goes there.
#   ref/fc/run.sh <out dir> [jobs (4)]   ->  <out>/bin, <out>/build.log
set -e
here=$(cd "$(dirname "$0")/../.." && pwd)
out=$1; j=${2:-4}
mkdir -p "$out"
export CGO_ENABLED=0 GOTOOLCHAIN=local GOFLAGS=-buildvcs=false
export GOCACHE=${GOCACHE:-/home/exedev/paradigms/data/minio-p4/gocache}
FC_P4DUMP=${P4DUMP:-} prlimit --as=12884901888 "$here/build/fcp4" build -j "$j" -C /home/exedev/targets/minio \
  -o "$out/objs" -link "$out/bin" . > "$out/build.log" 2>&1
rm -rf "$out/objs"
