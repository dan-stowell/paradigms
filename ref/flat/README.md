# The flat files

Every system in the grid reads these and nothing else: no system parses Go objects. They are written by
`ref/flat/extract.sh <workload>` into `data/<workload>/` (git-ignored, about 1.1 GB for MinIO), by a copy of
bottom-up's hlink (`ref/hlink`, from bottom-up `774e37c`) with one added file, `ref/hlink/dumpflat.c`, that writes
hlink's own state at the phase boundaries. The link it dumps from is unchanged (its output is byte-identical to
the stock hlink's) and runs (`--version`).

`ref/flat/check.sh` builds three plain single-core C programs (`ref/flat/p1.c`, `p2.c`, `p3.c`) that read only
these files and reproduce every oracle exactly: that is the proof that the files say everything a problem needs.
Read them next to this file: they are the executable form of the rules below.

## Conventions

- Raw little-endian arrays, no header. The suffix is the element type: `.u8 .u16 .u32 .i32 .i64 .u64`;
  `.u64x2` is pairs of u64 (a 128-bit key: k1 then k2).
- `meta.txt`: `key value` lines (counts and a few addresses). MinIO: `nslots 5205427`, `nrel 9403582`, ...
- **Ids.** A symbol slot has a dense id `1..nslots` (0 is "none"). Ids above `nslots` (up to `nids-1`) are symbols
  the linker makes; they only appear in problem 3's address table. Per-slot arrays have `nslots+1` entries.
- **CSR.** `X_off.u32` / `X_start.u32` with `n+1` (or `nslots+2`) entries: the rows of item `i` are
  `[off[i], off[i+1])`.
- **Strings.** `P.off.u64` (count+1 offsets) and `P.bin` (bytes): string `i` is `bin[off[i]:off[i+1]]`.
  Strings are interned per table, so equal ids mean equal strings. String 0 is "".

## Common

| file | what |
|---|---|
| `slot_obj.u32` | per slot: its object (0..nobjs-1). Objects are in hlink's load order; an object's slots are contiguous |
| `name.off.u64`, `name.bin` | per slot: its symbol name (not interned; content-addressed symbols mostly have none). **Slot g's name is `name.bin[off[g-1] : off[g]]`** (off[0] = 0; there is no entry for slot 0), not `off[g]..off[g+1]` |

## Problem 1: resolve

Each object's slots are, in order: definitions referenced by package index (kind 1), content-addressed definitions
with a 64-bit hash (2) and with a longer hash (3), definitions by name (4), references by name to symbols defined
elsewhere (5).

| file | what |
|---|---|
| `p1_kind.u8` | per slot: 1..5 as above |
| `p1_namekey.u64x2` | per slot: the 128-bit key of (name, ABI) if the slot takes part in the join by name, else (0,0). (k1 is never 0 for a real key.) Kinds 4 and 5 take part; kind 1 slots with a key are *candidates* (below); static, unnamed and debug-info symbols have no key |
| `p1_rank.u32` | per definition (kinds 1, 4): its rank. bit 31: not DUPOK; bit 30: not DUPOK and has content (text symbols count as having content even with no bytes; DUPOK definitions never have bit 30); bit 29: a definition; bits 0-27: min(size, 2^28-1). References: 0 |
| `p1_flags.u8` | per slot: bit 0: the record says UsedInIface; bit 1: a reference made only by debug information |
| `p1_want.u64x2` | keys of names the linker itself looks up (builtins, `main.main`, the entry, `-X` names...): they take part in the join with value 0 and no slot |
| `p1_builtin.u64x2` | the key of builtin `i` (`nbuiltin` of them), for references with package index "builtin" |
| `slot_rel.u32` | CSR (`nslots+2`): the relocations of slot `g` are `[slot_rel[g], slot_rel[g+1])` (definitions only) |
| `rel_off.i32 rel_siz.u8 rel_type.u16 rel_add.i64` | per relocation: offset in the symbol, width, cmd/internal/objabi type (bit 15 = weak), addend |
| `rel_pkg.u32 rel_idx.u32` | per relocation: the target as the object names it: (package index, symbol index) |
| `obj_pkg_off.u32`, `obj_pkg.u32` | CSR per object: `npkg+5` bases. Entry `p < npkg`: the id base of that imported package (0xFFFFFFFF: not linked); then 5 entries for the special package indices Self, Builtin, Hashed, Hashed64, None (= 2147483643..2147483647): the base of this object's own block of that kind |
| `p1_hashkey.u64x2` | per content-addressed slot: kind 2: (the 8 bytes, the symbol's size); kind 3: (hash bytes 0-7, hash bytes 8-11) |
| `p1c_mark.u8` | per content-addressed slot: 1 if the flood (problem 2) reached it, in hlink's sense: any of live, pc-table, funcdata or name-needed was set on it (so 807,866 mark-1 slots are not in `oracle/p2_live.u32`; don't rebuild marks from the live set), 2 if it was not reached but a relocation the flood does not follow names it, else 0 |

**1a. The join by name.** Group the slots with a key (kinds 4 and 5) and the wanted keys by key. A kind-1 candidate
joins a group only if its key is already there (it never creates a group). The winner of a group is the maximum
of `rank << 32 | ~id` (highest rank, then lowest id); every member's canonical id is the winner. Every other slot
is its own canonical id, except a kind-5 slot with `p1_flags` bit 1, whose canonical id is 0. A group whose
winner is a reference stands for an undefined symbol. Builtin `i` resolves to its key's winner if that is a
definition (rank bit 29), else 0. Oracle: `oracle/p1_canon.u32` (per slot), `oracle/p1_builtin.u32`.

**1b. Relocation targets.** For each relocation of slot `g` in object `o`: with `sp = rel_pkg - 2147483643`,
`t = sp < 5 ? npkg + sp : rel_pkg`; if `t < npkg+5` and `base = obj_pkg[o][t] != 0xFFFFFFFF`, the target slot is
`builtin[rel_idx]` when `sp == 1`, else `base + rel_idx`; then its canonical id (1a). Otherwise 0.
Oracle: `oracle/p1_reltarget.u32` (per relocation, in `slot_rel` order).

**1c. The content-hash join** (it runs after the flood in hlink, over what the flood reached: `p1c_mark`).
Among the slots with mark 1 or 2, group kind 2 by k1, kind 3 by (k1, k2). Winner: a mark-1 copy beats a mark-2
one; then for kind 2 the largest size (k2), then the lowest id; for kind 3 the lowest id. If the winner has mark
2 the group is not in the output and nothing changes. Otherwise every member's canonical id becomes the winner,
except a mark-2 kind-2 copy larger than the winner (it keeps itself). Oracle: `oracle/p1c_canon.u32` (the
final canonical id of every slot).

## Problem 2: reach

The graph after 1a (content-addressed copies are still themselves: the flood treats each copy as its own symbol).
Ids are canonical slot ids.

| file | what |
|---|---|
| `aux_start.u32` | CSR per slot (`nslots+2`), like `slot_rel`: raw AuxFuncdata records for every definition, including noncanonical and non-text slots; references have empty rows |
| `aux_pkg.u32`, `aux_idx.u32` | per AuxFuncdata record: raw (package index, symbol index), in object record order, before any canonicalization. Decode with the source slot's object package bases and 1a canon/builtins, exactly as relocation targets |
| `p2_flags.u8` | per slot: 1 def (a definition, not an undefined symbol), 2 text, 4 reflect (calls reflect's Method), 8 gotype, 16 iface (converted to an interface, known from resolve), 32 has something to follow |
| `p2_edge_off.u32`, `p2_edge_dst.u32`, `p2_edge_kind.u8` | CSR per source (only canonical definitions with flag 32 have rows). Kind 0 plain: a relocation or funcdata the flood follows, dst = target id. Kind 1 useiface: dst is a type converted to an interface (an itab already replaced by its type). Kind 2 ifacemethod: dst = a signature id (an interface method some live code may call). Kind 3 namedmethod: dst = a string id (a method name called by reflection-like generic code) |
| `p2_sig_name.u32`, `p2_sig_type.u32` | per signature id: (method name string id, function type id) |
| `p2_meth_off.u32` + `p2_meth_name.u32 p2_meth_mtyp.u32 p2_meth_ifn.u32 p2_meth_tfn.u32 p2_meth_exported.u8` | CSR per type: its methods: name string id, the method's function type, the interface-call entry, the direct entry (ids, 0 if none), and whether the name is exported |
| `p2_str.off.u64`, `p2_str.bin` | the method name strings |
| `p2_roots.u32` | the entry points (entry symbol, init tasks in order, a few runtime symbols) |

Rules (cmd/link's deadcode.go, as hlink does them; `s` ranges over live defined symbols):

    live(r)              for every root r
    live(t)              <- live(s), plain(s,t)
    iface(t)             <- flag 16 on t
    iface(t)             <- live(s), useiface(s,t)
    iface(t)             <- live(s), iface(s), gotype(s), plain(s,t), gotype(t), def(t)
    sig(x)               <- live(s), ifacemethod(s,x)
    named(n)             <- live(s), namedmethod(s,n)
    reflect              <- live(s), reflect(s)
    iface(m.mtyp), cand(m) <- live(s), iface(s), gotype(s), method(s,m)
    live(m.mtyp), live(m.ifn), live(m.tfn)
                         <- cand(m), (reflect and m.exported) or sig(m.name, m.mtyp) or named(m.name)

`sig(m.name, m.mtyp)` means some live signature id has that (name, type) pair. Id 0 is never live: a method target of 0 (none in MinIO) derives nothing. Oracle: `oracle/p2_live.u32`, the
sorted ids of every live symbol (663,630 for MinIO: this counts each content-addressed copy and undefined symbol;
after 1c, 360,376 canonical symbols are live: `oracle/live_names.txt`, which equals fclink's and the stock linker's
live list).

`build/p2lazy DATA REPS --aux` decodes relocation and funcdata targets in the flood.
Aux edges are followed only for canonical text definitions, with zero targets dropped.
Setup checks decoded relocation and aux target multisets per source against the existing
P2 CSR (relocations precede the funcdata suffix); only kinds 1-3 enter the extra CSR.
Without `--aux`, p2lazy retains its original precomputed funcdata mode. Both modes use
1a canon/builtin oracles and precomputed flags, roots, interface rules and method tables;
setup/validation and initial state clearing are outside the flood timing.

To add aux columns without regenerating shared inputs, build with `ref/hlink/build.sh`,
then run `ref/flat/add-aux.sh WORKLOAD`. It dumps frozen inputs into scratch, compares
every pre-existing dataset file byte-for-byte (including oracles; the old README is
copied as extraction metadata), and copies only the three aux files after all comparisons pass. `DATA_ROOT` can
select a writable dataset root (default: this checkout's `data`).

## Problem 3: lay out and relocate

The placed symbols: live canonical definitions that go into a bucket (a part of a section), in hlink's unit
order (a unit is an object, or a run of 8192 slots of a big one) and then id order.

| file | what |
|---|---|
| `p3_sym.u32` | per placed symbol: its id |
| `p3_bucket.u8` | 1 text, 2 text FIPS, 3 rodata, 4 rodata FIPS, 5 go:func, 6 typelink, 7 type, 8 itab, 9 noptrdata, 10 noptrdata FIPS, 11 data, 12 data FIPS, 13 bss, 14 noptrbss, 15 coverage counters |
| `p3_class.u8` | log2 of its alignment |
| `p3_size.u32`, `p3_unit.u32` | its size; its unit |
| `p3_tlstr.u32` + `p3_str.*` | typelink bucket only: its type string (string id) |
| `p3_bucket_start.u32`, `p3_bucket_minalign.u32` | per bucket (16 entries): its start address and minimum alignment |
| `p3_data.off.u64`, `p3_data.bin` | per placed symbol: its bytes (may be shorter than its size: the rest is zero; bss buckets: none) |
| `p3_rel_start.u32` + `p3_rel_off.u32 p3_rel_siz.u8 p3_rel_kind.u8 p3_rel_add.i64 p3_rel_tgt.u32` | CSR per placed symbol: its relocations that write bytes: offset, width (1, 2, 4, 8), kind (below), addend, target slot id |
| `p3_addr_all.u32` | per id (`nids`): hlink's raw final `addr` array: image addresses, but also PC-table offsets for non-placed symbols. **An input to 3b only**: it contains 3a's answer (`p3_addr_all[p3_sym[i]] == oracle/p3_addr[i]` for every placed symbol), so a 3a cell must not read it. A 3a+3b pipeline cell propagates its own 3a addresses through `oracle/p1c_canon.u32` (which already includes 1a); see the extraction limitation below |

**3a. Layout.** occupied(i) = size rounded up to 2^class, where a text symbol (buckets 1, 2) is at least `minFunc`
(16) bytes. For each bucket other than typelink: `va = start`; for each unit in order: if the unit has bytes in
this bucket, `va = round_up(va, max(minalign, 2^top))` with `top` its largest class with bytes; then for each class
from 12 down to 0: the class's symbols in list order get `va, va + occupied, ...`. Typelinks: sort by type string
(bytewise, then shorter first), then id; `va = start`; each gets `va = round_up(va, 2^class)` and then
`va += size` (not occupied). Oracle: `oracle/p3_addr.u32` (per placed symbol).

**3b. Relocate.** Copy each placed symbol's bytes to `image[addr - imageBase]` (not buckets 13-15) and apply its
relocations, `P = addr + off`, `T = p3_addr_all[tgt]`:

| kind | value written (`siz` bytes, little-endian) |
|---|---|
| 1 addr | `T + add`; T = 0 with tgt != 0 is an error |
| 2 addr, weak | as 1, but T = 0 with tgt != 0 means T = `addr_unreachableMethod` (meta) |
| 3 addroff | `T - sectbase(T) + add`; nothing if T = 0. sectbase: the largest of `lay_text, lay_rodata, lay_pclntab, lay_types, lay_noptrdata, lay_data, lay_bss, lay_noptrbss` (meta) not above T |
| 4 methodoff | as 3, but T = 0 writes 0xFFFFFFFF |
| 5 call, 7 pcrel | `T + add - (P + siz)`; tgt = 0: `add - (P + siz)`; T = 0 with tgt != 0 is an error |
| 6 call, weak | as 5, but T = 0 with tgt != 0 means T = `addr_mapinitnoop` (meta) |
| 8 const | `add` (TLS and size relocations folded in) |

Oracle: `oracle/p3_image.bin` is hlink's whole output file (file offset = address - imageBase); every placed
symbol's `[addr, addr+size)` must match it. hlink also writes the pclntab, GC bitmaps, type links, moduledata
and ELF headers; those are not part of problem 3.

## What the extraction did for the systems (so a timing is read fairly)

The flat files are not raw object files: they have names hashed into 128-bit keys (P1), references decoded
into canonical ids and itabs resolved to their types (P2), method tables decoded from type descriptors (P2), and
relocation types normalised and size relocations folded into constants (P3). hlink does that work inside its
phases; a system here does not. Compare a system's time with the plain C references on the same files first,
and with hlink's phase times only with that caveat.

## 3a -> 3b as one pipeline

`build/p3 DATA --pipeline` lays out (3a), builds every id's address from its own 3a addresses through
`oracle/p1c_canon.u32` (final canon, 1a included) plus the given `p3_fixed_id.u32`/`p3_fixed_addr.u32`, then
relocates (3b) and compares the image. It never reads `p3_addr_all.u32`. P3 pipeline OK on MinIO and kube-apiserver
(2026-11-16T16:02:18Z).

| file | what |
|---|---|
| `p3_fixed_id.u32`, `p3_fixed_addr.u32` | relocation targets the linker places itself, outside 3a's buckets (MinIO: 7: `runtime.buildVersion`, `runtime.modinfo`, `runtime.godebugDefault`, `runtime.firstmoduledata`, `runtime.lastmoduledatap`, `runtime.runtime_inittasks`, `go:fipsinfo`; kube: 6): canonical id and final address. A given input, like the bucket starts |

`p3_addr_all.u32` is hlink's raw `addr` array: besides image addresses it holds PC-table offsets of non-placed
symbols (layout.c `addrUnit`), so 661,018 ids differ from "placed address through canon"; no relocation reads
those. Found by Codex (results/codex-c.md), fixed by extracting the specials.

### Asynchronous reach reference

`p2async.c` keeps P2's transfer rules and serial method phase, with private LIFO
stacks and mutex-protected published chunks (up to 256 items) for stealing.
Outstanding work credits cover both private work and published chunks; idle
workers sleep, and each round ends with one pthread join phase. There are no
per-level barriers. `OMP_NUM_THREADS` selects its pthread count for comparison
with `p2par`; it does not use the OpenMP runtime or its wait policy.

Build with `gcc -O2 -march=native -pthread -o build-g/p2async ref/flat/p2async.c`;
run `OMP_NUM_THREADS=8 build-g/p2async data/minio 100` (last argument: repetitions).
Both reach programs report mean/total kernel time and final-repetition item
counts in addition to best time. See `results/codex-g.md` for the interleaved
measurements and the distinction between identical live sets and variable pops.
`check.sh` defaults to `build-g/`; set `BUILD_DIR` to override it.
