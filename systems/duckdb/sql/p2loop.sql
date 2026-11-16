-- Problem 2, reach, as an iterated frontier (semi-naive evaluation driven from outside: dk's .repeat).
-- Each round joins only what is new: dl, the symbols that became live last round, and du, the live types that
-- became "used in interface" (iface + gotype + def) last round. The rules are ref/flat/README.md's.
.cell 2loop
CREATE OR REPLACE TEMP TABLE live AS SELECT DISTINCT id FROM root WHERE id <> 0;
CREATE OR REPLACE TEMP TABLE iface AS SELECT id FROM sym WHERE fl & 16 <> 0;
CREATE OR REPLACE TEMP TABLE uii (id UINTEGER);           -- types already handled as used-in-interface
CREATE OR REPLACE TEMP TABLE sigs (x UINTEGER);           -- interface methods some live code calls
CREATE OR REPLACE TEMP TABLE named (n UINTEGER);          -- method names called by name
CREATE OR REPLACE TEMP TABLE refl (b INTEGER);            -- one row once reflect's Method is live
CREATE OR REPLACE TEMP TABLE cand AS SELECT * FROM meth LIMIT 0;  -- methods of used-in-interface types
CREATE OR REPLACE TEMP TABLE called (m UINTEGER);
CREATE OR REPLACE TEMP TABLE dl AS FROM live;
CREATE OR REPLACE TEMP TABLE du AS
SELECT id FROM dl JOIN sym USING (id) SEMI JOIN iface USING (id) WHERE fl & 9 = 9;
INSERT INTO uii FROM du;
.repeat
-- what the new live symbols name
CREATE OR REPLACE TEMP TABLE out_l AS SELECT kind, dst FROM edge JOIN dl ON edge.src = dl.id;
INSERT INTO sigs SELECT DISTINCT dst FROM out_l ANTI JOIN sigs ON sigs.x = out_l.dst WHERE kind = 2;
INSERT INTO named SELECT DISTINCT dst FROM out_l ANTI JOIN named ON named.n = out_l.dst WHERE kind = 3;
INSERT INTO refl SELECT 1 WHERE NOT EXISTS (FROM refl) AND EXISTS (SELECT 1 FROM dl JOIN sym USING (id) WHERE fl & 4 <> 0);
-- the new used-in-interface types: their methods become candidates
INSERT INTO cand SELECT meth.* FROM meth JOIN du ON meth.typ = du.id;
-- new iface marks: converted to an interface by live code, a gotype a used-in-interface type points at, a method's type
CREATE OR REPLACE TEMP TABLE ni AS
SELECT DISTINCT id FROM (
  SELECT dst AS id FROM out_l WHERE kind = 1
  UNION ALL
  SELECT plain.dst FROM plain JOIN du ON plain.src = du.id JOIN sym t ON t.id = plain.dst WHERE t.fl & 9 = 9
  UNION ALL
  SELECT mtyp FROM meth JOIN du ON meth.typ = du.id)
ANTI JOIN iface USING (id) WHERE id <> 0;
INSERT INTO iface FROM ni;
-- candidate methods something can now call
CREATE OR REPLACE TEMP TABLE call AS
SELECT c.* FROM cand c ANTI JOIN called USING (m)
LEFT JOIN (SELECT DISTINCT sig.name, sig.typ FROM sigs JOIN sig USING (x)) ls ON ls.name = c.name AND ls.typ = c.mtyp
WHERE ls.name IS NOT NULL OR c.name IN (SELECT n FROM named) OR (c.exported <> 0 AND EXISTS (FROM refl));
INSERT INTO called SELECT m FROM call;
-- new live symbols: plain targets of new live code, and what called methods keep
CREATE OR REPLACE TEMP TABLE nl AS
SELECT DISTINCT id FROM (SELECT dst AS id FROM out_l WHERE kind = 0 UNION ALL SELECT unnest([mtyp, ifn, tfn]) FROM call)
ANTI JOIN live USING (id) WHERE id <> 0;
INSERT INTO live FROM nl;
-- next round's frontiers
CREATE OR REPLACE TEMP TABLE du AS
SELECT DISTINCT id FROM (FROM nl UNION ALL FROM ni) JOIN sym USING (id)
SEMI JOIN live USING (id) SEMI JOIN iface USING (id) ANTI JOIN uii USING (id) WHERE fl & 9 = 9;
INSERT INTO uii FROM du;
CREATE OR REPLACE TEMP TABLE dl AS FROM nl;
.until SELECT (SELECT count(*) FROM dl) + (SELECT count(*) FROM du);
.nocell
.show SELECT count(*) FROM live;
.dump out/p2_live.u32 SELECT id FROM live ORDER BY id;
