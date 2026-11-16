# ATTACKS

New attacks on the grid's problems (CHARTER.md). Each entry: the problem, the old shape, the new
attack and where it came from, the evidence, what it would take. Before, observation, fix, number.

## 1. Reach: decode the references in bulk, but only the frontier's (draft: measuring)

**Problem.** Reach (2): the live set from the roots, over relocations plus the method rules.

**Before.** hlink floods straight over the raw objects: for each newly live symbol it walks its relocation records and
turns each (package index, symbol index) into a canonical id on the spot. 98-103 ms on 1 core, 22-23 ms on 8
(MinIO, GRID.md). The flat files instead hand every system a pre-decoded canonical CSR; a plain worklist flood over it
takes 39-43 ms on 1 core and the tuned level-synchronous one 27-30 / 13-17 ms (`build/p2`, `build/p2par`, 15:56Z, load 0.8).

**Observation.** The flood is not the expensive part; the decode is. dumpflat's (untuned) eager decode of every
reference into the CSR costs 133-139 ms on 1 core (`ref/hlink/dumpflat.c`, timed 15:57Z), more than hlink's whole
flood. And eager decoding is mostly wasted: the flood reads only **28.7%** of the CSR's edges on MinIO (2.39 M of
8.32 M) and **21.0%** on kube-apiserver (3.27 M of 15.6 M), and **a quarter** of the edges it does read repeat a
(source, target) pair (`python3 ref/flat/reachfacts.py data/<dataset>`). So "pre-decode everything, then flood" does
3.5-5x the decode work of hlink's lazy flood.

**Attack.** Keep the decode lazy but make it a bulk operation: per BFS level, gather the frontier's relocation ranges,
decode them as one vectorized gather (package-index table, then canon), dedup per source, then mark. This is a
data-parallel flood where the edge list is computed per level instead of stored. (The parallel eager pipeline,
ref/flat/p2pipe.c by Codex, is the comparison; numbers below when it lands.)

**Evidence so far.** kube-apiserver, 8 threads, eager pipeline (p2pipe, provisional, loaded machine): decode 9.3 ms
+ CSR build 28.6 ms + flood 24.5 ms = 63.0 ms total vs flood alone on the prebuilt CSR 24.1 ms. Building the CSR,
not decoding the targets, is the biggest piece.

**What it would take.** A level-batched lazy flood over the flat relocation columns (`rel_*`, `slot_rel`), checked
against `oracle/p2_live.u32`, timed against hlink's flood and p2pipe on both datasets.
