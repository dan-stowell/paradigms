-- Problem 1, resolve, as SQL over the tables of load1.sql. The rules are ref/flat/README.md's.
-- A winner is the maximum of rank << 32 | ~id: highest rank, then lowest id.

.cell 1a
-- the groups by name: every wanted name (value 0, no slot) and every slot by name (kinds 4, 5)
CREATE OR REPLACE TEMP TABLE grp AS
SELECT key, max(v) AS v
FROM (SELECT key, 0::UBIGINT AS v FROM want
      UNION ALL
      SELECT key, rank::UBIGINT << 32 | (4294967295 - id) FROM slot WHERE kind IN (4, 5) AND key <> 0)
GROUP BY key;
-- a definition by package index joins a group only if its name is already there
CREATE OR REPLACE TEMP TABLE win AS
SELECT key, max(v) AS v
FROM (FROM grp
      UNION ALL
      SELECT key, rank::UBIGINT << 32 | (4294967295 - id) FROM slot SEMI JOIN grp USING (key) WHERE kind = 1 AND key <> 0)
GROUP BY key;
-- every member's canonical id is its group's winner; a debug-only reference outside every group is 0
CREATE OR REPLACE TEMP TABLE canon AS
SELECT id, CASE WHEN win.v IS NOT NULL THEN (4294967295 - (win.v & 4294967295))::UINTEGER
                WHEN flags & 2 <> 0 THEN 0
                ELSE id END AS canon
FROM slot LEFT JOIN win USING (key);
CREATE OR REPLACE TEMP TABLE builtin_canon AS
SELECT i, CASE WHEN win.v <> 0 AND (win.v >> 32) & (1 << 29) <> 0 THEN (4294967295 - (win.v & 4294967295))::UINTEGER
               ELSE 0 END AS canon
FROM builtin LEFT JOIN win USING (key);

.cell 1b
-- an object's package table, keyed by the package index a relocation carries: 0..npkg-1, then the five specials
CREATE OR REPLACE TEMP TABLE pkgbase AS
SELECT o, (CASE WHEN t < n - 5 THEN t ELSE 2147483643 + t - (n - 5) END)::UINTEGER AS pkg, base
FROM (SELECT o, len(bases) AS n, generate_subscripts(bases, 1) - 1 AS t, unnest(bases, recursive := true) FROM obj)
WHERE base <> 4294967295;
-- each relocation (a slot's list, unnested) through its object's package table to a slot, then its canonical id
CREATE OR REPLACE TEMP TABLE reltarget AS
WITH rel AS (SELECT obj AS o, unnest(rels, recursive := true) FROM srel),
target AS (
  SELECT r, CASE WHEN pb.base IS NULL THEN 0
                 WHEN rel.pkg = 2147483644 THEN coalesce(bc.canon, 0) -- package index "builtin"
                 ELSE pb.base + idx END AS s
  FROM rel LEFT JOIN pkgbase pb USING (o, pkg)
           LEFT JOIN builtin_canon bc ON bc.i = CASE WHEN rel.pkg = 2147483644 THEN rel.idx END)
SELECT r, coalesce(canon.canon, 0) AS tgt
FROM target LEFT JOIN canon ON canon.id = target.s;

.cell 1c
-- the content-hash join over the copies the flood reached (mark 1) or only named (mark 2). A group: kind 2 by
-- k1, kind 3 by (k1, k2). Winner: a reached copy, then (kind 2) the largest, then the lowest id.
CREATE OR REPLACE TEMP TABLE canon2 AS
WITH copy AS (
  SELECT id, kind, mark, hk1, CASE WHEN kind = 2 THEN 0 ELSE hk2 END AS k2, CASE WHEN kind = 2 THEN hk2 ELSE 0 END AS size
  FROM slot WHERE kind IN (2, 3) AND mark > 0),
grp AS (
  SELECT kind, hk1, k2, arg_min({'id': id, 'mark': mark, 'size': size}, (mark, -size::HUGEINT, id)) AS w
  FROM copy GROUP BY ALL),
moved AS ( -- nothing changes in a group whose winner was only named, nor for a named copy larger than the winner
  SELECT copy.id, w.id AS canon
  FROM copy JOIN grp USING (kind, hk1, k2)
  WHERE w.mark = 1 AND w.id <> copy.id AND NOT (copy.kind = 2 AND copy.mark = 2 AND copy.size > w.size))
SELECT id, coalesce(moved.canon, canon.canon) AS canon
FROM canon LEFT JOIN moved USING (id);
.nocell

.dump out/p1_canon.u32 SELECT canon FROM canon ORDER BY id;
.dump out/p1_builtin.u32 SELECT canon FROM builtin_canon ORDER BY i;
.dump out/p1_reltarget.u32 SELECT tgt FROM reltarget ORDER BY r;
.dump out/p1c_canon.u32 SELECT canon FROM canon2 ORDER BY id;
