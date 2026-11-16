-- p1.fut: problem 1, resolve (ref/flat/README.md): 1a the join by name, 1b relocation targets,
-- 1c the content-hash join. 128-bit keys arrive as interleaved u64 pairs (k1 at 2i, k2 at 2i+1).

def rank_def = 1u32 << 29

def mix64 (h: u64) : u64 =
  let h = (h ^ (h >> 30)) * 0xBF58476D1CE4E5B9
  let h = (h ^ (h >> 27)) * 0x94D049BB133111EB
  in h ^ (h >> 31)

-- ---- A hash table built without atomics: rounds of linear probing.
-- In round r every key still looking probes slot h(key)+r; the empty slots it hits are claimed by
-- reduce_by_index (largest element wins); a key is placed when its slot holds an equal key (it won,
-- or an equal key did). Equal keys probe in lock step, so they always land together.
def slot_of (m: i64) (k1: u64) (k2: u64) (r: i64) : i64 =
  (i64.u64 (mix64 (k1 ^ mix64 k2)) + r) & (m - 1)

-- tab: per slot, the element that owns it (-1: empty); slot: per element, its slot
def hash_insert [n] (m: i64) (k1: [n]u64) (k2: [n]u64) : ([m]i64, [n]i64) =
  let (tab, slot, _, _) =
    loop (tab, slot, pend: []i64, r) = (replicate m (-1i64), replicate n (-1i64), iota n, 0)
    while length pend > 0 do
      let pos = map (\e -> slot_of m k1[e] k2[e] r) pend
      let tab = reduce_by_index tab i64.max (-1) (map (\p -> if tab[p] < 0 then p else -1) pos) pend
      let placed = map2 (\e p -> let o = tab[p] in k1[o] == k1[e] && k2[o] == k2[e]) pend pos
      let slot = scatter slot (map2 (\e ok -> if ok then e else -1) pend placed) pos
      let pend = zip pend placed |> filter (\(_, ok) -> !ok) |> map (.0)
      in (tab, slot, pend, r + 1)
  in (tab, slot)

-- the slot holding key (k1, k2), or -1: walk the probe sequence until the key or an empty slot
def hash_find [m] [n] (tab: [m]i64) (tk1: [n]u64) (tk2: [n]u64) (k1: u64) (k2: u64) : i64 =
  let (p, _) =
    loop (p, r) = (-2, 0) while p == -2 do
      let q = slot_of m k1 k2 r
      let o = tab[q]
      in if o < 0 then (-1, r) else if tk1[o] == k1 && tk2[o] == k2 then (q, r) else (-2, r + 1)
  in p

def table_size (n: i64) = 1i64 << i64.i32 (64 - i64.clz (2 * n))   -- a power of two, load <= 1/2

-- ---- 1a. The join by name.
-- Keys of the linker's wanted names and of every keyed definition and reference by name (kinds 4, 5) go
-- into one table; each group's winner is the maximum of rank << 32 | ~id; kind-1 candidates only join a
-- group that is there.
entry resolve [N] (kind: [N]u8) (flags: [N]u8) (key: []u64) (rank: [N]u32) (want: []u64) (bkey: []u64) : ([N]u32, []u32) =
  let W = length want / 2
  let B = length bkey / 2
  let k1 g = key[2 * g]
  let k2 g = key[2 * g + 1]
  let [nn] (named: [nn]i64) = filter (\g -> (kind[g] == 4 || kind[g] == 5) && k1 g != 0) (iota N)
  let tk1 = map (\i -> want[2 * i]) (iota W) ++ map k1 named
  let tk2 = map (\i -> want[2 * i + 1]) (iota W) ++ map k2 named
  let m = table_size (W + nn)
  let (tab, slot) = hash_insert m tk1 tk2
  let value g = u64.u32 rank[g] << 32 | u64.u32 (!(u32.i64 g))
  let win = reduce_by_index (replicate m 0u64) u64.max 0 (drop W slot :> [nn]i64) (map value named)
  let cands = filter (\g -> kind[g] == 1 && k1 g != 0) (iota N)
  let win = reduce_by_index win u64.max 0 (map (\g -> hash_find tab tk1 tk2 (k1 g) (k2 g)) cands) (map value cands)
  let canon = map (\g ->
                     let p = if kind[g] != 2 && kind[g] != 3 && k1 g != 0 then hash_find tab tk1 tk2 (k1 g) (k2 g) else -1
                     in if p >= 0 then !(u32.u64 win[p]) else if flags[g] & 2 != 0 then 0 else u32.i64 g)
                  (iota N)
  let builtin = map (\i -> let p = hash_find tab tk1 tk2 bkey[2 * i] bkey[2 * i + 1]
                           in if p >= 0 && win[p] != 0 && u32.u64 (win[p] >> 32) & rank_def != 0
                              then !(u32.u64 win[p]) else 0)
                    (iota B)
  in (canon, builtin)

-- ---- 1b. Every relocation's target: its slot's object's package table, then the canonical id.
-- the row of CSR offsets `at` (n rows) that holds item j: the largest s with at s <= j
def row_of (n: i64) (at: i64 -> i64) (j: i64) : i64 =
  let (lo, _) = loop (lo, hi) = (0, n) while hi - lo > 1 do
                  let m = (lo + hi) / 2 in if at m <= j then (m, hi) else (lo, m)
  in lo

def pkg_self = 2147483643u32

entry reltargets [N] [S] [B] [R] (canon: [N]u32) (builtin: [B]u32) (slot_rel: [S]u32) (slot_obj: [N]u32)
                                 (pkg_off: []u32) (pkg: []u32) (rel_pkg: [R]u32) (rel_idx: [R]u32) : [R]u32 =
  let target r =
    let g = row_of S (\s -> i64.u32 slot_rel[s]) r
    let o = i64.u32 slot_obj[g]
    let base0 = i64.u32 pkg_off[o]
    let npk = pkg_off[o + 1] - pkg_off[o] - 5
    let p = rel_pkg[r]
    let idx = rel_idx[r]
    let sp = p - pkg_self
    let t = if sp < 5 then npk + sp else p
    let s = if t < npk + 5 && pkg[base0 + i64.u32 t] != 0xFFFFFFFF
            then (if sp == 1 then (if i64.u32 idx < B then builtin[i64.u32 idx] else 0)
                  else pkg[base0 + i64.u32 t] + idx)
            else 0
    in if s != 0 then canon[i64.u32 s] else 0
  in map target (iota R)

-- 1b, second shape: each relocation's slot by segment expansion instead of a binary search: scatter every
-- slot with a non-empty row to its row's first relocation, then a max-scan carries it over the row.
entry reltargets_scan [N] [S] [B] [R] (canon: [N]u32) (builtin: [B]u32) (slot_rel: [S]u32) (slot_obj: [N]u32)
                                      (pkg_off: []u32) (pkg: []u32) (rel_pkg: [R]u32) (rel_idx: [R]u32) : [R]u32 =
  let starts = map (\g -> if g + 1 < S && slot_rel[g] < slot_rel[g + 1] then i64.u32 slot_rel[g] else -1) (iota S)
  let owner = scatter (replicate R 0i64) starts (iota S) |> scan i64.max 0
  let target r g =
    let o = i64.u32 slot_obj[g]
    let base0 = i64.u32 pkg_off[o]
    let npk = pkg_off[o + 1] - pkg_off[o] - 5
    let p = rel_pkg[r]
    let idx = rel_idx[r]
    let sp = p - pkg_self
    let t = if sp < 5 then npk + sp else p
    let s = if t < npk + 5 && pkg[base0 + i64.u32 t] != 0xFFFFFFFF
            then (if sp == 1 then (if i64.u32 idx < B then builtin[i64.u32 idx] else 0)
                  else pkg[base0 + i64.u32 t] + idx)
            else 0
    in if s != 0 then canon[i64.u32 s] else 0
  in map2 target (iota R) owner

-- ---- 1c. The content-hash join over the copies the flood reached (mark 1) or names (mark 2).
-- Groups: kind 2 by k1, kind 3 by (k1, k2). Winner: the maximum of
--   reached << 63 | (kind 2: size) << 32 | ~id      (sizes of kind 2 are tiny: at most 8 here)
entry content_join [N] (canon: [N]u32) (kind: [N]u8) (hkey: []u64) (mark: [N]u8) : [N]u32 =
  let copies = filter (\g -> (kind[g] == 2 || kind[g] == 3) && mark[g] != 0) (iota N)
  let size g = if kind[g] == 2 then hkey[2 * g + 1] else 0
  let tk1 = map (\g -> hkey[2 * g]) copies
  let tk2 = map (\g -> if kind[g] == 2 then 0 else hkey[2 * g + 1] | (1 << 63)) copies
  let m = table_size (length copies)
  let (_, slot) = hash_insert m tk1 tk2
  let value g = u64.bool (mark[g] == 1) << 63 | u64.min (size g) 0x7FFFFFFF << 32 | u64.u32 (!(u32.i64 g))
  let win = reduce_by_index (replicate m 0u64) u64.max 0 slot (map value copies)
  let new = map2 (\g p ->
                    let w = i64.u32 (!(u32.u64 win[p]))
                    in if mark[w] == 2 || w == g || (kind[g] == 2 && mark[g] == 2 && size g > size w) then -1 else w)
                 copies slot
  in scatter (copy canon) (map2 (\g w -> if w < 0 then -1 else g) copies new) (map u32.i64 new)
