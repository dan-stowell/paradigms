-- load for problem 1: the flat files as tables (Arrow views over the mmapped columns, then CREATE TABLE).
-- A (name, ABI) key is one 128-bit value: DECIMAL(38,0) is how DuckDB reads a 16-byte Arrow decimal, and it is
-- stored as a HUGEINT, so equality and hashing are on the raw bits. The hash keys are split into two UBIGINTs
-- (1c orders kind 2 by k2, the size).
-- A CSR (slot_rel, obj_pkg_off) is an Arrow list: srel.rels is each slot's relocations, obj.bases each object's
-- package table.
.table slot nslots+1 id=# kind=p1_kind.u8 flags=p1_flags.u8 rank=p1_rank.u32 key=p1_namekey.u64x2 hk1=p1_hashkey.u64x2:lo hk2=p1_hashkey.u64x2:hi mark=p1c_mark.u8
.table want * key=p1_want.u64x2
.table builtin nbuiltin i=# key=p1_builtin.u64x2
.table srel nslots+1 id=# obj=slot_obj.u32 rels=slot_rel.u32[r=#,pkg=rel_pkg.u32,idx=rel_idx.u32,ty=rel_type.u16]
.table obj nobjs o=# bases=obj_pkg_off.u32[base=obj_pkg.u32]
