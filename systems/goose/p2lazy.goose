// p2lazy.goose: problem 2 (reach), one thread, with no relocation graph: when a canonical definition becomes live
// its relocations are decoded from the relocation columns right there ((package, index) -> slot through its object's
// package bases -> canonical id), the shape hlink floods raw objects with (ATTACKS.md 1; ref/flat/p2lazy.c is the C
// form). Funcdata aux edges, kinds 1-3 and the method tables still come from the p2_* files: at load, each row's
// relocation edges are cut out of it (its first n kind-0 edges, n = its decodable plain relocations), the rest kept.
// The 1a canon comes from oracle/p1_canon.u32. Checks itself against oracle/p2_live.u32.
//   p2lazy /home/exedev/paradigms/data/minio
import flat;

let t_start = clock();

let fDef: u8 = 1;
let fReflect: u8 = 4;
let fGoType: u8 = 8;
let fIface: u8 = 16;
let fFollow: u8 = 32;

var live: u8[>..] = zeros(meta("nids"));
type Sym = u32 in live;
var flags: u8[>..] = load_u8("p2_flags.u8");
var iface: u8[>..] = [];
var idone: u8[>..] = [];
// the relocation columns (README: slot_rel, rel_pkg, rel_idx, rel_type, slot_obj, obj_pkg_off, obj_pkg)
var rel_pkg: u32[>..] = load_u32("rel_pkg.u32");
type RelEnd = u32 upto rel_pkg;
var slot_rel: RelEnd[>..] = load_ids<RelEnd>("slot_rel.u32");
var rel_idx: u32[>..] = load_u32("rel_idx.u32");
var rel_type: u16[>..] = load_u16("rel_type.u16");
var slot_obj: u32[>..] = load_u32("slot_obj.u32");
var obj_pkg_off: u32[>..] = load_u32("obj_pkg_off.u32");
var obj_pkg: u32[>..] = load_u32("obj_pkg.u32");
var canon: Sym[>..] = load_ids<Sym>("oracle/p1_canon.u32");
var builtin: u32[>..] = load_u32("oracle/p1_builtin.u32");
// what is left of the p2 edges: aux (kind 0 past the relocations), kinds 1-3
var edge_dst: Sym[>..] = [];
type EdgeEnd = u32 upto edge_dst;
var edge_off: EdgeEnd[>..] = [];
var edge_kind: u8[>..] = [];
var named: u8[>..] = zeros(meta("p2_strings") + 1);
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
var sigtab: u64[>..] = [];
var roots: Sym[>..] = load_ids<Sym>("p2_roots.u32");

// a relocation the flood follows: not weak, not one of the types that name no dependency
fn plain(t: u16) -> bool { t & 32768 == 0 && (t < 22 || t > 26) && t != 102 }

// relocation r of slot g, decoded: its target's canonical id, 0 if none (1b's rule)
fn target(g: i64, r: i64) -> u32 {
    let o = slot_obj[g];
    let b0 = obj_pkg_off[o];
    let np = obj_pkg_off[o as i64 + 1] - b0 - 5;
    let p = rel_pkg[r];
    let sp = p - 2147483643;
    let t = if sp < 5 { np + sp } else { p };
    guard t < np + 5 else { return 0; }
    let base = obj_pkg[b0 + t];
    guard base != 0xFFFFFFFF else { return 0; }
    let x = if sp == 1 { if rel_idx[r] < builtin.len { builtin[rel_idx[r]] } else { 0 } } else { base + rel_idx[r] };
    return if x != 0 && x < canon.len { canon[x] } else { 0 };
}

// does the flood decode slot g's relocations (a canonical definition with something to follow)
fn decodes(g: i64) -> bool { g < canon.len && g < flags.len && flags[g] & fDef != 0 && flags[g] & fFollow != 0 && canon[g] == g }

// load-time: the p2 rows without their relocation edges
fn keep_aux() {
    var off: u32[>..] = [];
    load("p2_edge_off.u32", off);
    var dst: u32[>..] = [];
    load("p2_edge_dst.u32", dst);
    var kind: u8[>..] = [];
    load("p2_edge_kind.u8", kind);
    for g in live.len {
        edge_off.push(edge_dst.len as EdgeEnd);
        guard g + 1 < off.len else { continue; }
        var skip = 0;
        if decodes(g) {
            var r = slot_rel[g] as i64;
            while r < slot_rel[g + 1] {
                let ty = rel_type[r];
                if ty == 26 { r += 3; continue; }
                if plain(ty) && target(g, r) != 0 { skip++; }
                r++;
            }
        }
        for e in off[g]..off[g + 1] {
            if kind[e] == 0 && skip > 0 { skip--; continue; }
            guard dst[e] < live.len else { abort("p2_edge_dst: not an id"); }
            edge_dst.push(dst[e] as Sym);
            edge_kind.push(kind[e]);
        }
    }
    edge_off.push(edge_dst.len as EdgeEnd);
}

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

// a plain edge c -> t: mark t, and a live gotype converted to an interface makes its gotype targets so too
checkfree inline fn plain_edge(t: Sym, uii: bool, work: Sym[>..<]&) {
    if uii && flags[t] & fGoType != 0 && flags[t] & fDef != 0 && iface[t] == 0 {
        iface[t] = 1;
        if live[t] != 0 { work.push(t); }
    }
    mark(t, work);
}

checkfree fn sig_add(k: u64) {
    var h = (mix64(k) >> 1) as! i64;
    loop {
        let i = h % sigtab.len;
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

fn drain(work: Sym[>..<]&, markable: Meth[>..<]&) -> bool {
    assert(flags.len >= live.len && iface.len >= live.len && idone.len >= live.len);
    assert(edge_off.len > live.len && meth_off.len > live.len && edge_kind.len >= edge_dst.len);
    assert(rel_type.len >= rel_pkg.len && rel_idx.len >= rel_pkg.len);
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
        if decodes(c) {
            // the lazy part: this symbol's relocations, decoded now
            var skip = 0;     // type 26 hides the next two relocations
            for r in slot_rel[c]..slot_rel[c as i64 + 1] {
                if skip > 0 { skip--; continue; }
                let ty = rel_type[r];
                if ty == 26 { skip = 2; continue; }
                if plain(ty) {
                    let t = target(c, r);
                    if t != 0 { plain_edge(t as Sym, uii, work); }
                }
            }
        }
        for e in edge_off[c]..edge_off[c as i64 + 1] {
            let t = edge_dst[e];
            match edge_kind[e] {
                0 => plain_edge(t, uii, work),
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

// clears the flood's state, so the flood can be timed again on the same inputs
fn reset() {
    let nids = live.len;
    for g in nids { live[g] = 0; idone[g] = 0; iface[g] = if flags[g] & fIface != 0 { 1 } else { 0 }; }
    for i in sig_seen.len { sig_seen[i] = 0; }
    for i in named.len { named[i] = 0; }
    for i in sigtab.len { sigtab[i] = 0; }
}

fn main() {
    let nids = live.len;
    while flags.len < nids { flags.push(0); }
    while slot_rel.len <= nids { slot_rel.push(rel_pkg.len as RelEnd); }
    while meth_off.len <= nids { meth_off.push(meth_name.len as MethEnd); }
    keep_aux();
    iface.resize(nids, 0);
    idone.resize(nids, 0);
    var hs = 1;
    while hs < 4 * sig_seen.len + 64 { hs <<= 1; }
    sigtab.resize(hs, 0);
    let t1 = clock();
    // the flood, timed 5 times on the inputs in memory (the warm time is the best)
    var best = 1.0e9;
    var rounds = 0;
    var reps = 5;
    let a = args();
    if a.len > 2 { let n, ok = parse_int(a[2]); if ok { reps = n; } }
    for k in reps {
        reset();
        let ta = clock();
        rounds = flood();
        best = min(best, clock() - ta);
    }
    let t2 = clock();
    var list: u32[>..] = [];
    for g in nids { if live[g] != 0 { list.push(g as u32); } }
    var want: u32[>..] = [];
    load("oracle/p2_live.u32", want);
    var bad = 0;
    if list.len != want.len { bad = 1; }
    else { for i in list.len { if list[i] != want[i] { bad++; } } }
    var note: u8[>..] = [];
    report(note, "load+cut", t_start, t1);
    format(note, "lazy flood best of ", reps, " ");
    format_flt(note, best * 1000.0, 1);
    format(note, " ms  ");
    print("p2lazy goose (1 thread): ", note, edge_dst.len, " non-relocation edges kept, ", list.len, " live, ", rounds,
          " rounds; ", bad, " differ from oracle/p2_live.u32");
    print(if bad == 0 { "P2 OK" } else { "P2 FAIL" });
    if bad != 0 { exit(1); }
}
