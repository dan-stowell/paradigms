# GraphIt cells (for GRID.md)

**2 reach** - correct, MinIO and kube-apiserver, all 13 schedules, 1 and 8 threads (`systems/graphit/check.sh` ->
"all P2 OK": live 663,630 / 926,486 = `oracle/p2_live.u32` element-wise) - MinIO, `bench.sh`, 5 interleaved rounds,
2026-11-16T20:40Z load 2.4-3.4, min / median / max: warm **39.8 / 45.0 / 46.2 (1, `serial`)**, **16.8 / 24.2 / 28.5
(8, `hybrid_bv`)** vs C p2 37.1 / 42.3 / 49.4 and p2par 27.6 / 33.9 / 34.8 (1), 10.6 / 15.9 / 19.7 (8) in the same
window; e2e 290 / 295 / 322 (1), 254 / 278 / 325 (8) vs p2par 77-93 / 45-53, plus the conversion to GraphIt's
graph file (towsg, 1 core C: 118 ms build + 490 ms write). kube-apiserver (20:42Z): warm 53.6 / 58.0 / 59.7 (1),
23.4 / 31.2 / 37.1 (8, `densepush`) vs p2 61.9 / 66.3 / 79.5, p2par 39.0 / 44.6 (1), 15.1 / 19.5 (8) - 50 lines
.gt + 1 per schedule + 145 converter - easy: the flood is one `edges.from(frontier).to(f).applyModified(reach, st,
true)` with `reach` = `st[dst] min= st[guard]`, schedules one line each; forced: the rules compiled into the graph
(facts as vertices, each AND as an edge whose weight is the other input, the (name, type) method join as converter
edges; 6.24 M vertices / 10.56 M edges), the to-filter written `0 < st[v]` to dodge a CAS miscompile, int state,
graph only from a 269 MB file; impossible: nothing (no C++ host loop needed). Schedules: pull 25-50x worse (59
rounds x all vertices), hybrid/bitvector/sliding/densepush within noise of push at 8, serial best at 1 (parallel
push is 1.3x slower on one thread), cache segments and NUMA no help (one node; NUMA codegen needed a patch).
