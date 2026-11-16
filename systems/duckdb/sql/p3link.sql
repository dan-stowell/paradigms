-- 3a -> 3b as one pipeline: every id's address from this run's own layout (p3a.sql's addr) through the final
-- canonical ids (p1.sql's canon2, 1a and 1c), plus the symbols the linker places itself (p3_fixed_*, a given input)
.table fixed * id=p3_fixed_id.u32 addr=p3_fixed_addr.u32
.cell 3link
CREATE OR REPLACE TEMP TABLE addr3 AS SELECT i, addr FROM addr;
CREATE OR REPLACE TEMP TABLE addrall AS
WITH placedaddr AS (SELECT id, addr FROM addr UNION ALL SELECT id, addr FROM fixed)
SELECT canon2.id, p.addr AS a FROM canon2 JOIN placedaddr p ON p.id = canon2.canon
UNION ALL
SELECT id, addr FROM placedaddr WHERE id > getvariable('nslots');
.nocell
