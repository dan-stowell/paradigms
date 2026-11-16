-- Problem 2, reach, third attempt: whole floods as recursive CTEs, one outer round per method phase.
-- The rules are ref/flat/README.md's. The first two attempts (p2loop.sql) step every rule one BFS level at a
-- time (45 rounds x ~25 statements). Here a round is the C reference's outer round (ref/flat/p2.c: 7 on MinIO):
--   1. live: the plain closure of the new seeds, in one WITH RECURSIVE query (DuckDB keeps the recursion's
--      hash table across its iterations instead of rebuilding it per statement);
--   2. what the new live code names: signatures, method names, reflect;
--   3. iface: one more WITH RECURSIVE over the "used in interface" edges (a live used-in-interface type marks
--      the gotypes it points at and its methods' types), expanding only live types not handled before;
--   4. the methods something can now call: their mtyp/ifn/tfn are the next round's seeds.
-- Steps 1 and 3 hold all of the BFS depth; the outer loop runs only as often as a method call adds code.
.cell 2rprep
-- plain edges, and the other kinds; gt: defined go types; ie: the used-in-interface edges (from a defined gotype
-- to a defined gotype it points at, and to its methods' types)
CREATE OR REPLACE TEMP TABLE e0 AS SELECT DISTINCT src, e.dst FROM (SELECT id AS src, unnest(out) AS e FROM sym WHERE len(out) > 0) WHERE e.kind = 0 AND e.dst <> 0;
-- the plain edges split by target: ei into a symbol that has edges itself (the recursion walks these), el into a
-- leaf (added in one join after the recursion). A recursion's iteration scans its whole edge table, so the split
-- halves every BFS level's work (5.2 M distinct plain edges, 2.7 M inner).
CREATE OR REPLACE TEMP TABLE srcs AS SELECT DISTINCT src AS id FROM e0;
CREATE OR REPLACE TEMP TABLE ei AS FROM e0 SEMI JOIN srcs ON srcs.id = e0.dst;
CREATE OR REPLACE TEMP TABLE el AS FROM e0 ANTI JOIN srcs ON srcs.id = e0.dst;
CREATE OR REPLACE TEMP TABLE eo AS SELECT src, e.kind, e.dst FROM (SELECT id AS src, unnest(out) AS e FROM sym WHERE len(out) > 0) WHERE e.kind <> 0;
CREATE OR REPLACE TEMP TABLE gt AS SELECT id FROM sym WHERE fl & 9 = 9;
CREATE OR REPLACE TEMP TABLE rf AS SELECT id FROM sym WHERE fl & 4 <> 0;
CREATE OR REPLACE TEMP TABLE meth AS SELECT id AS typ, unnest(meths, recursive := true) FROM sym SEMI JOIN gt USING (id) WHERE len(meths) > 0;
CREATE OR REPLACE TEMP TABLE ie AS
SELECT DISTINCT src, dst FROM (
  SELECT src, dst FROM e0 SEMI JOIN gt g1 ON g1.id = e0.src SEMI JOIN gt g2 ON g2.id = e0.dst
  UNION ALL SELECT typ, mtyp FROM meth WHERE mtyp <> 0);
.nocell
.cell 2rec
CREATE OR REPLACE TEMP TABLE live (id UINTEGER);
CREATE OR REPLACE TEMP TABLE iface AS SELECT id FROM sym WHERE fl & 16 <> 0;
CREATE OR REPLACE TEMP TABLE uii (id UINTEGER);            -- live used-in-interface types already expanded
CREATE OR REPLACE TEMP TABLE sigs (name UINTEGER, typ UINTEGER);  -- (name, type) of called interface methods
CREATE OR REPLACE TEMP TABLE named (n UINTEGER);
CREATE OR REPLACE TEMP TABLE refl (b INTEGER);
CREATE OR REPLACE TEMP TABLE cand AS SELECT * FROM meth LIMIT 0;   -- methods of used-in-interface types
CREATE OR REPLACE TEMP TABLE seed AS SELECT DISTINCT id FROM root WHERE id <> 0;
-- ew: the plain edges into symbols not live yet (pruned every round, so a recursion never needs an anti join
-- against the growing live set: each of its iterations would rebuild that hash table)
CREATE OR REPLACE TEMP TABLE ew AS FROM ei;
CREATE OR REPLACE TEMP TABLE iel AS FROM ie LIMIT 0;
.repeat
-- 1. the plain closure of the seeds, minus what was live before
CREATE OR REPLACE TEMP TABLE nl AS
WITH RECURSIVE r(id) AS (
  FROM seed
  UNION
  SELECT ew.dst FROM r JOIN ew ON ew.src = r.id)
SELECT id FROM r UNION SELECT el.dst FROM el SEMI JOIN r ON r.id = el.src EXCEPT FROM live;
INSERT INTO live FROM nl;
CREATE OR REPLACE TEMP TABLE ew AS FROM ew ANTI JOIN nl ON nl.id = ew.dst;
-- 2. what the new live code names
CREATE OR REPLACE TEMP TABLE o AS SELECT eo.kind, eo.dst FROM eo SEMI JOIN nl ON nl.id = eo.src;
INSERT INTO sigs SELECT DISTINCT sig.name, sig.typ FROM o JOIN sig ON sig.x = o.dst WHERE o.kind = 2;
INSERT INTO named SELECT DISTINCT dst FROM o WHERE kind = 3;
INSERT INTO refl SELECT 1 WHERE NOT EXISTS (FROM refl) AND EXISTS (FROM nl SEMI JOIN rf USING (id));
-- 3. new iface marks (converted by new live code), then the used-in-interface flood from every live iface gotype
--    not expanded yet; a target is marked iface, and expanded in turn if it is a live gotype
INSERT INTO iface SELECT DISTINCT dst FROM o WHERE kind = 1 AND dst <> 0 EXCEPT FROM iface;
-- iel: the used-in-interface edges out of live types into types not expanded yet (kept up to date, as ew)
INSERT INTO iel FROM ie SEMI JOIN nl ON nl.id = ie.src ANTI JOIN uii ON uii.id = ie.dst;
CREATE OR REPLACE TEMP TABLE nu AS
WITH RECURSIVE f(id) AS (
  SELECT id FROM iface SEMI JOIN gt USING (id) SEMI JOIN live USING (id) ANTI JOIN uii USING (id)
  UNION
  SELECT iel.dst FROM f JOIN iel ON iel.src = f.id)
FROM f;
INSERT INTO iface SELECT id FROM nu EXCEPT FROM iface;
CREATE OR REPLACE TEMP TABLE nu AS SELECT id FROM nu SEMI JOIN gt USING (id) SEMI JOIN live USING (id);
INSERT INTO uii FROM nu;
DELETE FROM iel WHERE dst IN (FROM nu);
INSERT INTO cand SELECT meth.* FROM meth SEMI JOIN nu ON nu.id = meth.typ;
-- 4. the candidate methods something can now call: their entries are the next seeds
CREATE OR REPLACE TEMP TABLE call AS
SELECT c.* FROM cand c SEMI JOIN sigs ON sigs.name = c.name AND sigs.typ = c.mtyp
UNION SELECT c.* FROM cand c SEMI JOIN named ON named.n = c.name
UNION SELECT c.* FROM cand c WHERE c.exported <> 0 AND EXISTS (FROM refl);
DELETE FROM cand WHERE m IN (SELECT m FROM call);
CREATE OR REPLACE TEMP TABLE seed AS
SELECT DISTINCT id FROM (SELECT unnest([mtyp, ifn, tfn]) AS id FROM call) WHERE id <> 0;  -- a seed already live costs nothing: ew has no edges out of live symbols
.until SELECT count(*) FROM seed;
.nocell
.show SELECT count(*) FROM live;
.dump out/p2_live.u32 SELECT id FROM live ORDER BY id;
