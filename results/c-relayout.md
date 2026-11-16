# Layout after an edit: local for the addition, still broad for the deletion

**Result.** With today's layout, the five-line addition moves **261,723 of 360,315 common placed symbols**, changes **1,226,535 relocation values**, and changes **35,129,895 produced image bytes**. The deletion moves **352,863 of 353,117 common placed symbols**, changes **1,824,729 relocation values**, and changes **76,696,695 produced bytes**. The current layout makes even the addition a broad relink.

A retained package/bucket allocation with 4 KB reservations reduces the addition to **3 moved addresses, 34 repatches, 1,013 changed bytes**, and **0.443 µs** per maintained update (1 core). The deletion is different: **101,483 moved addresses, 869,439 repatches, 32,924,681 changed bytes**, and **95.1 ms**, versus **98.9 ms** for the reference pipeline: **within noise, no demonstrated speedup**. Padding stops movement across packages; it does not stop compaction inside a package or the global typelink sort.

There is one impossible literal check in the request: relocating A at different addresses cannot generally reproduce the old relocated bytes verbatim. Absolute addresses and cross-block PC-relative displacements must change. The experiment measures that failure explicitly and checks the replacement claim independently: identical non-relocation bytes, correct new relocation values, and a byte-identical incremental/from-scratch slack image. **184,545 of A's symbols have different relocated bytes; zero non-relocation bytes differ.** This is a P3 image experiment, not a runnable padded ELF.

## (1) Before: the current rule

`p3compare.c` uses `diff/idmap_b.u32` for symbol identity, the actual `oracle/p3_addr.u32` addresses, and actual relocated oracle bytes. A relocation site is paired by **(unified source ID, offset, width, occurrence)**. Added/deleted sites are separate from paired sites with different output bits. This avoids pretending an ordinal relocation remains the same after inserted instructions; it also avoids counting a changed target whose written value happens to stay equal.

For image comparison, copy only P3-produced symbol ranges (buckets 1–12) into otherwise zero images; compare at absolute image positions over the union of A/B ranges. Removed bytes compare to zero unless B occupies that position. Bucket attribution uses the union of A/B bucket masks. If a byte switches buckets it appears in both bucket rows, so **per-bucket image counts cannot be summed as unique global bytes**. BSS/counters contribute addresses but no produced bytes. Unproduced ELF/pclntab/GC/moduledata bytes are excluded.

### Edit 1: current layout

| Bucket | Placed + / − | Common moved / common | Relocation sites + / − | Paired values changed / paired | Image bytes changed |
|---|---:|---:|---:|---:|---:|
| 1 text | 0 / 0 | 89,289 / 109,877 | 8 / 5 | 806,879 / 1,144,391 | 26,804,270 |
| 2 text FIPS | 0 / 0 | 803 / 803 | 0 / 0 | 5,646 / 8,384 | 480,189 |
| 3 rodata | 0 / 0 | 0 / 57,032 | 0 / 0 | 19,495 / 40,137 | 22,652 |
| 4 rodata FIPS | 0 / 0 | 0 / 23 | 0 / 0 | 0 / 0 | 0 |
| 5 go:func | 1 / 0 | 35,599 / 35,599 | 0 / 0 | 362 / 18,988 | 939,408 |
| 6 typelink | 0 / 0 | 33,261 / 33,261 | 0 / 0 | 151,328 / 428,898 | 2,189,623 |
| 7 type | 0 / 0 | 95,151 / 95,151 | 0 / 0 | 202,655 / 347,641 | 4,554,306 |
| 8 itab | 0 / 0 | 7,620 / 7,620 | 0 / 0 | 28,066 / 31,827 | 122,689 |
| 9 noptrdata | 0 / 0 | 0 / 2,342 | 0 / 0 | 982 / 1,082 | 984 |
| 10 noptrdata FIPS | 0 / 0 | 0 / 23 | 0 / 0 | 0 / 0 | 0 |
| 11 data | 0 / 0 | 0 / 13,236 | 0 / 0 | 11,122 / 32,448 | 16,131 |
| 12 data FIPS | 0 / 0 | 0 / 0 | 0 / 0 | 0 / 0 | 0 |
| 13 bss | 0 / 0 | 0 / 3,795 | 0 / 0 | 0 / 0 | 0 |
| 14 noptrbss | 0 / 0 | 0 / 1,553 | 0 / 0 | 0 / 0 | 0 |
| 15 coverage | 0 / 0 | 0 / 0 | 0 / 0 | 0 / 0 | 0 |

### Edit 2: current layout

| Bucket | Placed + / − | Common moved / common | Relocation sites + / − | Paired values changed / paired | Image bytes changed |
|---|---:|---:|---:|---:|---:|
| 1 text | 9 / 1,636 | 107,987 / 108,241 | 73 / 21,267 | 999,112 / 1,123,129 | 38,646,079 |
| 2 text FIPS | 0 / 0 | 803 / 803 | 0 / 0 | 5,832 / 8,384 | 949,637 |
| 3 rodata | 148 / 1,117 | 55,915 / 55,915 | 7 / 460 | 39,624 / 39,677 | 22,634,283 |
| 4 rodata FIPS | 0 / 0 | 23 / 23 | 0 / 0 | 0 / 0 | 4,821 |
| 5 go:func | 134 / 877 | 34,722 / 34,722 | 34 / 374 | 1,762 / 18,614 | 1,360,144 |
| 6 typelink | 0 / 833 | 32,428 / 32,428 | 0 / 8,278 | 383,749 / 420,620 | 3,602,508 |
| 7 type | 145 / 2,262 | 92,889 / 92,889 | 0 / 6,692 | 330,426 / 340,949 | 8,191,308 |
| 8 itab | 4 / 170 | 7,450 / 7,450 | 58 / 642 | 31,183 / 31,185 | 298,977 |
| 9 noptrdata | 0 / 5 | 2,337 / 2,337 | 0 / 0 | 1,081 / 1,082 | 1,654,798 |
| 10 noptrdata FIPS | 0 / 0 | 23 / 23 | 0 / 0 | 0 / 0 | 197,298 |
| 11 data | 0 / 270 | 12,966 / 12,966 | 0 / 488 | 31,960 / 31,960 | 294,869 |
| 12 data FIPS | 0 / 0 | 0 / 0 | 0 / 0 | 0 / 0 | 0 |
| 13 bss | 0 / 12 | 3,783 / 3,783 | 0 / 0 | 0 / 0 | 0 |
| 14 noptrbss | 0 / 16 | 1,537 / 1,537 | 0 / 0 | 0 / 0 | 0 |
| 15 coverage | 0 / 0 | 0 / 0 | 0 / 0 | 0 / 0 | 0 |

Edit 1: one placed symbol added, none removed; unique image union 77,574,856 bytes, of which 35,129,895 change. Edit 2: 440 added, 7,198 removed; unique image union 91,796,980 bytes, of which 76,696,695 change. The 12,320 reach deaths are not 12,320 placed deaths: P2 counts aliases and symbols outside these P3 buckets.

## (2) Observation: reserving package space contains some movement

**Unit choice.** Use `(bucket, slot_obj)` (object/package) instead of extractor `p3_unit`. The extractor splits large objects every 8,192 slots; edit 2 moves 95 common placed symbols across those artificial boundaries even when their package is unchanged. Package grouping avoids that identity artifact. Within a package, sort by descending alignment class and then unified ID, with text's minimum 16-byte occupancy. This is a new, explicitly defined layout rule, not p3.c's unit-before-class ordering. Arena starts respect `p3_bucket_minalign` and the largest A alignment. Typelink is the exception: one global arena, sorted by type string then unified ID; it preserves the global ordering constraint instead of breaking it into packages.

**Retained allocation.** Reserve `ceil((A_occupied + 1)/q)*q` bytes for every package/bucket arena, including an empty arena. The `+1` deliberately reserves the next boundary even for an exact multiple. Round q up if A's largest class requires it. The arenas and section boundaries are allocated once from A and retained for B. Growth within capacity moves no other arena; shrinking leaves a zero hole at its end. Symbols inside a changed arena may move. Empty package/bucket reservations make adding to a previously empty bucket possible without moving neighbors; this is deliberately conservative, not a minimum-space allocator.

The initial plan preserves A's gaps between bucket ends and the next bucket start, translating section anchors and the seven fixed-symbol anchors with those gaps. Original A oracle addresses supply bucket endpoints and identify the two weak fallback symbols; they do not supply B's slack addresses. Weak fallback dependencies are then indexed as dependencies on their actual placed function IDs. A's external generated-region anchors remain frozen; B's original compact-layout addresses would be wrong in the retained plan. Membership of fixed symbols is checked. Rebuilding omitted generated metadata or producing an executable is outside this check.

### Capacity policies (same A plan tested against both edits)

All numbers include virtual BSS reservations; original P3 file image is 109,486,380 bytes. “% image” below divides reserved slack by that file size, explicitly including BSS slack in the numerator. A's occupied arena payload is 112,446,436 bytes including BSS; it is 78,303,235 bytes for produced buckets only.

| Policy | Reserved arena bytes | Slack bytes | Slack / original file image | Spilling arenas edit 1 / edit 2 |
|---|---:|---:|---:|---:|
| Next 64 B | 113,555,456 | 1,109,020 | 1.01% | 1 / 38 |
| Next 128 B | 114,655,232 | 2,208,796 | 2.02% | 1 / 27 |
| Next 256 B | 116,901,120 | 4,454,684 | 4.07% | 1 / 18 |
| Next 512 B | 121,470,464 | 9,024,028 | 8.24% | 0 / 9 |
| Next 1 KB | 130,830,336 | 18,383,900 | 16.79% | 0 / 6 |
| **Next 4 KB, measured** | **188,649,472** | **76,203,036** | **69.60%** | **0 / 0** |
| +1%, then next 64 B | 114,657,280 | 2,210,844 | 2.02% | 1 / 15 |
| +5%, then next 64 B | 119,147,776 | 6,701,340 | 6.12% | 1 / 4 |
| +10%, then next 64 B | 124,767,872 | 12,321,436 | 11.25% | 1 / 4 |

4 KB is sufficient for both edits among these tested choices, **not a proven minimum**. Retrospectively, the sum of positive arena growth is only 272 bytes for edit 1 and 7,056 for edit 2 (largest single growth 256 / 1,070 bytes). Reserving for every possible package/bucket costs far more than knowing which arena will grow. An allocator with shared spare extents or smaller empty-arena reservations is an obvious next experiment, not a measured result here.

For produced buckets alone, the chosen policy reserves **137,150,464 bytes**, including **58,847,229 bytes of slack (53.75% of the original file image)**. The produced address extent, including preserved generated-region gaps, is 168,301,184 bytes. The complete virtual span including BSS is 219,800,224 bytes; total slack is 53.05% of A's original 143,655,120-byte virtual span. Large all-zero holes are represented in the check buffers; they are not written on every update.

### Movement and writes under the same retained 4 KB plan

“Repatch” includes every relocation in a changed or moved source symbol, plus reverse-index sites in otherwise unchanged symbols whose effective target address changed. A moved source counts because PC-relative writes depend on the source address too. This is the conservative required work of this algorithm, not the minimum number of distinct values that differ; constant writes and some unchanged relative displacements may be replayed. Added/removed placed counts are the same as the current-layout tables.

| Bucket | Edit 1 moved | Edit 1 repatched | Edit 1 bytes changed | Edit 2 moved | Edit 2 repatched | Edit 2 bytes changed |
|---|---:|---:|---:|---:|---:|---:|
| 1 text | 3 | 34 | 1,009 | 12,438 | 291,252 | 8,100,157 |
| 2 text FIPS | 0 | 0 | 0 | 0 | 124 | 326 |
| 3 rodata | 0 | 0 | 0 | 17,066 | 14,979 | 20,637,391 |
| 4 rodata FIPS | 0 | 0 | 0 | 0 | 0 | 0 |
| 5 go:func | 0 | 0 | 4 | 13,375 | 5,952 | 451,732 |
| 6 typelink | 0 | 0 | 0 | 32,424 | 404,622 | 2,319,788 |
| 7 type | 0 | 0 | 0 | 25,673 | 137,427 | 1,378,690 |
| 8 itab | 0 | 0 | 0 | 192 | 7,065 | 20,542 |
| 9 noptrdata | 0 | 0 | 0 | 1 | 29 | 224 |
| 10 noptrdata FIPS | 0 | 0 | 0 | 0 | 0 | 0 |
| 11 data | 0 | 0 | 0 | 304 | 7,989 | 15,831 |
| 12 data FIPS | 0 | 0 | 0 | 0 | 0 | 0 |
| 13 bss | 0 | 0 | 0 | 10 | 0 | 0 |
| 14 noptrbss | 0 | 0 | 0 | 0 | 0 | 0 |
| 15 coverage | 0 | 0 | 0 | 0 | 0 | 0 |
| **Total** | **3** | **34** | **1,013** | **101,483** | **869,439** | **32,924,681** |

Unlike the current-layout bucket masks, slack arenas are fixed and disjoint, so these byte counts sum. They compare all produced arena bytes, including cleared holes; omitted generated regions and BSS remain zero.

## (3) The maintained C update

`p3inc.c` keeps A's image, arena reservations, unified-ID addresses, sorted membership lists, and a doubly linked reverse index **effective canonical target → relocation sites**. The initial index is built once. The P2 diff is not a P3 data/relocation diff: setup uses the supplied ID map and compares normalized A/B symbol records (size/alignment, raw bytes, type string, relocation tuple with canonical unified target) to materialize symbol replacements and changed-arena membership lists. Full scans, canonicalization, sorting, allocations, and this diff construction are **outside update timing**. The supplied ID map's own construction cost is also excluded. The benchmark keeps both versions' records to alternate A→B and B→A; this is not an end-to-end source-edit time.

The timed operation does the actual maintenance, not a replay of a precomputed byte patch:

1. Remove old reverse-list entries and insert new ones for changed symbol records.
2. Recompute addresses only in changed arenas; discover moved symbols by comparing with maintained addresses.
3. Clear changed/retired source ranges before any copies, so overlapping moves and removals leave deterministic zero holes. Publish the new addresses, recopy changed/moved symbols, and relocate them.
4. Walk the maintained reverse lists for moved targets and patch remaining sites, skipping sources already copied. The dirty/moved flags are cleared through the touched-ID list, not a full-table sweep.

Unchanged records remain in the existing reverse index; canonical/alias changes are detected in the normalized source relocation records. A new/dead target is a zero↔address transition. Weak absent targets depend on the retained fallback function. There is no scanning of all relocations in the timed update.

| Forward mechanism | Edit 1 | Edit 2 |
|---|---:|---:|
| Changed symbol records, including additions/removals | 2 | 8,243 |
| Changed arenas | 2 | 468 |
| Symbols visited by changed-arena layout | 6 | 144,965 |
| Raw bytes copied (read and written) | 1,032 | 14,459,737 |
| Old image bytes cleared | 750 | 35,522,160 |
| Relocations repatched | 34 | 869,439 |
| Relocation bytes written | 136 | 4,288,536 |
| Reverse-list sites visited | 9 | 404,592 |
| **Total logical image bytes written** | **1,918** | **54,270,433** |

The write count is copy + clear + relocation stores, counting repeated writes; it is not unique bytes changed. Add the raw-copy read count for logical image/data traffic: 2,950 / 68,730,170 bytes. Metadata reads, reverse-list updates, cache-line transfers, and read-for-ownership are not included in that counter. The deletion's pointer chasing, internal compaction, and typelink repatching remain substantial: the typelink arena alone moves 32,424 common symbols and repatches 404,622 relocations.

### Checks and their limits

Both A→B and B→A images equal independently allocated from-scratch slack images **over the complete 219,800,224-byte buffer**, including zero holes. The benchmark alternates real updates for every timed repetition, then checks A and B again. Before the first update, B addresses in changed arenas are poisoned to zero; unchanged-arena addresses must equal A's. The scratch path lays out every arena and copies/relocates every produced symbol; it does not apply the diff. It shares the primary relocation function, so a second implementation checks that function:

`p3inc_check.c` does not include `p3inc.c`. It reads exported addresses, anchors, and images; independently resolves canonical targets, evaluates all eight relocation kinds, and builds a fresh zero image. It checks both the original reference/oracle bytes at original addresses and the slack bytes at new addresses, symbol by symbol and over the whole image. Both edits pass with zero original-symbol errors, zero slack-symbol errors, zero whole-image byte errors, and zero non-relocation byte differences. Alignment and overlapping placed ranges are checked independently. Original `p3.c --pipeline` and its `-fopenmp` build also pass for A and both B datasets.

The requested literal “same relocated bytes, only addresses differ” check **fails as expected**: 184,545 A symbols differ from their original bytes; edit-1 B also differs in 184,545 and edit-2 B in 181,066. These differences occur entirely in relocation fields. Claiming literal equality would mean retaining stale pointers/displacements. Negative checks deliberately corrupt a relocation byte, misalign an address, and overlap two addresses; all are rejected, and the restored exports pass.

No random-diff fuzzing, new-package allocator, spill handling, regeneration of omitted metadata, or runnable-binary check is claimed. Inputs whose required capacities do not fit fail an assertion. Supporting a new larger alignment, changed fixed-symbol membership, or spill placement is further work. Frozen generated-region anchors and folded constant relocations are part of this flat-P3 experiment's inputs, not proof that a complete Go linker can leave those regions unchanged.

### Time: five interleaved rounds, one core

Measured 2026-11-16 **20:30:47–20:31:45 UTC**, pinned to CPU 0, `OMP_NUM_THREADS=1`, `gcc 13.3.0 -O3`, ordinary pages, each run under `prlimit --as=8589934592` (8 GiB). `uptime` one-minute load **3.35–4.18** (raw logs include all three averages). Each incremental process accumulates ≥1 second separately in forward and reverse updates; each reference process repeats the entire 3a+propagate+3b pipeline in-process for ≥1 second. Numbers are min/median/max of the five process means, not minima of individual micro-iterations.

`p3bench.c` includes the original p3 comparator/constants and copies its layout and relocation loops, with pipeline canonical-address propagation. Input mapping and final checks are outside timing; layout scratch, address-table allocation/propagation, fresh zero-image allocation, first touches, and frees are inside every repetition. `p3parbench` builds the same code with `-fopenmp`, using the reference's dynamic symbol loop at one thread. The original programs are separately checked, not silently replaced by an unrelated baseline. These full-pipeline numbers include O(nids) address propagation absent from GRID's isolated 3a/3b times.

| Operation | Edit 1 min / median / max (ms) | Edit 2 min / median / max (ms) |
|---|---:|---:|
| Incremental A→B | 0.000434 / **0.000443** / 0.000464 | 93.073 / **95.076** / 113.536 |
| Incremental B→A | 0.000416 / **0.000430** / 0.000444 | 96.821 / **102.583** / 124.127 |
| Plain p3 pipeline, full B | 102.155 / **112.790** / 125.975 | 88.051 / **98.921** / 114.487 |
| p3par pipeline, full B, 1 thread | 95.977 / **98.999** / 105.635 | 80.156 / **81.270** / 93.542 |

The addition is orders of magnitude cheaper once state and diff exist. The deletion overlaps the full-recompute ranges: **no measured win**; its median is slower than p3par's. Reverse timings are provided to show maintained state survives alternating updates; the full baselines rebuild B, not A, so do not use the reverse column as a matched B→A speedup. Setup in the check run is 772 / 803 ms, including input loading, normalized records, diff/index construction, capacity planning, two full expected images, and the mutable A image. It is excluded from the sub-microsecond claim.

## (4) Reproduce

All commands run in this clone. Dataset inputs are read-only under `/home/exedev/paradigms/data`. Only scratch/export/build files below `build-relayout/` are written. `DATA_ROOT` can select another read-only dataset parent.

```sh
# gcc -O3 builds, per-bucket census, both slack updates/directions,
# independent relocation/image checks, original serial/OpenMP pipeline checks,
# and three negative checks. All C runs use the 8 GiB address-space cap.
ref/flat/check-p3inc.sh > results/c-relayout-check.txt 2>&1

# Five interleaved rounds, each kernel >=1 s, one pinned core;
# CPU defaults to the first allowed CPU; CPU=... overrides it.
ref/flat/bench-p3inc.py 5 > results/c-relayout-bench.txt 2>&1

# Individual maintained update, force policy 5 = next 4 KB in both edits.
prlimit --as=8589934592 build-relayout/p3inc   /home/exedev/paradigms/data/minio /home/exedev/paradigms/data/minio-edit2 1 5
```

Sources: `ref/flat/p3inc.c`, `p3compare.c`, `p3inc_check.c`, `p3bench.c`; runners `check-p3inc.sh`, `check-p3inc-negative.py`, `bench-p3inc.py`. Raw evidence: [checks](c-relayout-check.txt), [benchmarks](c-relayout-bench.txt). Independent design/correctness review and reference-harness details: [review](c-relayout-review.md), [reference notes](c-relayout-reference-notes.md). No changes to ATTACKS.md or the datasets.

## ATTACKS section 5 — Observation 4 draft

**Observation 4: layout must retain space as well as identities; package slack makes the addition local but does not make the deletion cheap** ([results/c-relayout.md](c-relayout.md), C, 1 core, five interleaved rounds). Today's layout moves **261,723 / 352,863** common placed symbols after edits 1 / 2, changes **1.23 M / 1.82 M** relocation values and **35.1 / 76.7 MB** of produced image bytes. Retained package/bucket arenas (next 4 KB, global typelink arena) reduce this to **3 / 101,483** moved addresses, **34 / 869,439** repatches and **1,013 B / 32.9 MB** changed bytes. A maintained reverse target→site index updates edit 1 in **0.443 µs**, versus **112.8 ms** for full p3; edit 2 takes **95.1 ms versus 98.9 ms**, overlapping ranges: no demonstrated win. Padding stops cross-package cascades but leaves within-package compaction and typelink sorting; the conservative policy costs **58.8 MB of produced-region slack**, plus **17.4 MB** for BSS/counters. Incremental and from-scratch slack images agree byte for byte, and an independent evaluator checks relocation semantics; literal old relocated bytes cannot remain identical at new addresses. Next: smaller/shared spare reservations and stable placement inside large packages and the global type list, with the omitted linker-generated metadata brought into the maintained state.

## Attempt 2: holes, no compaction — 2026-11-16

The earlier 404,622 typelink repatches were not stores to a small sorted table. Bucket 6 holds full type descriptors: 32,424 moved descriptors account for 404,617 actual relocation stores (420,604 records minus 15,987 missing-target ADDROFF skips), and five reverse-index sites in four unmoved descriptors account for the remaining five. A has 3,416,136 bytes of these descriptors, containing 428,898 relocation records. See [reproducible diagnosis](c-relayout2-typelink.md) and [raw counts](c-relayout2-typelink.txt).

The new experiment keeps descriptors at stable addresses and maintains a separate sorted array of 32-bit offsets from the types anchor. Its capacity is 133,044 bytes; deletion writes 129,712 bytes of offsets and clears a 3,332-byte suffix. Restoration writes 133,044 bytes. Edit 1 does not change this table. This is an exact alternative lookup representation **under the new rule**, not ABI compatibility: the frozen hlink writes a contiguous sorted descriptor span into moduledata (`ref/hlink/layout.c`, `ref/hlink/special.c`). A runtime consumer and generated metadata change would be required for an executable. Those changes remain outside this P3 experiment.

### Allocation rule and space

`p3holes.c` gives each existing arena 64 bytes of tail slack (rounded only for alignment), and each bucket a shared 64 KiB spare extent. These reservations depend on A, not on either edit. A common symbol retains its address and allocation capacity if its arena, size and alignment still fit. Shrinking retains capacity. Removal, or growth beyond capacity, releases the old slot; allocation takes the first aligned hole in its own arena, then its tail, then the shared bucket spare. Free lists coalesce adjacent holes, including overflow allocations owned by that arena. All frees precede allocations, in ascending unified ID. There is no compaction. The one necessary growth move is edit 1's 266→532-byte function; edit 2 grows no common allocations.

Initial reserved slack is **2,313,220 bytes total (2.113% of the original 109,486,380-byte image)**, including **1,817,765 produced-region bytes (1.660%)**. This replaces 76,203,036 / 58,847,229 bytes respectively. The complete virtual check image is 145,913,440 bytes; total slack is 1.585% of that span. The additional offset table has 133,044 bytes of capacity, counted separately from arena slack. Free overflow stays owned by its arena; new packages and exhausted fixed spare extents fail checks rather than moving neighbors. This is a measured policy for these histories, not a general capacity guarantee.

### Correctness and scope

The from-scratch rule is now history-dependent: start with compact A plus its reservations, then apply the same deterministic allocation decisions for every transition. The reference reconstructs allocator state from A, discovers replacements by full scans, and renders every final symbol into a fresh zero image. Incremental and reference results agree over all 145,913,440 image bytes, all addresses and capacities, exact hole lists, arena tails, shared-spare cursors, and the offset table. Shared allocation primitives are deliberate; this is not a second independent allocator implementation.

The existing `p3inc_check.c` independently evaluates raw relocation inputs and checks original oracle bytes, new symbol bytes, whole-image bytes, alignment, overlap and non-relocation bytes. It passes both edits and every endpoint of A→B2→A→B1→A. A separate checker derives table ordering from raw type strings and verifies all offsets and the zero suffix. Existing corruption negatives (relocation byte, misalignment, overlap) pass for both edits; an added corrupt-table negative is rejected. The original serial/OpenMP p3 pipelines still pass A and both edits, and the complete previous attempt's check runner also passes unchanged. A capped UBSan allocator test checks 20,000 deterministic random allocation/free steps plus explicit growth, shrink, overflow, coalescing and zero-size retention cases. Review found and fixed a zero-size common-symbol movement bug. [Review](c-relayout2-review.md), [implementation details](c-relayout2-implementation.md), [new checks](c-relayout2-check.txt), [unchanged legacy checks](c-relayout2-legacy-check.txt).

Setup still loads and normalizes both versions, builds the reverse index and prepares symbol replacement lists and sorted membership outside timing. Updates perform actual allocation, reverse-index mutation, clears, copies, relocations and table stores. There is no precomputed image patch. The ID maps for B1 and B2 are each relative to A; the tested chain goes through A between them, with no simultaneous live B1/B2-only symbols. This does not establish a general identity map between arbitrary edited versions.

### Mechanism and fragmentation

Fresh-history counts below are from the first pair or first chain. “Image changed” excludes the separate offset table; table changes are 0 bytes for edit 1, 82,779 bytes for the first deletion and 83,104 bytes on its return (the table is rewritten with 133,044 bytes of stores in either direction). “Written” counts clear + copy + relocation stores + table stores, including repeated writes; it is not unique changed bytes or measured DRAM traffic.

| Transition | Common moved | Repatches | Image changed B | Table written B | Total written B | Reverse sites visited |
|---|---:|---:|---:|---:|---:|---:|
| A→B1 | 1 | 12 | 755 | 0 | 884 | 4 |
| B1→A | 0 | 5 | 514 | 0 | 846 | 0 |
| A→B2 | 0 | 45,336 | 20,807,401 | 133,044 | 23,011,289 | 937 |
| B2→A | 0 | 82,848 | 20,807,505 | 133,044 | 23,164,467 | 14,724 |
| chain A→B2 | 0 | 45,336 | 20,807,401 | 133,044 | 23,011,289 | 937 |
| chain B2→A | 0 | 82,848 | 20,807,505 | 133,044 | 23,164,467 | 14,724 |
| chain A→B1 | 1 | 12 | 755 | 0 | 884 | 4 |
| chain B1→A | 0 | 5 | 514 | 0 | 846 | 0 |

The deletion clears 21,877,879 bytes, copies 791,390, and writes 208,976 relocation bytes; restoration clears 795,768, copies 21,841,851, and writes 393,804 relocation bytes. The remaining bulk work is deletion/restoration of payload, rather than movement of surviving descriptors. Hole zeros are included in the whole-image check.

| First chain endpoint | Free holes | Hole bytes | Unused arena tails B | Unused bucket spare B | Unused retained live capacity B |
|---|---:|---:|---:|---:|---:|
| chain A→B2 | 1,077 | 21,090,008 | 1,327,410 | 978,645 | 0 |
| chain B2→A | 756 | 16,210 | 1,325,905 | 971,088 | 17 |
| chain A→B1 | 757 | 16,498 | 1,325,889 | 970,544 | 17 |
| chain B1→A | 758 | 16,514 | 1,325,889 | 970,544 | 273 |

Returning A does not restore its original compact addresses for symbols that were removed and later added. Common symbols that remain live and fit their retained capacity stay fixed. After repeated chains the allocator reaches a checked fixed point: final A has 766 holes totaling 16,514 bytes, 1,325,889 unused tail bytes, 970,544 unused shared-spare bytes, and 273 unused bytes retained inside live allocations. Repeated deletion exposes 1,116 holes totaling 21,099,374 bytes. The first chain's final 758 holes become 766, but hole bytes do not increase; no unbounded fragmentation is observed in these repeated histories. This is not evidence for arbitrary edit sequences.

Warm edit 1 uses the already enlarged function slot: forward movement falls from 1 to 0, repatches from 12 to 8, changed image bytes from 755 to 514, writes from 884 to 868, and reverse visits from 4 to 0. Its return remains 0 moved / 5 repatches / 514 changed / 846 written / 0 visits. Warm edit 2 has 0 moved and the same repatch/write/visit counts as above; changed bytes become 20,807,505 in both directions. The repeated chain has those same warm mechanism counts. Timing below measures these maintained histories, not the initial growth move.

### Timing: five interleaved rounds on one core

Measured **2026-11-16 20:50:31–20:53:14 UTC**, pinned CPU 0, `OMP_NUM_THREADS=1`, GCC 13.3.0 `-O3`, `prlimit --as=8589934592`, one-minute uptime load **3.21–3.72**. Every reference process runs its pipeline for ≥1 second; pair processes accumulate ≥1 second separately in each direction; each chain process accumulates ≥1 second across complete four-step cycles. Five process means are reported as min / median / max, in milliseconds. Setup, full-history replay, independent checks and reporting are excluded. Initial correctness work overlapped the start of the window; no quiet-machine selection was used. The same `p3bench.c` and OpenMP build as attempt 1 provide the full 3a+propagate+3b baselines, now including full A for a matched reverse comparison.

| Operation | Edit 1 min / median / max ms | Edit 2 min / median / max ms |
|---|---:|---:|
| Maintained A→B | 0.000144 / 0.000151 / 0.000157 | 4.448853 / 4.881273 / 5.781769 |
| Maintained B→A | 0.000123 / 0.000129 / 0.000143 | 6.497711 / 6.961310 / 8.058126 |
| Full p3 B | 101.240832 / 102.155579 / 108.474510 | 84.920457 / 87.484673 / 89.899610 |
| Full p3par B, 1 thread | 93.463467 / 96.788175 / 111.563251 | 76.113474 / 80.541478 / 82.198053 |
| Full p3 A (reverse baseline) | 100.851895 / 104.378179 / 128.794972 | 101.047109 / 102.753216 / 108.308166 |
| Full p3par A, 1 thread | 92.711534 / 93.667449 / 113.285430 | 93.617489 / 95.509769 / 98.521815 |

The deletion is now **17.9× faster than full p3 and 16.5× faster than one-thread p3par** by medians, with non-overlapping ranges. Restoration is 14.8× / 13.7× faster than its matched full-A baselines. The standalone addition costs 0.151 µs forward and 0.129 µs reverse after its enlarged slot is retained. These are prepared-diff maintenance times, not end-to-end source-edit times.

| Repeated chain step | Min / median / max ms |
|---|---:|
| A→B2 | 4.420412 / 4.656871 / 5.045008 |
| B2→A | 6.389357 / 7.003920 / 7.364053 |
| A→B1 | 0.004286 / 0.004326 / 0.004955 |
| B1→A | 0.000956 / 0.000989 / 0.001438 |

The chain's addition is slower than the isolated repeated pair despite identical warm mechanism counts: its state follows the large deletion/restoration work instead of an uninterrupted sub-microsecond loop. The chain remains sequential, retaining its allocator and reverse index throughout.

For transparency, the first transition in each of the five fresh pair processes was also timed once (these individual first-update observations do not each accumulate a second):

| First-history transition | Min / median / max ms |
|---|---:|
| A→B1 | 0.022324 / 0.023406 / 0.025760 |
| B1→A | 0.006169 / 0.006640 / 0.007411 |
| A→B2 | 7.041479 / 7.864617 / 7.954365 |
| B2→A | 7.305309 / 8.906149 / 11.965453 |

### Reproduce attempt 2

```sh
# Old checks remain unchanged and continue to pass.
ref/flat/check-p3inc.sh > results/c-relayout2-legacy-check.txt 2>&1
# New rule, both directions, full chain, independent evaluator/table checks,
# original p3/p3par pipelines, allocator UBSan tests, all negatives.
ref/flat/check-p3holes.sh > results/c-relayout2-check.txt 2>&1
# Five interleaved rounds, pinned core, 8 GiB limit; pair and chain timings.
ref/flat/bench-p3holes.py 5 > results/c-relayout2-bench.txt 2>&1
# Exact attribution of the old 404,622 stores.
prlimit --as=8589934592 python3 ref/flat/diagnose-typelink.py \
  /home/exedev/paradigms/data build-relayout/check-edit2
```

All outputs/builds are inside this clone; data inputs are read-only. [Raw benchmark evidence](c-relayout2-bench.txt). The final check rebuild also includes restored dataset/fixed-anchor preconditions; these are outside the timed update and do not change the benchmark kernel. No ATTACKS.md changes or push are made.

### Updated ATTACKS section 5 — Observation 4 draft

**Observation 4: retain individual allocations, not just package reservations; deletion becomes local in addresses but still writes its removed payload.** Attempt 1's package compaction and globally sorted type descriptors moved 101,483 common symbols and required 869,439 repatches after edit 2, costing 95.1 ms. A history-defined allocator with holes, 64-byte arena tails and shared 64 KiB bucket spares moves **zero common addresses**, performs **45,336 repatches**, changes **20.8 MB** of image bytes, and writes **23.0 MB** including clearing deleted payload and rewriting a **133,044-byte sorted offset table**. Five interleaved one-core rounds measure **4.88 ms versus 87.48 ms full p3 / 80.54 ms p3par at one thread**; restoration is **6.96 ms versus 102.75 / 95.51 ms**, with non-overlapping ranges. Initial produced slack falls from **58.8 MB to 1.82 MB (1.66% of the original image)**; total slack including virtual buckets is **2.31 MB (2.11%)**. Edit 1 moves only its growing function on the first application, then runs in **0.151 µs** with no moves after retaining the larger slot. The A→B2→A→B1→A chain passes history-replay equality and independent relocation/table evaluation; its final free holes total **16,514 bytes**, with **273 bytes** retained unused inside live allocations. The old typelink repatches were relocations inside moved descriptors, not 4-byte table entries. Stable descriptors plus a sorted offset sidecar require a runtime lookup-format change for this frozen toolchain; these are checked P3 images, not executable binaries. Next boundary: generated metadata and consumer integration, plus spare growth and fragmentation under broader edit histories.
