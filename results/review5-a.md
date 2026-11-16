## A. Halide P3a: output-independent address validation

**Before.** The driver copied `oracle/p3_addr.u32` into its output before layout.
This made a skipped pipeline call capable of passing the address check. There is
an important qualification: the original generator also had `addr(i) = 0`, so
an omitted update after that initialization would not necessarily retain the
oracle. The review's blanket omitted-write claim is stronger than the source
alone establishes. Oracle-prefilling a computed output is still unnecessary.

**Observation.** `check32` already compares every entry, checks the oracle length,
and counts all mismatches (only the diagnostic printing stops after five).
MinIO has **360,315** placed addresses. The address type is `u32`, so the poison
is **0xFFFFFFFF**, not a 64-bit value. With one actual layout unit omitted,
addresses 0 and 1 retain poison instead of 4,198,400 and 96,599,872.

**Fix.** `runLayout` poisons the full buffer before **every** invocation, including
warm repetitions, and checks Halide's return code. Each schedule additionally
gets a separate poisoned checked run. The generator's redundant zero-init is
now `undef`: the updates must supply every address. Runtime `P3_SKIP_UNIT=N`
skips that unit's ordinary and typelink address writes; default -1 omits none.
This is an omitted write inside the generator, not corruption after execution.
The e2e driver checks all addresses before relocation, so a failed address cannot
cause out-of-bounds image writes. Only isolated P3b runs load oracle addresses.
The e2e total includes the early address check; its separate 3b timer excludes it.

**Number.** All eight schedules pass all **360,315** address comparisons at both
1 and 8 threads; e2e also reports **P3 OK**, zero image-symbol differences and
zero relocation errors at both thread counts. With `P3_SKIP_UNIT=0`, both e2e
runs report exactly **2** address differences, **P3 FAIL**, and exit **1**.
The 8-thread warm benchmark with the same omitted unit also fails with exactly
2 differences and exit 1 after all repetitions, proving reuse cannot hide it.
For `layout_c3_w1`, five calibrated timing batches give min/median/max
**57.13 / 61.88 / 64.19 ms** at 1 thread and
**21.54 / 40.35 / 44.13 ms** at 8 threads. Poisoning is included in these times;
these establish continued operation, not a speedup against historical numbers.
Window 2026-11-16 19:39–19:41 UTC; preceding uptime samples show
4h24m–4h25m uptime, 1-minute load **4.11–9.66**. Other review work shared the VM.

Reproduce (all writes stay in the clone; input is read-only):

```sh
systemd-run --user --scope -p MemoryMax=12G systems/halide/cells/build.sh p3
bash results/review5-a-check.sh
```

Build output: `results/review5-a-build.log`; all oracle outcomes, fault exits,
timestamps, uptime and every schedule's timing: `results/review5-a-check.log`.
