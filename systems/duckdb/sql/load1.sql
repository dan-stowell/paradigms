-- load for problem 1: the flat files as tables (Arrow views over the mmapped columns, then CREATE TABLE).
-- A (name, ABI) key is one 128-bit value: DECIMAL(38,0) is how DuckDB reads a 16-byte Arrow decimal, and it is
-- stored as a HUGEINT, so equality and hashing are on the raw bits. The hash keys are split into two UBIGINTs
-- (1c orders kind 2 by k2, the size).
.table slot nslots+1 id=# kind=p1_kind.u8 flags=p1_flags.u8 rank=p1_rank.u32 key=p1_namekey.u64x2 obj=slot_obj.u32 rel0=slot_rel.u32 rel1=slot_rel.u32@1 hk1=p1_hashkey.u64x2:lo hk2=p1_hashkey.u64x2:hi mark=p1c_mark.u8
.table want * key=p1_want.u64x2
.table builtin nbuiltin i=# key=p1_builtin.u64x2
.table rel nrel r=# pkg=rel_pkg.u32 idx=rel_idx.u32
.table objpkg_off nobjs o=# p0=obj_pkg_off.u32 p1=obj_pkg_off.u32@1
.table objpkg_base * base=obj_pkg.u32 j=#
