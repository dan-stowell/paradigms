-- Problem 2, prep: the relations the loop joins on. adj: a symbol's out-edges (a list, unnested only when the
-- symbol becomes live); meth: a type's methods; plain: the plain edges (the gotype rule needs them by source).
.cell 2prep
CREATE OR REPLACE TEMP TABLE adj AS SELECT id AS src, out FROM sym WHERE len(out) > 0;
CREATE OR REPLACE TEMP TABLE meth AS SELECT id AS typ, unnest(meths, recursive := true) FROM sym WHERE len(meths) > 0;
-- the edges of the frontiers, as views: re-bound to the current dl / du every time a statement reads them
CREATE OR REPLACE TEMP TABLE dl (id UINTEGER);
CREATE OR REPLACE TEMP TABLE du (id UINTEGER);
CREATE OR REPLACE TEMP VIEW out_dl AS SELECT unnest(out, recursive := true) FROM adj JOIN dl ON adj.src = dl.id;
CREATE OR REPLACE TEMP VIEW out_du AS SELECT unnest(out, recursive := true) FROM adj JOIN du ON adj.src = du.id;
.nocell
