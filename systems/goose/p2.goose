// p2.goose: problem 2 (reach), one thread: the live set from the roots, a worklist flood over CSR edges plus
// the method rules (ref/flat/README.md; ref/flat/p2.c is the plain C form). Checks itself against
// oracle/p2_live.u32.   p2 /home/exedev/paradigms/data/minio
import flat;

let t_start = clock();

let fDef: u8 = 1;
let fReflect: u8 = 4;
let fGoType: u8 = 8;
let fIface: u8 = 16;

// Every symbol id is an index of `live` (nids entries); the other per-symbol arrays are padded to at least as
// long, so a Sym indexes all of them without a check.
var live: u8[>..] = zeros(meta("nids"));
type Sym = u32 in live;
var flags: u8[>..] = load_u8("p2_flags.u8");
var iface: u8[>..] = [];
var idone: u8[>..] = [];       // its iface-gotype edges and methods were followed
var edge_dst: Sym[>..] = load_ids<Sym>("p2_edge_dst.u32");
type EdgeEnd = u32 upto edge_dst;
var edge_off: EdgeEnd[>..] = load_ids<EdgeEnd>("p2_edge_off.u32");
var edge_kind: u8[>..] = load_u8("p2_edge_kind.u8");
var named: u8[>..] = zeros(meta("p2_strings") + 1);       // per method-name string: some live code calls a method by that name
type Str = u32 in named;
var meth_name: Str[>..] = load_ids<Str>("p2_meth_name.u32");
type Meth = u32 in meth_name;
type MethEnd = u32 upto meth_name;
var meth_off: MethEnd[>..] = load_ids<MethEnd>("p2_meth_off.u32");
var meth_mtyp: Sym[>..] = load_ids<Sym>("p2_meth_mtyp.u32");
var meth_ifn: Sym[>..] = load_ids<Sym>("p2_meth_ifn.u32");
var meth_tfn: Sym[>..] = load_ids<Sym>("p2_meth_tfn.u32");
var meth_exp: u8[>..] = load_u8("p2_meth_exported.u8");
var sig_seen: u8[>..] = zeros(meta("p2_sigs"));
type Sig = u32 in sig_seen;
var sig_name: u32[>..] = load_u32("p2_sig_name.u32");
var sig_type: u32[>..] = load_u32("p2_sig_type.u32");
var sigtab: u64[>..] = [];     // open addressing over (name, type) of the live signatures; 0 empty
var roots: Sym[>..] = load_ids<Sym>("p2_roots.u32");

checkfree inline fn mark(c: Sym, work: Sym[>..<]&) {
    if c != 0 && live[c] == 0 {
        live[c] = 1;
        work.push(c);
    }
}

checkfree inline fn set_iface(c: Sym, work: Sym[>..<]&) {
    if c != 0 && iface[c] == 0 {
        iface[c] = 1;
        if live[c] != 0 { work.push(c); }
    }
}

checkfree fn mix64(x: u64) -> u64 {
    var h = x;
    h ^= h >> 30;
    h *= 0xBF58476D1CE4E5B9;
    h ^= h >> 27;
    h *= 0x94D049BB133111EB;
    return h ^ (h >> 31);
}

checkfree fn sig_add(k: u64) {
    var h = (mix64(k) >> 1) as! i64;
    loop {
        let i = h % sigtab.len;      // % is Euclidean: always an index
        if sigtab[i] == 0 { sigtab[i] = k + 1; return; }
        if sigtab[i] == k + 1 { return; }
        h++;
    }
}

checkfree fn sig_has(k: u64) -> bool {
    var h = (mix64(k) >> 1) as! i64;
    loop {
        let i = h % sigtab.len;
        if sigtab[i] == 0 { return false; }
        if sigtab[i] == k + 1 { return true; }
        h++;
    }
}

// drains the worklist: plain edges mark, useiface edges set iface, and a live gotype that is converted to an
// interface follows its plain gotype edges (iface) and offers its methods (markable)
checkfree fn drain(work: Sym[>..<]&, markable: Meth[>..<]&) -> bool {
    assert(flags.len >= live.len && iface.len >= live.len && idone.len >= live.len);
    assert(edge_off.len > live.len && meth_off.len > live.len && edge_kind.len >= edge_dst.len);
    assert(meth_mtyp.len >= meth_name.len && sig_name.len >= sig_seen.len && sig_type.len >= sig_seen.len);
    var reflect = false;
    while work.len > 0 {
        let c = work.pop();
        let f = flags[c];
        if f & fDef == 0 { continue; }
        if f & fReflect != 0 { reflect = true; }
        let uii = f & fGoType != 0 && iface[c] != 0;
        if uii {
            if idone[c] != 0 { continue; }
            idone[c] = 1;
        }
        for e in edge_off[c]..edge_off[c as i64 + 1] {
            let t = edge_dst[e];
            match edge_kind[e] {
                0 => {
                    if uii && flags[t] & fGoType != 0 && flags[t] & fDef != 0 && iface[t] == 0 {
                        iface[t] = 1;
                        if live[t] != 0 { work.push(t); }
                    }
                    mark(t, work);
                },
                1 => set_iface(t, work),
                2 => {
                    if t < sig_seen.len {
                        let s = t as Sig;
                        if sig_seen[s] == 0 {
                            sig_seen[s] = 1;
                            sig_add((sig_name[s] as u64) << 32 | sig_type[s]);
                        }
                    }
                },
                3 => { if t < named.len { named[t] = 1; } },
                _ => {},
            }
        }
        if uii {
            for m in meth_off[c]..meth_off[c as i64 + 1] {
                set_iface(meth_mtyp[m], work);
                markable.push(m);
            }
        }
    }
    return reflect;
}

fn flood() -> i64 {
    var work: Sym[>..<] = [];
    var markable: Meth[>..<] = [];
    var reflect = false;
    var rounds = 0;
    for r in roots { mark(r, work); }
    loop {
        rounds++;
        if drain(work, markable) { reflect = true; }
        // the methods something can now call
        var keep = 0;
        for i in markable.len {
            let m = markable[i];
            let call = (reflect && meth_exp[m] != 0) || named[meth_name[m]] != 0
                || sig_has((meth_name[m] as u64) << 32 | meth_mtyp[m]);
            if call {
                mark(meth_mtyp[m], work);
                mark(meth_ifn[m], work);
                mark(meth_tfn[m], work);
            } else {
                markable[keep] = m;
                keep++;
            }
        }
        markable.resize(keep);
        if work.len == 0 { break; }
    }
    return rounds;
}

fn main() {
    let t1 = clock();
    let nids = live.len;
    while flags.len < nids { flags.push(0); }
    while edge_off.len <= nids { edge_off.push(edge_dst.len as EdgeEnd); }
    while meth_off.len <= nids { meth_off.push(meth_name.len as MethEnd); }
    let nsig = sig_seen.len;
    iface.resize(nids, 0);
    idone.resize(nids, 0);
    for g in nids { if flags[g] & fIface != 0 { iface[g] = 1; } }
    var hs = 1;
    while hs < 4 * nsig + 64 { hs <<= 1; }
    sigtab.resize(hs, 0);
    let rounds = flood();
    let t2 = clock();

    var list: u32[>..] = [];
    for g in nids { if live[g] != 0 { list.push(g as u32); } }
    var want: u32[>..] = [];
    load("oracle/p2_live.u32", want);
    let t3 = clock();
    var bad = 0;
    if list.len != want.len { bad = 1; }
    else { for i in list.len { if list[i] != want[i] { bad++; } } }
    var note: u8[>..] = [];
    report(note, "load", t_start, t1);
    report(note, "flood", t1, t2);
    report(note, "list", t2, t3);
    print("p2 goose (1 thread): ", note, list.len, " live, ", rounds, " rounds; ", bad, " differ from oracle/p2_live.u32");
    print(if bad == 0 { "P2 OK" } else { "P2 FAIL" });
    if bad != 0 { exit(1); }
}
