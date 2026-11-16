# Goose P4 function-offset oracle check

2026-11-16, branch `codex-goose-fnoff`, clone `/home/exedev/paradigms-g`.

Each worker sends `at[fitem[f]]` relative to its range in `Out` for both joins;
the last worker also sends the end sentinel. Main places these by function index
and adds `base[k]`. `check()` compares all 136,635 (`nfn+1`) offsets with
`oracle/p4_fn_off.u64`, counts unequal or missing/extra entries, and includes the
count in the failure result. Gathering starts after receipt of `Place`, after
all timed repetitions. No layout, emit, or timed-repetition code changed.

The release v2026.4 compiler and native build follow systems/goose/NOTES.md.
All output paths are inside this clone; the external dataset was read only.
Build from the clone root (exit 0):

```sh
mkdir -p systems/goose/bin scratch/goose-fnoff/{tmp,cache,out}
TMPDIR=$PWD/scratch/goose-fnoff/tmp XDG_CACHE_HOME=$PWD/scratch/goose-fnoff/cache systemd-run --user --scope -q -p MemoryMax=8G /home/exedev/tools/goose/rel/goose-v2026.4-linux-x64/goose build systems/goose/p4.goose -o systems/goose/bin/p4
# building the package p4's C
# built systems/goose/bin/p4
```

Run commands below use `cd systems/goose`, with P4_REPS/P4_K/P4_NOCHECK unset.
These are correctness checks, not a performance comparison.

```text
$ P4_OUT=/home/exedev/paradigms-g/scratch/goose-fnoff/out systemd-run --user --scope -q -p MemoryMax=8G bin/p4 /home/exedev/paradigms/data/minio-p4 1
p4 goose (1 workers, x1): spawn+load 284.9 ms  rep wall 97.9/97.9/97.9 ms  layout 9.5/9.5/9.5 ms  emit 88.2/88.2/88.2 ms  join (files) 67.3 ms  (slowest worker 57.3 ms)  60733909 bytes, 2381252 pieces, 1780281 relocations
function offsets: 136635, oracle 136635, 0 differ
code: 60733909 bytes; relocations: 1780281, oracle 1780281, 0 differ
P4 OK
exit=0
$ P4_OUT=/home/exedev/paradigms-g/scratch/goose-fnoff/out systemd-run --user --scope -q -p MemoryMax=8G bin/p4 /home/exedev/paradigms/data/minio-p4 8
p4 goose (8 workers, x1): spawn+load 108.4 ms  rep wall 23.6/23.6/23.6 ms  layout 3.3/3.3/3.3 ms  emit 20.8/20.8/20.8 ms  join (files) 65.4 ms  (slowest worker 41.7 ms)  60733909 bytes, 2381252 pieces, 1780281 relocations
function offsets: 136635, oracle 136635, 0 differ
code: 60733909 bytes; relocations: 1780281, oracle 1780281, 0 differ
P4 OK
exit=0
$ P4_OUT=/home/exedev/paradigms-g/scratch/goose-fnoff/out P4_JOIN=queue systemd-run --user --scope -q -p MemoryMax=8G bin/p4 /home/exedev/paradigms/data/minio-p4 1
p4 goose (1 workers, x1): spawn+load 264.5 ms  rep wall 96.8/96.8/96.8 ms  layout 9.5/9.5/9.5 ms  emit 87.1/87.1/87.1 ms  join (queue) 246.2 ms  (slowest worker 0.0 ms)  60733909 bytes, 2381252 pieces, 1780281 relocations
function offsets: 136635, oracle 136635, 0 differ
code: 60733909 bytes; relocations: 1780281, oracle 1780281, 0 differ
P4 OK
exit=0
$ P4_OUT=/home/exedev/paradigms-g/scratch/goose-fnoff/out P4_JOIN=queue systemd-run --user --scope -q -p MemoryMax=8G bin/p4 /home/exedev/paradigms/data/minio-p4 8
p4 goose (8 workers, x1): spawn+load 128.3 ms  rep wall 20.9/20.9/20.9 ms  layout 2.7/2.7/2.7 ms  emit 18.2/18.2/18.2 ms  join (queue) 106.9 ms  (slowest worker 0.0 ms)  60733909 bytes, 2381252 pieces, 1780281 relocations
function offsets: 136635, oracle 136635, 0 differ
code: 60733909 bytes; relocations: 1780281, oracle 1780281, 0 differ
P4 OK
exit=0
```

Fault test: temporarily insert the following immediately before `if pl.m.file`
in the worker join, then build a separate binary (same TMPDIR/XDG_CACHE_HOME as
above). This changes only the reported first function offset, not emitted bytes
or relocations. Restore the source in a `finally` block and delete `bin/p4-fault`
after the test; no fault switch remains.

```goose
    if k == 0 { foff[0] += 1; } // temporary fault
```

Build from clone root; run from `systems/goose`:

```text
$ systemd-run --user --scope -q -p MemoryMax=8G /home/exedev/tools/goose/rel/goose-v2026.4-linux-x64/goose build systems/goose/p4.goose -o systems/goose/bin/p4-fault
built systems/goose/bin/p4-fault
$ P4_OUT=/home/exedev/paradigms-g/scratch/goose-fnoff/out systemd-run --user --scope -q -p MemoryMax=8G bin/p4-fault /home/exedev/paradigms/data/minio-p4 8
p4 goose (8 workers, x1): spawn+load 152.4 ms  rep wall 34.1/34.1/34.1 ms  layout 3.4/3.4/3.4 ms  emit 27.0/27.0/27.0 ms  join (files) 59.8 ms  (slowest worker 41.3 ms)  60733909 bytes, 2381252 pieces, 1780281 relocations
function offsets: 136635, oracle 136635, 1 differ
code: 60733909 bytes; relocations: 1780281, oracle 1780281, 0 differ
P4 FAIL
exit=1
$ P4_OUT=/home/exedev/paradigms-g/scratch/goose-fnoff/out P4_JOIN=queue systemd-run --user --scope -q -p MemoryMax=8G bin/p4-fault /home/exedev/paradigms/data/minio-p4 8
p4 goose (8 workers, x1): spawn+load 192.7 ms  rep wall 36.8/36.8/36.8 ms  layout 6.8/6.8/6.8 ms  emit 31.6/31.6/31.6 ms  join (queue) 117.3 ms  (slowest worker 0.0 ms)  60733909 bytes, 2381252 pieces, 1780281 relocations
function offsets: 136635, oracle 136635, 1 differ
code: 60733909 bytes; relocations: 1780281, oracle 1780281, 0 differ
P4 FAIL
exit=1
```

Final verification: compare the `layout`, `emit_item`/`emit`, worker repetition
loop, and main timed-repetition block against `git show HEAD:systems/goose/p4.goose`
with Python string equality assertions; rerun the unchanged production binary
after restoring source:

```text
layout, emit, worker timed reps, main timed reps: byte-identical to HEAD
Temporary fault removed from source; fault binary deleted.
$ P4_OUT=/home/exedev/paradigms-g/scratch/goose-fnoff/out systemd-run --user --scope -q -p MemoryMax=8G bin/p4 /home/exedev/paradigms/data/minio-p4 8
p4 goose (8 workers, x1): spawn+load 146.8 ms  rep wall 30.1/30.1/30.1 ms  layout 3.0/3.0/3.0 ms  emit 23.4/23.4/23.4 ms  join (files) 55.0 ms  (slowest worker 35.4 ms)  60733909 bytes, 2381252 pieces, 1780281 relocations
function offsets: 136635, oracle 136635, 0 differ
code: 60733909 bytes; relocations: 1780281, oracle 1780281, 0 differ
P4 OK
exit=0
```
