# Independent P2 asynchronous review

Reviewed 2026-11-16, including the pthread worker conversion, against `p2par.c`
and `ref/flat/README.md` problem 2. No code changes by reviewer.

## Correctness reasoning

The graph rules are positive and monotone. First live transitions enqueue ordinary
processing. First iface transitions either precede live processing, or enqueue a
second pass when the type is already live. That second pass is necessary: merely
claiming monotonicity does not remove the need to revisit live types. The
`ifaceDone` exchange allows exactly one interface-aware expansion per type; a
second claimant may skip because the owner remains outstanding until its
expansion is complete. Method candidates are collected by that unique owner;
method selection, signatures, named methods and reflection keep the original
serial phase between complete floods. Undefined live symbols remain in the live
set without being expanded. Symbol-zero behavior is inherited from the baseline;
method targets explicitly exclude zero.

Pending counts credits, not individual items. Every active worker with private
work owns one credit; every nonempty published mailbox owns one. Publication
adds a credit before exposing a chunk. Taking a whole mailbox transfers its
credit without changing the count. The worker retires its credit only when the
private stack is empty, after processing the previous item and publishing all
children. Thus pending zero cannot occur with queued, executing, or not-yet-
published work. Mailbox vectors are accessed under their mutex, with the atomic
`available` field only an advisory fast path. Private vectors, method vectors,
reflection slots and counters have one owner. Pthread creation publishes setup,
and joins complete before method processing or the next round resets state.
Idle workers sleep for 50 microseconds; mutex waits and joins block.

The relaxed live/iface handshake needs store-to-load ordering. Abstract relaxed
atomics alone allow both the iface setter to miss live and the first live
processor to miss iface. On this x86_64 VM, GCC emits memory `xchg` for tas;
these exchanges are implicitly locked and supply that ordering. The final
implementation includes a sequentially consistent fence after tas on non-x86
to preserve the handshake there while keeping flag accesses relaxed. I checked
the fence placement and the final formatting changes; no new issue was found.
The non-x86 path was reviewed but was not executed on this x86 VM. This was a
portability finding, not an observed failure on this VM.

## Independent check

Commands, all limited to 8 GiB:

```sh
prlimit --as=8589934592 -- bash -c 'gcc -Wall -Wextra -O2 -march=native -pthread -o build-g/p2async-review ref/flat/p2async.c
for data in minio kube-apiserver; do
  for threads in 1 2 8; do
    OMP_NUM_THREADS=$threads ./build-g/p2async-review data/$data 1
  done
done'
prlimit --as=8589934592 -- bash -c 'objdump -d build-g/p2async-review | rg -m 5 "xchg.*\("'
```

The compiler emitted no warnings. All six independently run checks printed
`P2 OK`: MinIO 663,630 live, kube-apiserver 926,486 live, each exactly matching
its oracle and each taking seven rounds. These are correctness spot checks, not
performance measurements or a replacement for the parent's 20-run matrix.

| Dataset | Threads | Queue pops | Cross-worker steals |
|---|---:|---:|---:|
| MinIO | 1 | 672505 | 0 |
| MinIO | 2 | 671973 | 22 |
| MinIO | 8 | 672692 | 333 |
| kube-apiserver | 1 | 945046 | 0 |
| kube-apiserver | 2 | 940187 | 15 |
| kube-apiserver | 8 | 937406 | 299 |

**Equal live sets do not imply equal work-item counts.** A live transition can
queue a type before an iface transition queues it a second time; another
schedule coalesces the same two facts before the first processing. The
`ifaceDone` guard makes redundant interface processing harmless, but queue pops
still vary, as these oracle-equal executions demonstrate. Report the actual
counts and their variation rather than asserting equality to p2par.
