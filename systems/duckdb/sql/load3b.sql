-- load for problem 3b: each placed symbol's bytes and its relocations (an Arrow list over p3_rel_start)
.table sym3b p3_placed i=# b=p3_bucket.u8 bytes=p3_data.off.u64 rels=p3_rel_start.u32[off=p3_rel_off.u32,siz=p3_rel_siz.u8,kind=p3_rel_kind.u8,add=p3_rel_add.i64,tgt=p3_rel_tgt.u32]
