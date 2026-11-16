-- load for problem 2: the flat files as tables (Arrow views over the mmapped columns, then CREATE TABLE)
.table sym nslots+1 id=# fl=p2_flags.u8 e0=p2_edge_off.u32 e1=p2_edge_off.u32@1 m0=p2_meth_off.u32 m1=p2_meth_off.u32@1
.table edge0 * dst=p2_edge_dst.u32 kind=p2_edge_kind.u8 e=#
.table meth0 * name=p2_meth_name.u32 m=# mtyp=p2_meth_mtyp.u32 ifn=p2_meth_ifn.u32 tfn=p2_meth_tfn.u32 exported=p2_meth_exported.u8
.table sig * name=p2_sig_name.u32 x=# typ=p2_sig_type.u32
.table root * id=p2_roots.u32
