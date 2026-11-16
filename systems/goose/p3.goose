// p3.goose: problem 3, one thread: 3a addresses for the placed symbols (a scan per bucket over units and
// alignment classes; typelinks sorted by type string), 3b copy every symbol's bytes into the image and patch its
// relocations (ref/flat/README.md; ref/flat/p3.c is the plain C form). Checks itself against oracle/p3_addr.u32
// and every placed symbol's bytes in oracle/p3_image.bin.   p3 /home/exedev/paradigms/data/minio
import flat;

let t_start = clock();

let bText: u8 = 1;
let bTextFIPS: u8 = 2;
let bTypelink: u8 = 6;
let bBSS: u8 = 13;
let nBuckets = 16;
let nClasses = 13;

let L = meta("p3_placed");
let nunits = meta("nunits");
let minFunc = meta("minFunc") as u32;
let imageBase = meta("imageBase");
let psym: u32[>..] = load_u32("p3_sym.u32");
let pbucket: u8[>..] = load_u8("p3_bucket.u8");
let pclass: u8[>..] = load_u8("p3_class.u8");
let psize: u32[>..] = load_u32("p3_size.u32");
let punit: u32[>..] = load_u32("p3_unit.u32");
let tlstr: u32[>..] = load_u32("p3_tlstr.u32");
let soff: u64[>..] = load_u64("p3_str.off.u64");
let sbin: u8[>..] = load_u8("p3_str.bin");
let bstart: u32[>..] = load_u32("p3_bucket_start.u32");
let bmin: u32[>..] = load_u32("p3_bucket_minalign.u32");
// 3b
let all: u32[>..] = load_u32("p3_addr_all.u32");
let doff: u64[>..] = load_u64("p3_data.off.u64");
let dbin: u8[>..] = load_u8("p3_data.bin");
let rstart: u32[>..] = load_u32("p3_rel_start.u32");
let roff: u32[>..] = load_u32("p3_rel_off.u32");
let rtgt: u32[>..] = load_u32("p3_rel_tgt.u32");
let rsiz: u8[>..] = load_u8("p3_rel_siz.u8");
let rkind: u8[>..] = load_u8("p3_rel_kind.u8");
let radd: i64[>..] = load_i64("p3_rel_add.i64");
let sect: u32[8] = [meta("lay_text") as u32, meta("lay_rodata") as u32, meta("lay_pclntab") as u32, meta("lay_types") as u32,
                    meta("lay_noptrdata") as u32, meta("lay_data") as u32, meta("lay_bss") as u32, meta("lay_noptrbss") as u32];
let unreach = meta("addr_unreachableMethod") as u32;
let mapinit = meta("addr_mapinitnoop") as u32;

fn rnd(v: u32, a: u32) -> u32 { (v + a - 1) & ~(a - 1) }

fn tlname(i: u32) -> const u8[:] { sbin[soff[tlstr[i]]..soff[tlstr[i] as i64 + 1]] }

// ---- 3a
fn layout(addr: u32[>..]&) {
    addr.resize(L, 0);
    // bytes per (unit, bucket, class), then each unit's block of each bucket in unit order, classes from the
    // largest alignment down; inside a class the symbols in list order
    var cs: u32[>..] = [];
    cs.resize(nunits * nBuckets * nClasses, 0);
    var occ: u32[>..] = [];
    occ.resize(L, 0);
    for i in L {
        let b = pbucket[i];
        var s = psize[i];
        if (b == bText || b == bTextFIPS) && s < minFunc { s = minFunc; }
        occ[i] = rnd(s, 1 << pclass[i]);
        if b != bTypelink { cs[(punit[i] * nBuckets + b) * nClasses + pclass[i]] += occ[i]; }
    }
    for b in 1..nBuckets {
        if b == bTypelink { continue; }
        var va = bstart[b];
        for u in nunits {
            let row = (u * nBuckets + b) * nClasses;
            var top = -1;
            for c in nClasses { if cs[row + c] != 0 { top = c; } }
            if top >= 0 { va = rnd(va, max(bmin[b], 1 << top)); }
            for k in nClasses {
                let c = nClasses - 1 - k;
                let n = cs[row + c];
                cs[row + c] = va;      // from here on: the class's next address
                va += n;
            }
        }
    }
    var tl: u32[>..] = [];
    for i in L {
        if pbucket[i] == bTypelink { tl.push(i as u32); continue; }
        let at = (punit[i] * nBuckets + pbucket[i]) * nClasses + pclass[i];
        addr[i] = cs[at];
        cs[at] += occ[i];
    }
    sort(tl) { x, y => block {
        let c = compare(tlname(x), tlname(y));
        break if c != 0 { c < 0 } else { psym[x] < psym[y] };
    } };
    var va = bstart[bTypelink];
    for i in tl {
        va = rnd(va, 1 << pclass[i]);
        addr[i] = va;
        va += psize[i];
    }
}

// ---- 3b
fn put(img: u8[:], at: i64, v: i64, n: u8) {
    for k in n { img[at + k] = (v >> (8 * k)) as! u8; }
}

fn relocate(addr: u32[:], img: u8[>..]&) -> i64 {
    var nerr = 0;
    for i in L {
        guard pbucket[i] < bBSS else { continue; }      // bss, noptrbss and coverage counters have no bytes
        let a = addr[i];
        let base = a - imageBase;
        let bytes = dbin[doff[i]..doff[i + 1]];
        img[base..base + bytes.len].move_from(bytes);
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
                        if rkind[r] == 4 { put(img, base + off, 0xFFFFFFFF, 4); }
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
            if sz == 1 || sz == 2 || sz == 4 || sz == 8 { put(img, base + off, v, sz); } else { nerr++; }
        }
    }
    return nerr;
}

fn main() {
    let t0 = clock();
    var addr: u32[>..] = [];
    layout(addr);
    let t1 = clock();
    var bad = check32("addr", addr, "oracle/p3_addr.u32");
    // 3b with the oracle's addresses, so that it does not depend on 3a
    var oaddr: u32[>..] = [];
    load("oracle/p3_addr.u32", oaddr);
    let t2 = clock();
    var img: u8[>..] = [];
    img.resize(meta("image_bytes"), 0);
    let t2b = clock();
    let nerr = relocate(oaddr, img);
    let t3 = clock();
    var want: u8[>..] = [];
    load("oracle/p3_image.bin", want);
    var nbad = 0;
    var nbytes = 0;
    for i in L {
        guard pbucket[i] < bBSS else { continue; }
        let o = oaddr[i] - imageBase;
        let n = psize[i] as i64;
        nbytes += n;
        if img[o..o + n] != want[o..o + n] {
            if nbad < 5 { print("  symbol ", i, " (id ", psym[i], ", bucket ", pbucket[i], ") differs"); }
            nbad++;
        }
    }
    var note: u8[>..] = [];
    format(note, "p3 goose (1 thread): ");
    report(note, "load", t_start, t0);
    report(note, "3a layout", t0, t1);
    report(note, "3b image alloc", t2, t2b);
    report(note, "3b copy+relocate", t2b, t3);
    format(note, nbytes, " bytes, ", rstart[L], " relocations, ", nerr, " errors; ", nbad, " placed symbols differ from oracle/p3_image.bin");
    print(note);
    if a_out() { guard write_file(arg_out(), img) else { abort("cannot write the image"); } }
    bad += nbad + (if nerr != 0 { 1 } else { 0 });
    print(if bad == 0 { "P3 OK" } else { "P3 FAIL" });
    if bad != 0 { exit(1); }
}

fn a_out() -> bool { args().len > 2 }
fn arg_out() -> u8[>..] { str(args()[2]) }
