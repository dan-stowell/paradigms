-- ATTACKS 1 setup (after p1.sql and p2prep.sql): the relocations a flood follows, and the edges that do not come
-- from relocations. Untimed in the C version (ref/flat/p2lazy.c); here its own cell, 2aux.
.cell 2aux
-- the sources: canonical definitions with something to follow, with their relocation lists
CREATE OR REPLACE TEMP TABLE rsrc AS
SELECT srel.id, obj AS o, rels FROM srel JOIN sym USING (id) JOIN canon USING (id)
WHERE sym.fl & 33 = 33 AND canon.canon = srel.id AND len(rels) > 0;
-- the relocations of the sources in table T (id, o, r, pkg, idx, ty) that the flood follows: not weak, not one of
-- the non-plain types, and not one of the two after a type-26 relocation
CREATE OR REPLACE MACRO followed(t) AS TABLE
SELECT id, o, r, pkg, idx FROM (
  SELECT *, lag(ty, 1) OVER w AS t1, lag(ty, 2) OVER w AS t2 FROM query_table(t) WINDOW w AS (PARTITION BY id ORDER BY r))
WHERE ty & 32768 = 0 AND ty NOT IN (22, 23, 24, 25, 26, 102) AND coalesce(t1, 0) <> 26 AND coalesce(t2, 0) <> 26;
CREATE OR REPLACE TEMP VIEW rsrc_all AS SELECT id, o, unnest(rels, recursive := true) FROM rsrc;
-- the edges that are not relocations (funcdata aux edges, kinds 1-3): in each source's row, the kind-0 entries
-- after the first n, n = its followed relocations with a target (the extractor writes relocation edges first)
CREATE OR REPLACE TEMP TABLE adjx AS
WITH np AS (SELECT id AS src, count(*) AS n FROM followed('rsrc_all') JOIN reltarget USING (r) WHERE tgt <> 0 GROUP BY id),
e AS (SELECT id AS src, unnest(out, recursive := true), generate_subscripts(out, 1) AS pos FROM sym WHERE len(out) > 0),
k AS (SELECT *, row_number() OVER (PARTITION BY src, kind = 0 ORDER BY pos) AS nk FROM e)
SELECT src, list({'dst': dst, 'kind': kind} ORDER BY pos) AS out
FROM k LEFT JOIN np USING (src) WHERE kind <> 0 OR nk > coalesce(n, 0) GROUP BY src;
.nocell
