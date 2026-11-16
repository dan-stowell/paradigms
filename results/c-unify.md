# Stable symbol identity in plain C (ATTACKS 5, "first make symbol identity stable")

**Result.** `ref/flat/unify.c` computes the same B -> unified id map as `systems/ddflow`'s `facts::unify`, and the
same method-string map. Both are **byte-identical** to `data/minio-edit{,2}/diff/{idmap_b,strmap_b}.u32` for both
edits, in every mode (1 core, OpenMP 8, fingerprints, exact). On 1 core, with the data in memory, it takes
**35 ms for edit 1 and 51 ms for edit 2**. End to end, mapping all 716 MB of columns from the page cache, it takes
**84 / 115 ms**. ddflow takes 0.7-1.1 s for the same mapping. Comparing every object's key columns accounts for
almost all of the 35 ms. That comparison is a memory-bandwidth floor: 714 MB at about 21 GB/s.

If the extractor also wrote a 64-bit fingerprint per object, the link would compare 1,421 numbers. Unify would
then cost **2.3 ms for edit 1 and 20 ms for edit 2** warm. End to end it would cost **11 ms and 44 ms**, reading
only the pages it touches. What remains is per-slot work in the one object that changed: one hash and one table
operation per slot on both sides. For edit 2 that object is `cmd`, which has 319 K slots. The rest is writing a
20.8 MB map.

Before this: ATTACKS 5 Observation 1 put identity recovery at 0.7-1.1 s (Rust, `HashMap` per differing object,
reading the name and key columns of both dumps). That was 20-500x the maintained-reach updates it feeds (3 µs and
5-10 ms in `results/c-incremental.md`).

## What the program does

    gcc -O3 -march=native -o build/unify ref/flat/unify.c                      # 1 core
    gcc -O3 -march=native -fopenmp -o build/unify_omp ref/flat/unify.c         # fast path in parallel
    build/unify data/minio data/minio-edit2 --check                            # -> byte-identical x2, CHECK OK
    build/unify data/minio data/minio-edit2 --stats --reps 0                   # the key census below

The key of a slot is the same as in ddflow: (kind, name bytes, 128-bit name key, 128-bit content hash, rank if
unnamed). Objects pair by index.

1. **Fast path, per object.** If the slot counts are equal and the key column ranges are equal, the object maps by
   position. Equality is tested with `memcmp` over kind, name key, content hash and name bytes. One fused loop
   covers name boundaries and the rank of unnamed slots. Of 1,421 objects, 1,420 take this path in both edits.
   `--fp` replaces the test with a fingerprint per object. The fingerprint is a hash of the same columns, computed
   outside the timing, as an extractor would write it.
2. **Slow path, per differing object.**
   - **Key classes.** Each slot gets a key class (equal class means equal key). For each slot on both sides, the
     program computes a 64-bit hash in a sequential pass. It hashes name bytes only when the slot has no 128-bit
     name key. A u32 open-addressing table with prefetch assigns dense classes in A order.
   - **Common prefix and suffix.** B slots in the common prefix and suffix take their A counterpart's class with
     no lookup.
   - **Verification.** Every other member is checked against its class representative with a full key compare. A
     64-bit hash collision would therefore be detected, and the object would fall back to `classifyExact`, which
     compares keys on insertion (`--exact` forces this path; it gives the same bytes).
   - **Matching (ddflow's rules).**
     - A class that occurs once on each side matches, if it is named or content-hashed.
     - The rest goes to Myers' O(ND) diff on class ids. This is a line-for-line port of ddflow's forward pass and
       backtrack, so ties break the same way.
     - Unmatched B slots get ids from `nids_a` in B order.
3. Linker-made ids map by position. `p2_str` strings map by content (18,937 entries in a small table).

## Correctness

`ref/flat/bench-unify.sh` checks before it times. It runs 12 checks (2 edits x 6 modes: c1, c1fp, c1lazy, exact,
omp8, omp8fp), and every one reports `0 differ (byte-identical)` for both `idmap_b.u32` (5,205,438 and 5,205,398
entries) and `strmap_b.u32` (18,937 entries). The checks are in `results/c-unify-bench.txt`. There were no
differences to explain. Two things had to match ddflow exactly:

- **The key.** Rank is part of the key only for unnamed slots.
- **Myers' tie-break.** The match is the shortest edit script with ddflow's tie rule. The common-prefix shortcut
  in step 2 does not change the result. It only assigns a class to a slot whose key was already proven equal by
  position. It does not skip uniqueness counting or Myers.

The `--fp` modes trust a 64-bit fingerprint per object. A collision would make a changed object map by position:
the probability is about 1,421 / 2^64 per link. Everything else is exact.

## Mechanism counts (edit 1 / edit 2)

| | edit 1 | edit 2 |
|---|---|---|
| objects that differ | 1 of 1,421 (amztime) | 1 of 1,421 (`cmd`) |
| slots in differing objects (A + B) | 168 + 171 | 318,974 + 318,937 |
| common prefix / suffix | 23 / 2 | 32,775 / 18,964 |
| matched by a unique key | 89 | 142,581 |
| Myers input (A + B), total D | 79 + 82, D = 31 | 176,393 + 176,356, D = 125 |
| matched by Myers | 65 | 176,312 |
| A only / B only (fresh ids) | 14 / 17 | 81 / 44 |
| table probes, full-key verifications | 341, 171 | 600,160, 434,487 |
| hash-collision fallbacks | 0 | 0 |
| bytes compared by the fast path (both dumps, all key columns) | 714 MB | 714 MB |
| bytes the slow path reads (key columns of the differing object, both sides) | about 20 KB | about 46 MB |
| pages touched end to end, fingerprint mode, lazy mapping (minor faults) | 5,226 (5,083 are the 20.8 MB output) | 13,229 |

## Times

5 interleaved rounds (every mode and edit once per round), 2026-11-16 20:32-20:34Z, `prlimit --as=8G`. Load was
**3.4-4.7** (a Codex benchmark was running in `/home/exedev/paradigms-c` at the same time), on an AMD EPYC 9554P VM
with 8 vCPUs. Times are in ms, min / median / max.

- **Warm**: the inputs are mapped, and unify repeats until at least 3 iterations and 1 s; the time is per
  iteration.
- **End to end**: map the columns from the page cache, then run one unify, in one process.

| edit | mode | warm per unify | of which fast path | of which slow-path classes | of which Myers | end to end | of which mapping |
|---|---|---|---|---|---|---|---|
| 1 | **c1** (1 core) | 34.0 / **35.3** / 37.2 | 34.4 | 0.02 | 0.003 | 78.5 / **83.8** / 103.5 | 40.1 |
| 1 | c1fp (fingerprints given) | 2.28 / **2.33** / 2.41 | 1.50 | 0.01 | 0.002 | (maps all, see c1lazy) | |
| 1 | **c1lazy** (fingerprints in a file, lazy map) | 2.34 / 2.35 / 2.38 | 1.52 | 0.01 | | 10.4 / **10.9** / 11.1 | 0.25 |
| 1 | omp8 | 8.8 / **15.5** / 21.1 | 14.5 | 0.02 | | 56.8 / 64.2 / 73.7 | 46.1 |
| 1 | omp8fp | 1.56 / **1.58** / 3.32 | 0.73 | 0.01 | | | |
| 2 | **c1** | 48.7 / **50.7** / 55.8 | 33.2 | 15.8 | 0.54 | 114.3 / **115.2** / 129.6 | 41.5 |
| 2 | c1fp | 19.2 / **19.4** / 21.1 | 1.43 | 16.0 | 0.56 | | |
| 2 | **c1lazy** | 19.5 / 20.1 / 21.7 | 1.45 | 16.5 | 0.55 | 44.0 / **44.3** / 61.9 | 0.24 |
| 2 | omp8 | 28.0 / **33.2** / 34.0 | 14.3 | 16.1 | 0.53 | 88.7 / 92.4 / 94.2 | 44.7 |
| 2 | omp8fp | 18.6 / **20.1** / 20.5 | 0.86 | 17.1 | 0.55 | | |

The full table, with min/median/max for every column, comes from `ref/flat/summarize-unify.py
results/c-unify-bench.txt`.

**Reading the table**

- **The fast path is memory bandwidth.** On 1 core, 714 MB takes 34 ms (about 21 GB/s). On 8 threads it takes
  8-15 ms (50-90 GB/s on a shared machine). At this load, 8 threads help less than 3x.
- **The fingerprint fast path costs 1.5 ms.** That is the cost of writing the 20.8 MB map. Comparing 1,421
  fingerprints costs microseconds. The c1lazy edit-1 end to end (10.9 ms) is mostly first-touch page faults on
  that output array. A map stored as 1,421 per-object offsets, plus explicit entries for the changed objects,
  would make it about 0.
- **The slow path scales with the changed object, not with the edit.** It costs about 25 ns per slot: 16 ms for
  `cmd`'s 638 K slots, against an edit that changed 141 symbols. Go compiles a package into one object, so the
  unit of change is the package. Myers is cheap (0.5 ms) because D is small.
- **Slow-path history.** A first version compared full keys on every table insert (an exact table of 20-byte
  entries). It took 95 ms on edit 2's slow path. A per-slot hash, a u32 table with prefetch, and one verification
  pass brought it to 16 ms.

## What is irreducible, and what an extractor would need

**Cost per link today.** With two dumps and nothing else, the floor is reading both dumps' key columns, about
700 MB. At memory bandwidth that is 34 ms on 1 core. Fingerprints change the picture:

- **Changed objects.** With a fingerprint per object (the build cache already has a content hash per package),
  finding the changed objects is O(objects).
- **Inside a changed object.** Exactly ddflow's mapping needs one hash and one table operation per slot of the
  changed object, on both sides. Uniqueness is a property of the whole object, so the work is not limited to the
  edited region. This is the same kind of work as problem 1a's join (one hash per named key), but only over the
  changed package: 638 K slots and 16 ms here, against 1a's 5.2 M slots.
- **Writing the map.** Then the 20.8 MB map is written (1.5 ms warm, about 10 ms on fresh pages). A per-object
  offset form would avoid most of that.

**An extractor that emits stable ids directly.** Here the link-time cost becomes zero: the diff is the edit
(31 / 141 symbols). The census below is from `--stats` on `data/minio` (5,205,427 slots; edit 2's dump is the
same to within the edit):

| slot class | count | share | stable key |
|---|---|---|---|
| named or content-hashed, key unique in its object | 2,381,070 | 45.7% | itself: (package, name, ABI) or the content hash. Kind 1 169,515, kind 2 47,874, kind 3 896,724 named + 575,091 hash-only, kind 4 544,887, kind 5 146,979 |
| key repeated within its object | 1,163,748 | 22.4% | (key, ordinal among equals). Kind 3: 1,160,805 content-hashed copies with the same name and hash, so they are identical content and any pairing is equally right. Kind 4: 2,927 named definitions, which are DWARF `go:info.…$abstract` entries repeated in one object (for example `cmd.(*rebalanceMetrics).log.1$abstract` 4 times). Kind 1: 16 |
| unnamed, no content hash, kind 5 (references) | 705,871 | 13.6% | **none needed**: every one is a debug-only reference (`p1_flags` bit 1), with canonical id 0 and never live. The extractor can drop them or key them by referrer |
| unnamed, no content hash, kind 1 (definitions) | 954,738 | 18.3% | **none in the flat files**. 656,913 are targets of no relocation and no funcdata, 280,632 only of other unnamed slots, and 17,193 of a named slot's relocation. 80,955 unnamed slots have a (kind, rank) that happens to be unique in their object |

**What has no key.** Ignoring the debug-only references, only one class lacks a stable key: the 954,738 unnamed
definitions, about 18% of slots. ddflow called these "unnamed DWARF range/location symbols", and they are exactly
what Myers aligns. A stable id for them needs a parent: (the named symbol that owns it, aux type, ordinal).

**Where the parent could come from (not checked).** In Go objects such symbols are probably aux symbols: DWARF
info, loc, ranges and lines, and pc tables, attached to a function through its Aux records. hlink already reads
those Aux records (`ref/hlink/common.h` `oAuxs`), but `dumpflat` writes only the funcdata ones. Writing
(aux type, target) for every aux record would provide the parent. This hypothesis is untested here.

**Duplicates.** The 1.16 M repeated keys need only an ordinal. For content-hashed copies the choice among them
cannot change content, only which copy's id a relocation names.

## Files

- `ref/flat/unify.c` is the program. It takes `--check`, `--stats`, `--exact`, `--fp`, `--fpdir DIR` and
  `--write DIR`.
- `ref/flat/bench-unify.sh [ROUNDS]` runs the checks, then the interleaved timing. Its output is in
  `results/c-unify-bench.txt`.
- `ref/flat/summarize-unify.py results/c-unify-bench.txt` prints the min/median/max table.
