# ddflow ideas (from measurements; details in results/ddflow-incremental.md)

1. **Stable symbol identity is the precondition, not the dataflow.** Slot ids are positional (object base + index),
   so the amztime edit (3 slots added in object 109) moves the id of every slot in objects 110..1420. Before any
   incremental engine can help, ids must be mapped by name/content (`facts::unify`: unique keys first, Myers
   alignment of the unnamed rest). With that map the fact diff of the edit is 9-27 facts of 9.7 M.

2. **Tie-breaks by position still churn object-locally.** P1a's winner rule "lowest id" becomes "lowest (object,
   index in object)" to survive other objects growing. But deleting the console call near the start of `cmd`
   shifts the index of every later named slot in `cmd`: 34,497 of the P1 diff's 34,700 facts are that churn, while
   only 73 canonical ids and 171 relocation targets really change. A tie-break that does not use position (e.g.
   by object then name hash; only the rare DUPOK equal-rank ties need it) would make the P1 diff proportional to
   the edit. Measurement to make: rerun `p1inc ... minio-edit2` with `Member = (rank, !obj, hash(name), id)`.

3. **Closure numbering is a rename.** Deleting closure `serverMain.func15.1` renumbers `func15.2 -> func15.1`,
   `func15.3 -> func15.2`, ... Name-keyed identity sees those as delete + add (and their DWARF children too). A
   compiler that named closures by content or source position-stable keys would shrink such diffs.

4. **One worker for updates, many for the first full run** (under load only: quiet, 20:12Z, 8 workers update edit 2 in
   29 ms vs 59 on 1). Small updates are 1.4 ms on 1 worker and 6-10 ms on 8
   (MinIO edit 1); edit 2's update is 71 ms on 1 and ~540 ms on 8. The full run is 1.8x faster on 8. A relinker
   would want the full build wide and the edit loop narrow; timely fixes the worker count per process.
