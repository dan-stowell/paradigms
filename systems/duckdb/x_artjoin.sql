-- ART index vs hash join for a frontier-sized build side (DuckDB P2 attempt 3 evidence): duckdb < x_artjoin.sql
.timer on
SET threads = 8;
SET dynamic_or_filter_threshold = 100000;
SET index_scan_max_count = 1000000;
CREATE TABLE e AS SELECT (random()*5000000)::UINTEGER src, (random()*5000000)::UINTEGER dst FROM range(5000000);
CREATE TABLE ex AS FROM e;
CREATE INDEX exi ON ex(src);
CREATE TABLE f AS SELECT (random()*5000000)::UINTEGER id FROM range(40);
CREATE TABLE f2 AS SELECT (random()*5000000)::UINTEGER id FROM range(2000);
SELECT count(*) FROM f JOIN e ON e.src = f.id;
SELECT count(*) FROM f JOIN ex ON ex.src = f.id;
SELECT count(*) FROM f2 JOIN e ON e.src = f2.id;
SELECT count(*) FROM f2 JOIN ex ON ex.src = f2.id;
SELECT count(*) FROM e WHERE src IN (1,2,3,4,5,6,7,8,9,10);
SELECT count(*) FROM ex WHERE src IN (1,2,3,4,5,6,7,8,9,10);
SELECT count(*) FROM e;
