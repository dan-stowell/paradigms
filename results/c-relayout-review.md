# Independent design audit for C incremental relayout

Read-only inspection of `ref/flat/p3.c`, `ref/flat/README.md`, dataset metadata,
and the unified ID maps, 2026-11-16. No input was changed.

* **An unchanged symbol's relocated bytes generally cannot remain identical when
  its address or a target's address changes.** Absolute relocations encode the
  new target, and PC-relative relocations encode the new target minus the new
  site. Thus the literal request that slack A reproduce p3.c's relocated bytes
  "same bytes, only addresses differ" is incompatible with nontrivial slack.
  A meaningful check is original-layout exact oracle equality, slack equality
  outside relocation fields, and independently recomputed relocation values
  under the new addresses. The report must distinguish these checks explicitly.
* **Unit indices are not stable identity.** Joining common placed symbols through
  `idmap_b.u32` finds zero changed `p3_unit` values for edit 1, but 95 for edit 2,
  all in object 1. Removing slots changes which side of an 8192-slot boundary
  these symbols occupy. Object/package grouping avoids that artifact. If using
  extractor unit IDs, the changes must count as layout changes; alternatively
  freeze units by A's unified IDs and specify where new symbols go.
* **Slack can change bucket and section bases.** Eight ordered section bases in
  `meta.txt` determine kind 3/4 relocation subtraction. They cannot remain the
  old numeric values if padding moves sections. Fixed symbols must likewise have
  a stated location policy; edit 2's seven fixed symbols all move with noptrdata
  in the ordinary extraction. Do not accidentally use B's ordinary addresses in
  a persistent slack layout.
* **Weak fallback targets matter.** A has 1,297 absent-target kind-2 relocations
  and 83 absent-target kind-6 relocations. `addr_unreachableMethod` and
  `addr_mapinitnoop` must refer to the slack addresses of those functions.
* **Typelinks use a different ordering rule.** Bucket 6 is globally sorted by
  type-string bytes, length, then symbol ID. Package-local typelink ordering is
  a semantic change; freezing the whole typelink bucket as one allocation unit
  preserves its ordering but potentially moves all following typelinks within
  that unit after deletions. The produced regions omit generated pclntab,
  GC bitmaps, typelink metadata, moduledata, ELF headers; a valid P3 experiment
  is not proof of an executable incremental binary.
* **Reverse indexing must use resolved canonical targets.** Resolve aliases with
  `oracle/p1c_canon.u32` before indexing (or maintain alias reverse mappings),
  including canonical-target changes even when the raw target ID stays fixed.
  Repatch moved sources for PC-relative writes, deleted/changed sites, moved
  targets, changed addends/kinds/widths, fixed symbols, weak fallbacks, and
  section-base changes. A broad changed-unit recopy is correct but should be
  counted as such, rather than described as only changed-symbol copies.
* **Clearing matters.** Shrinking/deleted symbols leave old bytes; if from-scratch
  slack images start zeroed, the update must zero retired bytes and padding that
  used to be occupied. Comparing only surviving symbol contents misses this.
* **A repeated benchmark must measure an update.** Reapplying an already compiled
  patch to already-B memory measures replay, not discovery/index maintenance.
  Either alternate A/B with maintained reverse state or restore A outside timing,
  then time changed-unit layout, address propagation, dirty-site discovery, byte
  copies/clears, patches, and reverse-index updates. Report excluded diff creation
  and input-loading costs separately.

Reproduce the unit-identity observation with Python's `array('I')`: load A's
`p3_sym.u32` and `p3_unit.u32`, index them by symbol ID, then for every B placed
symbol compare its `p3_unit` with A's at `idmap_b[B_symbol]` when present. The
relocation-kind observations are counts of A's `p3_rel_kind` equal to 2 or 6
with nonzero `p3_rel_tgt` and zero `p3_addr_all[target]`.

## Implementation review and independent executable check

`ref/flat/p3inc_check.c` independently evaluates the eight relocation kinds. It
reads original flat inputs and exported slack addresses/images/anchors; it does
not include `p3inc.c`. It reconstructs each original-layout symbol and compares
with `oracle/p3_image.bin`, reconstructs the complete zero-filled slack image,
compares every slack image byte, checks symbol alignment and non-overlap, and
separately counts differences inside/outside relocation fields. Target aliases
are resolved independently through the supplied final canon and unified ID map;
weak fallbacks are found from original function addresses and translated through
the exported placed-symbol address map.

The final minimum-bucket-alignment revision was rebuilt with `gcc -O3`, using
policy 5 (4096 bytes) for both edits. Both original and slack checks passed:

| dataset | original symbol failures | slack whole-image byte failures | literal symbol byte mismatches versus original | non-relocation byte mismatches |
|---|---:|---:|---:|---:|
| A | 0 | 0 | 184,545 | 0 |
| edit 1 | 0 | 0 | 184,545 | 0 |
| edit 2 | 0 | 0 | 181,066 | 0 |

This proves relocation semantics, not binary executability or correctness of
regenerating omitted linker metadata. Exported section/fixed anchors are the
chosen layout's inputs to this independent evaluator; their intended placement
policy still comes from `p3inc.c` and the report.

Three negative checks alter only scratch exports, restore them in a `finally`
block, and require rejection: one relocation byte, a misaligned placed address,
and overlapping placed addresses. All three were rejected, and the restored
exports passed. Reproduce with:

```sh
gcc -O3 -o build-relayout/p3inc-review ref/flat/p3inc.c
gcc -O3 -o build-relayout/p3inc_check ref/flat/p3inc_check.c
mkdir -p build-relayout/check-edit1 build-relayout/check-edit2
A=/home/exedev/paradigms/data/minio
for edit in 1 2; do
  B=/home/exedev/paradigms/data/minio-edit
  if [ "$edit" = 2 ]; then B=${B}2; fi
  env P3INC_EXPORT=build-relayout/check-edit$edit prlimit --as=8589934592 \
    build-relayout/p3inc-review "$A" "$B" 0.01 5
  prlimit --as=8589934592 build-relayout/p3inc_check \
    "$A" "$B" build-relayout/check-edit$edit
done
python3 ref/flat/check-p3inc-negative.py build-relayout/p3inc_check \
  "$A" /home/exedev/paradigms/data/minio-edit build-relayout/check-edit1
```

Review prompted fixes for minimum bucket alignment and raw-zero weak targets.
The implementation deliberately freezes A's generated-region gaps and fixed
anchors, and retains separate preloaded A/B symbol records. It updates reverse
lists for changed symbols inside the timed operation. Diff extraction, full
input comparisons, record construction and arena planning are setup costs.
The scope remains these two measured diffs: no random-diff fuzzer or dynamic
allocation/spill implementation has been checked. A general implementation must
also handle a new alignment larger than A's arena alignment, new packages,
changed fixed-symbol membership, and spills.
