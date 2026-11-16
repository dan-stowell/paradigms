-- ATTACKS 1, eager: every followed relocation decoded up front (1b's reltarget), joined into one adjacency list per
-- source with the other edges (adjx), then the same flood (p2loop.sql)
.cell 2eager
CREATE OR REPLACE TEMP TABLE adj AS
SELECT src, list({'dst': dst, 'kind': kind}) AS out FROM (
  SELECT id AS src, tgt AS dst, 0::UTINYINT AS kind FROM followed('rsrc_all') JOIN reltarget USING (r) WHERE tgt <> 0
  UNION ALL
  SELECT src, unnest(out, recursive := true) FROM adjx)
GROUP BY src;
CREATE OR REPLACE TEMP TABLE dl (id UINTEGER);
CREATE OR REPLACE TEMP TABLE du (id UINTEGER);
CREATE OR REPLACE TEMP VIEW out_dl AS SELECT unnest(out, recursive := true) FROM adj JOIN dl ON adj.src = dl.id;
CREATE OR REPLACE TEMP VIEW out_du AS SELECT unnest(out, recursive := true) FROM adj JOIN du ON adj.src = du.id;
.nocell
