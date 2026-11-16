# Independent CP7 review

Reviewed `ref/p4/p4runs.c` independently of the author. Reproduction:
`results/cp7-review-check.sh /home/exedev/paradigms/data/minio-p4`.
Captured output: `results/codex-cp7-review-check.log`. The script builds its own
UBSan binary under `/tmp`, runs A and C with eight threads requested but a two-thread
limit, then changes only one oracle function offset in a symlinked copy under `/tmp`.
All executions have a 12 GiB address-space cap; the source dataset is read-only.

## Checks and findings

- A and final C pass UBSan and reproduce all 60,733,909 bytes, all 136,635 function
  offsets, and all 1,780,281 sorted relocations. Requested eight threads correctly
  becomes an actual team of two. The negative offset check exits 1 with `P4 FAIL`
  despite unchanged code bytes and relocation oracle. Thus function-offset checking
  is unconditional, unlike the inherited p4/p4par checker.
- An earlier independent normal build also checked A/B/C/D at eight threads with
  `P4_HUGE=0`; all passed the full oracle. At that point C used only normalized
  bytes and hole descriptors as its key; final C's stricter key was checked again
  with the separate UBSan build above. These one-repeat runs are correctness tests,
  not performance evidence.
- A contains at most one original item with holes/relocations per run, attaching
  adjacent hole-free items to it without crossing function boundaries. B/C create
  exactly one run per function, including empty functions. Hole locations are
  rebased into each run; rel32 targets remain original-item addresses plus their
  original auxiliary offsets. The timed loop still computes the relative value.
- Final C checks original item kind and node-template-ID/segment sequence, plus
  glue lengths, full normalized bytes, and ordered hole offset/width/encoding/type.
  Operand values and arbitrary glue IDs are excluded. Hash collisions are checked
  with full equality before sharing storage. There are **55,389** distinct runs and
  **48,985,021** stored blob bytes. Each function retains its own operand records;
  deduplication does not remove patch work or emitted memcpy calls.
- Initial review found the logical input-size formula omitted `fn_item.u32`, which
  is still read during emit. The author added its **546,540 bytes**. Final C reports
  **332,365,935 logical input bytes**. This is not process RSS: original flat-file
  mappings remain present, run buffers are overallocated, and C allocates the packed
  pool at the original 60.7 MB capacity. A/D also expand copy bytes to 60.7 MB.
- The copied p4par header initially advertised unsupported `P4_NOPATCH`; corrected.

## Timing interpretation

Preprocessing begins before original-file mapping, allocation, layout, and hole-list
construction; C's additional hash/compare/packing is reported separately. Emission
includes memcpy, every patch, marker/ordinary relocation appends, and concatenation
of reusable worker vectors. Output allocation, one warmup, sorting, and oracle checks
are excluded. The kernel uses p4par's original-item function quantiles and output-page
allocator. Reporting minima over repetitions matches the baseline protocol.

The prepared unmerged D control is important: precomputing offsets and flattening
hole records removes expensive lookups even without merging. Comparing A/B only
with p4par would confound that change with fewer pieces. A/B/D also have differing
run-metadata footprints; the data cannot attribute every difference solely to
memcpy call count. Original input versus prepared input should be described plainly,
and logical serialized size must not be called peak memory. No remaining correctness
blocker found in the tested dataset or final C key.
