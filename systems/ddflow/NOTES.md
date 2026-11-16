# ddflow: Differential Dataflow column (incremental relink axis)

Differential Dataflow 0.26.0 / timely 0.32.0, Rust 1.100. The question this column adds: **after a small source
edit, how much of a relink's work must be redone?** Full details and every number: `results/ddflow-incremental.md`.

## Surprises

- **A real one-function edit changes 31 of 9.7 M reach facts and 3 live ids; DD updates the live set in 1.4 ms**
  (1 worker, median of 25) vs 49-57 ms for ref/flat/p2 and 31 ms for p2par on 1 thread recomputing, ~1,900x
  DD's own full run (2.7 s). Resolve (1a+1b) for the same edit: 0.7 ms vs C 287 / p1par 22 (8 threads).
- **Deleting one call kills 12,320 live ids (the console UI, 21.5 MB of image) and the update is slower than
  recomputing in C**: 65 ms (1 worker) vs p2 49 ms / p2par 19 ms (8). Retraction through the 45-round loop is
  DD's costly case.
- **8 workers make updates slower**, sometimes 8x (edit 2: 526-555 ms median on 8 vs 65 on 1); full runs get
  1.8-2.2x faster on 8. Per-round coordination dominates small, many-round updates.
- **The first obstacle is the ids, not the engine.** Ids are positional; edit 1 adds 3 slots to one object and
  shifts every id after it. Identity had to be rebuilt by name/hash matching plus a Myers alignment for unnamed
  DWARF symbols (an ordinal-only key gave edit 2 an 82 K-edge spurious diff; alignment: 234).

## Versions and paths
- rustc 1.100.0 (rustup), `differential-dataflow =0.26.0`, `timely =0.32.0` (Cargo.lock committed).
- `RUSTUP_HOME=/home/exedev/tools/rust/rustup CARGO_HOME=/home/exedev/tools/rust/cargo`, target dir (not in git)
  `CARGO_TARGET_DIR=/home/exedev/tools/rust/target-ddflow`.
- Edited sources: `/home/exedev/targets/minio-edit` (edit.patch), `/home/exedev/targets/minio-edit2`
  (edit2.patch); frozen objects `/home/exedev/targets/frozen-edit/{minio-edit,minio-edit2}`; flat files
  `/home/exedev/paradigms/data/{minio-edit,minio-edit2}`. Rebuild: `FROZEN=/home/exedev/targets/frozen-edit
  /home/exedev/targets/bottom-up/link/freeze.sh /home/exedev/targets/minio-edit minio-edit`, then
  `FROZEN=/home/exedev/targets/frozen-edit ref/flat/extract.sh minio-edit` (from /home/exedev/paradigms).

## Build, run, check

    export RUSTUP_HOME=/home/exedev/tools/rust/rustup CARGO_HOME=/home/exedev/tools/rust/cargo PATH=/home/exedev/tools/rust/cargo/bin:$PATH
    export CARGO_TARGET_DIR=/home/exedev/tools/rust/target-ddflow
    cd systems/ddflow && nice -n 10 cargo build --release; B=$CARGO_TARGET_DIR/release/ddflow; D=/home/exedev/paradigms/data
    prlimit --as=12884901888 $B p2 $D/minio -w 8            # -> check A: ... extra 0, missing 0 -> OK / P2 OK
    prlimit --as=12884901888 $B p2 $D/kube-apiserver -w 8   # -> P2 OK (926,486 live)
    prlimit --as=12884901888 $B p2 $D/minio phased -w 1     # index the facts, then the flood alone
    prlimit --as=12884901888 $B inc $D/minio $D/minio-edit 5 -w 1     # full A, 5x update A->B and back -> P2 OK
    prlimit --as=12884901888 $B p1inc $D/minio $D/minio-edit2 5 -w 8  # resolve 1a+1b, same -> P1 OK
    systems/ddflow/bench.sh 5                               # every timing, interleaved with C p1/p1par/p2/p2par

The check is in the binary: the live set (sorted) against `oracle/p2_live.u32`, extra/missing counts printed;
`inc` also checks after A->B against B's oracle mapped into the unified ids, after B->A against A's, and a
from-scratch dataflow on B in the same process against the updated set; `p1inc` checks canon (5.2 M) and every
nonzero relocation target (9.3 M) against `oracle/p1_canon.u32` and `oracle/p1_reltarget.u32` of both versions.
Exit status 1 on any mismatch.

## Cells

| cell | correct | 1 worker warm / e2e | 8 workers warm / e2e | lines |
|---|---|---|---|---|
| 2 reach, MinIO | **P2 OK** | 3077 / 3343 / 3591; e2e 3468-4103 | 1491 / 1814 / 2439; e2e 1903-2907 | 71 (dataflow) |
| 2 reach, kube-apiserver | **P2 OK** | 5605 / 5945 / 6787; e2e 6378-7597 | 2608 / 2762 / 2948; e2e 3419-3826 | same |
| 2 by phase, MinIO: index facts / flood | | 2285 / 988 (medians) | 751 / 1257 | |
| 2 incremental, edit 1 (+call) | **OK** (= oracle B = from scratch) | update 1.3 / 1.4 / 3.4 | 1.8 / 9.0 / 81.9 | +driver |
| 2 incremental, edit 2 (-call) | **OK** | update 54.9 / 65.3 / 113 | 28.0 / 555 / 688 | |
| 1a+1b resolve, full | **P1 OK** (both versions) | 8659 / 9310 / 10823 | 1285 / 2023 / 3203 | 23 (dataflow) |
| 1a+1b incremental, edit 1 / edit 2 | **OK** | 0.7 / 87 (medians) | 29 / 64 | |

ms, min / median / max of 5 interleaved rounds, `bench-20261116T1940Z.txt`, 2026-11-16T19:39-19:49Z, load
4.4-5.6 (8 shared cores; provisional). Same window, C: p2 flood 44.6 / 52.4 / 58.0 (e2e 66-85), p2par 1 thread
30.9-39.2, 8 threads 16.2-18.6; kube p2 67.7-157, p2par 8 20.8-29.8; p1 1a+1b 261-563, p1par 8 19.9-43.7.
warm = facts in memory, the dataflow builds its arrangements inside the timing (there is no "prebuilt index"
input in DD: `phased` splits that off); e2e = process start to exit incl. reading the flat files and the check.

So the full computation is **~60x the single-core C flood (1 worker) and ~100x p2par (8)**; DD is not a
from-scratch engine. The incremental cells are the point of the column.

Per cell, one line each:
- **2 reach** — easy: every deadcode rule is a join or concat into two mutually recursive `Variable`s (`live`,
  `iface`) in one `iterative` scope; the conditional method rule is three semijoins. Forced: the static parts
  (type-edges, method tables, "iface only for type descriptors") hoisted out of the loop by hand; sig ids decoded
  to (name, type) at load. Impossible: nothing.
- **2 incremental** — easy: the update is `feed(minus, -1); feed(plus, +1); advance` on the same dataflow, no
  code beyond the full run. Forced: a stable id space (`facts::unify`, ~120 lines, outside DD). Slow case:
  retractions of large reachable regions.
- **1a+1b** — easy: winner = `reduce` taking the max member per name key; candidates = `semijoin` / `antijoin` on
  the groups; 1b = join of relocations with canon. Forced: "lowest id" tie-break as (object, index in object);
  the package-table decode (`base + idx`) done at load. Slow: 9.3 M relocations through two arrangements.
- 1c, 3a, 3b: not attempted (not this column's question).

## Tuning (P2 full, 1 worker, same machine, noisy)

1. First correct version: one edge arrangement keyed by source (value = kind, payload) shared by all four edge
   kinds, then filters; type edges and method tables by `semijoin` (arranging the 8 M edges twice more):
   3.8-5.4 s.
2. Type edges and typed methods by `join_core` against the already-built edge arrangement (no re-arrangement of
   8 M edges): 2.8-3.4 s.
3. Per-kind arrangements (8 M plain edges `(u32, u32)`; the three small kinds separately) and `iface` kept only for
   type descriptors, with every gotype filter moved out of the loop: 3.1-3.3 s (within noise of 2, fewer
   operators per round).
4. `i32` diffs, `u32` inner timestamps: no measurable change under load 7-10. Kept.
- 45 rounds (`DD_ROUNDS=1` prints it). The flood alone is ~1 s at 1 worker (~22 ms per round) vs C's 50 ms; not
  profiled further (no perf on the VM).
- `-w 8`: full 1.8x faster (index phase 3x, flood no faster). Batching input: workers insert disjoint strides.

## Code excerpt (src/main.rs, the loop of `reach`)

```rust
scope.iterative::<u32, _, _>(|inner| {
    let step = Product::new(Default::default(), 1);
    let (live_v, live) = VecVariable::new(inner.clone(), step);
    let (iface_v, iface) = VecVariable::new(inner.clone(), step); // iface type descriptors
    let live_a = live.clone().arrange_by_self();
    let targets = live_a.clone().join_core(plain.enter(inner), |_, &(), &t| Some(t));
    let useiface = live_a.clone().join_core(useiface.enter(inner), |_, &(), &t| Some(t));
    let sig = live_a.clone().join_core(ifacemethod.enter(inner), |_, &(), &x| Some(x)).distinct_core::<Diff>();
    let named = live_a.clone().join_core(namedmethod.enter(inner), |_, &(), &n| Some(n)).distinct_core::<Diff>();
    let reflect = live_a.clone().join_core(reflectcall.enter(inner), |_, _, _| Some(())).distinct_core::<Diff>();
    // live iface type descriptors: their type edges and their methods
    let lit = live_a.join_core(iface.arrange_by_self(), |&s, _, _| Some(s)).arrange_by_self();
    let iface_t = lit.clone().join_core(typeedge.enter(inner), |_, _, &t| Some(t));
    let iface_m = lit.clone().join_core(methodtype.enter(inner), |_, _, &t| Some(t));
    let cand = lit.join_core(typemethod.enter(inner), |_, _, &m| Some(m));
    // the methods something can call
    let by_reflect = cand.clone().filter(|m| m.4 != 0).map(|m| ((), m)).semijoin(reflect).map(|(_, m)| m);
    let by_sig = cand.clone().map(|m| ((m.0, m.1), m)).semijoin(sig).map(|(_, m)| m);
    let by_name = cand.map(|m| (m.0, m)).semijoin(named).map(|(_, m)| m);
    let called = by_reflect.concat(by_sig).concat(by_name)
        .flat_map(|(_, mt, ifn, tfn, _)| [mt, ifn, tfn]).filter(|&x| x != 0);
    live_v.set(roots.enter(inner).concat(targets).concat(called).distinct_core::<Diff>());
    iface_v.set(iface0.enter(inner).concat(useiface).concat(iface_t).concat(iface_m).distinct_core::<Diff>());
    live.leave(scope)
})
```

And the incremental relink is only this (src/main.rs, `inc`):

```rust
inp.feed(&d.minus, -1, true, idx, peers);   // facts only in A, retracted
inp.feed(&d.plus, 1, true, idx, peers);     // facts only in B, inserted
df.run(worker);                             // advance the inputs' time, step until the probe passes it
```

Lines: reach dataflow 71 (no comments), resolve dataflow 23, driver (inputs, phases, checks) ~250, flat-file
loading + id unification + diff 330 (`facts.rs`), resolve loading/driver ~270 (`p1.rs`).

## Gotchas
- DD 0.26: `VecCollection<'scope, T, D, R>`, `scope.iterative::<u32,_,_>`, `VecVariable::new(scope, step)`;
  `distinct()` returns `isize` diffs (use `distinct_core::<R>()` with another diff type).
- Outputs of `concat`/`antijoin` are not consolidated across workers: a record can be +1 on one worker and -1 on
  another. Inspecting per worker needs `.consolidate()` first (p1inc failed at 8 workers without it).
- `timely::execute_from_args` parses `-w N`; positional args come first.
- Rebuilding in a copy of the MinIO tree marks the module `+dirty`, which changes `runtime.modinfo` (P3 data only).
