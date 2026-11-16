-- ATTACKS 1, lazy: a symbol's relocations are decoded when it becomes live. out_dl / out_du unnest only the
-- frontier's relocation lists and map each (package index, symbol index) through the object's package table and
-- the canonical ids, as 1b does for all of them; then the same flood (p2loop.sql)
.cell 2lazy
CREATE OR REPLACE TEMP TABLE dl (id UINTEGER);
CREATE OR REPLACE TEMP TABLE du (id UINTEGER);
CREATE OR REPLACE MACRO decoded(t) AS TABLE
WITH target AS (
  SELECT id, CASE WHEN pb.base IS NULL THEN 0
                  WHEN f.pkg = 2147483644 THEN coalesce(bc.canon, 0) -- package index "builtin"
                  ELSE pb.base + idx END AS s
  FROM followed(t) f LEFT JOIN pkgbase pb USING (o, pkg)
                     LEFT JOIN builtin_canon bc ON bc.i = CASE WHEN f.pkg = 2147483644 THEN f.idx END)
SELECT target.id AS src, canon.canon AS dst FROM target JOIN canon ON canon.id = target.s WHERE canon.canon <> 0;
CREATE OR REPLACE TEMP VIEW rel_dl AS SELECT id, o, unnest(rels, recursive := true) FROM rsrc SEMI JOIN dl USING (id);
CREATE OR REPLACE TEMP VIEW rel_du AS SELECT id, o, unnest(rels, recursive := true) FROM rsrc SEMI JOIN du USING (id);
CREATE OR REPLACE TEMP VIEW out_dl AS
SELECT dst, 0::UTINYINT AS kind FROM decoded('rel_dl')
UNION ALL SELECT unnest(out, recursive := true) FROM adjx JOIN dl ON adjx.src = dl.id;
CREATE OR REPLACE TEMP VIEW out_du AS
SELECT dst, 0::UTINYINT AS kind FROM decoded('rel_du')
UNION ALL SELECT unnest(out, recursive := true) FROM adjx JOIN du ON adjx.src = du.id;
.nocell
