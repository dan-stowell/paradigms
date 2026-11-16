-- p4.fut: problem 4, code generation by shape (ref/p4/README.md): every function body fc wrote, rebuilt from
-- byte templates + operands + glue. Lengths by a map, places by a scan, the bytes by a copy shaped as a scatter
-- (or a gather), the holes by a flattened map over (item, hole) pairs scattered into the image, the jumps by
-- gathering at[v], the relocations by a filter and two expands. The C driver (p4.c) only loads and checks.
import "lib/segmented/segmented"

def exscan [n] (xs: [n]i64) : [n]i64 = map2 (-) (scan (+) 0 xs) xs

-- An item is a glue run or one segment of a node's template. Its bytes: (from the templates?, offset, length).
def item_src (item_kind: []u8) (item_ref: []u32) (item_seg: []u32) (glue_off: []u64)
             (node_tmpl: []u32) (tmpl_off: []u64) (tmpl_seg_off: []u32) (tseg_end: []u32) (i: i64) : (bool, i64, i64) =
  let r = i64.u32 item_ref[i]
  in if item_kind[i] == 0
     then (false, i64.u64 glue_off[r], i64.u64 glue_off[r + 1] - i64.u64 glue_off[r])
     else let t = i64.u32 node_tmpl[r]
          let g = i64.u32 tmpl_seg_off[t] + i64.u32 item_seg[i]
          let lo = if item_seg[i] == 0 then 0 else i64.u32 tseg_end[g - 1]
          in (true, i64.u64 tmpl_off[t] + lo, i64.u32 tseg_end[g] - lo)

-- the copy as a scatter of fixed-size chunks: each item cut into `chunk`-byte pieces (an expand), each piece a
-- row of (index, byte) writes, -1 (ignored by scatter) past the item's end
def copy_chunks [n] (chunk: i64) (total: i64) (at: [n]i64) (src: [n](bool, i64, i64)) (glue_bin: []u8) (tmpl_bin: []u8) =
  let (owner, k) = repl_segm_iota (map (\(_, _, l) -> (l + chunk - 1) / chunk) src)
  let row i k = let (fromt, o, l) = src[i]
                in #[sequential] map (\j -> let p = k * chunk + j
                                           in if p >= l then (-1, 0u8)
                                              else (at[i] + p, if fromt then tmpl_bin[o + p] else glue_bin[o + p]))
                                         (iota chunk)
  let (is, vs) = unzip (flatten (map2 row owner k))
  in scatter (replicate total 0u8) is vs

-- the copy as a gather: every output byte finds its item (scatter of item starts + a max-scan), then reads it
def copy_gather [n] (total: i64) (at: [n]i64) (src: [n](bool, i64, i64)) (glue_bin: []u8) (tmpl_bin: []u8) =
  let starts = map2 (\a (_, _, l) -> if l == 0 then -1 else a) at src
  let owner = scatter (replicate total 0i64) starts (iota n) |> scan i64.max 0
  in map2 (\p i -> let (fromt, o, _) = src[i]
                   let q = o + p - at[i]
                   in if fromt then tmpl_bin[q] else glue_bin[q]) (iota total) owner

-- Per template segment g: the first of its template's holes that lies in it, and how many. Holes and segments
-- are both sorted (template, offset), so the starts are an exclusive scan of the per-segment counts.
def seg_holes [T] [G] [H] (tmpl_seg_off: [T]u32) (tseg_end: [G]u32) (tmpl_hole_off: [T]u32) (hole_at: [H]u32) =
  let ntmpl = T - 1
  let first = map (\t -> if t < ntmpl && tmpl_hole_off[t] < tmpl_hole_off[t + 1] then i64.u32 tmpl_hole_off[t] else -1) (iota T)
  let owner = scatter (replicate H 0i64) first (iota T) |> scan i64.max 0
  let seg_of h = let t = owner[h]
                 in loop g = i64.u32 tmpl_seg_off[t] while tseg_end[g] <= hole_at[h] do g + 1
  let cnt = hist (+) 0 G (map seg_of (iota H)) (replicate H 1i64)
  in (exscan cnt, cnt)

type rel = (i64, u8, u16, u32, i64)   -- (offset, width, type, symbol, addend)

-- The entry: the image, each function's offset, the relocations (unsorted), and the count of bad holes.
entry emit [F] [M] [N] [GL] [GR] [NN] [O] [T] [G] [H]
    (chunk: i64)
    (fn_item: [F]u32) (fn_mark_off: [F]u32) (mark_off: [M]u32) (mark_type: [M]u16) (mark_sym: [M]u32) (mark_add: [M]i64)
    (item_kind: [N]u8) (item_ref: [N]u32) (item_seg: [N]u32)
    (glue_off: [GL]u64) (glue_bin: []u8) (glue_rel_off: [GL]u32)
    (grel_off: [GR]u32) (grel_siz: [GR]u8) (grel_type: [GR]u16) (grel_sym: [GR]u32) (grel_add: [GR]i64)
    (node_tmpl: [NN]u32) (node_op: []u32) (op_val: [O]i64) (op_aux: [O]i64)
    (tmpl_off: [T]u64) (tmpl_bin: []u8) (tmpl_seg_off: [T]u32) (tseg_end: [G]u32)
    (tmpl_hole_off: [T]u32) (hole_at: [H]u32) (hole_w: [H]u8) (hole_enc: [H]u8) (hole_rtype: [H]u16) =
  -- 1. lay out: every item's length, then where it goes
  let src = map (item_src item_kind item_ref item_seg glue_off node_tmpl tmpl_off tmpl_seg_off tseg_end) (iota N)
  let lens = map (.2) src
  let at = exscan lens
  let total = if N == 0 then 0 else at[N - 1] + lens[N - 1]
  let at_of i = if i == N then total else at[i]
  -- 2. copy every item's bytes (chunk 0: as a gather, else as a scatter of chunk-byte rows)
  let code = if chunk == 0 then copy_gather total at src glue_bin tmpl_bin
             else copy_chunks chunk total at src glue_bin tmpl_bin
  -- 3. the holes: one (item, hole) pair per hole of a node segment
  let (hstart, hcnt) = seg_holes tmpl_seg_off tseg_end tmpl_hole_off hole_at
  let seg i = let r = i64.u32 item_ref[i] in i64.u32 tmpl_seg_off[i64.u32 node_tmpl[r]] + i64.u32 item_seg[i]
  let pairs = expand (\i -> if item_kind[i] == 0 then 0 else hcnt[seg i]) (\i j -> (i, hstart[seg i] + j)) (iota N)
  -- per pair: where it goes, which operand, the value to write
  let hole (i, h) =
    let r = i64.u32 item_ref[i]
    let t = i64.u32 node_tmpl[r]
    let g = seg i
    let lo = if item_seg[i] == 0 then 0 else i64.u32 tseg_end[g - 1]
    let o = i64.u32 node_op[r] + h - i64.u32 tmpl_hole_off[t]
    let p = at[i] + i64.u32 hole_at[h] - lo
    let v = op_val[o]
    let w = i64.u8 hole_w[h]
    in match hole_enc[h]
       case 1 -> (p, w, v, w == 1 || w == 2 || w == 4 || w == 8)
       case 2 -> (p, 4, at_of v + op_aux[o] - (p + 4), true)   -- rel32 to (item v, offset a)
       case 3 -> (p, 0, 0, true)                                -- a relocation: the bytes stay
       case _ -> (p, 0, 0, false)
  let hs = map hole pairs
  let writes = map (\(p, n, v, _) -> #[sequential] map (\j -> if j < n then (p + j, u8.i64 (v >> (8 * j))) else (-1, 0u8)) (iota 8)) hs
  let (wi, wv) = unzip (flatten writes)
  let code = scatter code wi wv
  let nerr = map (\(_, _, _, ok) -> i64.bool (!ok)) hs |> reduce (+) 0
  -- 4. relocations: the holes that are relocations, the glue's own, and each function's markers
  let rh = filter (\((_, h), _) -> hole_enc[h] == 3) (zip pairs hs)
  let from_hole ((i, h), (p, _, _, _)) : rel =
    let r = i64.u32 item_ref[i]
    let o = i64.u32 node_op[r] + h - i64.u32 tmpl_hole_off[i64.u32 node_tmpl[r]]
    in (p, hole_w[h], hole_rtype[h], u32.i64 op_val[o], op_aux[o])
  let rel_holes = map from_hole rh
  let rel_glue = expand (\i -> if item_kind[i] != 0 then 0
                               else let r = i64.u32 item_ref[i] in i64.u32 glue_rel_off[r + 1] - i64.u32 glue_rel_off[r])
                        (\i j -> let k = i64.u32 glue_rel_off[i64.u32 item_ref[i]] + j
                                 in (at[i] + i64.u32 grel_off[k], grel_siz[k], grel_type[k], grel_sym[k], grel_add[k]))
                        (iota N)
  let nfn = F - 1
  let rel_mark = expand (\f -> if f == nfn then 0 else i64.u32 fn_mark_off[f + 1] - i64.u32 fn_mark_off[f])
                        (\f j -> let m = i64.u32 fn_mark_off[f] + j
                                 in (at_of (i64.u32 fn_item[f]) + i64.u32 mark_off[m], 0u8, mark_type[m], mark_sym[m], mark_add[m]))
                        (iota F)
  let (ro, rs, rt, ry, ra) = unzip5 (rel_mark ++ rel_glue ++ rel_holes)
  let fn_off = map (\f -> at_of (i64.u32 fn_item[f])) (iota F)
  in (code, fn_off, ro, rs, rt, ry, ra, nerr)

-- the lay-out step alone (lengths + scan), to time it apart as ref/p4/p4.c does
entry layout [N] [GL] [NN] [T] [G]
    (item_kind: [N]u8) (item_ref: [N]u32) (item_seg: [N]u32) (glue_off: [GL]u64)
    (node_tmpl: [NN]u32) (tmpl_off: [T]u64) (tmpl_seg_off: [T]u32) (tseg_end: [G]u32) : [N]i64 =
  map (item_src item_kind item_ref item_seg glue_off node_tmpl tmpl_off tmpl_seg_off tseg_end) (iota N)
  |> map (.2) |> exscan
