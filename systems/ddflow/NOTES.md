# ddflow: Differential Dataflow column (hello world)

## Versions and paths
- rustc 1.100.0 (stable, rustup), `differential-dataflow =0.26.0`, `timely =0.32.0` (pinned in Cargo.toml; Cargo.lock committed).
- Install: `RUSTUP_HOME=/home/exedev/tools/rust/rustup CARGO_HOME=/home/exedev/tools/rust/cargo`, rustup `-y --no-modify-path`. Binaries in `/home/exedev/tools/rust/cargo/bin`.
- Target dir (not in git): `/home/exedev/tools/rust/target-ddflow`.

## Build and run
    export RUSTUP_HOME=/home/exedev/tools/rust/rustup CARGO_HOME=/home/exedev/tools/rust/cargo PATH=/home/exedev/tools/rust/cargo/bin:$PATH
    export CARGO_TARGET_DIR=/home/exedev/tools/rust/target-ddflow
    cd systems/ddflow && nice -n 10 cargo build --release
    prlimit --as=12884901888 nice -n 10 $CARGO_TARGET_DIR/release/ddflow /home/exedev/paradigms/data/minio -w 8

## What it does
Loads `p2_edge_off/dst/kind` (kind 0 = plain edges only, 8,150,701 of 8,320,873) and `p2_roots` (629 ids), runs
reachability with `iterate` (join, concat roots, `distinct`), then applies edge batches as updates at later timestamps:
remove 100 evenly-spaced edges, re-add them, remove/re-add the entry root's out-edges (that root has 1 plain edge).
The live set is accumulated from the output diffs.

## Live set vs oracle: NOT equal, by design
Plain-edge reachability gives 265,350 live ids; oracle `p2_live.u32` has 663,630. Ours is a strict subset (0 ids
outside the oracle; 398,280 missing). The gap is the deadcode rules beyond plain edges (useiface, ifacemethod,
namedmethod, reflect, method tables). Not implemented; they are mutually recursive with the flood and would be
extra `iterate` variables. Exact equality is a follow-up, not claimed here.

## Numbers (release, 2026-11-16 18:57 UTC, machine busy: load 10.8 before, 11.0 after; 8 cores shared)
| | -w 1 | -w 8 |
|---|---|---|
| full compute (load edges + fixpoint) | 2.68 s | 2.60 s |
| remove 100 edges | 1.2 ms | 62 ms |
| re-add 100 edges | 1.2 ms | 76 ms |
| remove entry root's edge | 31 ms | 359 ms |
| re-add entry root's edge | 37 ms | 587 ms |

Incremental update is 70x to 2000x faster than recompute at 1 worker. At 8 workers small updates are slower
(coordination per timestamp dominates, plus a loaded machine); 8 workers did not help the full run either (insertion
is done by all workers but the fixpoint is latency-bound with a long chain depth).
File read is ~0.2-0.3 s and is not in the "full" time. Count after remove100 is 265,349 (one live node lost), after
re-add back to 265,350.

## Gotchas
- DD 0.26 API differs from older examples: `VecCollection<'scope, T, D>`, `iterate(|scope, inner| ..)`, `Worker` is not generic.
- `timely::execute_from_args` parses `-w N`; pass args after the binary's positional DATA_DIR (we skip argv[0] only).
- Edges inserted per worker by stride (`i % peers == idx`); updates inserted by worker 0 only.
- Peak memory was well under the 12 GB `prlimit --as` cap (it ran fine).
