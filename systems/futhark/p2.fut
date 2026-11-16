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
                          (m_name: [M]u32) (m_sig: [M]i64) (sig_rep: [S]i64) (nstr: i64) : []bool =
  let fl c = if c <= nsl then flags[c] else 0
  let live = replicate N false
  let iface = map (\c -> fl c & f_iface != 0) (iota N)
  let (front, owner) = dedup (replicate N 0) (map i64.u32 roots |> filter (> 0))
  let live = scatter live front (map (const true) front)
  let (live, _, _, _, _, _, _, _, _) =
    loop (live, iface, owner, front, sig_seen, named, reflect, cand, fired) =
         (live, iface, owner, front, replicate S false, replicate (nstr + 1) false, false,
          replicate M false, replicate M false)
    while length front > 0 do
      let proc = filter (\c -> fl c & f_def != 0) front
      let reflect = reflect || any (\c -> fl c & f_reflect != 0) proc
      let uii c = fl c & f_gotype != 0 && iface[c]
      let (src, dst, kind) = unzip3 (edges (length proc) (\i -> proc[i]))
      -- what this level learns
      let gotdef t = fl t & f_gotype != 0 && fl t & f_def != 0
      let ms = expand (\c -> if uii c then i64.u32 (moff[c + 1] - moff[c]) else 0) (\c j -> i64.u32 moff[c] + j) proc
      let cand = scatter cand ms (map (const true) ms)
      let sig_seen = scatter sig_seen (map2 (\t k -> if k == 2 then sig_rep[t] else -1) dst kind) (map (const true) dst)
      let named = scatter named (map2 (\t k -> if k == 3 then t else -1) dst kind) (map (const true) dst)
      let call m = cand[m] && !fired[m] &&
                   ((reflect && m_exp[m] != 0) || named[i64.u32 m_name[m]] || (m_sig[m] >= 0 && sig_seen[m_sig[m]]))
      let fm = filter call (iota M)
      let fired = scatter fired fm (map (const true) fm)
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
      in (live, iface, owner, front, sig_seen, named, reflect, cand, fired)
  in live

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
  in flood nids nsl flags roots edges moff m_mtyp m_ifn m_tfn m_exp m_name m_sig sig_rep nstr |> live_list

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
                 (m_exp: [M]u8) (nstr: i64) : []u32 =
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
    let starts = map (\(c, r) -> r == i64.u32 slot_rel[c]) rs
    let fol = followed starts (map (\(_, r) -> rel_type[r]) rs)
    let rt = map2 (\(c, r) f -> if f then decode canon builtin pkg_off pkg (i64.u32 slot_obj[c]) rel_pkg[r] rel_idx[r] else 0) rs fol
    let rkeep = zip rs rt |> filter (\(_, t) -> t != 0)
    in map (\(c, e) -> (c, i64.u32 xdst[e], xkind[e])) xs ++ map (\((c, _), t) -> (c, i64.u32 t, 0u8)) rkeep
  in flood nids nsl flags roots edges moff m_mtyp m_ifn m_tfn m_exp m_name m_sig sig_rep nstr |> live_list
