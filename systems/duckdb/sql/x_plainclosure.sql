.cell plain-closure-recursive-cte
CREATE OR REPLACE TEMP TABLE pe AS SELECT src, e.dst FROM (SELECT src, unnest(out) AS e FROM adj) WHERE e.kind = 0;
CREATE OR REPLACE TEMP TABLE pl AS
WITH RECURSIVE r(id) AS (SELECT id FROM root WHERE id <> 0 UNION SELECT pe.dst FROM r JOIN pe ON pe.src = r.id)
SELECT id FROM r;
.nocell
.show SELECT count(*) FROM pl;
