#!/usr/bin/env bash
# Independent compiler/runtime check and negative test for unconditional fn offsets.
set -euo pipefail
cd "$(dirname "$0")/.."
d=${1:-/home/exedev/paradigms/data/minio-p4}
tmp=$(mktemp -d /tmp/cp7-review.XXXXXX)
# Keep generated input and binary under /tmp; original dataset is never written.
prlimit --as=12884901888 gcc -O1 -g -fsanitize=undefined -fno-sanitize-recover=all -fopenmp \
    -o "$tmp/p4runs" ref/p4/p4runs.c
for mode in A C; do
    OMP_NUM_THREADS=8 OMP_THREAD_LIMIT=2 OMP_WAIT_POLICY=passive P4_REPS=1 \
        prlimit --as=12884901888 "$tmp/p4runs" "$d" "$mode"
done
python3 - "$d" "$tmp/data" <<'PY'
from pathlib import Path
import struct, sys
src=Path(sys.argv[1]).resolve(); dst=Path(sys.argv[2]); dst.mkdir()
for p in src.iterdir():
    if p.name!='oracle' and p.is_file(): (dst/p.name).symlink_to(p)
(dst/'oracle').mkdir()
for p in (src/'oracle').iterdir():
    if p.name!='p4_fn_off.u64' and p.is_file(): (dst/'oracle'/p.name).symlink_to(p)
b=bytearray((src/'oracle/p4_fn_off.u64').read_bytes())
struct.pack_into('<Q',b,8,struct.unpack_from('<Q',b,8)[0]+1)
(dst/'oracle/p4_fn_off.u64').write_bytes(b)
PY
set +e
OMP_NUM_THREADS=1 OMP_WAIT_POLICY=passive P4_REPS=1 \
    prlimit --as=12884901888 "$tmp/p4runs" "$tmp/data" C > "$tmp/negative.log" 2>&1
status=$?
set -e
cat "$tmp/negative.log"
[[ $status == 1 ]]
rg -q 'function offsets: 136635 checked, 1 errors' "$tmp/negative.log"
rg -q '^P4 FAIL$' "$tmp/negative.log"
echo 'Independent CP7 checks OK; altered function offset correctly rejected.'
