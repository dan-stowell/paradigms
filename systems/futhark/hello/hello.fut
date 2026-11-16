-- Prefix sum (scan) + histogram (reduce_by_index) over a u32 array; returns checksums.
def nbuckets : i64 = 65536

entry hello [n] (xs: [n]u32) : [3]u64 =
  let ys = map u64.u32 xs
  let ps = scan (+) 0u64 ys                          -- inclusive prefix sum
  let h  = reduce_by_index (replicate nbuckets 0i64) (+) 0i64
             (map (\x -> i64.u32 (x & 0xFFFF)) xs) (replicate n 1i64)
  let ck_scan = reduce (^) 0u64 (map2 (\i p -> p * u64.i64 (i+1)) (iota n) ps)
  let ck_hist = reduce (+) 0i64 (map2 (\i c -> c * (i+1)) (iota nbuckets) h)
  in [ps[n-1], ck_scan, u64.i64 ck_hist]
