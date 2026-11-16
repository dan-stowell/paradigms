-- p2.fut: problem 2, reach (ref/flat/README.md): a level-synchronous flood. Each level takes the frontier
-- (ids that just became live, or live gotypes that just became iface), expands their edge rows into one flat
-- edge list (a segmented gather), and applies the rules to it with scatters. All rules are monotone, so the
-- order the flood visits things in does not change the fixpoint; methods are checked once per level.
import "lib/segmented/segmented"
import "msort"

def f_def = 1u8
def f_reflect = 4u8
def f_gotype = 8u8
def f_iface = 16u8
def f_follow = 32u8

-- per method: the representative signature with the same (name, type) (the first in key order), or -1;
-- per signature: its representative. A sort of the 3,940 signature keys and a binary search per method.
def sig_reps [S] [M] (sig_name: [S]u32) (sig_type: [S]u32) (m_name: [M]u32) (m_mtyp: [M]u32) : ([S]i64, [M]i64) =
  let key n t = u64.u32 n << 32 | u64.u32 t
  let sorted = merge_sort_cpu (\(a, _) (b, _) -> a <= b) (zip (map2 key sig_name sig_type) (iota S))
  let (skey, sidx) = unzip sorted
  let firstpos = map (\i -> if i == 0 || skey[i - 1] != skey[i] then i else 0) (iota S) |> scan i64.max 0
  let rep = scatter (replicate S 0) sidx (map (\p -> sidx[p]) firstpos)
  let find k = let (lo, _) = loop (lo, hi) = (0, S) while hi - lo > 1 do
                               let m = (lo + hi) / 2 in if skey[m] <= k then (m, hi) else (lo, m)
               in if S > 0 && skey[lo] == k then sidx[firstpos[lo]] else -1
  in (rep, map2 (\n t -> find (key n t)) m_name m_mtyp)

-- keep each id once (the copy at the position that wins the scatter); `owner` is scratch, any contents
def dedup [N] (owner: *[N]i32) (ts: []i64) : ([]i64, *[N]i32) =
  let owner = scatter owner ts (map i32.i64 (indices ts))
  let keep = filter (\(t, j) -> owner[t] == i32.i64 j) (zip ts (indices ts)) |> map (.0)
  in (keep, owner)

-- The flood. `edges k (\i -> front[i])` gives (a frontier passed as a count and a getter, because a
-- function argument cannot be size-polymorphic in Futhark) the (src, dst, kind) of every edge row of the frontier's sources.
def flood [L] [M] [S] (N: i64) (nsl: i64) (flags: [L]u8) (roots: []u32)
                          (edges: i64 -> (i64 -> i64) -> [](i64, i64, u8))
                          (moff: []u32) (m_mtyp: [M]u32) (m_ifn: [M]u32) (m_tfn: [M]u32) (m_exp: [M]u8)
                          (m_name: [M]u32) (m_sig: [M]i64) (sig_rep: [S]i64) (nstr: i64) : ([]bool, i64) =
  let fl c = if c <= nsl then flags[c] else 0
  let live = replicate N false
  let iface = map (\c -> fl c & f_iface != 0) (iota N)
  let (front, owner) = dedup (replicate N 0) (map i64.u32 roots |> filter (> 0))
  let live = scatter live front (map (const true) front)
  let (live, _, _, _, _, _, _, _, levels) =
    loop (live, iface, owner, front, sig_seen, named, reflect, pending, levels) =
         (live, iface, owner, front, replicate S false, replicate (nstr + 1) false, false, replicate 0 0i64, 0i64)
    while length front > 0 do
      let proc = filter (\c -> fl c & f_def != 0) front
      let reflect = reflect || any (\c -> fl c & f_reflect != 0) proc
      let uii c = fl c & f_gotype != 0 && iface[c]
      let (src, dst, kind) = unzip3 (edges (length proc) (\i -> proc[i]))
      -- what this level learns
      let gotdef t = fl t & f_gotype != 0 && fl t & f_def != 0
      let ms = expand (\c -> if uii c then i64.u32 (moff[c + 1] - moff[c]) else 0) (\c j -> i64.u32 moff[c] + j) proc
      let sig_seen = scatter sig_seen (map2 (\t k -> if k == 2 then sig_rep[t] else -1) dst kind) (map (const true) dst)
      let named = scatter named (map2 (\t k -> if k == 3 then t else -1) dst kind) (map (const true) dst)
      -- the candidate methods that have not fired yet (each type's methods become candidates once)
      let call m = (reflect && m_exp[m] != 0) || named[i64.u32 m_name[m]] || (m_sig[m] >= 0 && sig_seen[m_sig[m]])
      let cands = pending ++ ms
      let fires = map call cands
      let fm = zip cands fires |> filter (.1) |> map (.0)
      let pending = zip cands fires |> filter (\(_, f) -> !f) |> map (.0)
      let to_iface = map3 (\s t k -> if t > 0 && (k == 1 || (k == 0 && uii s && gotdef t)) then t else -1) src dst kind
                     ++ map (\m -> i64.u32 m_mtyp[m]) ms
                     |> filter (> 0)
      let to_live = map2 (\t k -> if k == 0 then t else -1) dst kind
                    ++ flatten (map (\m -> [i64.u32 m_mtyp[m], i64.u32 m_ifn[m], i64.u32 m_tfn[m]]) fm)
                    |> filter (> 0)
      let touched = to_live ++ to_iface
      let was = map (\t -> (live[t], iface[t])) touched
      let live = scatter live to_live (map (const true) to_live)
      let iface = scatter iface to_iface (map (const true) to_iface)
      -- the next frontier: newly live, or live and gotype and newly iface
      let new = map2 (\t (l, i) -> if (live[t] && !l) || (live[t] && !i && iface[t] && fl t & f_gotype != 0) then t else -1)
                     touched was |> filter (>= 0)
      let (front, owner) = dedup owner new
      in (live, iface, owner, front, sig_seen, named, reflect, pending, levels + 1)
  in (live, levels)

def live_list [N] (live: [N]bool) : []u32 = filter (\g -> live[g]) (iota N) |> map u32.i64

-- ---- Eager: the prebuilt edge list (p2_edge_*).
entry reach [L] [E] [M] [S] (nids: i64) (flags: [L]u8) (eoff: []u32) (edst: [E]u32) (ekind: [E]u8)
                            (roots: []u32) (sig_name: [S]u32) (sig_type: [S]u32)
                            (moff: []u32) (m_name: [M]u32) (m_mtyp: [M]u32) (m_ifn: [M]u32) (m_tfn: [M]u32)
                            (m_exp: [M]u8) (nstr: i64) : []u32 =
  let nsl = L - 1
  let (sig_rep, m_sig) = sig_reps sig_name sig_type m_name m_mtyp
  let edges (k: i64) (get: i64 -> i64) =
    let proc = map get (iota k)
    let es = expand (\c -> i64.u32 (eoff[c + 1] - eoff[c])) (\c j -> (c, i64.u32 eoff[c] + j)) proc
    in map (\(c, e) -> (c, i64.u32 edst[e], ekind[e])) es
  in (flood nids nsl flags roots edges moff m_mtyp m_ifn m_tfn m_exp m_name m_sig sig_rep nstr).0 |> live_list

-- the eager flood's level count (for ATTACKS.md 3)
entry reach_levels [L] [E] [M] [S] (nids: i64) (flags: [L]u8) (eoff: []u32) (edst: [E]u32) (ekind: [E]u8)
                            (roots: []u32) (sig_name: [S]u32) (sig_type: [S]u32)
                            (moff: []u32) (m_name: [M]u32) (m_mtyp: [M]u32) (m_ifn: [M]u32) (m_tfn: [M]u32)
                            (m_exp: [M]u8) (nstr: i64) : i64 =
  let nsl = L - 1
  let (sig_rep, m_sig) = sig_reps sig_name sig_type m_name m_mtyp
  let edges (k: i64) (get: i64 -> i64) =
    let proc = map get (iota k)
    let es = expand (\c -> i64.u32 (eoff[c + 1] - eoff[c])) (\c j -> (c, i64.u32 eoff[c] + j)) proc
    in map (\(c, e) -> (c, i64.u32 edst[e], ekind[e])) es
  in (flood nids nsl flags roots edges moff m_mtyp m_ifn m_tfn m_exp m_name m_sig sig_rep nstr).1

-- ---- Lazy: a live source's relocation edges decoded from the relocation columns when the flood reaches it.
-- Which relocations the flood follows: not weak, a plain type, and not one of the two after an R_USETYPE-like
-- type 26 (which skips the next two relocations, unless it is itself skipped). The skip is a three-state
-- machine run along each row: each relocation is a function of the state (how many still to skip), and
-- composing them is a segmented scan.
type st3 = (u8, u8, u8)       -- the images of states 0, 1, 2
def st_then ((a0, a1, a2): st3) (g: st3) : st3 =
  let ap (b0, b1, b2) s = if s == 0 then b0 else if s == 1 then b1 else b2
  in (ap g a0, ap g a1, ap g a2)
def st_of (ty: u16) : st3 = (if ty == 26 then 2 else 0, 0, 1)
def plain (ty: u16) = ty & 0x8000 == 0 && ty != 22 && ty != 23 && ty != 24 && ty != 25 && ty != 26 && ty != 102

-- per relocation of a run of rows (row starts flagged): followed?
def followed [n] (starts: [n]bool) (ty: [n]u16) : [n]bool =
  let after = segmented_scan st_then (0, 1, 2) starts (map st_of ty) |> map (.0)
  in map3 (\i s t -> let before = if s then 0 else after[i - 1] in before == 0 && plain t) (iota n) starts ty

-- the same, for one relocation r of the row starting at s0, by looking back: walk back to a position no
-- type 26 can reach (state 0 there), then run the machine forward to r. Rows have few 26s, so this is a
-- step or two, and no scan over the expanded edges.
def followed_at [n] (ty: [n]u16) (s0: i64) (r: i64) : bool =
  let q = loop q = r while q > s0 && (ty[q - 1] == 26 || (q - 2 >= s0 && ty[q - 2] == 26)) do q - 1
  let st = loop st = 0u8 for p in q..<r do if st > 0 then st - 1 else if ty[p] == 26 then 2 else 0
  in st == 0 && plain ty[r]

def pkg_self = 2147483643u32
def decode (canon: []u32) (builtin: []u32) (pkg_off: []u32) (pkg: []u32) (o: i64) (p: u32) (idx: u32) : u32 =
  let base0 = i64.u32 pkg_off[o]
  let npk = pkg_off[o + 1] - pkg_off[o] - 5
  let sp = p - pkg_self
  let t = if sp < 5 then npk + sp else p
  let s = if t < npk + 5 && pkg[base0 + i64.u32 t] != 0xFFFFFFFF
          then (if sp == 1 then (if i64.u32 idx < length builtin then builtin[i64.u32 idx] else 0)
                else pkg[base0 + i64.u32 t] + idx)
          else 0
  in if s != 0 then canon[i64.u32 s] else 0

-- Setup (untimed, as ref/flat/p2lazy.c's): the p2_edge rows without their relocation edges (the first
-- `nplain` kind-0 entries of each row, nplain = the followed relocations with a target), i.e. funcdata
-- and kinds 1-3, as a CSR (xoff, xdst, xkind) returned concatenated: xoff ++ xdst, and xkind.
entry aux_edges [L] [L1] [R] [E] (flags: [L]u8) (canon: []u32) (slot_rel: [L1]u32) (rel_type: [R]u16) (rtgt: [R]u32)
                            (eoff: [L1]u32) (edst: [E]u32) (ekind: [E]u8) : ([]u32, []u8) =
  let starts = scatter (replicate R false) (map i64.u32 slot_rel) (replicate L1 true)
  let fol = map2 (\f t -> f && t != 0) (followed starts rel_type) rtgt
  let owner_r = scatter (replicate R 0i64) (map2 (\g s -> if g < L && slot_rel[g] < slot_rel[g + 1] then i64.u32 s else -1) (indices slot_rel) slot_rel) (indices slot_rel) |> scan i64.max 0
  let nplain = hist (+) 0 L owner_r (map i64.bool fol)
  let take g = g < L && flags[g] & f_def != 0 && flags[g] & f_follow != 0 && canon[g] == u32.i64 g
  let owner_e = scatter (replicate E 0i64) (map2 (\g s -> if g < L && eoff[g] < eoff[g + 1] then i64.u32 s else -1) (indices eoff) eoff) (indices eoff) |> scan i64.max 0
  let estart = map (\e -> e == 0 || owner_e[e - 1] != owner_e[e]) (iota E)
  let k0rank = segmented_scan (+) 0 estart (map (\k -> i64.bool (k == 0)) ekind)    -- inclusive
  let keep = map3 (\e g k -> take g && !(k == 0 && k0rank[e] <= nplain[g])) (iota E) owner_e ekind
  let cnt = hist (+) 0 (L + 1) owner_e (map i64.bool keep)
  let xoff = scan (+) 0 cnt |> map2 (\c s -> u32.i64 (s - c)) cnt
  let xoff = xoff ++ [u32.i64 (reduce (+) 0 cnt)]
  let kept = zip edst ekind |> zip keep |> filter (.0) |> map (.1)
  in (xoff ++ map (.0) kept, map (.1) kept)

entry reach_lazy [L] [R] [X] [M] [S] (nids: i64) (flags: [L]u8) (canon: []u32) (builtin: []u32)
                 (slot_rel: []u32) (slot_obj: []u32) (pkg_off: []u32) (pkg: []u32)
                 (rel_pkg: [R]u32) (rel_idx: [R]u32) (rel_type: [R]u16)
                 (xoffdst: []u32) (xkind: [X]u8)
                 (roots: []u32) (sig_name: [S]u32) (sig_type: [S]u32)
                 (moff: []u32) (m_name: [M]u32) (m_mtyp: [M]u32) (m_ifn: [M]u32) (m_tfn: [M]u32)
                 (m_exp: [M]u8) (nstr: i64) (look: bool) : []u32 =
  let nsl = L - 1
  let xoff = take (L + 2) xoffdst
  let xdst = drop (L + 2) xoffdst :> [X]u32
  let (sig_rep, m_sig) = sig_reps sig_name sig_type m_name m_mtyp
  let edges (k: i64) (get: i64 -> i64) =
    let proc = map get (iota k)
    -- the aux rows, as stored
    let xs = expand (\c -> i64.u32 (xoff[c + 1] - xoff[c])) (\c j -> (c, i64.u32 xoff[c] + j)) proc
    -- the relocation rows, decoded now
    let decodes c = flags[c] & f_follow != 0 && canon[c] == u32.i64 c
    let rs = expand (\c -> if decodes c then i64.u32 (slot_rel[c + 1] - slot_rel[c]) else 0)
                    (\c j -> (c, i64.u32 slot_rel[c] + j)) proc
    -- which relocations to follow: by looking back along the row (look), or by a segmented scan of the skip machine
    let fol = if look then map (\(c, r) -> followed_at rel_type (i64.u32 slot_rel[c]) r) rs
              else followed (map (\(c, r) -> r == i64.u32 slot_rel[c]) rs) (map (\(_, r) -> rel_type[r]) rs)
    let rt = map2 (\(c, r) f -> if f then decode canon builtin pkg_off pkg (i64.u32 slot_obj[c]) rel_pkg[r] rel_idx[r] else 0) rs fol
    let rkeep = zip rs rt |> filter (\(_, t) -> t != 0)
    in map (\(c, e) -> (c, i64.u32 xdst[e], xkind[e])) xs ++ map (\((c, _), t) -> (c, i64.u32 t, 0u8)) rkeep
  in (flood nids nsl flags roots edges moff m_mtyp m_ifn m_tfn m_exp m_name m_sig sig_rep nstr).0 |> live_list

-- ---- Hybrid (ATTACKS.md 3): data-parallel levels while the frontier is wide, a sequential worklist drain while
-- it is narrow. `ex[c]` records how far c has been expanded (0 no, 1 as plain, 2 as a live iface gotype), so the
-- two modes hand work back and forth through one frontier: a node is due when need(c) > ex[c]. The drain is
-- an ordinary `loop` outside any map: Futhark compiles it to sequential host code (scalar reads and in-place
-- writes, no parallel operation). It runs until the stack passes `hi` or `budget` pops are done, then gives
-- its stack back as the frontier. Pending methods live in a buffer of M (each type is expanded as an iface
-- gotype at most once, so each method enters it at most once).
-- The stack has a fixed size, so that it is not an existential array: a growable one (`++` when full) costs
-- several reference-count updates of its memory block per push in the generated C, which made the drain 3x
-- slower. The size bounds every push: the drain stops before a pop once sp > hi, and one pop pushes at most
-- its row and its methods (maxpush); a method check pushes at most 3 per pending method.
def push [K] (stack: *[K]i64) (sp: i64) (t: i64) : (*[K]i64, i64) = (stack with [sp] = t, sp + 1)

def flood_hybrid [L] [M] [S] [Eo] [E] (N: i64) (nsl: i64) (flags: [L]u8) (roots: []u32)
                 (eoff: [Eo]u32) (edst: [E]u32) (ekind: [E]u8)
                 (moff: []u32) (m_mtyp: [M]u32) (m_ifn: [M]u32) (m_tfn: [M]u32) (m_exp: [M]u8)
                 (m_name: [M]u32) (m_sig: [M]i64) (sig_rep: [S]i64) (nstr: i64)
                 (lo: i64) (hi: i64) (budget: i64) : ([N]bool, [6]i64) =
  let fl c = if c <= nsl then flags[c] else 0
  let isdef c = fl c & f_def != 0
  let gotype c = fl c & f_gotype != 0
  let gotdef t = gotype t && isdef t
  let call reflect (named: []bool) (sig_seen: []bool) m =
    (reflect && m_exp[m] != 0) || named[i64.u32 m_name[m]] || (m_sig[m] >= 0 && sig_seen[m_sig[m]])
  let maxpush = reduce i64.max 0 (map2 (-) (tail eoff) (init eoff) |> map i64.u32)
                + reduce i64.max 0 (map2 (-) (tail moff) (init moff) |> map i64.u32)
  let live = replicate N false
  let iface = map (\c -> fl c & f_iface != 0) (iota N)
  let ex = replicate N 0u8
  let (front, owner) = dedup (replicate N 0) (map i64.u32 roots |> filter (> 0))
  let live = scatter live front (map (const true) front)
  let (live, _, _, _, _, _, _, _, _, _, st) =
    loop (live, iface, ex, owner, front, sig_seen, named, reflect, pbuf, np, st) =
         (live, iface, ex, owner, front, replicate S false, replicate (nstr + 1) false, false, replicate M 0i64, 0i64,
          [0i64, 0, 0, 0, 0, 0])
    while length front > 0 do
      if length front >= lo then
        -- one data-parallel level (as `flood`, plus the ex bookkeeping)
        let need c = if gotype c && iface[c] then 2u8 else 1u8
        let fr = filter (\c -> need c > ex[c]) front
        let ex = scatter ex fr (map need fr)
        let proc = filter isdef fr
        let reflect = reflect || any (\c -> fl c & f_reflect != 0) proc
        let uii c = gotype c && iface[c]
        let es = expand (\c -> i64.u32 (eoff[c + 1] - eoff[c])) (\c j -> (c, i64.u32 eoff[c] + j)) proc
        let (src, dst, kind) = unzip3 (map (\(c, e) -> (c, i64.u32 edst[e], ekind[e])) es)
        let ms = expand (\c -> if uii c then i64.u32 (moff[c + 1] - moff[c]) else 0) (\c j -> i64.u32 moff[c] + j) proc
        let sig_seen = scatter sig_seen (map2 (\t k -> if k == 2 then sig_rep[t] else -1) dst kind) (map (const true) dst)
        let named = scatter named (map2 (\t k -> if k == 3 then t else -1) dst kind) (map (const true) dst)
        let cands = take np pbuf ++ ms
        let fires = map (call reflect named sig_seen) cands
        let fm = zip cands fires |> filter (.1) |> map (.0)
        let pending = zip cands fires |> filter (\(_, f) -> !f) |> map (.0)
        let pbuf = scatter pbuf (indices pending) pending
        let to_iface = map3 (\s t k -> if t > 0 && (k == 1 || (k == 0 && uii s && gotdef t)) then t else -1) src dst kind
                       ++ map (\m -> i64.u32 m_mtyp[m]) ms
                       |> filter (> 0)
        let to_live = map2 (\t k -> if k == 0 then t else -1) dst kind
                      ++ flatten (map (\m -> [i64.u32 m_mtyp[m], i64.u32 m_ifn[m], i64.u32 m_tfn[m]]) fm)
                      |> filter (> 0)
        let live = scatter live to_live (map (const true) to_live)
        let iface = scatter iface to_iface (map (const true) to_iface)
        let need c = if gotype c && iface[c] then 2u8 else 1u8
        let new = filter (\t -> live[t] && need t > ex[t]) (to_live ++ to_iface)
        let (front, owner) = dedup owner new
        in (live, iface, ex, owner, front, sig_seen, named, reflect, pbuf, length pending,
            st with [0] = st[0] + 1 with [2] = st[2] + length proc)
      else
        -- a sequential worklist drain, seeded with the frontier
        let K = length front + i64.min (hi + maxpush + 3 * M) (2 * N)
        let mark_live (live: *[N]bool) (stack: *[K]i64) (sp: i64) (t: i64) : (*[N]bool, *[K]i64, i64) =
          if t > 0 && !live[t] then let (stack, sp) = push stack sp t in (live with [t] = true, stack, sp)
          else (live, stack, sp)
        let mark_iface (live: [N]bool) (iface: *[N]bool) (stack: *[K]i64) (sp: i64) (t: i64) : (*[N]bool, *[K]i64, i64) =
          if t > 0 && !iface[t] then
            let iface = iface with [t] = true
            in if live[t] && gotype t then let (stack, sp) = push stack sp t in (iface, stack, sp) else (iface, stack, sp)
          else (iface, stack, sp)
        let stack0 = (copy front ++ replicate (K - length front) 0) :> [K]i64
        let (live, iface, ex, stack, sp, sig_seen, named, reflect, pbuf, np, pops, _) =
          loop (live, iface, ex, stack, sp, sig_seen, named, reflect, pbuf, np, pops, go) =
               (live, iface, ex, stack0, length front, sig_seen, named, reflect, pbuf, np, 0i64, true)
          while go do
            if sp == 0 then
              -- the stack is empty: the methods that can now be called
              let (live, stack, sp, pbuf, keep) =
                loop (live, stack, sp, pbuf, keep) = (live, stack, sp, pbuf, 0i64) for i < np do
                  let m = pbuf[i]
                  in if call reflect named sig_seen m then
                       let (live, stack, sp) = mark_live live stack sp (i64.u32 m_mtyp[m])
                       let (live, stack, sp) = mark_live live stack sp (i64.u32 m_ifn[m])
                       let (live, stack, sp) = mark_live live stack sp (i64.u32 m_tfn[m])
                       in (live, stack, sp, pbuf, keep)
                     else (live, stack, sp, pbuf with [keep] = m, keep + 1)
              in (live, iface, ex, stack, sp, sig_seen, named, reflect, pbuf, keep, pops, sp > 0)
            else if sp > hi || pops >= budget then
              (live, iface, ex, stack, sp, sig_seen, named, reflect, pbuf, np, pops, false)
            else
              let c = stack[sp - 1]
              let sp = sp - 1
              let nd = if gotype c && iface[c] then 2u8 else 1u8
              in if nd <= ex[c] then (live, iface, ex, stack, sp, sig_seen, named, reflect, pbuf, np, pops, true)
              else
              let ex = ex with [c] = nd
              in if !(isdef c) then (live, iface, ex, stack, sp, sig_seen, named, reflect, pbuf, np, pops, true)
              else
              let reflect = reflect || fl c & f_reflect != 0
              let uii = nd == 2
              let (live, iface, stack, sp, sig_seen, named) =
                loop (live, iface, stack, sp, sig_seen, named) for e in i64.u32 eoff[c]..<i64.u32 eoff[c + 1] do
                  let t = i64.u32 edst[e]
                  let k = ekind[e]
                  in if k == 0 then
                       let (live, stack, sp) = mark_live live stack sp t
                       let (iface, stack, sp) = if uii && gotdef t then mark_iface live iface stack sp t else (iface, stack, sp)
                       in (live, iface, stack, sp, sig_seen, named)
                     else if k == 1 then
                       let (iface, stack, sp) = mark_iface live iface stack sp t
                       in (live, iface, stack, sp, sig_seen, named)
                     else if k == 2 then (live, iface, stack, sp, sig_seen with [sig_rep[t]] = true, named)
                     else (live, iface, stack, sp, sig_seen, named with [t] = true)
              let (iface, stack, sp, pbuf, np) =
                if !uii then (iface, stack, sp, pbuf, np)
                else loop (iface, stack, sp, pbuf, np) for m in i64.u32 moff[c]..<i64.u32 moff[c + 1] do
                       let (iface, stack, sp) = mark_iface live iface stack sp (i64.u32 m_mtyp[m])
                       in (iface, stack, sp, pbuf with [np] = m, np + 1)
              in (live, iface, ex, stack, sp, sig_seen, named, reflect, pbuf, np, pops + 1, true)
        let front = take sp stack
        in (live, iface, ex, owner, front, sig_seen, named, reflect, pbuf, np,
            st with [1] = st[1] + 1 with [3] = st[3] + pops)
  in (live, st)

def hybrid_args [L] [E] [M] [S] (nids: i64) (flags: [L]u8) (eoff: []u32) (edst: [E]u32) (ekind: [E]u8)
                (roots: []u32) (sig_name: [S]u32) (sig_type: [S]u32)
                (moff: []u32) (m_name: [M]u32) (m_mtyp: [M]u32) (m_ifn: [M]u32) (m_tfn: [M]u32)
                (m_exp: [M]u8) (nstr: i64) (lo: i64) (hi: i64) (budget: i64) =
  let (sig_rep, m_sig) = sig_reps sig_name sig_type m_name m_mtyp
  in flood_hybrid nids (L - 1) flags roots eoff edst ekind moff m_mtyp m_ifn m_tfn m_exp m_name m_sig sig_rep nstr lo hi budget

-- lo: frontier width below which a level is drained sequentially (0: all levels data-parallel; huge: one
-- sequential worklist); hi, budget: the drain hands its stack back once it holds more than hi, or after budget pops
entry reach_hybrid [L] [E] [M] [S] (nids: i64) (flags: [L]u8) (eoff: []u32) (edst: [E]u32) (ekind: [E]u8)
                (roots: []u32) (sig_name: [S]u32) (sig_type: [S]u32)
                (moff: []u32) (m_name: [M]u32) (m_mtyp: [M]u32) (m_ifn: [M]u32) (m_tfn: [M]u32)
                (m_exp: [M]u8) (nstr: i64) (lo: i64) (hi: i64) (budget: i64) : []u32 =
  (hybrid_args nids flags eoff edst ekind roots sig_name sig_type moff m_name m_mtyp m_ifn m_tfn m_exp nstr lo hi budget).0
  |> live_list

-- the counts: [data-parallel levels, sequential drains, nodes expanded in parallel, pops in drains, 0, 0]
entry hybrid_stats [L] [E] [M] [S] (nids: i64) (flags: [L]u8) (eoff: []u32) (edst: [E]u32) (ekind: [E]u8)
                (roots: []u32) (sig_name: [S]u32) (sig_type: [S]u32)
                (moff: []u32) (m_name: [M]u32) (m_mtyp: [M]u32) (m_ifn: [M]u32) (m_tfn: [M]u32)
                (m_exp: [M]u8) (nstr: i64) (lo: i64) (hi: i64) (budget: i64) : [6]i64 =
  (hybrid_args nids flags eoff edst ekind roots sig_name sig_type moff m_name m_mtyp m_ifn m_tfn m_exp nstr lo hi budget).1

-- ---- Batched levels (ATTACKS.md 3, second attempt): a loop inside the map. Each round cuts the frontier into
-- up to C chunks; each chunk runs a sequential worklist from its seeds (a map over chunks, a `loop` inside),
-- following plain edges into symbols that were not live at the round's start, for at most B pops and CAP
-- edges. It writes the edges it walked (dst, kind | 4 if the source was a live iface gotype) and the symbols it
-- expanded into its own buffers; the round then applies the rules to all of them at once with the same
-- scatters as a level. A small open-addressing table (H slots, 8 probes) keeps a chunk from pushing a symbol
-- twice; when it is full the chunk just does not push (the edge is still applied, so the symbol is due in the
-- next round): exploring ahead is optional, which is what makes it safe. Chunks may expand the same symbol
-- (both found it); the round dedups the expanded list, and the rules are idempotent.
def flood_batch [L] [M] [S] [Eo] [E] (N: i64) (nsl: i64) (flags: [L]u8) (roots: []u32)
                (eoff: [Eo]u32) (edst: [E]u32) (ekind: [E]u8)
                (moff: []u32) (m_mtyp: [M]u32) (m_ifn: [M]u32) (m_tfn: [M]u32) (m_exp: [M]u8)
                (m_name: [M]u32) (m_sig: [M]i64) (sig_rep: [S]i64) (nstr: i64)
                (C: i64) (B: i64) (capmin: i64) (H: i64) : ([N]bool, [6]i64) =
  let fl c = if c <= nsl then flags[c] else 0
  let isdef c = fl c & f_def != 0
  let gotype c = fl c & f_gotype != 0
  let gotdef t = gotype t && isdef t
  let deg c = if isdef c then i64.u32 (eoff[c + 1] - eoff[c]) else 0
  let call reflect (named: []bool) (sig_seen: []bool) m =
    (reflect && m_exp[m] != 0) || named[i64.u32 m_name[m]] || (m_sig[m] >= 0 && sig_seen[m_sig[m]])
  let live = replicate N false
  let iface = map (\c -> fl c & f_iface != 0) (iota N)
  let ex = replicate N 0u8
  let (front, owner) = dedup (replicate N 0) (map i64.u32 roots |> filter (> 0))
  let live = scatter live front (map (const true) front)
  let (live, _, _, _, _, _, _, _, _, _, st) =
    loop (live, iface, ex, owner, front, sig_seen, named, reflect, pbuf, np, st) =
         (live, iface, ex, owner, front, replicate S false, replicate (nstr + 1) false, false, replicate M 0i64, 0i64,
          [0i64, 0, 0, 0, 0, 0])
    while length front > 0 do
      let need c = if gotype c && iface[c] then 2u8 else 1u8
      let uii c = gotype c && iface[c]
      let fr = filter (\c -> need c > ex[c]) front
      let nf = length fr
      let nc = i64.max 1 (i64.min C nf)
      let per = (nf + nc - 1) / nc
      let CAP = i64.max capmin (reduce i64.max 0 (map deg fr))
      let SCAP = per + H
      let chunk (ci: i64) =
        let s0 = i64.min nf (ci * per)
        let s1 = i64.min nf (s0 + per)
        let stack = replicate SCAP 0i32
        let stack = loop stack for i < s1 - s0 do stack with [i] = i32.i64 fr[s1 - 1 - i]
        let (_, dst, kd, nb, exl, ne, stack, sp, _) =
          loop (seen, dst, kd, nb, exl, ne, stack, sp, go) =
               (replicate H (-1i32), replicate CAP 0i32, replicate CAP 0u8, 0i64, replicate B 0i32, 0i64,
                stack, s1 - s0, true)
          while go && sp > 0 && ne < B do
            let c = i64.i32 stack[sp - 1]
            let d = deg c
            in if nb + d > CAP then (seen, dst, kd, nb, exl, ne, stack, sp, false)
            else
              let sp = sp - 1
              let exl = exl with [ne] = i32.i64 c
              let u = if uii c then 4u8 else 0u8
              let r0 = i64.u32 eoff[c]
              let (seen, dst, kd, stack, sp) =
                loop (seen, dst, kd, stack, sp) for j < d do
                  let t = i64.u32 edst[r0 + j]
                  let k = ekind[r0 + j]
                  let dst = dst with [nb + j] = i32.i64 t
                  let kd = kd with [nb + j] = k | (if k == 0 then u else 0)
                  in if k == 0 && t > 0 && !live[t] then
                       -- push t unless this chunk already did (bounded probing; full: leave it to the next round)
                       let h0 = (t * 0x9E3779B1) & (H - 1)
                       let (slot, _) = loop (slot, p) = (-1i64, 0i64) while slot == -1 && p < 8 do
                                         let q = (h0 + p) & (H - 1)
                                         in if seen[q] == i32.i64 t then (-2, p + 1)
                                            else if seen[q] < 0 then (q, p + 1) else (-1, p + 1)
                       in if slot >= 0 then (seen with [slot] = i32.i64 t, dst, kd, stack with [sp] = i32.i64 t, sp + 1)
                          else (seen, dst, kd, stack, sp)
                     else (seen, dst, kd, stack, sp)
              in (seen, dst, kd, nb + d, exl, ne + 1, stack, sp, true)
        in (dst, kd, nb, exl, ne, stack, sp)
      let res = map chunk (iota nc)
      let (dsts, kds, nbs) = unzip3 (map (\(a, b, c, _, _, _, _) -> (a, b, c)) res)
      let (exls, nes, stks, sps) = unzip4 (map (\(_, _, _, d, e, f, g) -> (d, e, f, g)) res)
      let ed = expand (\ci -> nbs[ci]) (\ci j -> (i64.i32 dsts[ci, j], kds[ci, j])) (iota nc)
      let (dst, kind) = unzip ed
      let (proc_all, owner) = dedup owner (expand (\ci -> nes[ci]) (\ci j -> i64.i32 exls[ci, j]) (iota nc))
      let left = expand (\ci -> sps[ci]) (\ci j -> i64.i32 stks[ci, j]) (iota nc)
      -- the rules, as in a level, over everything the chunks walked
      let ex = scatter ex proc_all (map need proc_all)
      let proc = filter isdef proc_all
      let reflect = reflect || any (\c -> fl c & f_reflect != 0) proc
      let ms = expand (\c -> if uii c then i64.u32 (moff[c + 1] - moff[c]) else 0) (\c j -> i64.u32 moff[c] + j) proc
      let sig_seen = scatter sig_seen (map2 (\t k -> if k & 3 == 2 then sig_rep[t] else -1) dst kind) (map (const true) dst)
      let named = scatter named (map2 (\t k -> if k & 3 == 3 then t else -1) dst kind) (map (const true) dst)
      let cands = take np pbuf ++ ms
      let fires = map (call reflect named sig_seen) cands
      let fm = zip cands fires |> filter (.1) |> map (.0)
      let pending = zip cands fires |> filter (\(_, f) -> !f) |> map (.0)
      let pbuf = scatter pbuf (indices pending) pending
      let to_iface = map2 (\t k -> if t > 0 && (k == 1 || (k == 4 && gotdef t)) then t else -1) dst kind
                     ++ map (\m -> i64.u32 m_mtyp[m]) ms
                     |> filter (> 0)
      let to_live = map2 (\t k -> if k & 3 == 0 then t else -1) dst kind
                    ++ flatten (map (\m -> [i64.u32 m_mtyp[m], i64.u32 m_ifn[m], i64.u32 m_tfn[m]]) fm)
                    |> filter (> 0)
      let live = scatter live to_live (map (const true) to_live)
      let iface = scatter iface to_iface (map (const true) to_iface)
      let need c = if gotype c && iface[c] then 2u8 else 1u8
      let new = filter (\t -> live[t] && need t > ex[t]) (to_live ++ to_iface ++ left)
      let (front, owner) = dedup owner new
      in (live, iface, ex, owner, front, sig_seen, named, reflect, pbuf, length pending,
          st with [0] = st[0] + 1 with [2] = st[2] + length proc_all with [3] = st[3] + reduce (+) 0 nes
             with [4] = st[4] + length dst)
  in (live, st)

def batch_args [L] [E] [M] [S] (nids: i64) (flags: [L]u8) (eoff: []u32) (edst: [E]u32) (ekind: [E]u8)
                (roots: []u32) (sig_name: [S]u32) (sig_type: [S]u32)
                (moff: []u32) (m_name: [M]u32) (m_mtyp: [M]u32) (m_ifn: [M]u32) (m_tfn: [M]u32)
                (m_exp: [M]u8) (nstr: i64) (C: i64) (B: i64) (capmin: i64) (H: i64) =
  let (sig_rep, m_sig) = sig_reps sig_name sig_type m_name m_mtyp
  in flood_batch nids (L - 1) flags roots eoff edst ekind moff m_mtyp m_ifn m_tfn m_exp m_name m_sig sig_rep nstr C B capmin H

-- C chunks per round, B pops per chunk, capmin edges per chunk buffer (at least the widest due row), H: the
-- chunk's push table (a power of two)
entry reach_batch [L] [E] [M] [S] (nids: i64) (flags: [L]u8) (eoff: []u32) (edst: [E]u32) (ekind: [E]u8)
                (roots: []u32) (sig_name: [S]u32) (sig_type: [S]u32)
                (moff: []u32) (m_name: [M]u32) (m_mtyp: [M]u32) (m_ifn: [M]u32) (m_tfn: [M]u32)
                (m_exp: [M]u8) (nstr: i64) (C: i64) (B: i64) (capmin: i64) (H: i64) : []u32 =
  (batch_args nids flags eoff edst ekind roots sig_name sig_type moff m_name m_mtyp m_ifn m_tfn m_exp nstr C B capmin H).0
  |> live_list

-- the counts: [rounds, 0, distinct symbols expanded, expansions (with chunks' duplicates), edges walked, 0]
entry batch_stats [L] [E] [M] [S] (nids: i64) (flags: [L]u8) (eoff: []u32) (edst: [E]u32) (ekind: [E]u8)
                (roots: []u32) (sig_name: [S]u32) (sig_type: [S]u32)
                (moff: []u32) (m_name: [M]u32) (m_mtyp: [M]u32) (m_ifn: [M]u32) (m_tfn: [M]u32)
                (m_exp: [M]u8) (nstr: i64) (C: i64) (B: i64) (capmin: i64) (H: i64) : [6]i64 =
  (batch_args nids flags eoff edst ekind roots sig_name sig_type moff m_name m_mtyp m_ifn m_tfn m_exp nstr C B capmin H).1
