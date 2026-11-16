// p2rules.goose: problem 2's rules for a worker that owns a range of ids (shared by p2par and p2async).
import flat;

let fDef: u8 = 1;
let fReflect: u8 = 4;
let fGoType: u8 = 8;
let fIface: u8 = 16;
// op bits on a message: ids are below 2^29
let opMark: u32 = 1 << 31;
let opIface: u32 = 1 << 30;     // a useiface edge, or a method's type: iface(t)
let opIfaceGT: u32 = 1 << 29;   // a plain edge from a live iface gotype: iface(t) if t is a gotype definition
let idMask: u32 = (1 << 29) - 1;

// ---- one worker's state: its own globals (a worker's globals are its own copies)
var lo: i64 = 0;                // owned ids are [lo, lo + live.len)
var bounds: i64[>..] = [];      // the owners' first ids, W + 1 entries
var live: u8[>..] = [];
type L = u32 in live;           // a local id
var flags: u8[>..] = [];
var iface: u8[>..] = [];
var idone: u8[>..] = [];
var edst: u32[>..] = [];        // global ids
type EEnd = u32 upto edst;
var eoff: EEnd[>..] = [];
var ekind: u8[>..] = [];
var mname: u32[>..] = [];
type M = u32 in mname;
type MEnd = u32 upto mname;
var moff: MEnd[>..] = [];
var mtyp: u32[>..] = [];
var mifn: u32[>..] = [];
var mtfn: u32[>..] = [];
var mexp: u8[>..] = [];
var sig_name: u32[>..] = [];
var sig_type: u32[>..] = [];
var sig_seen: u8[>..] = [];
var named: u8[>..] = [];
var sigtab: u64[>..] = [];
var reflect = false;
var facts_new = false;      // a fact this worker found since its methods were last checked
var work: L[>..<] = [];
var out: u32[>..<] = [];        // messages to other owners, op | id
var new_sigs: u32[>..<] = [];
var new_named: u32[>..<] = [];
var markable: M[>..<] = [];

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

// ---- the local rules; `op` says which of them a message or an edge asks for
fn apply(t: u32, op: u32) {
    let d = t - lo;
    guard d >= 0 && d < live.len else { out.push(op | t); return; }
    let c = d as L;
    if op & opIfaceGT != 0 && flags[c] & fGoType != 0 && flags[c] & fDef != 0 && iface[c] == 0 {
        iface[c] = 1;
        if live[c] != 0 { work.push(c); }
    }
    if op & opIface != 0 && t != 0 && iface[c] == 0 {
        iface[c] = 1;
        if live[c] != 0 { work.push(c); }
    }
    if op & opMark != 0 && t != 0 && live[c] == 0 {
        live[c] = 1;
        work.push(c);
    }
}

fn add_sig(s: u32) {
    guard s < sig_seen.len && sig_seen[s] == 0 else { return; }
    sig_seen[s] = 1;
    sig_add((sig_name[s] as u64) << 32 | sig_type[s]);
}

// drains the worklist, or stops early once `limit` messages for other owners are waiting
fn drain(limit: i64) {
    while work.len > 0 && out.len < limit {
        let c = work.pop();
        let f = flags[c];
        if f & fDef == 0 { continue; }
        if f & fReflect != 0 && !reflect { reflect = true; facts_new = true; }
        let uii = f & fGoType != 0 && iface[c] != 0;
        if uii {
            if idone[c] != 0 { continue; }
            idone[c] = 1;
        }
        let plain = if uii { opMark | opIfaceGT } else { opMark };
        for e in eoff[c]..eoff[c as i64 + 1] {
            let t = edst[e];
            match ekind[e] {
                0 => apply(t, plain),
                1 => apply(t, opIface),
                2 => { if t < sig_seen.len && sig_seen[t] == 0 { add_sig(t); new_sigs.push(t); facts_new = true; } },
                3 => { if t < named.len && named[t] == 0 { named[t] = 1; new_named.push(t); facts_new = true; } },
                _ => {},
            }
        }
        if uii {
            for m in moff[c]..moff[c as i64 + 1] {
                apply(mtyp[m], opIface);
                markable.push(m);
            }
        }
    }
}

// the markable methods something can now call; from: the first one not checked against the current facts
fn methods(from: i64) -> bool {
    var keep = from;
    var any = false;
    for i in from..markable.len {
        let m = markable[i];
        let call = (reflect && mexp[m] != 0) || (mname[m] < named.len && named[mname[m]] != 0)
            || sig_has((mname[m] as u64) << 32 | mtyp[m]);
        if call {
            apply(mtyp[m], opMark);
            apply(mifn[m], opMark);
            apply(mtfn[m], opMark);
            any = true;
        } else {
            markable[keep] = markable[i];
            keep++;
        }
    }
    markable.resize(keep);
    return any;
}

// ---- loading a worker's part: each file is read whole into scratch (freed at the scope's end) and the
// owned range copied out
fn load_part(k: i64, W: i64) {
    let nids = meta("nids");
    let nsig = meta("p2_sigs");
    var hi = 0;
    {
        var off: u32[>..] = [];
        load("p2_edge_off.u32", off);
        let ne = off[off.len - 1] as i64;
        bounds.push(0);
        var id = 0;
        for j in 1..W {
            while id < off.len - 1 && off[id] * W < ne * j { id++; }
            bounds.push(id);
        }
        bounds.push(nids);
        lo = bounds[k];
        hi = bounds[k + 1];
        let nsl = off.len - 2;
        let elo = if lo <= nsl { off[lo] } else { off[nsl + 1] };
        let ehi = if hi <= nsl { off[hi] } else { off[nsl + 1] };
        var all: u32[>..] = [];
        load("p2_edge_dst.u32", all);
        for e in elo..ehi { edst.push(all[e]); }
        var kinds: u8[>..] = [];
        load("p2_edge_kind.u8", kinds);
        for e in elo..ehi { ekind.push(kinds[e]); }
        for g in lo..hi + 1 {
            let x = if g <= nsl { off[g] } else { off[nsl + 1] };
            eoff.push((x - elo) as EEnd);
        }
    }
    {
        var off: u32[>..] = [];
        load("p2_meth_off.u32", off);
        let nsl = off.len - 2;
        let mlo = if lo <= nsl { off[lo] } else { off[nsl + 1] };
        let mhi = if hi <= nsl { off[hi] } else { off[nsl + 1] };
        var a: u32[>..] = [];
        load("p2_meth_name.u32", a);
        for m in mlo..mhi { mname.push(a[m]); }
        a.clear();
        load("p2_meth_mtyp.u32", a);
        for m in mlo..mhi { mtyp.push(a[m]); }
        a.clear();
        load("p2_meth_ifn.u32", a);
        for m in mlo..mhi { mifn.push(a[m]); }
        a.clear();
        load("p2_meth_tfn.u32", a);
        for m in mlo..mhi { mtfn.push(a[m]); }
        var b: u8[>..] = [];
        load("p2_meth_exported.u8", b);
        for m in mlo..mhi { mexp.push(b[m]); }
        for g in lo..hi + 1 {
            let x = if g <= nsl { off[g] } else { off[nsl + 1] };
            moff.push((x - mlo) as MEnd);
        }
    }
    {
        var f: u8[>..] = [];
        load("p2_flags.u8", f);
        for g in lo..hi {
            let x = if g < f.len { f[g] } else { 0 };
            flags.push(x);
            live.push(0);
            idone.push(0);
            iface.push(if x & fIface != 0 { 1 } else { 0 });
        }
    }
    load("p2_sig_name.u32", sig_name);
    load("p2_sig_type.u32", sig_type);
    sig_seen.resize(nsig, 0);
    named.resize(meta("p2_strings") + 1, 0);
    var hs = 1;
    while hs < 4 * nsig + 64 { hs <<= 1; }
    sigtab.resize(hs, 0);
}

fn owner(t: u32) -> i64 {
    var j = 0;
    while j + 1 < bounds.len - 1 && bounds[j + 1] <= t { j++; }
    return j;
}
