// p4.goose: problem 4 (code generation by shape, ref/p4/README.md) on W workers that share no memory.
// Worker k owns a run of whole functions (cut at item quantiles). It reads only its own items, nodes, operands,
// glue and markers (ranges of the flat files: nodes, glue and operands are stored in function order) plus the
// whole template table, which it turns into inline typed records: a segment knows its bytes and its holes. Then,
// with no message in between, it lays out its items from 0 (a jump is within one function, so local offsets are
// enough), copies each item's bytes into its own buffer and patches the holes. Main only says go and, at the end,
// joins the buffers in order (each relocation shifted by its worker's base) and checks the result against the
// oracle (code byte for byte, relocations as a sorted list).
//   p4 /home/exedev/paradigms/data/minio-p4 [workers 1..8]
// P4_REPS=n: n timed reps after one warm-up rep (buffers reused, as ref/p4/p4.c). P4_HUGE=1: each worker's
// output on 2 MB pages (flatio.c fresh_pages).
import flat;
import par;

let encInt: u8 = 1;
let encRel32: u8 = 2;
let encReloc: u8 = 3;

// ---- the template table, as inline typed records (every worker builds its own copy)
struct Hole { at: u32, w: u8, enc: u8, rtype: u16 }            // at: offset in its segment
var tbin: u8[>..] = [];
var holes: Hole[>..] = [];
type HoleEnd = u32 upto holes;
// one segment of a template: where its bytes are, and its holes [h0, h1); o0 = the template's first hole
struct Seg { bin: i64, len: i64, h0: HoleEnd, h1: HoleEnd, o0: u32 }
var segs: Seg[>..] = [];
var tseg0: u32[>..] = [];        // per template: its first segment

// ---- this worker's part of the program
enum Item { Glue { g: u32 }, Piece { n: u32, s: u32 } }    // a glue run, or segment s of node n
struct Op { v: i64, a: i64 }
struct GRel { off: u32, siz: u8, rtype: u16, sym: u32, add: i64 }
struct Rel { off: i64, add: i64, sym: u32, rtype: u16, siz: u8 }
var items: Item[>..] = [];
var fitem: i64[>..] = [];       // per function: its first item (local), and one past the last
var ntmpl: u32[>..] = [];       // per node: its template
var nop: i64[>..] = [];         // per node: its first operand (local)
var ops: Op[>..] = [];
var goff: i64[>..] = [];        // per glue run: its bytes in gbin
var gbin: u8[>..] = [];
var grel0: i64[>..] = [];       // per glue run: its relocations in grels
var grels: GRel[>..] = [];
var fmark: i64[>..] = [];       // per function: its markers in marks
var marks: GRel[>..] = [];
var item0 = 0;                  // this worker's first item, function, glue run, node
var at: i64[>..] = [];          // per item: where it goes (local)
var code: u8[>..] = [];
var rels: Rel[>..<] = [];

fn load_templates() {
    load("tmpl.bin", tbin);
    let toff = load_u64("tmpl.off.u64");
    let segoff = load_u32("tmpl_seg_off.u32");
    let segend = load_u32("tseg_end.u32");
    let holeoff = load_u32("tmpl_hole_off.u32");
    let hat = load_u32("hole_at.u32");
    let hw = load_u8("hole_w.u8");
    let henc = load_u8("hole_enc.u8");
    let hrt = load_u16("hole_rtype.u16");
    for t in toff.len - 1 {
        tseg0.push(segs.len as u32);
        var h = holeoff[t];
        var lo: u32 = 0;
        for s in segoff[t]..segoff[t + 1] {
            let hi = segend[s];
            let h0 = h;
            while h < holeoff[t + 1] && hat[h] < hi {
                holes.push(Hole { at: hat[h] - lo, w: hw[h], enc: henc[h], rtype: hrt[h] });
                h++;
            }
            segs.push(Seg { bin: (toff[t] + lo) as i64, len: hi - lo, h0: h0 as HoleEnd, h1: h as HoleEnd,
                            o0: holeoff[t] });
            lo = hi;
        }
    }
}

fn load_part(f0: i64, f1: i64) {
    let fi = load_u32("fn_item.u32");      // small (0.5 MB): read whole
    item0 = fi[f0];
    let i1 = fi[f1] as i64;
    for f in f0..f1 + 1 { fitem.push(fi[f] - item0); }
    var kind: u8[>..] = [];
    var ref: u32[>..] = [];
    var seg: u32[>..] = [];
    load("item_kind.u8", kind, item0, i1 - item0);
    load("item_ref.u32", ref, item0, i1 - item0);
    load("item_seg.u32", seg, item0, i1 - item0);
    // the glue runs and nodes this range uses: contiguous (stored in item order)
    var g0 = -1; var g1 = 0; var n0 = -1; var n1 = 0;
    for i in kind.len {
        if kind[i] == 0 { if g0 < 0 { g0 = ref[i]; } g1 = max(g1, ref[i] + 1); }
        else { if n0 < 0 { n0 = ref[i]; } n1 = max(n1, ref[i] + 1); }     // a parent's last segment follows its children
    }
    if g0 < 0 { g0 = 0; g1 = 0; }
    if n0 < 0 { n0 = 0; n1 = 0; }
    for i in kind.len {
        if kind[i] == 0 { items.push(Item.Glue { g: (ref[i] - g0) as u32 }); }
        else { items.push(Item.Piece { n: (ref[i] - n0) as u32, s: seg[i] }); }
    }
    load("node_tmpl.u32", ntmpl, n0, n1 - n0);
    var no: u32[>..] = [];
    load("node_op.u32", no, n0, n1 - n0 + 1);
    let o0 = no[0] as i64;
    let o1 = no[no.len - 1] as i64;
    for x in no { nop.push(x - o0); }
    var ov: i64[>..] = [];
    var oa: i64[>..] = [];
    load("op_val.i64", ov, o0, o1 - o0);
    load("op_aux.i64", oa, o0, o1 - o0);
    for o in ov.len { ops.push(Op { v: ov[o], a: oa[o] }); }
    var go: u64[>..] = [];
    load("glue.off.u64", go, g0, g1 - g0 + 1);
    let b0 = go[0] as i64;
    for x in go { goff.push(x as i64 - b0); }
    load("glue.bin", gbin, b0, go[go.len - 1] as i64 - b0);
    var gr: u32[>..] = [];
    load("glue_rel_off.u32", gr, g0, g1 - g0 + 1);
    let r0 = gr[0] as i64;
    for x in gr { grel0.push(x - r0); }
    load_rels("grel_", grels, r0, gr[gr.len - 1] - r0);
    var fm: u32[>..] = [];
    load("fn_mark_off.u32", fm, f0, f1 - f0 + 1);
    let m0 = fm[0] as i64;
    for x in fm { fmark.push(x - m0); }
    load_rels("mark_", marks, m0, fm[fm.len - 1] - m0);
}

// relocation records [first, first + n) from the grel_* (or mark_*: zero width) columns
fn load_rels(pre: const u8[:], out: GRel[>..]&, first: i64, n: i64) {
    var off: u32[>..] = [];
    var siz: u8[>..] = [];
    var rt: u16[>..] = [];
    var sym: u32[>..] = [];
    var add: i64[>..] = [];
    load(str(pre, "off.u32"), off, first, n);
    load(str(pre, "type.u16"), rt, first, n);
    load(str(pre, "sym.u32"), sym, first, n);
    load(str(pre, "add.i64"), add, first, n);
    if pre == "grel_" { load("grel_siz.u8", siz, first, n); } else { siz.resize(n, 0); }
    for r in n { out.push(GRel { off: off[r], siz: siz[r], rtype: rt[r], sym: sym[r], add: add[r] }); }
}

// ---- 1. lay out: the length of every item, then where it goes
fn layout() -> i64 {
    var pos = 0;
    for i in items.len {
        at[i] = pos;
        pos += match items[i] {
            Glue x => goff[x.g + 1] - goff[x.g],
            Piece x => segs[tseg0[ntmpl[x.n]] + x.s].len,
        };
    }
    at[items.len] = pos;
    return pos;
}

// the low w bytes of v at P
fn put(P: i64, v: i64, w: u8) {
    for k in w { code[P + k] = (v >> (8 * k)) as! u8; }
}

// ---- 2. emit: copy each item's bytes, patch the holes of node segments
fn emit() -> i64 {
    rels.clear();
    var nerr = 0;
    for f in fitem.len - 1 {
        let fstart = at[fitem[f]];
        for m in fmark[f]..fmark[f + 1] {
            let r = marks[m];
            rels.push(Rel { off: fstart + r.off, add: r.add, sym: r.sym, rtype: r.rtype, siz: 0 });
        }
        for i in fitem[f]..fitem[f + 1] {
            let p = at[i];
            match items[i] {
                Glue x => {
                    code[p..at[i + 1]].move_from(gbin[goff[x.g]..goff[x.g + 1]]);
                    for k in grel0[x.g]..grel0[x.g + 1] {
                        let r = grels[k];
                        rels.push(Rel { off: p + r.off, add: r.add, sym: r.sym, rtype: r.rtype, siz: r.siz });
                    }
                },
                Piece x => {
                    let sg = segs[tseg0[ntmpl[x.n]] + x.s];
                    code[p..p + sg.len].move_from(tbin[sg.bin..sg.bin + sg.len]);
                    let o = nop[x.n] - sg.o0;        // operand of hole h: o + h
                    for h in sg.h0..sg.h1 {
                        let hl = holes[h];
                        let op = ops[o + h];
                        let P = p + hl.at;
                        match hl.enc {
                            encInt => put(P, op.v, hl.w),
                            encRel32 => put(P, at[op.v - item0] + op.a - (P + 4), 4),
                            encReloc => rels.push(Rel { off: P, add: op.a, sym: op.v as! u32, rtype: hl.rtype, siz: hl.w }),
                            _ => { nerr++; },
                        }
                    }
                },
            }
        }
    }
    return nerr;
}

// ---- messages
struct Ready { k: i64 }
struct Go { go: bool }
struct Done { k: i64, layout: f64, emit: f64, bytes: i64, nerr: i64 }
struct Out { k: i64, code: u8[], rels: Rel[] }

fn run<T>(k: i64, W: i64, f0: i64, f1: i64, tag: T) {
    load_templates();
    load_part(f0, f1);
    at.resize(items.len + 1, 0);
    let huge = env_set("P4_HUGE");
    let n = layout();
    let hb = fresh_pages(code, n, huge);
    code.resize(n, 0);
    qput(Ready { k: k });
    let reps = env_int("P4_REPS", 1);
    for rep in reps + 1 {
        let g = qget<Mail<T, Go>>();
        let t0 = clock();
        let bytes = layout();
        let t1 = clock();
        let nerr = emit();
        let t2 = clock();
        qput(Done { k: k, layout: ms(t0, t1), emit: ms(t1, t2), bytes: bytes, nerr: nerr });
    }
    let g = qget<Mail<T, Go>>();
    qput(Out { k: k, code: code[..], rels: rels[..] });
}

thread_fn worker(k: i64, W: i64, f0: i64, f1: i64) {
    match k {
        0 => run(k, W, f0, f1, [0 as u8; 1]),
        1 => run(k, W, f0, f1, [0 as u8; 2]),
        2 => run(k, W, f0, f1, [0 as u8; 3]),
        3 => run(k, W, f0, f1, [0 as u8; 4]),
        4 => run(k, W, f0, f1, [0 as u8; 5]),
        5 => run(k, W, f0, f1, [0 as u8; 6]),
        6 => run(k, W, f0, f1, [0 as u8; 7]),
        7 => run(k, W, f0, f1, [0 as u8; 8]),
        _ => abort("at most 8 workers"),
    }
}

fn rel_less(x: Rel, y: Rel) -> bool {
    if x.off != y.off { return x.off < y.off; }
    if x.rtype != y.rtype { return x.rtype < y.rtype; }
    if x.sym != y.sym { return x.sym < y.sym; }
    if x.add != y.add { return x.add < y.add; }
    return x.siz < y.siz;
}

fn main() {
    let W = workers_arg();
    let t0 = clock();
    let reps = env_int("P4_REPS", 1);
    // functions cut where the items' quantiles fall
    let fi = load_u32("fn_item.u32");
    let nfn = fi.len - 1;
    var cut: i64[>..] = [];
    var f = 0;
    for k in W + 1 {
        let want = part_lo(fi[nfn], k, W);
        while f < nfn && fi[f] < want { f++; }
        cut.push(if k == W { nfn } else { f });
    }
    var ids: i64[>..] = [];
    for k in W { ids.push(thread_spawn(worker, k, W, cut[k], cut[k + 1])); }
    for k in W { let r = qget<Ready>(); }
    let t1 = clock();
    // ---- the timed reps: from the go to the last worker done
    var wall: f64[>..] = [];
    var lay: f64[>..] = [];
    var emt: f64[>..] = [];
    var size: i64[>..] = [];
    size.resize(W, 0);
    var nerr = 0;
    for rep in reps + 1 {
        let ta = clock();
        for j in W { send(j, Go { go: true }); }
        var lmax = 0.0;
        var emax = 0.0;
        nerr = 0;
        for j in W {
            let d = qget<Done>();
            lmax = max(lmax, d.layout);
            emax = max(emax, d.emit);
            size[d.k] = d.bytes;
            nerr += d.nerr;
        }
        let tb = clock();
        if rep > 0 { wall.push(ms(ta, tb)); lay.push(lmax); emt.push(emax); }
    }
    let t2 = clock();
    // ---- join: the buffers in order into one image, relocations shifted to their worker's base
    var base: i64[>..] = [];
    var total = 0;
    for j in W { base.push(total); total += size[j]; }
    var img: u8[>..] = [];
    img.resize(total, 0);
    var rel: Rel[>..] = [];
    var parts: i64[>..] = [];
    for j in W { send(j, Go { go: true }); }
    for j in W {
        let o = qget<Out>();
        img[base[o.k]..base[o.k] + o.code.len].move_from(o.code[..]);
        parts.push(o.k);
        parts.push(rel.len);
        let b = base[o.k];
        for r in o.rels { rel.push(Rel { off: r.off + b, add: r.add, sym: r.sym, rtype: r.rtype, siz: r.siz }); }
    }
    let t3 = clock();
    for id in ids { thread_wait(id); }
    // ---- check against the oracle
    var bad = nerr;
    var want: u8[>..] = [];
    load("oracle/p4_code.bin", want);
    let wfn = load_u64("oracle/p4_fn_off.u64");
    if img.len != want.len { print("code: ", img.len, " bytes, oracle ", want.len); bad++; }
    else if img[..] != want[..] {
        var nf = 0;
        for g in nfn { if img[wfn[g]..wfn[g + 1]] != want[wfn[g]..wfn[g + 1]] { if nf < 5 { print("  function ", g, " differs"); } nf++; } }
        print("code: ", nf, " functions differ");
        bad++;
    }
    let ts = clock();
    sort(rel) { x, y => rel_less(x, y) };
    let wo = load_u64("oracle/p4_rel_off.u64");
    let wsiz = load_u8("oracle/p4_rel_siz.u8");
    let wtype = load_u16("oracle/p4_rel_type.u16");
    let wsym = load_u32("oracle/p4_rel_sym.u32");
    let wadd = load_i64("oracle/p4_rel_add.i64");
    var wr: Rel[>..] = [];
    for i in wo.len { wr.push(Rel { off: wo[i] as i64, add: wadd[i], sym: wsym[i], rtype: wtype[i], siz: wsiz[i] }); }
    sort(wr) { x, y => rel_less(x, y) };
    var rbad = if rel.len != wr.len { 1 } else { 0 };
    for i in min(rel.len, wr.len) {
        let x = rel[i];
        let y = wr[i];
        if x.off != y.off || x.add != y.add || x.sym != y.sym || x.rtype != y.rtype || x.siz != y.siz {
            if rbad < 5 { print("  relocation ", i, " differs (offset ", x.off, ")"); }
            rbad++;
        }
    }
    bad += rbad;
    var note: u8[>..] = [];
    format(note, "p4 goose (", W, " workers, x", reps, "): ");
    report(note, "spawn+load", t0, t1);
    spread(note, "rep wall", wall);
    spread(note, "layout", lay);
    spread(note, "emit", emt);
    report(note, "join", t2, t3);
    format(note, img.len, " bytes, ", rel.len, " relocations (", rbad, " differ); ");
    report(note, "check sort", ts, clock());
    print(note);
    print(if bad == 0 { "P4 OK" } else { "P4 FAIL" });
    if bad != 0 { exit(1); }
}
