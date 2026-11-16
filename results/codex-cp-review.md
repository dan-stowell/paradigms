# Independent CP1/CP4 review

Reviewed `ref/p4/gen_cp1.py`, `p4cp1.c`, `gen_cp4.py`, and `p4cp4.c` on
2026-11-16. No blocking issue remains for the supplied MinIO dataset.

CP1 decodes from segment starts with Capstone and checks complete consumption,
then uses decoded displacement/ModRM locations rather than searching backward.
ModRM values 01/10 determine one/four-byte displacement size, which correctly
handles Capstone 5's reported two-byte size on a 66-prefixed disp32 store.
SIB keeps its byte; only ModRM changes. RIP and no-base SIB fields stay fixed:
kind 7 is not relaxed, and an unexpected kind 1/2/3 with mod=00 is rejected.
This is a proof for these extracted inputs, not an arbitrary x86 rewriter.

The dedup key includes shape, mode, normalized bytes, and full hole metadata.
Segment boundaries are deliberately outside the key, with a separate original
segmentation ID retained for each node. The emitter uses deduplicated bytes and
holes; original IDs recover only normalized segment boundaries. Therefore the
66,046 count is a template-key count, not the number of distinct template-plus-
segmentation combinations. Report the retained segmentation metadata explicitly.

`prlimit --as=12884901888 python3 results/cp-review-check.py` constructs seven
independent tiny templates under `/tmp` and runs the actual generator. It proves
BP and SIB disp8/disp32 pairs merge, RIP/no-base-SIB relocation bytes remain
unchanged, and the 66-prefix case retains disp32. Expected seven-to-five merge,
normalized byte strings, and ModRM offsets all pass; see
`codex-cp-review-synthetic.log`. This test does not reuse generator internals.

`P4_REPS=1 prlimit --as=12884901888 build/p4cp1 /home/exedev/paradigms/data/minio-p4`
passes all 60,733,909 bytes, all 1,780,281 relocations, and all function offsets
(`codex-cp-review-cp1.log`). It executes the runtime operand-derived shrink in
both layout and emission. The review identified an inherited p4.c limitation:
function offsets were checked only after a code mismatch. The lead added an
unconditional check to both new emitters. A read-only-symlink fixture with only
oracle function offset 1 incremented now produces `P4 FAIL` with exactly one
function-offset difference (`codex-cp-review-negative.log`), while code and
relocations remain equal.

CP4 generates precisely 4,096 ranked-template functions with constant copy sizes
and constant hole offsets. Multi-segment templates have a segment switch with a
straight-line arm per segment, as required by the segmented input representation.
The pointer table selects them; null entries fall through to the original generic
loop. The full oracle covers generated and generic code and all relocations.
An independent forced-fallback/thread-limit run passes (`codex-cp-review-cp4.log`):

```
OMP_NUM_THREADS=8 OMP_THREAD_LIMIT=2 OMP_WAIT_POLICY=passive P4_HUGE=0 \
P4_REPS=1 P4_CP4_COVERED=1 prlimit --as=12884901888 \
  build/p4cp4 /home/exedev/paradigms/data/minio-p4
```

The hot-only diagnostic is an appropriate paired test: both modes traverse the
same pre-gathered hot-item list in the same order, copy/patch at the same output
offsets, and collect private relocation records. Gathering, glue/cold items,
marker work, final relocation concatenation, and sorting are excluded. Thus its
1.5x comparison must not be described as whole-emit speedup. Full ordinary oracle
runs precede the diagnostic; its final code check alone is not a complete
relocation-oracle proof, because its hot-only relocation vector is not compared.
All review runs were short correctness probes, not performance measurements.
