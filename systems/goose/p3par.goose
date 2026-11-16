// p3par.goose: problem 3 on W workers that share no memory.
//   3a: worker k sizes the symbols of a run of whole units (occupied bytes per unit, bucket and class) and sends
//       the counts to main; main runs the per-bucket scan over all units (366 k small numbers) and sorts the
//       typelinks, and sends each worker the start address of each of its (unit, bucket, class) blocks; the
//       worker assigns its symbols' addresses and sends them back.
//   3b: the image is owned by address range: worker k builds bytes [A_k, A_k+1) of it, every placed symbol
//       that overlaps the range copied and patched (writes clipped to the range), and sends that segment to
//       main, which puts the segments together. Each worker holds all of 3b's inputs (read in parallel).
//       Timed two ways: the image left distributed (each worker's segment in its own memory, as a linker that has each
//       worker write its range of the output file would leave it), and gathered into one array in main.
//   p3par /home/exedev/paradigms/data/minio [workers 1..8]
// P3_REPS=n: the distributed 3b n times, each segment into fresh never-touched pages (faults inside the timed
// region); P3_THP=1: those pages, and main's gathered image, are 2 MB hugetlb pages (ATTACKS.md 2; flatio.c).
import flat;
import par;

let bText: u8 = 1;
let bTextFIPS: u8 = 2;
let bTypelink: u8 = 6;
let bBSS: u8 = 13;
let nBuckets = 16;
let nClasses = 13;

struct Counts { k: i64, ulo: i64, uhi: i64, cs: u32[], tl: u32[] }    // tl: this range's typelink symbols
struct Bases { cs: u32[] }
struct AddrPart { lo: i64, vals: u32[] }
struct Seg { at: i64, bytes: u8[] }
struct Ready { k: i64 }
struct Go { go: bool }
struct Done { k: i64, ms: f64, faults: i64, huge: i64 }

fn rnd(v: u32, a: u32) -> u32 { (v + a - 1) & ~(a - 1) }

// ---- a worker's own copies
var L = 0;
var lo = 0;
var hi = 0;
var punit: u32[>..] = [];
var pbucket: u8[>..] = [];
var pclass: u8[>..] = [];
var psize: u32[>..] = [];
var addr: u32[>..] = [];        // 3b: the oracle's addresses, so 3b does not depend on 3a (as in ref/flat/p3.c)
var all: u32[>..] = [];
var doff: u64[>..] = [];
var dbin: u8[>..] = [];
var rstart: u32[>..] = [];
var roff: u32[>..] = [];
var rtgt: u32[>..] = [];
var rsiz: u8[>..] = [];
var rkind: u8[>..] = [];
var radd: i64[>..] = [];
var sect: u32[8] = [0; 8];
var unreach: u32 = 0;
var mapinit: u32 = 0;
var minFunc: u32 = 0;
var imageBase = 0;

fn load_all() {
    L = meta("p3_placed");
    minFunc = meta("minFunc") as u32;
    imageBase = meta("imageBase");
    load("p3_unit.u32", punit);
    load("p3_bucket.u8", pbucket);
    load("p3_class.u8", pclass);
    load("p3_size.u32", psize);
    load("oracle/p3_addr.u32", addr);
    load("p3_addr_all.u32", all);
    load("p3_data.off.u64", doff);
    load("p3_data.bin", dbin);
    load("p3_rel_start.u32", rstart);
    load("p3_rel_off.u32", roff);
    load("p3_rel_tgt.u32", rtgt);
    load("p3_rel_siz.u8", rsiz);
    load("p3_rel_kind.u8", rkind);
    load("p3_rel_add.i64", radd);
    sect = [meta("lay_text") as u32, meta("lay_rodata") as u32, meta("lay_pclntab") as u32, meta("lay_types") as u32,
            meta("lay_noptrdata") as u32, meta("lay_data") as u32, meta("lay_bss") as u32, meta("lay_noptrbss") as u32];
    unreach = meta("addr_unreachableMethod") as u32;
    mapinit = meta("addr_mapinitnoop") as u32;
}

// part k of the symbol list, cut at unit boundaries
fn cut(k: i64, W: i64) -> i64 {
    if k >= W { return L; }
    var i = part_lo(L, k, W);
    while i > 0 && i < L && punit[i] == punit[i - 1] { i++; }
    return i;
}

fn occupied(i: i64) -> u32 {
    let b = pbucket[i];
    var s = psize[i];
    if (b == bText || b == bTextFIPS) && s < minFunc { s = minFunc; }
    return rnd(s, 1 << pclass[i]);
}

// one byte of a write, clipped to the segment
fn put(seg: u8[:], at: i64, v: i64, n: u8) {
    for k in n {
        let p = at + k;
        if p >= 0 && p < seg.len { seg[p] = (v >> (8 * k)) as! u8; }
    }
}

// bytes [a0, a1) of the image: every placed symbol that overlaps them
fn build_segment(a0: i64, a1: i64, seg: u8[>..]&) -> i64 {
    seg.resize(a1 - a0, 0);
    var nerr = 0;
    for i in L {
        guard pbucket[i] < bBSS else { continue; }
        let a = addr[i];
        let s = a - imageBase;                   // the symbol's offset in the image
        guard s < a1 && s + psize[i] > a0 else { continue; }
        let base = s - a0;                       // ... and in the segment (may be negative)
        let bytes = dbin[doff[i]..doff[i + 1]];
        let c0 = max(0, -base);
        let c1 = min(bytes.len, seg.len - base);
        if c0 < c1 { seg[base + c0..base + c1].move_from(bytes[c0..c1]); }
        for r in rstart[i]..rstart[i + 1] {
            let tg = rtgt[r];
            var ta = all[tg];
            let off = roff[r];
            let sz = rsiz[r];
            let P = a + off;
            var v: i64 = 0;
            match rkind[r] {
                1, 2 => {
                    if ta == 0 && tg != 0 && rkind[r] == 2 { ta = unreach; }
                    if ta == 0 && tg != 0 { nerr++; continue; }
                    v = ta + radd[r];
                },
                3, 4 => {
                    if ta == 0 {
                        if rkind[r] == 4 { put(seg, base + off, 0xFFFFFFFF, 4); }
                        continue;
                    }
                    var k = 7;
                    while k > 0 && ta < sect[k] { k--; }
                    v = (ta - sect[k]) + radd[r];
                },
                5, 6, 7 => {
                    if ta == 0 && tg != 0 && rkind[r] == 6 { ta = mapinit; }
                    if ta == 0 && tg != 0 { nerr++; continue; }
                    v = if tg == 0 { radd[r] - (P + sz) } else { ta + radd[r] - (P + sz) };
                },
                8 => { v = radd[r]; },
                _ => { nerr++; continue; },
            }
            if sz == 1 || sz == 2 || sz == 4 || sz == 8 { put(seg, base + off, v, sz); } else { nerr++; }
        }
    }
    return nerr;
}

fn run<T>(k: i64, W: i64, tag: T) {
    load_all();
    qput(Ready { k: k });
    let go = qget<Go>();
    let t0 = clock();
    // ---- 3a, part 1: the bytes of each (unit, bucket, class) of this worker's units
    lo = cut(k, W);
    hi = cut(k + 1, W);
    let ulo = if lo < L { punit[lo] as i64 } else { 0 };
    let uhi = if hi < L { punit[hi] as i64 } else { meta("nunits") };
    var cs: u32[>..] = [];
    cs.resize((uhi - ulo) * nBuckets * nClasses, 0);
    var tl: u32[>..] = [];
    for i in lo..hi {
        let b = pbucket[i];
        if b == bTypelink { tl.push(i as u32); continue; }
        cs[((punit[i] - ulo) * nBuckets + b) * nClasses + pclass[i]] += occupied(i);
    }
    qput(Counts { k: k, ulo: ulo, uhi: uhi, cs: cs[..], tl: tl[..] });
    // ---- 3a, part 2: the addresses, from the blocks' start addresses main sends
    let bases = qget<Mail<T, Bases>>();
    var next = copy(bases.m.cs);
    var out: u32[>..] = [];
    for i in lo..hi {
        if pbucket[i] == bTypelink { out.push(0); continue; }     // main places the typelinks
        let at = ((punit[i] - ulo) * nBuckets + pbucket[i]) * nClasses + pclass[i];
        out.push(next[at]);
        next[at] += occupied(i);
    }
    qput(AddrPart { lo: lo, vals: out[..] });
    let t1 = clock();
    // ---- 3b: this worker's part of the image, P3_REPS times (main starts each rep)
    let nimg = meta("image_bytes");
    let a0 = part_lo(nimg, k, W);
    let a1 = part_lo(nimg, k + 1, W);
    let reps = env_int("P3_REPS", 1);
    let huge = env_set("P3_THP");
    var seg: u8[>..] = [];
    var nerr = 0;
    var t2 = 0.0;
    for rep in reps {
        let g = qget<Mail<T, Go>>();
        seg.clear();
        let f0 = minor_faults(true);
        let ta = clock();
        let hb = fresh_pages(seg, a1 - a0, huge);
        nerr = build_segment(a0, a1, seg);
        t2 = clock();
        qput(Done { k: k, ms: (t2 - ta) * 1000.0, faults: minor_faults(true) - f0, huge: hb });
    }
    let g = qget<Mail<T, Go>>();
    qput(Seg { at: a0, bytes: seg[..] });
    if nerr != 0 { print("worker ", k, ": ", nerr, " relocation errors"); }
    var note: u8[>..] = [];
    format(note, "  worker ", k, ": symbols ", lo, "..", hi, ", image ", a0, "..", a1, "; ");
    report(note, "3a", t0, t1);
    if env_set("P3TRACE") { print(note); }
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

// typelinks: sorted by type string (bytewise, then shorter first), then id
// main's own inputs for the sort (globals only main uses: no worker gets a copy)
let m_psym = load_u32("p3_sym.u32");
let m_tlstr = load_u32("p3_tlstr.u32");
let m_soff = load_u64("p3_str.off.u64");
let m_sbin = load_u8("p3_str.bin");
let m_pclass = load_u8("p3_class.u8");
let m_psize = load_u32("p3_size.u32");
let m_bstart = load_u32("p3_bucket_start.u32");

fn place_typelinks(tl: u32[>..]&, addr: u32[>..]&) {
    sort(tl) { x, y => block {
        let c = compare(m_sbin[m_soff[m_tlstr[x]]..m_soff[m_tlstr[x] as i64 + 1]],
                        m_sbin[m_soff[m_tlstr[y]]..m_soff[m_tlstr[y] as i64 + 1]]);
        break if c != 0 { c < 0 } else { m_psym[x] < m_psym[y] };
    } };
    var va = m_bstart[bTypelink];
    for i in tl {
        va = rnd(va, 1 << m_pclass[i]);
        addr[i] = va;
        va += m_psize[i];
    }
}

fn main() {
    let W = workers_arg();
    let t0 = clock();
    let Lm = meta("p3_placed");
    let nunits = meta("nunits");
    let bstart = load_u32("p3_bucket_start.u32");
    let bmin = load_u32("p3_bucket_minalign.u32");
    var ids: i64[>..] = [];
    for k in W { ids.push(thread_spawn(worker, k, W)); }
    for k in W { let r = qget<Ready>(); }
    let t1 = clock();
    for k in W { qput(Go { go: true }); }
    // ---- 3a: the scan over every unit of every bucket
    var cs: u32[>..] = [];
    cs.resize(nunits * nBuckets * nClasses, 0);
    var ulo: i64[>..] = [];
    var uhi: i64[>..] = [];
    ulo.resize(W, 0);
    uhi.resize(W, 0);
    var tl: u32[>..] = [];
    for j in W {
        let c = qget<Counts>();
        let at = c.ulo * nBuckets * nClasses;
        cs[at..at + c.cs.len].move_from(c.cs[..]);
        ulo[c.k] = c.ulo;
        uhi[c.k] = c.uhi;
        tl.append(c.tl);
    }
    for b in 1..nBuckets {
        if b == bTypelink { continue; }
        var va = bstart[b];
        for u in nunits {
            let row = (u * nBuckets + b) * nClasses;
            var top = -1;
            for c in nClasses { if cs[row + c] != 0 { top = c; } }
            if top >= 0 { va = rnd(va, max(bmin[b], 1 << top)); }
            for x in nClasses {
                let c = nClasses - 1 - x;
                let n = cs[row + c];
                cs[row + c] = va;
                va += n;
            }
        }
    }
    for j in W { send(j, Bases { cs: cs[ulo[j] * nBuckets * nClasses..uhi[j] * nBuckets * nClasses] }); }
    var addr: u32[>..] = [];
    addr.resize(Lm, 0);
    var tlv = copy(tl);
    for j in W {
        let p = qget<AddrPart>();
        addr[p.lo..p.lo + p.vals.len].move_from(p.vals[..]);
    }
    place_typelinks(tlv, addr);
    let t2 = clock();
    // ---- 3b distributed: from the go to the last worker's segment built, P3_REPS times
    let reps = env_int("P3_REPS", 1);
    let huge = env_set("P3_THP");
    var times: f64[>..] = [];
    var wmax = 0.0;
    var flt = 0;
    var hb = 0;
    for rep in reps {
        let ta = clock();
        for j in W { send(j, Go { go: true }); }
        wmax = 0.0;
        flt = 0;
        hb = 0;
        for j in W {
            let d = qget<Done>();
            wmax = max(wmax, d.ms);
            flt += d.faults;
            hb += if d.huge > 0 { d.huge } else { 0 };
        }
        times.push((clock() - ta) * 1000.0);
    }
    let t2a = clock();
    // ---- 3b gathered: the segments into one array in main
    var img: u8[>..] = [];
    let f0 = minor_faults(false);
    let t2b = clock();
    let mhb = fresh_pages(img, meta("image_bytes"), huge);
    img.resize(meta("image_bytes"), 0);
    let t2c = clock();
    for j in W { send(j, Go { go: true }); }
    for j in W {
        let s = qget<Seg>();
        img[s.at..s.at + s.bytes.len].move_from(s.bytes[..]);
    }
    let t3 = clock();
    let gflt = minor_faults(false) - f0;
    for id in ids { thread_wait(id); }
    let t4 = clock();
    var bad = check32("addr", addr, "oracle/p3_addr.u32");
    let oaddr = load_u32("oracle/p3_addr.u32");
    let pbucket = load_u8("p3_bucket.u8");
    let psize = load_u32("p3_size.u32");
    var want: u8[>..] = [];
    load("oracle/p3_image.bin", want);
    let imageBase = meta("imageBase");
    var nbad = 0;
    for i in Lm {
        guard pbucket[i] < bBSS else { continue; }
        let o = oaddr[i] - imageBase;
        let n = psize[i] as i64;
        if img[o..o + n] != want[o..o + n] {
            if nbad < 5 { print("  symbol ", i, " (bucket ", pbucket[i], ") differs"); }
            nbad++;
        }
    }
    var note: u8[>..] = [];
    format(note, "p3par goose (", W, " workers): ");
    report(note, "spawn+load", t0, t1);
    report(note, "3a", t1, t2);
    format(note, "3b distributed (", if huge { "2 MB" } else { "4 KB" }, " pages, x", reps, ") ");
    sort(times) { x, y => x < y };
    format_flt(note, times[0], 1);
    format(note, " / ");
    format_flt(note, times[times.len / 2], 1);
    format(note, " / ");
    format_flt(note, times[times.len - 1], 1);
    format(note, " ms min/median/max (slowest worker ");
    format_flt(note, wmax, 1);
    format(note, " ms, ", flt, " minor faults/rep in workers, ", hb, " bytes on huge pages)  ");
    report(note, "gather: image alloc", t2b, t2c);
    report(note, "segments into it", t2c, t3);
    format(note, "(", gflt, " faults in the process, ", mhb, " image bytes on huge pages)  ");
    format(note, "warm 3a + best 3b distributed ");
    format_flt(note, ms(t1, t2) + times[0], 1);
    format(note, " ms  ");
    format(note, nbad, " placed symbols differ from oracle/p3_image.bin");
    print(note);
    bad += nbad;
    print(if bad == 0 { "P3 OK" } else { "P3 FAIL" });
    if bad != 0 { exit(1); }
}
