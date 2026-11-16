# Independent static review of B

Reviewed changes in `ref/flat/p3.c`, `systems/goose/p3.goose`,
`systems/goose/p3par.goose`, and `results/review5-b-bench.py`, plus the reused
`flatio.c` file helpers and `par.goose` partition function. No benchmark was run
during B's measurement window and no implementation file was edited.

**Disjoint assembly.** `part_lo(n,k,W) = n*k/W` assigns each worker the half-open
range `[floor(n*k/W),floor(n*(k+1)/W))`; adjacent endpoints are equal, the first
is zero and the last is n. Segment length equals that range and `flat_write_at`
uses `pwrite` with its absolute start offset and handles partial successful
writes in a loop. Copy and relocation writes are clipped to each segment, so
symbols and relocation fields crossing a partition are independently computed
on either side. No worker writes another worker's output bytes. Main truncates
and sizes the file before issuing this repetition's Go messages; workers send
Done only after write-out and close. Main waits for all eight Done messages
before recreating the file for the next repetition, avoiding truncate/write
races. Workers retain only their own segments; the file path avoids Seg messages
and the master array gather. Per-repetition relocation errors are now accumulated
into failure status rather than merely printed.

**Timed work.** Parallel timing starts before file creation and ends after all
Done messages, including dispatch, page setup, relocation, all pwrite calls,
close and collection. One-worker Goose includes fresh page setup and creation /
write-out. C adds fopen/truncate, fwrite and fclose before its existing timer
ends; its existing timer starts after image allocation. This asymmetry is
explicitly disclosed in `review5-b.md` and makes this a comparison of working
implementations, not identical allocation costs. Checking/readback, source
loading, worker spawning and P3a are outside the reported P3b times. All writes
are buffered filesystem writes without fsync: the result is one assembled file
in the page cache, not a durable-storage throughput benchmark. The reused Goose
sidecar ignores close errors (C checks them); current checks read the final file
back, but this is not a production durability guarantee.

**Oracle independence.** Produced bytes derive from source byte/relocation
columns, never the oracle image. P3b uses oracle *addresses* intentionally, as
the C reference already does. The new expected buffer is separately zeroed,
then filled from oracle bytes only for placed, non-BSS symbol intervals. Whole
image equality then includes every zero gap and length. This explicitly checks
a normalized P3 image, not unmodified ELF headers/nonplaced contents. The Python
harness independently reconstructs the same contract from the dataset and
checks the entire final output file for every file-output process, including
C and single-worker Goose. Its corruption self-checks cover one nonzero placed
byte and offset 0 (the header gap for this dataset). The check runs after all
64 repetitions: it validates the final produced image, not every intermediate
file. That is a normal benchmark limitation, and should not be described as
64 separately validated outputs.

**Protocol.** Five rounds rotate all ten variants; pinning, 8G cgroups, explicit
clearing of allocation-related environment variables, P3_REPS=64 and recording
uptime are present. It asserts each measured process passes its oracle and
that even its minimum time times 64 exceeds one second. The final summary is
min/median/max of five **process minima**, not quantiles of all iterations.
No correctness or timing-scope blocker found for the stated buffered-file
experiment; the allocation and durability qualifications above matter.
