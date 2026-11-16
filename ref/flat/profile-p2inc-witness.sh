#!/usr/bin/env bash
# Untimed detailed counter diagnosis; timing builds omit PROFILE_SUPPORT.
set -euo pipefail
cd "$(dirname "$0")/../.."
mkdir -p build
export TMPDIR="$PWD/build"
unset LIFO COUNTS FUZZ CHAIN CHAIN_EDIT EVICT
D=/home/exedev/paradigms/data
git show 587b3514085808aacd8365164258d199acb0316e:ref/flat/p2inc.c > build/p2inc-baseline.c
patch -s -o build/p2inc-profile-baseline.c build/p2inc-baseline.c < ref/flat/p2inc-baseline-profile.patch
gcc -O2 -march=native -DPROFILE_SUPPORT -I ref/flat -o build/p2inc-profile-baseline build/p2inc-profile-baseline.c
gcc -O2 -march=native -DPROFILE_SUPPORT -o build/p2inc-profile ref/flat/p2inc.c
PROFILE_TSV=build/witness-baseline-targets.tsv FRESH_B=1 prlimit --as=12884901888 \
  build/p2inc-profile-baseline "$D/minio" "$D/minio-edit2" 1 > results/c-incremental-witness-diagnosis.txt
PROFILE_TSV=build/witness-final-targets.tsv FRESH_B=1 prlimit --as=12884901888 \
  build/p2inc-profile "$D/minio" "$D/minio-edit2" 1 > results/c-incremental-witness-profile.txt
gzip -cn build/witness-baseline-targets.tsv > results/c-incremental-witness-baseline-targets.tsv.gz
gzip -cn build/witness-final-targets.tsv > results/c-incremental-witness-final-targets.tsv.gz
