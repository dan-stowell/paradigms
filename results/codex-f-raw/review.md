# Independent review: p1compact

Reviewed `ref/flat/p1compact.c`, the p1par instrumentation diff, and check.sh integration. No correctness defect found.

- Exactly one dense join traversal: the chunk loop sets canon, reads flags/key/kind/rank, and emits keyed kind 4/5 members and kind 1 candidates. Later loops consume compact records only (plus wanted/builtin keys); the unchanged relocation pass and content join separately read their normal inputs.
- Bounds: chunk c has slot capacity floor(N(c+1)/C)-floor(Nc/C). Candidate scratch begins at floor(Nc/C). Member scratch begins there plus nw except chunk 0, which starts at zero and reserves nw extra entries. These intervals are disjoint and end within N or N+nw. Each scatter count is prefixed in partition/chunk order and exactly consumed; destination totals cannot exceed the corresponding capacity.
- Ownership: each chunk owns its scratch/count rows; each partition owns its table and disjoint output record interval. Every canonical slot belongs to at most one member/candidate list. Table construction, candidate max, candidate readback, and member readback occur sequentially under one partition owner. The parallel-loop barrier precedes builtin lookups. No CAS is needed.
- Determinism: prefix order preserves chunk/slot order; integer max is unchanged. Candidates never create entries, and absent candidates keep initial canon. A partition containing only candidates is safe because its table still has at least 64 zero entries.
- A Python source comparison asserted the relocation pass is byte-identical to p1par, and everything from the first oracle check through 1c and final return is byte-identical. check.sh builds and invokes p1compact.
- Independently ran `OMP_NUM_THREADS=1 prlimit --as=8589934592 build-f/p1compact data/kube-apiserver 1`: P1 OK; canon 13,270,145, builtin 299, relocation targets 19,824,324, final canon 13,270,145 entries all had zero differences. This was a single correctness run, not a timing result.
- Existing parent MinIO 8-thread oracle output also reports zero differences. The parent's interleaved benchmark runner requires P1 OK on every recorded run, providing repeated threaded oracle validation.

Limitations: allocation failure checks are absent as in p1par; capacities remain below the requested 8 GiB limit for both supplied datasets. Empty/invalid input or extreme thread counts are not separately hardened. No code changes requested.

## Independent byte-model review

Reviewed `results/codex-f-bytes.py` against both sources, without rescanning datasets during the benchmark. The formulas are internally consistent with the declared streaming/full-record model:

- Baseline reads `37N + 128Q + 4(M+H) + 56B + 24W + 16nb`: two dense key streams (32N), flags plus four kind traversals (5N), two candidate key traversals over Q distinct 64-byte lines (128Q), ranks for M members and H matching candidates, two modeled 28-byte record traversals, count/scatter wanted-key loads, and builtin keys.
- Compact reads `17N + 5K + 96L + 16W + 16nb`: one dense key stream plus flags, one kind and one rank per keyed slot, one scratch and two partition-record traversals at 32 bytes each, wanted keys, builtin keys.
- Common writes are initial canon `4(N+1)`, member/matching-candidate canon `4(M+H)`, builtin `4nb`, and table zeroing `24S`. Baseline adds `28B`; compact adds two materializations `64L`.
- H counts matching candidate slots, not unique keys, correctly matching both rank reads in p1par and final canon writes. Partition sizing reproduces the code's minimum 64 and power-of-two doubling of twice the member/wanted count. Candidate key line index g//4 is correct for aligned 16-byte keys in mmap storage.

Limitations to retain explicitly: this is not measured DRAM or total instruction memory traffic. It omits dynamic hash probes/updates (and changed CAS behavior), count/prefix metadata, allocation/first-touch overhead, write allocation, cache reuse, and compiler load elimination. Dense key estimates 16N/32N omit at most boundary cache-line rounding. The 56B term deliberately treats the baseline's separate rec/slotOf arrays as a logical 28-byte record on both traversals, though table building does not read slotOf; therefore it charges 4B extra versus field-specific stream counting. Compact readback similarly charges unused val/padding. These are acceptable declared full-record model conventions, not exact physical bytes.

## Final report and raw-data review

Independently recomputed min/median/max for all eight dataset/thread/version groups from the 40 measurements, plus median reductions, percentages and range gaps. Every table entry and the conclusion match the data: only kube-apiserver at one thread has separated join and total ranges; its join gap is 25.619194 ms and median reduction 37.5997%. All other comparisons overlap. Verified every raw log has four zero-difference comparisons and P1 OK, every recorded rep count is 100, and kernel durations span 2.441961–22.722994 s. Window timestamps and uptime/load values match the JSON.

Independently recalculated the final formulas from `bytes.jsonl`: both read/write totals and report MB reductions agree. The corrected baseline record term `28B+24M` properly charges table-build records and the separate slotOf/member readback streams. The report clearly states model limitations and that changed table ownership/CAS/barriers confound a pure bandwidth explanation. No remaining report or arithmetic correction requested.
