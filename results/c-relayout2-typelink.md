# Typelink diagnosis, 2026-11-16

Run (read-only inputs, optional Attempt 1 exports):

```sh
prlimit --as=8589934592 python3 ref/flat/diagnose-typelink.py /home/exedev/paradigms/data build-relayout/check-edit2
```

Bucket 6 is **full type descriptors**, not entries of a 4-byte offset table. `ref/hlink/dumpflat.c:492` extracts each symbol's actual size and relocations; `ref/hlink/layout.c:457` walks sorted descriptor symbols and advances by `symSiz(s)`.

| Dataset | Descriptors | Descriptor bytes | Relocation records | Actual stores | Missing-target ADDROFF skips | Sorted offset-table bytes |
|---|---:|---:|---:|---:|---:|---:|
| A / B1 | 33,261 | 3,416,136 | 428,898 | 412,513 | 16,385 | 133,044 |
| B2 | 32,428 | 3,339,176 | 420,620 | 404,633 | 15,987 | 129,712 |

B2's descriptor relocation kinds: 100,211 absolute address, 124,551 section-relative ADDROFF, 195,858 METHODOFF. These are pointers/offsets **inside descriptors**, not table records.

The Attempt 1 export reproduces the 404,622 count exactly: 32,424 moved descriptors contain 420,604 relocation records, of which 15,987 are missing-target ADDROFF skips; their copies therefore perform **404,617 stores**. The remaining four unmoved descriptors have **five reverse-index sites** whose targets moved, producing the other five stores. The diagnostic prints those source/kind/target triples.

A new placement rule can keep these descriptors at stable addresses and maintain only an auxiliary sorted array of `descriptor_address - types_anchor`. Rewriting that array needs 129,712 bytes for B2; keeping a deterministic zero suffix in A's 133,044-byte capacity adds 3,332 clearing bytes, for **133,044 total writes**. Restoration writes 133,044 bytes. Edit 1 changes neither type-list membership nor descriptor addresses, so needs no table writes. Count descriptor relocations separately from table writes.

**Runtime-format boundary:** this repository's frozen hlink uses a contiguous, sorted descriptor region, not an existing explicit typelink offset array: `layout.c:465` sets `lay.typedesclen = va - lay.types`, and `special.c:167` writes that span into moduledata. An offset array is an exact alternative lookup representation under the new experimental rule, but integrating it into this runtime requires a matching consumer/metadata change. Merely leaving holes in the descriptor region and emitting a sidecar does not produce a valid executable for that frozen runtime. The P3 image experiment must not claim executable/ABI equivalence.
