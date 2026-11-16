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

-- Where each template hole lies: its segment (as a number within its template) and that segment's start.
def hole_segs [T] [G] [H] (tmpl_seg_off: [T]u32) (tseg_end: [G]u32) (tmpl_hole_off: [T]u32) (hole_at: [H]u32) =
  let first = map (\t -> if t < T - 1 && tmpl_hole_off[t] < tmpl_hole_off[t + 1] then i64.u32 tmpl_hole_off[t] else -1) (iota T)
  let owner = scatter (replicate H 0i64) first (iota T) |> scan i64.max 0
  in map (\h -> let b = i64.u32 tmpl_seg_off[owner[h]]
                let g = loop g = b while tseg_end[g] <= hole_at[h] do g + 1
                in (g - b, if g == b then 0 else i64.u32 tseg_end[g - 1])) (iota H)

-- The holes of one class, per template, as a list of (hole, part) entries in (template, offset) order: `n h`
-- entries for hole h (ns[h]: 0 if not in the class; 2 for an 8-byte integer, written as two 4-byte parts). The
-- entries of template t are idx[start[t] : start[t + 1]], start being an exclusive scan at the template starts.
def class_holes [T] [H] (tmpl_hole_off: [T]u32) (ns: [H]i64) : ([](i64, i64), [T]i64) =
  let before = exscan ns
  let total = reduce (+) 0 ns
  let start = map (\h -> let h = i64.u32 h in if h == H then total else before[h]) tmpl_hole_off
  in (expand (\h -> ns[h]) (\h k -> (h, k)) (iota H), start)

type rel = (i64, u8, u16, u32, i64)   -- (offset, width, type, symbol, addend)

-- The entry: the image, each function's offset, the relocations (unsorted), and the count of bad holes.
-- `parts` (normally 7) switches pieces off to time them apart: 1 copy, 2 patch, 4 relocations.
entry emit [F] [M] [N] [GL] [GR] [NN] [O] [T] [G] [H]
    (parts: i64) (chunk: i64)
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
  let code = if parts & 1 == 0 then replicate total 0
             else if chunk == 0 then copy_gather total at src glue_bin tmpl_bin
             else copy_chunks chunk total at src glue_bin tmpl_bin
  -- 3. the holes. A node's segments are items: seg_item[nseg_off[r] + s] is the item of node r's segment s.
  let nseg r = let t = i64.u32 node_tmpl[r] in i64.u32 tmpl_seg_off[t + 1] - i64.u32 tmpl_seg_off[t]
  let nseg_off = exscan (map nseg (iota NN))
  let seg_item = scatter (replicate (N - GL + 1) 0i64)
                         (map (\i -> if item_kind[i] == 0 then -1 else nseg_off[i64.u32 item_ref[i]] + i64.u32 item_seg[i]) (iota N))
                         (iota N)
  let hseg = hole_segs tmpl_seg_off tseg_end tmpl_hole_off hole_at
  -- one (node, hole, part) triple per entry of the class in the node's template: a flattened map over nodes
  let holes_of (idx, start) =
    expand (\r -> let t = i64.u32 node_tmpl[r] in start[t + 1] - start[t])
           (\r j -> let (h, k) = idx[start[i64.u32 node_tmpl[r]] + j] in (r, h, k)) (iota NN)
  -- where a hole goes, and its operand
  let place r h =
    let (s, lo) = hseg[h]
    in (at[seg_item[nseg_off[r] + s]] + i64.u32 hole_at[h] - lo, i64.u32 node_op[r] + h - i64.u32 tmpl_hole_off[i64.u32 node_tmpl[r]])
  -- an integer's w bytes of v; a jump's rel32 to (item v, offset a)
  let patch (r, h, k) =
    let (p, o) = place r h
    in if hole_enc[h] == 2 then (p, 4, at_of op_val[o] + op_aux[o] - (p + 4))
       else (p + 4 * k, i64.u8 hole_w[h] - 4 * k, op_val[o] >> (32 * k))
  let patched = class_holes tmpl_hole_off (map2 (\e w -> if e == 3 then 0 else if w == 8 then 2 else 1) hole_enc hole_w)
  -- each (part of a) hole as a row of 4 (index, byte) writes, -1 past its width
  let bytes (p, n, v) = #[sequential] map (\k -> (if k < n then p + k else -1, u8.i64 (v >> (8 * k)))) (iota 4)
  let (wi, wv) = holes_of patched |> map patch |> map bytes |> flatten |> unzip
  let code = if parts & 2 == 0 then code else scatter code wi wv
  -- holes the encoder cannot have written (checked on the template table)
  let nerr = map (\h -> let w = hole_w[h] in match hole_enc[h]
                                            case 1 -> i64.bool (w != 1 && w != 2 && w != 4 && w != 8)
                                            case 2 -> i64.bool (w != 4)
                                            case 3 -> 0
                                            case _ -> 1) (iota H) |> reduce (+) 0
  -- 4. relocations: the holes that are relocations (their bytes stay), the glue's own, each function's markers
  let rel_holes = holes_of (class_holes tmpl_hole_off (map (\e -> i64.bool (e == 3)) hole_enc))
                  |> map (\(r, h, _) -> let (p, o) = place r h
                                        in (p, hole_w[h], hole_rtype[h], u32.i64 op_val[o], op_aux[o]))
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
  let rels = if parts & 4 == 0 then [] else rel_mark ++ rel_glue ++ rel_holes
  let (ro, rs, rt, ry, ra) = unzip5 rels
  let fn_off = map (\f -> at_of (i64.u32 fn_item[f])) (iota F)
  in (code, fn_off, ro, rs, rt, ry, ra, nerr)

-- the lay-out step alone (lengths + scan), to time it apart as ref/p4/p4.c does
entry layout [N] [GL] [NN] [T] [G]
    (item_kind: [N]u8) (item_ref: [N]u32) (item_seg: [N]u32) (glue_off: [GL]u64)
    (node_tmpl: [NN]u32) (tmpl_off: [T]u64) (tmpl_seg_off: [T]u32) (tseg_end: [G]u32) : [N]i64 =
  map (item_src item_kind item_ref item_seg glue_off node_tmpl tmpl_off tmpl_seg_off tseg_end) (iota N)
  |> map (.2) |> exscan
