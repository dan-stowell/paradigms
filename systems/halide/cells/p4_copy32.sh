#!/bin/bash
# Is the byte copy the cost of P4's emit in C? Builds ref/p4/p4.c twice: as is (memcpy per item) and with each item's
# memcpy replaced by whole 32-byte AVX2 load/stores that overrun into the next item (the output has 64 bytes of
# slack; the inputs are file mappings with room in their last page), then runs them interleaved, 5 times each.
#   systems/halide/cells/p4_copy32.sh [REF_P4_C] [DATA]   (needs a p4.c with P4_REPS, i.e. main's ref/p4/p4.c)
set -euo pipefail
here=$(cd "$(dirname "$0")/../../.." && pwd)
src=${1:-$here/ref/p4/p4.c}; D=${2:-/home/exedev/paradigms/data/minio-p4}
T=$(mktemp -d); mkdir -p $T/ref/p4 $T/ref/flat
cp "$src" $T/ref/p4/p4.c; cp "$(dirname "$src")/../flat/flat.h" $T/ref/flat/
python3 - $T/ref/p4/p4.c $T/ref/p4/p4v.c <<'EOF'
import sys
s = open(sys.argv[1]).read()
s = s.replace('#include "../flat/flat.h"', '#include "../flat/flat.h"\n#include <immintrin.h>\n'
              'static inline void copy32(u8 *d, const u8 *s, u64 n) { for (u64 k = 0; k < n; k += 32) '
              '_mm256_storeu_si256((__m256i *)(d + k), _mm256_loadu_si256((const __m256i *)(s + k))); }\n')
for a in ["memcpy(P, glueBin + glueOff[r], glueOff[r + 1] - glueOff[r]);", "memcpy(P, tmplBin + tmplOff[t] + lo, hi - lo);"]:
    assert s.count(a) == 1, a
    s = s.replace(a, a.replace("memcpy", "copy32"))
a = "u8 *code = malloc(ncode ? ncode : 1);"
assert a in s
s = s.replace(a, "u8 *code = malloc(ncode + 64);")
open(sys.argv[2], "w").write(s)
EOF
gcc -O2 -march=native -o $T/p4 $T/ref/p4/p4.c
gcc -O2 -march=native -o $T/p4v $T/ref/p4/p4v.c
date -u; uptime
for i in 1 2 3 4 5; do
    echo "memcpy: $(P4_REPS=10 $T/p4 $D | grep -E '^p4|P4 ' | cut -c1-70 | tr '\n' ' ')"
    echo "copy32: $(P4_REPS=10 $T/p4v $D | grep -E '^p4|P4 ' | cut -c1-70 | tr '\n' ' ')"
done
rm -rf $T
