# Independent witness review, 2026-11-16

Reviewed the witness implementation in `ref/flat/p2inc.c` separately from its author.
The final large-data oracle, FUZZ, CHAIN, and timing runs are the lead's responsibility;
this review supplies small rerunnable checks with explicit expected answers.

## What the proof depends on

- A live witness is either a currently present plain edge from a live defined source
  with a smaller live stamp, or a still-called method with a smaller method stamp.
  `hasEdge` checks both the immutable CSR/removal hash and added-edge lists, so an
  unchanged live source cannot preserve a target via a removed witness edge.
- An interface witness checks the applicable useiface edge, active type edge, or
  method owner, and the appropriate live/type stamp. Method-table rows are immutable
  in this program; unsupported row diffs are still rejected.
- A method condition witness checks live/defined state, a smaller live stamp, and
  the current signature/name edge or reflect flag. A method target that asserts the
  same condition has a later stamp and cannot justify the method itself.
- Type deactivation and method uncalling propagate to their consequences. A queued
  deactivation can temporarily leave a witness apparently true, but its eventual
  removal rechecks the dependent fact before deletion completes.
- Skipping previously rejected cursor entries is safe for correctness because it
  can only miss a valid support, leading to over-deletion; rederivation still scans
  all current predecessors without requiring their stamps to be smaller. Across
  arbitrary edge additions/restorations, the claim that an earlier entry can *never*
  become valid again is too strong: an unchanged older source may regain its edge.
  Thus the strict no-rescan efficiency argument applies within a deletion phase,
  not to every future update sharing a target stamp. The implementation remains
  exact under this conservative miss.

No witness-induced false-positive derivation was found in these paths. This is not
an exhaustive proof of all pre-existing aggregate bookkeeping or integer overflow
behavior.

## Independent tiny regressions

`ref/flat/test-p2inc-witness.c` includes the implementation and constructs small
indexed graphs directly. Every update compares against an isolated scratch flood,
and also asserts an explicit expected answer independent of that flood:

1. `cycle`: roots 1 and 4 both reach 2, and 2/3 form a cycle. Removing the cached
   witness must find the other root; removing the other edge must kill both cycle members.
2. `overlay`: the same shape exists entirely as added edges. Removing cached
   overlay supports must not leave the cycle alive.
3. `method`: source 2 supplies a signature enabling a method of rooted active type
   5. Its called target 7 supplies the same signature. Removing the only root path
   to 2 must uncall the method and kill 2, 6 and 7 despite that self-supporting
   signature cycle. Interface fact 6 remains justified by type 5.
4. `flags`: removing the definition flag from rooted source 1 must kill its
   downstream 2/3 cycle while keeping the root itself live.

5. `iface`: two rooted active types support an unrooted type cycle via plain edges.
   Losing the cached interface witness must retain the alternative type support;
   losing the final rooted edge must remove both live and interface cycle facts.
6. `condition-overlay`: two rooted sources assert a method signature using overlay
   edges. Removing the cached condition edge must find the other one; restoring
   the first edge creates a new pool entry. Removing the second then the restored
   first must finally uncall the method even though both sources remain live.


Commands, run from `/home/exedev/paradigms-w`:

```sh
TMPDIR="$PWD/build" gcc -O2 -o build/test-p2inc-witness ref/flat/test-p2inc-witness.c
for case in cycle overlay method flags iface condition-overlay; do
  prlimit --as=12884901888 build/test-p2inc-witness "$case"
  LIFO=1 prlimit --as=12884901888 build/test-p2inc-witness "$case"
  COUNTS=1 prlimit --as=12884901888 build/test-p2inc-witness "$case"
  LIFO=1 COUNTS=1 prlimit --as=12884901888 build/test-p2inc-witness "$case"
done
```

All six passed under all four scheduling/count configurations (24 runs) during review. Each fixture uses a separate process, so zeroed
static state does not conceal contamination between fixtures.


The fixture reads `LIFO` and `COUNTS` itself because it does not run production
`main`. Count mode also independently enumerates all current outgoing edges after
initialization and every update and asserts the exact number of smaller-stamp live
plain predecessors for each live target. This catches bookkeeping errors even when
the resulting live set has not yet diverged.

The same 24 cases passed an AddressSanitizer+UndefinedBehaviorSanitizer build:
`TMPDIR="$PWD/build" gcc -O1 -g -fsanitize=address,undefined -o build/test-p2inc-witness-asan ref/flat/test-p2inc-witness.c`.
Use the above matrix with that binary and `ASAN_OPTIONS=detect_leaks=0`; fixture
allocations intentionally live until process exit. ASan reserves a large virtual
address range, so the 12 GiB address-space cap applies to the ordinary runs, not
these sanitizer runs.

## Follow-up review: condition edge identity and FIFO schedule

The optimized base condition witness can replace `hasEdge` with `!removed`:
its source was selected from the immutable signature/name source index for that
exact method key. That base edge cannot change identity. An overlay condition
witness instead saves the pool index, checks `alive`, source definition/liveness,
and the smaller stamp. Pool records are append-only (no slot reuse), so the saved
index still denotes the original edge even if `realloc` moves the pool. The
`condition-overlay` regression explicitly exercises removal and re-addition with
a live source. Method row immutability is necessary to both arguments.

The FIFO worklist gives new facts smaller stamps before their consequences just
as the old LIFO schedule did. It changes which proof is chosen, not the derivation
rules. All six fixtures pass under either schedule, with or without plain support
counts; alternative-support cases choose the actual initial witness rather than
assuming a particular traversal order.

## Instrumentation correction sent to author

The optimized `tryCall` calls `condWF` while creating witnesses during setup and
fresh floods. In a `PROFILE_SUPPORT` build, its per-target profile arrays therefore
need clearing immediately before every update; clearing only `cRev` is insufficient.
The baseline diagnosis is unaffected because the original initial flood did not
call `condWF`. Timing should use a build without per-target instrumentation.

The author subsequently added `profReset` before each update; this resolves the setup-accounting issue.
