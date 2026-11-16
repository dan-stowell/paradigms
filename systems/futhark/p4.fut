-- p4.fut: problem 4, code generation by shape (ref/p4/README.md): every function body fc wrote, rebuilt from
-- byte templates + operands + glue. Lengths by a map, places by an exclusive scan, the bytes by a copy (five
-- shapes, `how` in emit), the holes by a flattened map over (node, hole) pairs scattered into the image, the jumps
-- by gathering at[v], the relocations by three expands (reloc holes, glue relocations, markers) concatenated.
-- The C driver (p4.c) only loads the flat files and checks the outputs.
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

def fchunk = 32i64   -- the row of the fused copy + patch, a constant so the row can live in registers

-- the copy and the patch in one scatter: each chunk-byte row of a node segment is read from its template, then
-- `fix i base row` writes the holes that fall in it (a short sequential loop inside the row)
def copy_patch [n] (chunk: i64) (total: i64) (at: [n]i64) (src: [n](bool, i64, i64)) (glue_bin: []u8) (tmpl_bin: []u8)
               (fix: i64 -> i64 -> *[chunk]u8 -> *[chunk]u8) =
  let (owner, k) = repl_segm_iota (map (\(_, _, l) -> (l + chunk - 1) / chunk) src)
  let row i k = let (fromt, o, l) = src[i]
                let base = k * chunk
                let bs = #[sequential] map (\j -> let p = base + j
                                                 in if p >= l then 0u8 else if fromt then tmpl_bin[o + p] else glue_bin[o + p])
                                               (iota chunk)
                in if fromt then fix i base bs else bs
  -- the rows of bytes are materialized (the loop in `fix` stops fusion); their indices are a map, fused into the scatter
  let bytes = flatten (map2 row owner k)
  let index x = let i = owner[x / chunk]
                let p = k[x / chunk] * chunk + x % chunk
                in if p < src[i].2 then at[i] + p else -1
  in scatter (replicate total 0u8) (map index (indices bytes)) bytes

-- row[to : to + m] = xs[from : from + m], a memcpy
def blit [S] (row: *[S]u8) (to: i64) (xs: []u8) (from: i64) (m: i64) : *[S]u8 =
  row with [to : to + m] = (xs[from : from + m] :> [to + m - to]u8)

-- the copy in blocks: the image cut into P equal blocks, a map over the blocks, and inside each a sequential loop
-- over the items that overlap it, each copied by a slice update (a memcpy), then `fix i lo row` for a node segment
-- (its holes). The first item of a block is a binary search in `at`.
def copy_blocks [n] [S] (P: i64) (total: i64) (at: [n]i64) (src: [n](bool, i64, i64)) (glue_bin: []u8) (tmpl_bin: []u8)
                    (fix: i64 -> i64 -> *[S]u8 -> *[S]u8) : *[total]u8 =
  let block b =
    let lo = b * S
    let hi = i64.min total (lo + S)
    -- the last item that starts at or before lo
    let (i0, _) = loop (l, h) = (0, n) while h - l > 1 do let m = (l + h) / 2 in if at[m] <= lo then (m, h) else (l, m)
    let (row, _) =
      loop (row, i) = (replicate S 0u8, i0) while i < n && at[i] < hi do
        let (fromt, o, l) = src[i]
        let a = i64.max at[i] lo
        let e = i64.min (at[i] + l) hi
        let m = i64.max 0 (e - a)
        let from = o + a - at[i]
        let row = if fromt then blit row (a - lo) tmpl_bin from m else blit row (a - lo) glue_bin from m
        let row = if fromt then fix i lo row else row
        in (row, i + 1)
    in row
  in take total (flatten (map block (iota P))) :> [total]u8

-- the copy as a gather: every output byte finds its item (scatter of item starts + a max-scan), then reads it
def copy_gather [n] (total: i64) (at: [n]i64) (src: [n](bool, i64, i64)) (glue_bin: []u8) (tmpl_bin: []u8) =
  let starts = map2 (\a (_, _, l) -> if l == 0 then -1 else a) at src
  let owner = scatter (replicate total 0i64) starts (iota n) |> scan i64.max 0
  in map2 (\p i -> let (fromt, o, _) = src[i]
                   let q = o + p - at[i]
                   in if fromt then tmpl_bin[q] else glue_bin[q]) (iota total) owner

-- Where each template hole lies: its segment (as a number within its template), that segment's start, and the
-- segment's global number (into tseg_end).
def hole_segs [T] [G] [H] (tmpl_seg_off: [T]u32) (tseg_end: [G]u32) (tmpl_hole_off: [T]u32) (hole_at: [H]u32) =
  let first = map (\t -> if t < T - 1 && tmpl_hole_off[t] < tmpl_hole_off[t + 1] then i64.u32 tmpl_hole_off[t] else -1) (iota T)
  let owner = scatter (replicate H 0i64) first (iota T) |> scan i64.max 0
  -- the first segment of the template that ends after the hole starts (a binary search: call templates have hundreds)
  in map (\h -> let b = i64.u32 tmpl_seg_off[owner[h]]
                let (g, _) = loop (lo, hi) = (b, i64.u32 tmpl_seg_off[owner[h] + 1] - 1) while lo < hi do
                               let m = (lo + hi) / 2 in if tseg_end[m] <= hole_at[h] then (m + 1, hi) else (lo, m)
                in (g - b, if g == b then 0 else i64.u32 tseg_end[g - 1], g)) (iota H)

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
-- `how` picks the shape of the copy + patch (below); `parts` (normally 7) switches pieces off to time them apart:
-- 1 copy, 2 patch, 4 relocations (how 0, 1, 3).
entry emit [F] [M] [N] [GL] [GR] [NN] [O] [T] [G] [H]
    (parts: i64) (how: i64) (chunk: i64)
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
    let (s, lo, _) = hseg[h]
    in (at[seg_item[nseg_off[r] + s]] + i64.u32 hole_at[h] - lo, i64.u32 node_op[r] + h - i64.u32 tmpl_hole_off[i64.u32 node_tmpl[r]])
  -- what to write for part k of hole h at p, operand o: an integer's w bytes of v; a jump's rel32 to (item v, offset a)
  let value h k p o =
    if hole_enc[h] == 2 then (p, 4, at_of op_val[o] + op_aux[o] - (p + 4))
    else (p + 4 * k, i64.u8 hole_w[h] - 4 * k, op_val[o] >> (32 * k))
  let patch (r, h, k) = let (p, o) = place r h in value h k p o
  let patched = class_holes tmpl_hole_off (map2 (\e w -> if e == 3 then 0 else if w == 8 then 2 else 1) hole_enc hole_w)
  -- each (part of a) hole as a row of 4 (index, byte) writes, -1 past its width
  let bytes (p, n, v) = #[sequential] map (\k -> (if k < n then p + k else -1, u8.i64 (v >> (8 * k)))) (iota 4)
  -- the patched entries of template segment g are seg_first[g] ..< seg_first[g] + seg_cnt[g]
  let seg_cnt = hist (+) 0 G (map (\(h, _) -> hseg[h].2) patched.0) (map (const 1i64) patched.0)
  let seg_first = exscan seg_cnt
  -- write the holes of item i (a node segment) that fall in the window [wlo, wlo + W) of the image into `row`,
  -- which holds that window: a short sequential loop, for the shapes that copy and patch in one pass
  let patch_item [W] (i: i64) (wlo: i64) (row: *[W]u8) : *[W]u8 =
    let r = i64.u32 item_ref[i]
    let t = i64.u32 node_tmpl[r]
    let g = i64.u32 tmpl_seg_off[t] + i64.u32 item_seg[i]
    let o0 = i64.u32 node_op[r] - i64.u32 tmpl_hole_off[t]
    in loop row for e in seg_first[g] ..< seg_first[g] + seg_cnt[g] do
         let (h, k) = patched.0[e]
         let (p, n, v) = value h k (at[i] + i64.u32 hole_at[h] - hseg[h].1) (o0 + h)
         let q = p - wlo
         in loop row for b < 4 do
              if b < n && q + b >= 0 && q + b < W then row with [q + b] = u8.i64 (v >> (8 * b)) else row
  -- 2. copy every item's bytes, then patch the holes, in one of five shapes (`how`):
  --   0: the copy as a scatter of chunk-byte rows, then the patch as a second scatter
  --   1: the copy as a gather, then the patch as a scatter
  --   2: copy and patch in one scatter of 32-byte rows (patch_item inside each row)
  --   3: the copy in P = chunk blocks (a map over blocks, a sequential loop of slice copies inside), patch as a scatter
  --   4: copy and patch in P = chunk blocks (patch_item after each item's copy, inside the block's loop)
  let S = (total + chunk - 1) / chunk   -- the block size of shapes 3 and 4
  let code =
    if how == 2 then copy_patch fchunk total at src glue_bin tmpl_bin (\i base (row: *[fchunk]u8) : *[fchunk]u8 -> patch_item i (at[i] + base) row)
    else if how == 4 then copy_blocks chunk total at src glue_bin tmpl_bin (\i lo (row: *[S]u8) : *[S]u8 -> patch_item i lo row)
    else
    let code = if parts & 1 == 0 then replicate total 0
               else if how == 1 then copy_gather total at src glue_bin tmpl_bin
               else if how == 3 then copy_blocks chunk total at src glue_bin tmpl_bin (\_ _ (row: *[S]u8) : *[S]u8 -> row)
               else copy_chunks chunk total at src glue_bin tmpl_bin
    in if parts & 2 == 0 then code
       else let (wi, wv) = holes_of patched |> map patch |> map bytes |> flatten |> unzip
            in scatter code wi wv
  -- holes the encoder cannot have written (checked on the template table)
  let nerr = map (\h -> let w = hole_w[h] in match hole_enc[h]
                                            case 1 -> i64.bool (w != 1 && w != 2 && w != 4 && w != 8)
                                            case 2 -> i64.bool (w != 4)
                                            case 3 -> 0
                                            case _ -> 1) (iota H) |> reduce (+) 0
  -- 4. relocations: the holes that are relocations (their bytes stay), the glue's own, each function's markers
  let rels = if parts & 4 == 0 then [] else
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
    in rel_mark ++ rel_glue ++ rel_holes
  let (ro, rs, rt, ry, ra) = unzip5 rels
  let fn_off = map (\f -> at_of (i64.u32 fn_item[f])) (iota F)
  in (code, fn_off, ro, rs, rt, ry, ra, nerr)

-- the lay-out step alone (lengths + scan), to time it apart as ref/p4/p4.c does
entry layout [N] [GL] [NN] [T] [G]
    (item_kind: [N]u8) (item_ref: [N]u32) (item_seg: [N]u32) (glue_off: [GL]u64)
    (node_tmpl: [NN]u32) (tmpl_off: [T]u64) (tmpl_seg_off: [T]u32) (tseg_end: [G]u32) : [N]i64 =
  map (item_src item_kind item_ref item_seg glue_off node_tmpl tmpl_off tmpl_seg_off tseg_end) (iota N)
  |> map (.2) |> exscan
