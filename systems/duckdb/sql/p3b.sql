-- Problem 3b, relocate. SQL computes every write (position, bytes) and (position, width, value); dk's .blob and
-- .int copy them into the image (SQL has no "store at byte offset"). The rules are ref/flat/README.md's.
.cell 3b
.image image_bytes
-- every placed symbol's bytes at addr - imageBase (bss buckets have none)
.blob SELECT addr - getvariable('imageBase') AS pos, bytes FROM sym3b JOIN addr3 USING (i) WHERE b < 13;
-- every relocation's value. P = addr + off, T = the target's address
CREATE OR REPLACE TEMP VIEW patch AS
WITH r AS (
  SELECT addr3.addr::BIGINT AS a, unnest(rels, recursive := true) FROM sym3b JOIN addr3 USING (i) WHERE b < 13),
t AS (
  SELECT r.*, CASE WHEN kind = 2 AND tgt <> 0 AND coalesce(addrall.a, 0) = 0 THEN getvariable('addr_unreachableMethod')
                   WHEN kind = 6 AND tgt <> 0 AND coalesce(addrall.a, 0) = 0 THEN getvariable('addr_mapinitnoop')
                   ELSE coalesce(addrall.a, 0) END::BIGINT AS T
  FROM r LEFT JOIN addrall ON addrall.id = r.tgt),
sect AS ( -- the section base below T: the largest lay_* not above it
  SELECT t.*, CASE WHEN T >= getvariable('lay_noptrbss') THEN getvariable('lay_noptrbss')
                   WHEN T >= getvariable('lay_bss') THEN getvariable('lay_bss')
                   WHEN T >= getvariable('lay_data') THEN getvariable('lay_data')
                   WHEN T >= getvariable('lay_noptrdata') THEN getvariable('lay_noptrdata')
                   WHEN T >= getvariable('lay_types') THEN getvariable('lay_types')
                   WHEN T >= getvariable('lay_pclntab') THEN getvariable('lay_pclntab')
                   WHEN T >= getvariable('lay_rodata') THEN getvariable('lay_rodata')
                   ELSE getvariable('lay_text') END::BIGINT AS sb
  FROM t)
SELECT a - getvariable('imageBase') + off AS pos, siz, kind, tgt, T,
       CASE WHEN kind IN (1, 2) THEN T + add
            WHEN kind IN (3, 4) THEN T - sb + add
            WHEN kind IN (5, 6, 7) THEN T + add - (a + off + siz)
            WHEN kind = 8 THEN add END AS v,
       (kind IN (1, 2, 5, 6, 7) AND T = 0 AND tgt <> 0) OR kind NOT BETWEEN 1 AND 8 OR siz NOT IN (1, 2, 4, 8) AS err
FROM sect;
.int SELECT pos, CASE WHEN kind = 4 AND T = 0 THEN 4 ELSE siz END, CASE WHEN kind = 4 AND T = 0 THEN 4294967295 ELSE v END FROM patch WHERE NOT err AND NOT (kind = 3 AND T = 0);
.nocell
.show SELECT count(*) AS relocations, count(*) FILTER (err) AS errors FROM patch;
.imgout out/p3_image.bin
