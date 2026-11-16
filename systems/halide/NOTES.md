# Halide: the grid cells (problems 1, 2, 3)

Halide 21.0.0, C++ AOT generators (install notes at the end). Branch `worktree-agent-a45ed3bb4a5a5b2a6`.
Code: `systems/halide/cells/`: `gen_p1.cpp gen_p2.cpp gen_p3.cpp` (the Halide algorithms and schedules),
`join.h` (a hash join in Halide, used by 1a, 1c and P2), `p1.cpp p2.cpp p3.cpp` (drivers: mmap the flat files,
wrap them zero-copy as `Halide::Runtime::Buffer`, call the AOT pipelines, check against the oracles), `bench.h`
(timing), `build.sh` (one `.a` per schedule variant). Timings: `systems/halide/bench.sh` (one line per cell,
threads, warm/e2e).

Build and check (each driver checks every variant it runs against the oracles and ends with `P<n> OK`):

    systems/halide/cells/build.sh                      # ~1 min: generators, every variant, drivers
    B=systems/halide/cells/build; D=/home/exedev/paradigms/data/minio
    HL_NUM_THREADS=8 $B/p1 $D e2e 1                    # P1 OK: canon, builtin, reltarget, canon after 1c
    HL_NUM_THREADS=8 $B/p2 $D e2e 1                    # P2 OK: live set (lazy decode, parallel)
    HL_NUM_THREADS=8 $B/p3 $D e2e 0 2                  # P3 OK: 3a addresses, 3b image
    systems/halide/bench.sh                            # all timings

Threads are Halide's own pool (`HL_NUM_THREADS`). Its idle workers spin for a short while, then sleep on a
condition variable; there is no wait-policy knob, so the 8-thread numbers are with Halide's default (the RESUME.md
load warning: no 100x collapse was seen, see the ranges).

TIMINGS_PLACEHOLDER
