// p1.goose: problem 1 (resolve), one thread (ref/flat/README.md; ref/flat/p1.c is the plain C form).
//   1a the join by name (winner: highest rank, then lowest id), 1b every relocation's target,
//   1c the content-hash join over what the flood reached. Checks itself against the four oracles.
//   p1 /home/exedev/paradigms/data/minio
import flat;

let t_start = clock();

let rankDef: u32 = 1 << 29;
let PkgIdxSelf: u32 = 2147483643;

let N = meta("nslots");
let kind: u8[>..] = load_u8("p1_kind.u8");
let flags: u8[>..] = load_u8("p1_flags.u8");
let key: Key[>..] = load_keys("p1_namekey.u64x2");
let want: Key[>..] = load_keys("p1_want.u64x2");
let bkey: Key[>..] = load_keys("p1_builtin.u64x2");
let rank: u32[>..] = load_u32("p1_rank.u32");
let slot_rel: u32[>..] = load_u32("slot_rel.u32");
let slot_obj: u32[>..] = load_u32("slot_obj.u32");
let pkg_off: u32[>..] = load_u32("obj_pkg_off.u32");
let pkg: u32[>..] = load_u32("obj_pkg.u32");
let rel_pkg: u32[>..] = load_u32("rel_pkg.u32");
let rel_idx: u32[>..] = load_u32("rel_idx.u32");
let hkey: Key[>..] = load_keys("p1_hashkey.u64x2");
let hmark: u8[>..] = load_u8("p1c_mark.u8");

// ---- 1a. one open-addressing table of groups: the key, and the best rank << 32 | ~id seen
struct Group { k: Key, val: u64 }
var tab: Group[>..] = [];
var mask: u64 = 0;

// the group of k: its slot, or -1 (add: make it)
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

fn offer(i: i64, g: i64) {
    let v = (rank[g] as u64) << 32 | (~(g as! u32)) as u64;
    if v > tab[i].val { tab[i].val = v; }
}

fn winner(i: i64) -> u32 { ~(tab[i].val as! u32) }

fn join_by_name(canon: u32[>..]&, builtin: u32[>..]&) {
    var sz = 1;
    while sz < N + 4 * want.len { sz <<= 1; }
    tab.resize(sz, Group { k: Key { 0, 0 }, val: 0 });
    mask = (sz - 1) as u64;
    for k in want { find(k, true); }
    canon.push(0);
    for g in 1..N + 1 {
        canon.push(if flags[g] & 2 != 0 { 0 } else { g as u32 });   // a reference only debug info makes
        guard (kind[g] == 4 || kind[g] == 5) && key[g].k1 != 0 else { continue; }
        offer(find(key[g], true), g);
    }
    // definitions by package index join only a name that is already there
    for g in 1..N + 1 {
        guard kind[g] == 1 && key[g].k1 != 0 else { continue; }
        let i = find(key[g], false);
        if i >= 0 { offer(i, g); }
    }
    for g in 1..N + 1 {
        guard kind[g] != 2 && kind[g] != 3 && key[g].k1 != 0 else { continue; }
        let i = find(key[g], false);
        if i >= 0 { canon[g] = winner(i); }
    }
    for k in bkey {
        let i = find(k, false);
        builtin.push(if i >= 0 && tab[i].val != 0 && ((tab[i].val >> 32) as! u32) & rankDef != 0 { winner(i) } else { 0 });
    }
}

// ---- 1b. a package-indexed reference is a read of its object's package table
fn rel_targets(canon: u32[:], builtin: u32[:], tgt: u32[>..]&) {
    tgt.resize(rel_pkg.len, 0);
    for g in 1..N + 1 {
        let o = slot_obj[g];
        let pb = pkg[pkg_off[o]..pkg_off[o as i64 + 1]];
        let npk = pb.len - 5;
        for r in slot_rel[g]..slot_rel[g + 1] {
            let p = rel_pkg[r];
            let idx = rel_idx[r];
            let sp = p - PkgIdxSelf;
            let t: i64 = if sp < 5 { npk + sp } else { p };
            var s: u32 = 0;
            if t < pb.len && pb[t] != 0xFFFFFFFF {
                s = if sp == 1 { if idx < builtin.len { builtin[idx] } else { 0 } } else { pb[t] + idx };
            }
            tgt[r] = if s != 0 { canon[s] } else { 0 };
        }
    }
}

// ---- 1c. the content-hash join over the copies the flood reached (mark 1) or names without following (2)
struct Class { k: Key, win: u32 }
var htab: Class[>..] = [];
var hmask: u64 = 0;

fn class_key(g: i64) -> Key {
    // short hashes group by k1 (k2 is the size); long ones by both, kept apart from short ones by the top bit
    return if kind[g] == 2 { Key { hkey[g].k1, 0 } } else { Key { hkey[g].k1, hkey[g].k2 | 0x8000000000000000 } };
}

fn hfind(k: Key) -> i64 {
    var h = mix64(k.k1 ^ k.k2) & hmask;
    loop {
        let i = h as! i64;
        if htab[i].win == 0 { htab[i].k = k; return i; }
        if htab[i].k == k { return i; }
        h = (h + 1) & hmask;
    }
}

fn content_join(canon: u32[:], out: u32[>..]&) {
    var hs = 1;
    while hs < 4 * (N / 2) { hs <<= 1; }
    htab.resize(hs, Class { k: Key { 0, 0 }, win: 0 });
    hmask = (hs - 1) as u64;
    for g in 1..N + 1 {
        guard (kind[g] == 2 || kind[g] == 3) && hmark[g] != 0 else { continue; }
        let i = hfind(class_key(g));
        let w = htab[i].win;
        if w == 0 { htab[i].win = g as u32; continue; }
        let wr = hmark[w] == 2;
        let gr = hmark[g] == 2;
        // a reached copy beats one only named; then the larger short copy; ties keep the lower id
        let better = if wr != gr { wr } else { kind[g] == 2 && hkey[g].k2 > hkey[w].k2 };
        if better { htab[i].win = g as u32; }
    }
    out.append(canon);
    for g in 1..N + 1 {
        guard (kind[g] == 2 || kind[g] == 3) && hmark[g] != 0 else { continue; }
        let w = htab[hfind(class_key(g))].win;
        guard hmark[w] != 2 && w != g as u32 else { continue; }   // nothing reached the class: not in the output
        guard !(kind[g] == 2 && hmark[g] == 2 && hkey[g].k2 > hkey[w].k2) else { continue; }
        out[g] = w;
    }
}

fn main() {
    let t0 = clock();
    var canon: u32[>..] = [];
    var builtin: u32[>..] = [];
    join_by_name(canon, builtin);
    let t1 = clock();
    var tgt: u32[>..] = [];
    rel_targets(canon, builtin, tgt);
    let t2 = clock();
    var canon2: u32[>..] = [];
    content_join(canon, canon2);
    let t3 = clock();
    var note: u8[>..] = [];
    format(note, "p1 goose (1 thread): ");
    report(note, "load", t_start, t0);
    report(note, "1a join by name", t0, t1);
    report(note, "1b relocation targets", t1, t2);
    report(note, "1c content-hash join", t2, t3);
    print(note);
    var bad = check32("canon", canon, "oracle/p1_canon.u32");
    bad += check32("builtin", builtin, "oracle/p1_builtin.u32");
    bad += check32("reltarget", tgt, "oracle/p1_reltarget.u32");
    bad += check32("canon after 1c", canon2, "oracle/p1c_canon.u32");
    print(if bad == 0 { "P1 OK" } else { "P1 FAIL" });
    if bad != 0 { exit(1); }
}
