# Reference helpers for c-relayout

`ref/flat/p3bench.c` includes the original `p3.c` under a renamed main so it shares its comparator and constants. Its timed layout and relocation loops are literal copies of the reference loops; address propagation is the same pipeline algorithm (placed table, fixed symbols, final canon). It allocates and frees all layout scratch, address tables and a fresh zero image on **every repetition**. Mapping inputs and final checks are outside timing. No output-file I/O; ordinary calloc output, no huge-page option. Runs repeat in-process for >=1 second of measured work and report the mean per repetition. Build both forms:

```
gcc -O3 -o build-relayout/p3bench ref/flat/p3bench.c
gcc -O3 -fopenmp -o build-relayout/p3parbench ref/flat/p3bench.c
OMP_NUM_THREADS=1 prlimit --as=8589934592 build-relayout/p3bench /home/exedev/paradigms/data/minio-edit 1
OMP_NUM_THREADS=1 prlimit --as=8589934592 build-relayout/p3parbench /home/exedev/paradigms/data/minio-edit2 1
```

Every final address is compared to `oracle/p3_addr.u32`; every final produced symbol's bytes are compared to `oracle/p3_image.bin`. `P3BENCH ... OK` requires zero address, image and relocation errors. This is a full pipeline timing, including O(nids) propagation, whereas GRID's separate 3a and 3b entries do not include that propagation.

`ref/flat/p3compare.c` is independent of the slack implementation. It compares actual oracle addresses and relocated bytes. Symbols are paired by the provided unified map. Relocation sites are paired by (unified source id, byte offset, width), preserving original order for duplicate sites; newly added/deleted sites are reported separately from common sites whose written byte value changes. This site-based definition deliberately does not equate two relocations simply because they share an ordinal after inserted instructions. A site with a changed target but equal output bits is not counted as a changed value.

```
gcc -O3 -o build-relayout/p3compare ref/flat/p3compare.c
prlimit --as=8589934592 build-relayout/p3compare /home/exedev/paradigms/data/minio /home/exedev/paradigms/data/minio-edit
prlimit --as=8589934592 build-relayout/p3compare /home/exedev/paradigms/data/minio /home/exedev/paradigms/data/minio-edit2
```

Image comparisons copy only produced symbol ranges (buckets 1–12) into otherwise zero images, then compare at file/address positions over the union of those ranges. Removed range bytes therefore compare to zero unless occupied by another B symbol; newly produced bytes compare to zero unless occupied in A. Bucket attribution is the union of the two versions' masks; if a byte belongs to different buckets in A and B it is counted in both bucket rows. Thus per-bucket image union/change counts **must not be summed as globally unique bytes**. BSS buckets have address counts but no image-byte or relocation counts. The program asserts matching buckets for common unified symbols, no overlapping nonzero symbol ranges within each version, valid widths within symbol sizes, and unique unified placed ids. Both edits passed.
