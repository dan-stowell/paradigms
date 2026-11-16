-- Problem 2, prep: the relations the loop joins on. adj: a symbol's out-edges (a list, unnested only when the
-- symbol becomes live); meth: a type's methods; plain: the plain edges (the gotype rule needs them by source).
.cell 2prep
CREATE OR REPLACE TEMP TABLE adj AS SELECT id AS src, out FROM sym WHERE len(out) > 0;
CREATE OR REPLACE TEMP TABLE meth AS SELECT id AS typ, unnest(meths, recursive := true) FROM sym WHERE len(meths) > 0;
.nocell
