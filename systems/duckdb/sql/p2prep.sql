-- Problem 2, prep: the CSR rows as relations. An edge's source is the symbol whose row range holds it: an
-- equi-join on 4096-edge buckets (each source listed under every bucket its range touches), then the range test.
.cell 2prep
CREATE OR REPLACE TEMP TABLE srcb AS
SELECT unnest(range(e0 >> 12, ((e1 - 1) >> 12) + 1)) AS b, id, e0, e1 FROM sym WHERE e1 > e0;
CREATE OR REPLACE TEMP TABLE edge AS
SELECT id AS src, dst, kind FROM edge0 JOIN srcb ON srcb.b = edge0.e >> 12 AND e >= e0 AND e < e1;
-- a type's methods
CREATE OR REPLACE TEMP TABLE meth AS
SELECT id AS typ, meth0.* FROM (SELECT id, unnest(range(m0, m1)) AS m FROM sym WHERE m1 > m0) JOIN meth0 USING (m);
CREATE OR REPLACE TEMP TABLE plain AS SELECT src, dst FROM edge WHERE kind = 0;
.nocell
