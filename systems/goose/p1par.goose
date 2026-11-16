// p1par.goose: problem 1 (resolve) on W workers that share no memory: a shuffle join.
// Worker k reads slots [lo, hi) of every per-slot column (and their relocations) and nothing else big.
//   1a: it sends each keyed slot (id, kind, rank, key) to the worker that owns the key's hash; an owner builds
//       the groups of its keys, picks the winners and replies with each entry's canonical id, in the order the
//       entries came. Then every worker sends its canonical range to every other (an allgather: 1b reads
//       canon[] at any id).
//   1b: the relocation targets of its slots, sent to main.
//   1c: the same shuffle by content-hash class over the copies the flood reached.
// main only starts the phases, collects the results and checks them.
//   p1par /home/exedev/paradigms/data/minio [workers 1..8]
// P1_REPS=n: the three phases n times (main starts each round; tables and outputs rebuilt): min/median/max
import flat;
import par;

let rankDef: u32 = 1 << 29;
let PkgIdxSelf: u32 = 2147483643;
let NONE: u32 = 0xFFFFFFFF;

struct NameEnt { g: u32, kind: u8, rank: u32, k: Key }
struct NameBatch { from: i64, ents: NameEnt[] }
struct HashEnt { g: u32, mark: u8, kind: u8, h: Key }
struct HashBatch { from: i64, ents: HashEnt[] }
struct Reply { from: i64, vals: u32[] }
struct CanonPart { from: i64, lo: i64, vals: u32[] }       // to the other workers (in a Mail) and to main
struct TgtPart { from: i64, lo: i64, vals: u32[] }
struct Canon2Part { from: i64, lo: i64, vals: u32[] }
struct BuiltinPart { idx: u32[], vals: u32[] }
struct Builtins { vals: u32[] }
struct Ready { k: i64, load_ms: f64 }
struct Go { go: bool }

// ---- a worker's own state
var lo = 0;
var hi = 0;
var kind: u8[>..] = [];
var flags: u8[>..] = [];
var key: Key[>..] = [];
var rank: u32[>..] = [];
var slot_obj: u32[>..] = [];
var slot_rel: u32[>..] = [];     // hi - lo + 1 entries, file values (relocation numbers)
var rel_pkg: u32[>..] = [];
var rel_idx: u32[>..] = [];
var hkey: Key[>..] = [];
var hmark: u8[>..] = [];
var want: Key[>..] = [];
var bkey: Key[>..] = [];
var pkg_off: u32[>..] = [];
var pkg: u32[>..] = [];

fn owner(h: u64, W: i64) -> i64 { (((h >> 40) as! i64) * W) >> 24 }

// ---- the groups of the keys this worker owns
struct Group { k: Key, val: u64 }
var tab: Group[>..] = [];
var mask: u64 = 0;

fn find(k: Key, add: bool) -> i64 {
    var h = mix64(k.k1) & mask;
    loop {
        let i = h as! i64;
        if tab[i].k.k1 == 0 {
            if !add { return -1; }
            tab[i].k = k;
            return i;
        }
        if tab[i].k == k { return i; }
        h = (h + 1) & mask;
    }
}

fn offer(i: i64, g: u32, r: u32) {
    let v = (r as u64) << 32 | (~g) as u64;
    if v > tab[i].val { tab[i].val = v; }
}

// ---- the content-hash classes this worker owns
struct Class { k: Key, win: u32, wmark: u8, wsize: u64 }
var htab: Class[>..] = [];
var hmask: u64 = 0;

fn class_key(kd: u8, h: Key) -> Key { if kd == 2 { Key { h.k1, 0 } } else { Key { h.k1, h.k2 | 0x8000000000000000 } } }

fn hfind(k: Key) -> i64 {
    var h = mix64(k.k1 ^ k.k2) & hmask;
    loop {
        let i = h as! i64;
        if htab[i].win == 0 { htab[i].k = k; return i; }
        if htab[i].k == k { return i; }
        h = (h + 1) & hmask;
    }
}

// does copy e beat the class's current winner? (a total order, so the order of arrival does not matter)
fn beats(c: Class&, e: HashEnt) -> bool {
    let wr = c.wmark == 2;
    let gr = e.mark == 2;
    if wr != gr { return wr; }                    // a reached copy beats one only named
    if e.kind == 2 && e.h.k2 != c.wsize { return e.h.k2 > c.wsize; }   // the larger short copy
    return e.g < c.win;                           // then the lower id
}

fn pow2_at_least(n: i64) -> i64 {
    var s = 1;
    while s < n { s <<= 1; }
    return s;
}

fn load_part(k: i64, W: i64) {
    let N = meta("nslots");
    lo = part_lo(N + 1, k, W);
    hi = part_lo(N + 1, k + 1, W);
    let n = hi - lo;
    load("p1_kind.u8", kind, lo, n);
    load("p1_flags.u8", flags, lo, n);
    load("p1_namekey.u64x2", key, lo, n);
    load("p1_rank.u32", rank, lo, n);
    load("slot_obj.u32", slot_obj, lo, n);
    load("slot_rel.u32", slot_rel, lo, n + 1);
    let rlo = slot_rel[0] as i64;
    let rhi = slot_rel[n] as i64;
    load("rel_pkg.u32", rel_pkg, rlo, rhi - rlo);
    load("rel_idx.u32", rel_idx, rlo, rhi - rlo);
    load("p1_hashkey.u64x2", hkey, lo, n);
    load("p1c_mark.u8", hmark, lo, n);
    load("p1_want.u64x2", want);
    load("p1_builtin.u64x2", bkey);
    load("obj_pkg_off.u32", pkg_off);
    load("obj_pkg.u32", pkg);
}

// counting sort of items by owner: the start of each owner's run in `order` (W + 1 entries)
fn bucket(owners: u8[:], order: u32[>..]&, W: i64) -> i64[>..] {
    var start: i64[>..] = [];
    start.resize(W + 1, 0);
    for o in owners { start[o as i64 + 1] += 1; }
    for j in W { start[j + 1] += start[j]; }
    var pos = copy(start);
    order.resize(owners.len, 0);
    for o, i in owners {
        order[pos[o]] = i as u32;
        pos[o] += 1;
    }
    return start;
}

fn run<T>(k: i64, W: i64, tag: T) {
    let t0 = clock();
    load_part(k, W);
    let n = hi - lo;
    qput(Ready { k: k, load_ms: ms(t0, clock()) });
  for rep in env_int("P1_REPS", 1) {
    let go = qget<Go>();
    tab.clear();
    htab.clear();
    let t1 = clock();

    // ---- 1a: shuffle the keyed slots to their keys' owners
    var keyed: u32[>..] = [];        // local index of each keyed slot
    var own: u8[>..] = [];
    for i in n {
        let kd = kind[i];
        if (kd == 1 || kd == 4 || kd == 5) && key[i].k1 != 0 {
            keyed.push(i as u32);
            own.push(owner(mix64(key[i].k1), W) as! u8);
        }
    }
    var order: u32[>..] = [];
    let start = bucket(own, order, W);
    for j in W {
        var ents: NameEnt[>..] = [];
        for p in start[j]..start[j + 1] {
            let i = keyed[order[p]];
            ents.push(NameEnt { g: (lo + i) as u32, kind: kind[i], rank: rank[i], k: key[i] });
        }
        send(j, NameBatch { from: k, ents: ents[..] });
    }
    var batches: Mail<T, NameBatch>[>..] = [];
    var nin = 0;
    for j in W {
        batches.push(qget<Mail<T, NameBatch>>());
    }
    for b in batches { nin += b.m.ents.len; }
    var nwant = 0;
    for w in want { if owner(mix64(w.k1), W) == k { nwant++; } }
    let sz = pow2_at_least(2 * (nin + nwant) + 64);
    tab.resize(sz, Group { k: Key { 0, 0 }, val: 0 });
    mask = (sz - 1) as u64;
    for w in want { if owner(mix64(w.k1), W) == k { find(w, true); } }
    for b in batches { for e in b.m.ents { if e.kind != 1 { offer(find(e.k, true), e.g, e.rank); } } }
    // definitions by package index join only a name that is already there
    for b in batches {
        for e in b.m.ents {
            if e.kind == 1 {
                let i = find(e.k, false);
                if i >= 0 { offer(i, e.g, e.rank); }
            }
        }
    }
    for b in batches {
        var vals: u32[>..] = [];
        for e in b.m.ents {
            let i = find(e.k, false);
            vals.push(if i >= 0 { ~(tab[i].val as! u32) } else { NONE });
        }
        send(b.m.from, Reply { from: k, vals: vals[..] });
    }
    {
        var idx: u32[>..] = [];
        var vals: u32[>..] = [];
        for b, i in bkey {
            if owner(mix64(b.k1), W) != k { continue; }
            let t = find(b, false);
            idx.push(i as u32);
            vals.push(if t >= 0 && tab[t].val != 0 && ((tab[t].val >> 32) as! u32) & rankDef != 0 { ~(tab[t].val as! u32) } else { 0 });
        }
        qput(BuiltinPart { idx: idx[..], vals: vals[..] });
    }
    var canon: u32[>..] = [];
    for i in n { canon.push(if flags[i] & 2 != 0 { 0 } else { (lo + i) as u32 }); }
    for j in W {
        let r = qget<Mail<T, Reply>>();
        let at = start[r.m.from];
        for v, p in r.m.vals { if v != NONE { canon[keyed[order[at + p]]] = v; } }
    }
    let t2 = clock();
    // ---- the allgather of canon
    qput(CanonPart { from: k, lo: lo, vals: canon[..] });
    for j in W { if j != k { send(j, CanonPart { from: k, lo: lo, vals: canon[..] }); } }
    var all: u32[>..] = [];
    all.resize(meta("nslots") + 1, 0);
    all[lo..hi].move_from(canon[..]);
    for j in W - 1 {
        let p = qget<Mail<T, CanonPart>>();
        all[p.m.lo..p.m.lo + p.m.vals.len].move_from(p.m.vals[..]);
    }
    let bt = qget<Builtins>();
    let t3 = clock();

    // ---- 1b: the relocation targets of this worker's slots
    let rlo = slot_rel[0];
    var tgt: u32[>..] = [];
    tgt.resize(rel_pkg.len, 0);
    for i in n {
        let o = slot_obj[i];
        let pb = pkg[pkg_off[o]..pkg_off[o as i64 + 1]];
        let npk = pb.len - 5;
        for r in slot_rel[i] - rlo..slot_rel[i + 1] - rlo {
            let p = rel_pkg[r];
            let idx = rel_idx[r];
            let sp = p - PkgIdxSelf;
            let t: i64 = if sp < 5 { npk + sp } else { p };
            var s: u32 = 0;
            if t < pb.len && pb[t] != 0xFFFFFFFF {
                s = if sp == 1 { if idx < bt.vals.len { bt.vals[idx] } else { 0 } } else { pb[t] + idx };
            }
            tgt[r] = if s != 0 { all[s] } else { 0 };
        }
    }
    qput(TgtPart { from: k, lo: rlo, vals: tgt[..] });
    let t4 = clock();

    // ---- 1c: shuffle the marked content-addressed copies to their classes' owners
    var hashed: u32[>..] = [];
    var hown: u8[>..] = [];
    for i in n {
        if (kind[i] == 2 || kind[i] == 3) && hmark[i] != 0 {
            let c = class_key(kind[i], hkey[i]);
            hashed.push(i as u32);
            hown.push(owner(mix64(c.k1 ^ c.k2), W) as! u8);
        }
    }
    var horder: u32[>..] = [];
    let hstart = bucket(hown, horder, W);
    for j in W {
        var ents: HashEnt[>..] = [];
        for p in hstart[j]..hstart[j + 1] {
            let i = hashed[horder[p]];
            ents.push(HashEnt { g: (lo + i) as u32, mark: hmark[i], kind: kind[i], h: hkey[i] });
        }
        send(j, HashBatch { from: k, ents: ents[..] });
    }
    var hb: Mail<T, HashBatch>[>..] = [];
    var hin = 0;
    for j in W { hb.push(qget<Mail<T, HashBatch>>()); }
    for b in hb { hin += b.m.ents.len; }
    let hs = pow2_at_least(2 * hin + 64);
    htab.resize(hs, Class { k: Key { 0, 0 }, win: 0, wmark: 0, wsize: 0 });
    hmask = (hs - 1) as u64;
    for b in hb {
        for e in b.m.ents {
            let c .= htab[hfind(class_key(e.kind, e.h))];
            if c.win == 0 || beats(c, e) { c.win = e.g; c.wmark = e.mark; c.wsize = e.h.k2; }
        }
    }
    for b in hb {
        var vals: u32[>..] = [];
        for e in b.m.ents {
            let c = htab[hfind(class_key(e.kind, e.h))];
            // nothing reached the class (not in the output), or the copy is the winner, or a larger copy only named
            let keep = c.wmark == 2 || c.win == e.g || (e.kind == 2 && e.mark == 2 && e.h.k2 > c.wsize);
            vals.push(if keep { NONE } else { c.win });
        }
        send(b.m.from, Reply { from: k, vals: vals[..] });
    }
    for j in W {
        let r = qget<Mail<T, Reply>>();
        let at = hstart[r.m.from];
        for v, p in r.m.vals { if v != NONE { canon[hashed[horder[at + p]]] = v; } }
    }
    qput(Canon2Part { from: k, lo: lo, vals: canon[..] });
    let t5 = clock();
    var note: u8[>..] = [];
    format(note, "  worker ", k, ": slots ", lo, "..", hi, "; ");
    report(note, "1a", t1, t2);
    report(note, "allgather", t2, t3);
    report(note, "1b", t3, t4);
    report(note, "1c", t4, t5);
    if env_set("P1TRACE") { print(note); }
  }
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
    let W = workers_arg();
    let t0 = clock();
    let N = meta("nslots");
    let nb = meta("nbuiltin");
    let nrel = meta("nrel");
    var ids: i64[>..] = [];
    for k in W { ids.push(thread_spawn(worker, k, W)); }
    var load_max = 0.0;
    for k in W { let r = qget<Ready>(); load_max = max(load_max, r.load_ms); }
    let t1 = clock();
    let reps = env_int("P1_REPS", 1);
    var builtin: u32[>..] = [];
    var canon: u32[>..] = [];
    var tgt: u32[>..] = [];
    var canon2: u32[>..] = [];
    var ta: f64[>..] = [];
    var tb: f64[>..] = [];
    var tc: f64[>..] = [];
    var tw: f64[>..] = [];
    var t4 = 0.0;
  for rep in reps {
    builtin.clear();
    canon.clear();
    tgt.clear();
    canon2.clear();
    let r1 = clock();
    for k in W { qput(Go { go: true }); }
    builtin.resize(nb, 0);
    for k in W {
        let p = qget<BuiltinPart>();
        for i, j in p.idx { builtin[i] = p.vals[j]; }
    }
    for k in W { qput(Builtins { vals: builtin[..] }); }
    canon.resize(N + 1, 0);
    for k in W {
        let p = qget<CanonPart>();
        canon[p.lo..p.lo + p.vals.len].move_from(p.vals[..]);
    }
    let r2 = clock();
    tgt.resize(nrel, 0);
    for k in W {
        let p = qget<TgtPart>();
        tgt[p.lo..p.lo + p.vals.len].move_from(p.vals[..]);
    }
    let r3 = clock();
    canon2.resize(N + 1, 0);
    for k in W {
        let p = qget<Canon2Part>();
        canon2[p.lo..p.lo + p.vals.len].move_from(p.vals[..]);
    }
    t4 = clock();
    ta.push(ms(r1, r2));
    tb.push(ms(r2, r3));
    tc.push(ms(r3, t4));
    tw.push(ms(r1, t4));
  }
    for id in ids { thread_wait(id); }
    let t5 = clock();
    var note: u8[>..] = [];
    format(note, "p1par goose (", W, " workers): ");
    report(note, "spawn+load", t0, t1);
    format(note, "x", reps, ": ");
    spread(note, "1a", ta);
    spread(note, "1b", tb);
    spread(note, "1c", tc);
    spread(note, "warm 1a+1b+1c", tw);
    report(note, "exit", t4, t5);
    print(note);
    var bad = check32("canon", canon, "oracle/p1_canon.u32");
    bad += check32("builtin", builtin, "oracle/p1_builtin.u32");
    bad += check32("reltarget", tgt, "oracle/p1_reltarget.u32");
    bad += check32("canon after 1c", canon2, "oracle/p1c_canon.u32");
    print(if bad == 0 { "P1 OK" } else { "P1 FAIL" });
    if bad != 0 { exit(1); }
}
