# Plain C 3a profile and independent tuned-code check

2026-11-16 17:01 UTC. `gcc -O2 -march=native -o build-i/p3 ref/flat/p3.c`, then
`prlimit --as=8589934592 build-i/p3 data/DATASET --layout`.
Each timing is the mean of repetitions totaling at least one second of kernel time;
input mapping and oracle checking are outside timing. These are exploratory profiles,
not the five interleaved comparisons in codex-i.md. Load at end: 4.73 / 3.58 / 4.11;
uptime 1:45. Both address arrays matched exactly, and default MinIO P3 still passed.

| phase | MinIO ms | kube ms | MinIO logical read/write bytes | kube logical read/write bytes |
|---|---:|---:|---:|---:|
| allocation/zeroing | .067081 | .074602 | 0 / 2,908,076 | 0 / 4,298,100 |
| classification | 1.671117 | 2.415663 | 4,778,322 / 2,749,476 | 5,661,086 / 3,260,892 |
| bucket prefix | .967150 | 1.427097 | 2,464,632 / 1,283,464 | 4,351,320 / 2,254,616 |
| scatter/collect typelinks | .832171 | 1.002528 | 4,612,017 / 2,749,476 | 5,433,711 / 3,260,892 |
| typelink sort | 5.084323 | 7.933259 | 33,039,706 / 3,713,576 | 50,286,632 / 5,272,824 |
| typelink address walk | .191060 | .299232 | 299,349 / 133,044 | 409,275 / 181,900 |
| total | 8.813298 | 13.153404 | | |

MinIO: 114 repetitions, 1.004716 s, 441,528 sort comparisons.
Kube: 77 repetitions, 1.012812 s, 630,658 comparisons.

Traffic is a logical array-access model, **not measured DRAM traffic**. Let L be
placed count, T typelinks, N=L-T, C=14*13*nunits, H=number of class entries tested
searching for each unit/bucket's highest occupied class. Classification reads
6L+8N bytes (size, bucket, class; normal unit and old class total), writes 4L+4N.
Prefix reads 4(H+C), writes 4C. Scatter reads L+13N, writes 8N+4T.
Walk reads 9T (index, class, size), writes 4T. Allocation counts requested zeroed
bytes (address and class tables); libc can defer their physical zeroing. Tiny
bucket metadata, allocator metadata, stack/register traffic, timer overhead,
cache-line fetches/write allocation, and speculative/vector overreads are excluded.
Typelink allocation is in scatter time, other allocation in allocation time.

Sort counts are taken in an additional **untimed** diagnostic pass using qsort's
unchanged comparator semantics. Each comparison reads 48 bytes of index/string-id/
offset metadata, plus both distinguishing string prefixes, plus 8 ID bytes when
tied. Prefix bytes are 8,120,114 / 14,726,816. Actual libc memcmp may fetch extra
vector bytes; full requested memcmp lengths would overstate necessary comparisons.
Array movement is modeled by glibc 2.39's direct mergesort: each moved u32 goes
source -> temporary -> destination, 8 read and 8 written bytes; an unconsumed right
suffix stays in place. An independent diagnostic merge recurrence counts this
movement and asserts its comparison count equals the real qsort diagnostic count.
Movement read and write counts each: 3,713,576 / 5,272,824 bytes. Model does not cover
glibc's allocation-failure heapsort fallback; the comparison-count assertion rejects
an incompatible model. Timed iterations always use original cmpTL without counters.

## Independent p3a_par check

Read source, rebuilt separately as `build-i/p3a_par_review`, and ran both datasets
at 1 and 8 threads under the 8 GiB limit with OMP_WAIT_POLICY=passive. All four
full oracle comparisons reported zero differences (360,315 / 430,349 entries).
Exploratory means, not an interleaved performance claim: MinIO 5.445 / 6.191 ms;
kube 8.614 / 6.315 ms. Repetitions each exceeded one second.

Unit-private classification/scatter preserves list order. Highest class after
scatter is correctly recovered from the first nonzero end offset: higher empty
classes remain zero. Bucket base scans preserve unit order and include minimum
alignment only for nonempty units. MSD digit 0 represents string termination,
byte values map to 1..256, and equal strings use symbol IDs. Task subranges and
temporary ranges are disjoint; each recursion waits for child tasks before return.
Every iteration reconstructs original typelinks and buckets; no sorted-order cache
or oracle-derived preprocessing was found. Only p3_addr.u32 is used for validation.

Fairness limitation: tuned code preallocates scratch outside timing, whereas plain
p3 retains original in-kernel allocation/zeroing (about .07 ms here). Tuned code
still zeros its per-unit class tables inside every timed iteration; first-use page
faults also occur in the timed loop. State this small distinction in the final
report. Input validation scans are also untimed. Long identical type strings can
cause deep MSD recursion and identical-string groups use quadratic insertion sort;
these are generalization limitations, not correctness failures on either dataset.
