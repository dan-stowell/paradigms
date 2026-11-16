# Shared brief for helpers (paradigms repo)

The project (CHARTER.md in /home/exedev/paradigms) takes paradigms that run at hardware speed elsewhere
and makes them do a linker's work on real data: linking MinIO (a large Go program, ~1,312 packages) from
the stock Go toolchain's object files. Reference: a C linker `hlink` (/home/exedev/targets/bottom-up/link,
branch link-hardware-speed) at ~4-5% of a computed hardware floor (FLOOR.md there), and SHAPES.md in
/home/exedev/targets/simd-lang (what made a whole-program Go build fast: joins, floods, scans,
reduce-by-key, arenas, memory-bound work).

Data sizes (MinIO): 5.2 M symbol slots, 9.4 M relocations, ~2.7 M by-name references, 360,376 live
symbols out of the reachable flood, 109 MB output image. Machine: 8 cores, 31 GiB, no GPU; the work is
memory-bound (DRAM read 92 GB/s, random loads 0.74 G/s into a 512 MB table).

The problems (each system reads flat little-endian column files, not Go objects):
1. Resolve: match every symbol reference to its definition: by name+ABI (a hash join with a winner rule:
   definition beats reference, non-DUPOK beats DUPOK, content beats BSS, larger wins, then earliest), by
   package index (a gather through a per-object table), by content hash (dedup of identical symbols).
2. Reach: the live set from the entry points, following relocations, plus Go's method rules (a method of
   a type is live only if the type is converted to an interface and some live code calls a method with that
   name+signature, or reflection is used). A flood / graph reachability with a conditional rule.
3. Lay out and relocate: assign addresses to live symbols (grouped by section, alignment, ordering rules:
   a scan/prefix sum), then patch every relocation of a live symbol in the output image (a gather + scatter).
4. Stretch: code generation by shape: group 1.4 M expressions into 4,096 node shapes (96.7% coverage),
   emit each shape from a byte template, patch operands.

Rules for helpers: write details to the file you were told to; reply in five lines or fewer. Never modify
/home/exedev/targets/*. Cap memory-hungry processes (prlimit --as=8G or systemd-run --user --scope -p MemoryMax=8G).
