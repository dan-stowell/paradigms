-- Problem 3a, lay out, as SQL. The rules are ref/flat/README.md's.
-- Most of it is prefix sums (window sums). The sequential step is "round va up to the next block's alignment,
-- then add the block": a round-up does not compose into a prefix sum.
.cell 3a
-- occupied bytes: size rounded up to the alignment, text at least minFunc
CREATE OR REPLACE TEMP TABLE sym3 AS
SELECT i, id, b, cl, unit, size, tl,
       ((CASE WHEN b IN (1, 2) THEN greatest(size, getvariable('minFunc')::UINTEGER) ELSE size END + (1 << cl) - 1) >> cl << cl)::UBIGINT AS occ
FROM placed;
-- each unit's part of a bucket is a block: aligned to max(minalign, its largest class with bytes), then its bytes
-- (a block of zero bytes is not aligned)
CREATE OR REPLACE TEMP TABLE blk AS
SELECT b, row_number() OVER (PARTITION BY b ORDER BY unit) AS j, unit, align, n
FROM (SELECT b, unit, CASE WHEN sum(occ) > 0 THEN greatest(max(minalign), 1 << max(cl) FILTER (occ > 0)) ELSE 1 END::UBIGINT AS align, sum(occ)::UBIGINT AS n
      FROM sym3 JOIN bucket USING (b) WHERE b <> 6 GROUP BY b, unit);
-- the blocks' addresses: va = round_up(va, align), then va += n, one unit after another: a fold (list_reduce)
-- per bucket whose accumulator is the list of block ends so far (a recursive CTE did the same in 340-620 ms:
-- 1,322 iterations of a join, see NOTES.md)
CREATE OR REPLACE TEMP TABLE ustart AS
WITH ends AS (
  SELECT b, list_reduce(list({'a': align, 'n': n, 'e': []::UBIGINT[]} ORDER BY j),
                        lambda acc, x: {'a': 0, 'n': 0, 'e': list_append(acc.e, (acc.e[-1] + x.a - 1) // x.a * x.a + x.n)},
                        {'a': 0, 'n': 0, 'e': [any_value(start)::UBIGINT]}).e AS e
  FROM blk JOIN bucket USING (b) GROUP BY b)
SELECT b, unit, (e[j] + align - 1) // align * align AS va FROM blk JOIN ends USING (b);
-- typelinks: in type-string order, each aligned to its class, advancing by its size. The round-up is solved as a
-- fixpoint over the paddings: guess 0, prefix-sum with the guess, recompute each padding from where its symbol
-- landed, repeat until none changes (each round fixes at least the next wrong one; MinIO: 1 round, every
-- typelink's size is a multiple of its alignment)
CREATE OR REPLACE TEMP TABLE tlk AS
SELECT i, (1 << cl)::UBIGINT AS align, size::UBIGINT AS n, 0::UBIGINT AS pad, row_number() OVER (ORDER BY tlstr.str, id) AS k
FROM sym3 JOIN tlstr ON tlstr.s = sym3.tl WHERE b = 6;
.repeat
CREATE OR REPLACE TEMP TABLE tlk2 AS
WITH x AS (SELECT *, (SELECT start FROM bucket WHERE b = 6) + coalesce(sum(n + pad) OVER (ORDER BY k ROWS BETWEEN UNBOUNDED PRECEDING AND 1 PRECEDING), 0) AS x FROM tlk)
SELECT i, align, n, ((x + align - 1) // align * align - x)::UBIGINT AS pad, k, pad AS old, x FROM x;
CREATE OR REPLACE TEMP TABLE tlk AS SELECT i, align, n, pad, k FROM tlk2;
.until SELECT count(*) FROM tlk2 WHERE pad <> old;
-- inside a unit's block, classes from 12 down to 0, and inside a class the symbols in list order
CREATE OR REPLACE TEMP TABLE addr AS
WITH cls AS (SELECT b, unit, cl, sum(occ) AS n FROM sym3 WHERE b <> 6 GROUP BY ALL),
cstart AS (SELECT b, unit, cl, ustart.va + coalesce(sum(n) OVER (PARTITION BY b, unit ORDER BY cl DESC ROWS BETWEEN UNBOUNDED PRECEDING AND 1 PRECEDING), 0) AS va
           FROM cls JOIN ustart USING (b, unit))
SELECT i, id, (va + coalesce(sum(occ) OVER (PARTITION BY b, unit, cl ORDER BY i ROWS BETWEEN UNBOUNDED PRECEDING AND 1 PRECEDING), 0))::UINTEGER AS addr
FROM sym3 JOIN cstart USING (b, unit, cl)
UNION ALL
SELECT i, id, (x + pad)::UINTEGER FROM sym3 JOIN tlk2 USING (i);
.nocell
.dump out/p3_addr.u32 SELECT addr FROM addr ORDER BY i;
