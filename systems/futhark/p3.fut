-- p3.fut: problem 3, lay out (3a) and relocate (3b), from the flat files (ref/flat/README.md).
-- Entries take the flat columns as arrays; the C driver (p3.c) mmaps the files and calls them.
import "lib/sorts/radix_sort"
import "msort"
import "lib/segmented/segmented"

def nb = 16i64          -- buckets
def nc = 13i64          -- alignment classes 0..12
def b_typelink = 6u8
def is_text (b: u8) = b == 1 || b == 2
def is_bss (b: u8) = b == 13 || b == 14 || b == 15

def rup (v: i64) (a: i64) : i64 = (v + a - 1) & !(a - 1)

-- An advance is the function x -> rup(x + c, a) + d, with a a power of two. "Align to a, then add n" is
-- (0, a, n), and the composition of two advances is again one (adv_then), so a walk that aligns and adds
-- is a scan. Proof sketch: if a2 <= a1 the first result is already a2-aligned; else rounding a1-aligned
-- y + e up to a2 is the same as rounding y + rup(e, a1) up to a2.
type adv = (i64, i64, i64)
def adv_id : adv = (0, 1, 0)
def adv_then ((c1, a1, d1): adv) ((c2, a2, d2): adv) : adv =
  let e = d1 + c2
  in if a2 <= a1 then (c1, a1, rup e a2 + d2) else (c1 + rup e a1, a2, d2)
def adv_apply ((c, a, d): adv) (x: i64) : i64 = rup (x + c) a + d

-- occupied bytes of a symbol
def occupied (min_func: u32) (b: u8) (c: u8) (s: u32) : i64 =
  let s = if is_text b then u32.max s min_func else s
  in rup (i64.u32 s) (1 << i64.u8 c)

-- The cells of the walk, in placement order: bucket, then unit, then class from 12 down to 0.
-- Returns each cell's start address. One hist for the bytes, one for each block's largest class,
-- then a scan of advances per bucket.
def cell_starts [L] (nunits: i64) (min_func: u32) (bstart: [nb]u32) (bminalign: [nb]u32)
                    (bucket: [L]u8) (class: [L]u8) (size: [L]u32) (unit: [L]u32) =
  let occ = map3 (occupied min_func) bucket class size
  let block = map2 (\b u -> if b == b_typelink then -1 else i64.u8 b * nunits + i64.u32 u) bucket unit
  let cell = map2 (\k c -> if k < 0 then -1 else k * nc + (nc - 1 - i64.u8 c)) block class
  let ncells = nb * nunits * nc
  let cbytes = hist (+) 0 ncells cell occ
  let top = hist i64.max (-1) (nb * nunits) (map2 (\k o -> if o > 0 then k else -1) block occ) (map i64.u8 class)
  let step i =
    let k = i / nc
    let b = k / nunits
    let a = if i % nc == 0 && top[k] >= 0 then i64.max (i64.u32 bminalign[b]) (1 << top[k]) else 1
    in (0, a, cbytes[i])
  let steps = map step (iota ncells)
  let ends = segmented_scan adv_then adv_id (map (\i -> i % (nunits * nc) == 0) (iota ncells)) steps
  let cstart = map3 (\i e (_, _, n) -> adv_apply e (i64.u32 bstart[i / (nunits * nc)]) - n) (iota ncells) ends steps
  in (occ, cell, cstart)

-- typelinks: sorted by type string (bytewise, then shorter first), then id; then align and add size
-- compare strings x and y from byte k0 on (equal ids are equal strings: interned)
def str_cmp (soff: []u64) (sbin: []u8) (k0: i64) (x: u32) (y: u32) : i32 =
  if x == y then 0 else
  let xo = i64.u64 soff[i64.u32 x]
  let xn = i64.u64 soff[i64.u32 x + 1] - xo
  let yo = i64.u64 soff[i64.u32 y]
  let yn = i64.u64 soff[i64.u32 y + 1] - yo
  let n = i64.min xn yn
  let (_, c) = loop (k, c) = (k0, 0) while c == 0 && k < n do
                 let p = sbin[xo + k]
                 let q = sbin[yo + k]
                 in (k + 1, if p < q then -1 else if p > q then 1 else 0)
  in if c != 0 then c else if xn < yn then -1 else if xn > yn then 1 else 0

def typelinks [L] (bstart: [nb]u32) (bucket: [L]u8) (class: [L]u8) (size: [L]u32)
                  (sym: [L]u32) (tlstr: [L]u32) (soff: []u64) (sbin: []u8) =
  let tl = filter (\i -> bucket[i] == b_typelink) (iota L)
  -- sort (first 8 bytes, next 8 bytes, index) tuples: most comparisons end in the two words, the rest
  -- compare the whole strings
  let word i k = let o = i64.u64 soff[i64.u32 tlstr[i]]
                 let n = i64.u64 soff[i64.u32 tlstr[i] + 1] - o
                 in loop w = 0u64 for j < 8 do w << 8 | (if k + j < n then u64.u8 sbin[o + k + j] else 0)
  let keyed = map (\i -> (word i 0, word i 8, i)) tl
  let leq (x0, x1, i) (y0, y1, j) =
    if x0 != y0 then x0 < y0 else if x1 != y1 then x1 < y1
    else let c = str_cmp soff sbin 16 tlstr[i] tlstr[j] in c < 0 || (c == 0 && sym[i] <= sym[j])
  let tl = merge_sort_cpu leq keyed |> map (.2)
  let steps = map (\i -> (0, 1 << i64.u8 class[i], i64.u32 size[i])) tl
  let ends = scan adv_then adv_id steps
  let addr = map2 (\e (_, _, n) -> adv_apply e (i64.u32 bstart[i64.u8 b_typelink]) - n) ends steps
  in (tl, addr)

-- 3a, data parallel: a stable radix sort by (bucket, class) makes every (unit, bucket, class) cell a run
-- (the list is already in unit order), and a segmented scan inside the runs gives each symbol's offset.
entry layout [L] (nunits: i64) (min_func: u32) (bstart: [nb]u32) (bminalign: [nb]u32)
                 (bucket: [L]u8) (class: [L]u8) (size: [L]u32) (unit: [L]u32)
                 (sym: [L]u32) (tlstr: [L]u32) (soff: []u64) (sbin: []u8) : [L]u32 =
  let (occ, cell, cstart) = cell_starts nunits min_func bstart bminalign bucket class size unit
  let key = map2 (\b c -> if b == b_typelink then 255u32 else u32.u8 b * 13 + 12 - u32.u8 c) bucket class
  let order = radix_sort_by_key (\i -> key[i]) 8 u32.get_bit (iota L)
  let scell = map (\i -> cell[i]) order
  let socc = map (\i -> occ[i]) order
  let heads = map (\j -> j == 0 || scell[j] != scell[j - 1]) (iota L)
  let within = segmented_scan (+) 0 heads socc
  let saddr = map3 (\c w o -> if c < 0 then 0 else cstart[c] + w - o) scell within socc
  let addr = scatter (replicate L 0) order saddr
  let (tl, taddr) = typelinks bstart bucket class size sym tlstr soff sbin
  in scatter addr tl taddr |> map u32.i64

-- 3a, second shape: the same cell starts, then the symbols placed by a sequential loop with a cursor per
-- cell (in-place updates), as the C reference does.
entry layout_loop [L] (nunits: i64) (min_func: u32) (bstart: [nb]u32) (bminalign: [nb]u32)
                      (bucket: [L]u8) (class: [L]u8) (size: [L]u32) (unit: [L]u32)
                      (sym: [L]u32) (tlstr: [L]u32) (soff: []u64) (sbin: []u8) : [L]u32 =
  let (occ, cell, cstart) = cell_starts nunits min_func bstart bminalign bucket class size unit
  let (addr, _) =
    loop (addr, cur) = (replicate L 0, copy cstart) for i < L do
      let c = cell[i]
      in if c < 0 then (addr, cur) else (addr with [i] = cur[c], cur with [c] = cur[c] + occ[i])
  let (tl, taddr) = typelinks bstart bucket class size sym tlstr soff sbin
  in scatter addr tl taddr |> map u32.i64

-- the pieces of 3a, to time them apart (same signature as layout; the results are not addresses)
entry piece_cells [L] (nunits: i64) (min_func: u32) (bstart: [nb]u32) (bminalign: [nb]u32)
                      (bucket: [L]u8) (class: [L]u8) (size: [L]u32) (unit: [L]u32)
                      (_sym: [L]u32) (_tlstr: [L]u32) (_soff: []u64) (_sbin: []u8) : [L]u32 =
  let (_, _, cstart) = cell_starts nunits min_func bstart bminalign bucket class size unit
  in map (\i -> u32.i64 cstart[i % length cstart]) (iota L)
entry piece_typelinks [L] (_nunits: i64) (_min_func: u32) (bstart: [nb]u32) (_bminalign: [nb]u32)
                      (bucket: [L]u8) (class: [L]u8) (size: [L]u32) (_unit: [L]u32)
                      (sym: [L]u32) (tlstr: [L]u32) (soff: []u64) (sbin: []u8) : [L]u32 =
  let (tl, taddr) = typelinks bstart bucket class size sym tlstr soff sbin
  in scatter (replicate L 0) tl taddr |> map u32.i64
entry piece_radix [L] (_nunits: i64) (_min_func: u32) (_bstart: [nb]u32) (_bminalign: [nb]u32)
                      (bucket: [L]u8) (class: [L]u8) (_size: [L]u32) (_unit: [L]u32)
                      (_sym: [L]u32) (_tlstr: [L]u32) (_soff: []u64) (_sbin: []u8) : [L]u32 =
  let key = map2 (\b c -> if b == b_typelink then 255u32 else u32.u8 b * 13 + 12 - u32.u8 c) bucket class
  in radix_sort_by_key (\i -> key[i]) 8 u32.get_bit (iota L) |> map u32.i64

-- 3b. Copy every symbol's bytes into the image, then patch its relocations.
def chunk = 64i64

-- copy, shaped as regular chunks: each symbol's bytes cut into 64-byte chunks (an expand), each chunk
-- a row of 64 (index, byte) writes, -1 (ignored by scatter) past the end; one flat scatter
def copy_chunks [L] (nimg: i64) (image_base: i64) (addr: [L]u32) (bucket: [L]u8) (doff: []u64) (dbin: []u8) =
  let len s = if is_bss bucket[s] then 0 else i64.u64 doff[s + 1] - i64.u64 doff[s]
  let nch = map (\s -> (len s + chunk - 1) / chunk) (iota L)
  let (owner, k) = repl_segm_iota nch
  let row s k = #[sequential] map (\j -> let p = k * chunk + j
                           in if p < len s then (i64.u32 addr[s] - image_base + p, dbin[i64.u64 doff[s] + p])
                                           else (-1, 0u8)) (iota chunk)
  let (is, vs) = unzip (flatten (map2 row owner k))
  in scatter (replicate nimg 0u8) is vs

-- the row of CSR offsets `at` (n rows) that holds item j: the largest s with at s <= j (binary search)
def row_of (n: i64) (at: i64 -> i64) (j: i64) : i64 =
  let (lo, _) = loop (lo, hi) = (0, n) while hi - lo > 1 do
                  let m = (lo + hi) / 2 in if at m <= j then (m, hi) else (lo, m)
  in lo

-- copy, shaped per byte: every data byte finds its symbol by binary search in the offsets
def copy_bytes [L] (nimg: i64) (image_base: i64) (addr: [L]u32) (bucket: [L]u8) (doff: []u64) (dbin: []u8) =
  let dst j = let s = row_of L (\s -> i64.u64 doff[s]) j
              in if is_bss bucket[s] then -1 else i64.u32 addr[s] - image_base + j - i64.u64 doff[s]
  in scatter (replicate nimg 0u8) (map dst (indices dbin)) dbin

def sect_base (sect: [8]u32) (t: u32) : u32 =
  loop b = sect[0] for k in 1...7 do if t >= sect[k] then sect[k] else b

def patch [L][R] (img: *[]u8) (image_base: i64) (addr: [L]u32) (bucket: [L]u8) (rstart: []u32)
                 (roff: [R]u32) (rsiz: [R]u8) (rkind: [R]u8) (radd: [R]i64) (rtgt: [R]u32)
                 (addr_all: []u32) (sect: [8]u32) (unreach: u32) (mapinit: u32) =
  let owner = map (row_of L (\s -> i64.u32 rstart[s])) (iota R)
  -- (value, bytes to write, error) of one relocation
  let value s r : (i64, i64, bool) =
    let a = i64.u32 addr[s]
    let tg = rtgt[r]
    let ta = addr_all[i64.u32 tg]
    let add = radd[r]
    let sz = i64.u8 rsiz[r]
    let pc = a + i64.u32 roff[r] + sz
    in match rkind[r]
       case 1 -> if ta == 0 && tg != 0 then (0, 0, true) else (i64.u32 ta + add, sz, false)
       case 2 -> let ta = if ta == 0 && tg != 0 then unreach else ta in (i64.u32 ta + add, sz, false)
       case 3 -> if ta == 0 then (0, 0, false) else (i64.u32 (ta - sect_base sect ta) + add, sz, false)
       case 4 -> if ta == 0 then (0xFFFFFFFF, 4, false) else (i64.u32 (ta - sect_base sect ta) + add, sz, false)
       case 5 -> if ta == 0 && tg != 0 then (0, 0, true) else (i64.u32 ta + add - pc, sz, false)
       case 7 -> if ta == 0 && tg != 0 then (0, 0, true) else (i64.u32 ta + add - pc, sz, false)
       case 6 -> let ta = if ta == 0 && tg != 0 then mapinit else ta in (i64.u32 ta + add - pc, sz, false)
       case 8 -> (add, sz, false)
       case _ -> (0, 0, true)
  let vals = map2 (\s r -> if is_bss bucket[s] then (0, 0, false) else value s r) owner (iota R)
  let writes = map3 (\s r (v, n, _) ->
                       let p = i64.u32 addr[s] + i64.u32 roff[r] - image_base
                       in #[sequential] map (\j -> if j < n then (p + j, u8.i64 (v >> (8 * j))) else (-1, 0u8)) (iota 8))
                    owner (iota R) vals
  let (is, vs) = unzip (flatten writes)
  let nerr = map (\(_, _, e) -> i64.bool e) vals |> reduce (+) 0
  in (scatter img is vs, nerr)

entry relocate [L][R] (nimg: i64) (image_base: i64) (addr: [L]u32) (bucket: [L]u8) (doff: []u64) (dbin: []u8)
                      (rstart: []u32) (roff: [R]u32) (rsiz: [R]u8) (rkind: [R]u8) (radd: [R]i64) (rtgt: [R]u32)
                      (addr_all: []u32) (sect: [8]u32) (unreach: u32) (mapinit: u32) : ([]u8, i64) =
  let img = copy_chunks nimg image_base addr bucket doff dbin
  in patch img image_base addr bucket rstart roff rsiz rkind radd rtgt addr_all sect unreach mapinit

entry relocate_bytes [L][R] (nimg: i64) (image_base: i64) (addr: [L]u32) (bucket: [L]u8) (doff: []u64) (dbin: []u8)
                      (rstart: []u32) (roff: [R]u32) (rsiz: [R]u8) (rkind: [R]u8) (radd: [R]i64) (rtgt: [R]u32)
                      (addr_all: []u32) (sect: [8]u32) (unreach: u32) (mapinit: u32) : ([]u8, i64) =
  let img = copy_bytes nimg image_base addr bucket doff dbin
  in patch img image_base addr bucket rstart roff rsiz rkind radd rtgt addr_all sect unreach mapinit

-- the pieces of 3b, to time them apart (same signature as relocate)
entry copy_only [L][R] (nimg: i64) (image_base: i64) (addr: [L]u32) (bucket: [L]u8) (doff: []u64) (dbin: []u8)
                      (_rstart: []u32) (_roff: [R]u32) (_rsiz: [R]u8) (_rkind: [R]u8) (_radd: [R]i64) (_rtgt: [R]u32)
                      (_addr_all: []u32) (_sect: [8]u32) (_unreach: u32) (_mapinit: u32) : ([]u8, i64) =
  (copy_chunks nimg image_base addr bucket doff dbin, 0)
entry patch_only [L][R] (nimg: i64) (image_base: i64) (addr: [L]u32) (bucket: [L]u8) (_doff: []u64) (_dbin: []u8)
                      (rstart: []u32) (roff: [R]u32) (rsiz: [R]u8) (rkind: [R]u8) (radd: [R]i64) (rtgt: [R]u32)
                      (addr_all: []u32) (sect: [8]u32) (unreach: u32) (mapinit: u32) : ([]u8, i64) =
  patch (replicate nimg 0) image_base addr bucket rstart roff rsiz rkind radd rtgt addr_all sect unreach mapinit
