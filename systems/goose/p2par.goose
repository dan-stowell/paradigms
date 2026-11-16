// p2par.goose: problem 2 (reach) on W workers that share no memory. Each worker owns a range of ids (cut so the
// ranges hold equal numbers of edges): their live/iface marks, their edges and their methods. A worker floods
// locally; an edge to an id another worker owns becomes a message (the id with op bits) in a per-owner inbox.
// The global facts (live signatures, names called by reflection, reflect seen) go to main, which merges them
// and broadcasts them with the go/stop decision of each superstep (bulk-synchronous rounds).
//   p2par /home/exedev/paradigms/data/minio [workers 1..8]
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

// ---- messages (flat values). Queues are per type, so a worker's inbox is a type of its own: Inbox<Tag>.
struct Inbox<T> { tag: T, ids: u32[] }
struct Go<T> { tag: T, done: bool, reflect: bool, sigs: u32[], named: u32[] }
struct Status { k: i64, sent: i64, local: i64, reflect: bool, sigs: u32[], named: u32[] }
struct Result { k: i64, live: u32[] }

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

fn drain() {
    while work.len > 0 {
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

// the inbox of worker j: one type per worker
fn send(j: i64, ids: u32[:]) {
    match j {
        0 => qput(Inbox<u8[1]> { tag: [0; 1], ids: ids }),
        1 => qput(Inbox<u8[2]> { tag: [0; 2], ids: ids }),
        2 => qput(Inbox<u8[3]> { tag: [0; 3], ids: ids }),
        3 => qput(Inbox<u8[4]> { tag: [0; 4], ids: ids }),
        4 => qput(Inbox<u8[5]> { tag: [0; 5], ids: ids }),
        5 => qput(Inbox<u8[6]> { tag: [0; 6], ids: ids }),
        6 => qput(Inbox<u8[7]> { tag: [0; 7], ids: ids }),
        7 => qput(Inbox<u8[8]> { tag: [0; 8], ids: ids }),
        _ => abort("at most 8 workers"),
    }
}

fn go(j: i64, done: bool, refl: bool, sigs: u32[:], nm: u32[:]) {
    match j {
        0 => qput(Go<u8[1]> { tag: [0; 1], done: done, reflect: refl, sigs: sigs, named: nm }),
        1 => qput(Go<u8[2]> { tag: [0; 2], done: done, reflect: refl, sigs: sigs, named: nm }),
        2 => qput(Go<u8[3]> { tag: [0; 3], done: done, reflect: refl, sigs: sigs, named: nm }),
        3 => qput(Go<u8[4]> { tag: [0; 4], done: done, reflect: refl, sigs: sigs, named: nm }),
        4 => qput(Go<u8[5]> { tag: [0; 5], done: done, reflect: refl, sigs: sigs, named: nm }),
        5 => qput(Go<u8[6]> { tag: [0; 6], done: done, reflect: refl, sigs: sigs, named: nm }),
        6 => qput(Go<u8[7]> { tag: [0; 7], done: done, reflect: refl, sigs: sigs, named: nm }),
        7 => qput(Go<u8[8]> { tag: [0; 8], done: done, reflect: refl, sigs: sigs, named: nm }),
        _ => abort("at most 8 workers"),
    }
}

// messages out, bucketed by owner (a counting pass and a scatter), one inbox per other worker
fn flush(k: i64, W: i64) -> i64 {
    var cnt: i64[9] = [0; 9];
    for x in out { cnt[owner(x & idMask) + 1] += 1; }
    for j in W { cnt[j + 1] += cnt[j]; }
    var pos: i64[9] = cnt;
    var sorted: u32[>..] = [];
    sorted.resize(out.len, 0);
    for x in out {
        let j = owner(x & idMask);
        sorted[pos[j]] = x;
        pos[j] += 1;
    }
    for j in W { if j != k { send(j, sorted[cnt[j]..cnt[j + 1]]); } }
    let n = out.len;
    out.clear();
    return n;
}

fn run<T>(k: i64, W: i64, tag: T) {
    let t0 = clock();
    load_part(k, W);
    var roots: u32[>..] = [];
    load("p2_roots.u32", roots);
    let t_loaded = clock();
    // a barrier: every worker has its part in memory before the flood starts (the warm time)
    qput(Status { k: k, sent: 0, local: 0, reflect: false, sigs: new_sigs[..], named: new_named[..] });
    let start = qget<Go<T>>();
    let t1 = clock();
    for r in roots { if r as i64 >= lo && (r as i64) < lo + live.len { apply(r, opMark); } }
    var checked = 0;
    var rounds = 0;
    var busy = 0.0;
    var tflush = 0.0;
    var tapply = 0.0;
    loop {
        rounds++;
        let ta = clock();
        loop {
            drain();
            if facts_new { checked = 0; facts_new = false; }
            let more = methods(checked);
            checked = markable.len;
            if !more && work.len == 0 { break; }
        }
        let tb = clock();
        let sent = flush(k, W);
        busy += tb - ta;
        tflush += clock() - tb;
        qput(Status { k: k, sent: sent, local: 0, reflect: reflect, sigs: new_sigs[..], named: new_named[..] });
        new_sigs.clear();
        new_named.clear();
        let g = qget<Go<T>>();
        if g.done { break; }
        var facts = false;
        if g.reflect && !reflect { reflect = true; facts = true; }
        for s in g.sigs { if s < sig_seen.len && sig_seen[s] == 0 { add_sig(s); facts = true; } }
        for n in g.named { if n < named.len && named[n] == 0 { named[n] = 1; facts = true; } }
        if facts { checked = 0; }
        for j in W - 1 {
            let m = qget<Inbox<T>>();
            let tc = clock();
            for x in m.ids { apply(x & idMask, x & ~idMask); }
            tapply += clock() - tc;
        }
    }
    let t2 = clock();
    var mine: u32[>..] = [];
    for i in live.len { if live[i] != 0 { mine.push((lo + i) as u32); } }
    qput(Result { k: k, live: mine[..] });
    var note: u8[>..] = [];
    format(note, "  worker ", k, ": ids ", lo, "..", lo + live.len, ", ", edst.len, " edges; ");
    report(note, "load", t0, t_loaded);
    report(note, "flood", t1, t2);
    format(note, "(drain ");
    format_flt(note, busy * 1000.0, 1);
    format(note, ", flush ");
    format_flt(note, tflush * 1000.0, 1);
    format(note, ", apply inbox ");
    format_flt(note, tapply * 1000.0, 1);
    format(note, ") ", rounds, " rounds");
    print(note);
}

thread_fn worker(k: i64, W: i64) {
    match k {
        0 => run(k, W, [0 as u8; 1]),
        1 => run(k, W, [0 as u8; 2]),
        2 => run(k, W, [0 as u8; 3]),
        3 => run(k, W, [0 as u8; 4]),
        4 => run(k, W, [0 as u8; 5]),
        5 => run(k, W, [0 as u8; 6]),
        6 => run(k, W, [0 as u8; 7]),
        7 => run(k, W, [0 as u8; 8]),
        _ => abort("at most 8 workers"),
    }
}

fn main() {
    let a = args();
    var W = 8;
    if a.len > 2 { let n, ok = parse_int(a[2]); if ok { W = n; } }
    guard W >= 1 && W <= 8 else { abort("1..8 workers"); }
    let t0 = clock();
    var ids: i64[>..] = [];
    for k in W { ids.push(thread_spawn(worker, k, W)); }
    // main: the barrier of each superstep, and the global facts
    let nsig = meta("p2_sigs");
    var seen_sig: u8[>..] = zeros(nsig);
    var seen_named: u8[>..] = zeros(meta("p2_strings") + 1);
    var refl = false;
    var rounds = 0;
    var msgs = 0;
    for i in W { let s = qget<Status>(); }
    let t_ready = clock();
    let trace = env_set("P2TRACE");
    var steps: u8[>..] = [];
    for j in W { go(j, false, false, [], []); }
    loop {
        var sigs: u32[>..] = [];
        var nm: u32[>..] = [];
        var sent = 0;
        var fresh = false;
        for i in W {
            let s = qget<Status>();
            sent += s.sent;
            if s.reflect && !refl { refl = true; fresh = true; }
            for x in s.sigs { if x < seen_sig.len && seen_sig[x] == 0 { seen_sig[x] = 1; sigs.push(x); fresh = true; } }
            for x in s.named { if x < seen_named.len && seen_named[x] == 0 { seen_named[x] = 1; nm.push(x); fresh = true; } }
        }
        rounds++;
        msgs += sent;
        if trace { format(steps, " ", sent, "@"); format_flt(steps, ms(t_ready, clock()), 1); }
        let done = sent == 0 && !fresh;
        for j in W { go(j, done, refl, sigs[..], nm[..]); }
        if done { break; }
    }
    let t_flood = clock();
    var parts: Result[>..] = [];      // variable-size elements, inline: walked, not indexed
    for i in W { parts.push(qget<Result>()); }
    for id in ids { thread_wait(id); }
    let t1 = clock();
    var list: u32[>..] = [];
    for j in W { for p in parts { if p.k == j { list.append(p.live); } } }
    var want: u32[>..] = [];
    load("oracle/p2_live.u32", want);
    var bad = 0;
    if list.len != want.len { bad = 1; }
    else { for i in list.len { if list[i] != want[i] { bad++; } } }
    var note: u8[>..] = [];
    format(note, "p2par goose (", W, " workers): ");
    report(note, "spawn+load", t0, t_ready);
    report(note, "flood", t_ready, t_flood);
    report(note, "collect", t_flood, t1);
    report(note, "total", t0, t1);
    format(note, rounds, " supersteps, ", msgs, " messages, ", list.len, " live; ", bad, " differ from oracle/p2_live.u32");
    print(note);
    if trace { print("  supersteps (messages@ms):", steps); }
    print(if bad == 0 { "P2 OK" } else { "P2 FAIL" });
    if bad != 0 { exit(1); }
}
