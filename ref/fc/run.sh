#!/usr/bin/env bash
# Build MinIO (or TARGET=<module dir> PKG=<package>, e.g. kube-apiserver) with build/fcp4 (ref/fc/build.sh).
# With P4DUMP=<file> the problem-4 dump goes there.
#   ref/fc/run.sh <out dir> [jobs (4)]   ->  <out>/bin, <out>/build.log
set -e
here=$(cd "$(dirname "$0")/../.." && pwd)
out=$1; j=${2:-4}
mkdir -p "$out"
export CGO_ENABLED=0 GOTOOLCHAIN=local GOFLAGS=-buildvcs=false
export GOCACHE=${GOCACHE:-/home/exedev/paradigms/data/minio-p4/gocache}
FC_P4DUMP=${P4DUMP:-} prlimit --as=12884901888 "$here/build/fcp4" build -j "$j" -C "${TARGET:-/home/exedev/targets/minio}" \
  -o "$out/objs" -link "$out/bin" "${PKG:-.}" > "$out/build.log" 2>&1
rm -rf "$out/objs"
