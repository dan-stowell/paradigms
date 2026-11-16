-- load for problem 3a: the placed symbols, the buckets, the type strings (Arrow views, then CREATE TABLE)
.table placed p3_placed i=# id=p3_sym.u32 b=p3_bucket.u8 cl=p3_class.u8 size=p3_size.u32 unit=p3_unit.u32 tl=p3_tlstr.u32
.table bucket 16 b=# start=p3_bucket_start.u32 minalign=p3_bucket_minalign.u32
.table tlstr * str=p3_str.off.u64 s=#
