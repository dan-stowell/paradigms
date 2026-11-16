# DuckDB: ideas a measurement suggested

## 1. "Round up, then add" is a monoid with bounded elements: 3a's unit packing is a parallel scan

**Evidence.** In SQL the unit packing (`va = round_up(va, align); va += n`, 1,322 units per bucket) was the one step
that is not a window sum. A Jacobi fixpoint over paddings took 1,121 rounds (paddings cascade); a recursive CTE took
1,322 iterations (340-620 ms); only a sequential fold (`list_reduce`) was fast (65-95 ms).

**Idea.** f(x) = round_up(x, A) + n with A a power of two. f2 after f1 is again of that form when A2 <= A1:
round_up(x, A1) + round_up(n1, A2) + n2. When A2 > A1 it is not, but a composition is always a chain of (A, n) pairs
with strictly increasing A, and alignments here are 2^0..2^12, so an element is at most 13 pairs. That is an
associative operator with bounded state: the packing becomes a parallel (Blelloch) scan, i.e. data-parallel 3a for
Futhark/Halide/SQL engines that offer a user scan. Kill test: implement the operator in Futhark's `scan` (or C with
OpenMP) and compare with the sequential loop on MinIO's 1,322 x 14 blocks and on 33 K typelinks.

## 2. A dense-id gather is the missing SQL primitive

**Evidence.** Every problem's cost in DuckDB is dominated by hash joins whose key is a dense id (canon, addresses,
flags): 9 M probes into 5 M rows ~ 140 ns/probe on one core (CLI test 16:20Z), vs a few ns for `canon[x]` in C. A
LIST index (`l[i+1]`) is worse (5.3 s). **Idea:** an engine that knows a key column is `0..n-1` (dk already
produces it: `id=#`) could replace the hash join by a positional gather. Kill test: DuckDB extension or a modified
join that detects a dense build key; expect 1b and 3b to drop several-fold.

## 3. Semi-naive SQL needs indexes that persist across rounds

**Evidence.** P2's 45 rounds cost ~25 statements each, 1-10 ms per statement whatever the frontier size, because each
round rebuilds hash tables on `live`, `iface`, `canon`. The lazy decode (ATTACKS 1) loses to the eager one for this
reason. **Idea:** keep the big relations' hash tables (or a sorted index) alive across statements; kill test: the
same rules in a system that does (Soufflé's B-trees, Differential Dataflow's arrangements).

## 4. P2 as an outer method loop around the engine's own recursive CTE

**Evidence** (17:10Z, load 3.7, `sql/x_plainclosure.sql` after load2 + p2prep): the plain-edge closure alone from the
roots (265,350 symbols, no method or interface rules) as one `WITH RECURSIVE ... UNION` statement: 425 ms on 8
threads, 998 ms on 1 — inside one statement, so no per-statement cost per level. **Idea:** drive only the method
rules from outside (few outer rounds), each round extending the closure with a recursive CTE seeded by the newly
live; the gotype -> iface propagation is a second closure over plain edges between gotypes. Kill test: beat the
2.0-2.4 s of p2loop.sql at 8 threads with P2 still exact.
