-- msort.fut: a bottom-up merge sort shaped for the multicore backend. Blocks of 64 are sorted by insertion
-- (a map over blocks, a sequential loop in each), then each level merges neighbouring runs: a map over
-- the pairs, each merged by a sequential loop (stable: ties take the left run). The library's merge_sort
-- (lib/sorts, built for GPUs) takes 13 ms for 33 K u64 on one core here; this one takes about 2 ms.
-- (Rows shorter than ~64 cost a copy call each in the generated code, which is why the base is 64.)
def merge_sort_cpu [n] 't (leq: t -> t -> bool) (xs: [n]t) : [n]t =
  if n <= 1 then xs else
  let base = 64i64
  let m = i64.max base (1i64 << i64.i32 (64 - i64.clz (n - 1)))   -- a power of two >= n
  -- pad with "nothing", which sorts after everything
  let leq' (ra, a) (rb, b) = !rb || (ra && leq a b)
  let ys = map (\i -> if i < n then (true, xs[i]) else (false, xs[0])) (iota m)
  let isort (row: [base](bool, t)) : [base](bool, t) =
    loop row = copy row for i in 1..<base do
      let x = copy row[i]
      let (row, j) = loop (row, j) = (row, i) while j > 0 && !(leq' row[j - 1] x) do
                       (row with [j] = copy row[j - 1], j - 1)
      in row with [j] = x
  let ys = flatten (map isort (unflatten (ys :> [(m / base) * base](bool, t)))) :> [m](bool, t)
  let (ys, _) =
    loop (ys, run) = (ys, base) while run < m do
      let w = 2 * run
      let merge (row: [w](bool, t)) : [w](bool, t) =
        let (out, _, _) =
          loop (out, i, j) = (copy row, 0, 0) for k < w do
            if j >= run || (i < run && leq' row[i] row[run + j])
            then (out with [k] = copy row[i], i + 1, j)
            else (out with [k] = copy row[run + j], i, j + 1)
        in out
      let rows = unflatten (ys :> [(m / w) * w](bool, t))
      in (flatten (map merge rows) :> [m](bool, t), w)
  in take n ys |> map (.1)
