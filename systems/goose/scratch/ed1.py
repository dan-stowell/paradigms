import sys
p=sys.argv[1]; s=open(p).read()
def rep(a,b):
    global s
    assert a in s, a[:60]
    s=s.replace(a,b,1)
rep('''struct Done { k: i64, layout: f64, emit: f64, bytes: i64, pieces: i64, nerr: i64 }
struct Out { k: i64, code: u8[], rels: Rel[] }
''','''struct Done { k: i64, layout: f64, emit: f64, bytes: i64, nrel: i64, pieces: i64, nerr: i64 }
struct Place { file: bool, base: i64, rbase: i64 }      // where this worker's bytes and relocations go
struct Out { k: i64, ms: f64, code: u8[], rels: Rel[] }

// the output files (flatio.c): created by main, written by each worker at its own offset
extern fn flat_create(path: const u8[:], size: i64) -> bool;
extern fn flat_write_at(path: const u8[:], data: const u8[:], elem: i64, at: i64) -> bool;
extern fn flat_write_at(path: const u8[:], data: const Rel[:], elem: i64, at: i64) -> bool = "flat_write_at";
extern fn flat_read(path: const u8[:], out: Rel[>..]&, elem: i64) -> bool = "flat_read";
let relBytes = 23;              // a Rel, packed
fn out_path(what: const u8[:]) -> u8[>..] {
    var d: u8[>..] = [];
    if !env("P4_OUT", d) { d.append("/tmp"); }
    return str(d, "/goose_p4_", what, ".bin");
}
''')
rep('''bytes: bytes, pieces: items.len, nerr: nerr });
    }
    let g = qget<Mail<T, Go>>();
    qput(Out { k: k, code: code[..], rels: rels[..] });''','''bytes: bytes, nrel: rels.len, pieces: items.len,
                    nerr: nerr });
    }
    // ---- join: write this worker's bytes and relocations (shifted to its base) into the output files, or send them
    let pl = qget<Mail<T, Place>>();
    let t3 = clock();
    if pl.m.file {
        for r in rels.len { rels[r].off += pl.m.base; }
        guard flat_write_at(out_path("code"), code[..], 1, pl.m.base) && flat_write_at(out_path("rel"), rels[..], relBytes, pl.m.rbase * relBytes)
        else { abort("cannot write the output"); }
        qput(Out { k: k, ms: ms(t3, clock()), code: [], rels: [] });
    } else {
        qput(Out { k: k, ms: 0.0, code: code[..], rels: rels[..] });
    }''')
rep('''    var size: i64[>..] = [];
    size.resize(W, 0);''','''    var size: i64[>..] = [];
    size.resize(W, 0);
    var nrel: i64[>..] = [];
    nrel.resize(W, 0);''')
rep('''            size[d.k] = d.bytes;''','''            size[d.k] = d.bytes;
            nrel[d.k] = d.nrel;''')
rep('''    // ---- join: the buffers in order into one image, relocations shifted to their worker's base
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
    for id in ids { thread_wait(id); }''','''    // ---- join: the workers' parts in order into one image (P4_JOIN=queue: sent to main; else written to files)
    let file = !env_set("P4_JOIN");
    var base: i64[>..] = [];
    var rbase: i64[>..] = [];
    var total = 0;
    var rtotal = 0;
    for j in W { base.push(total); total += size[j]; rbase.push(rtotal); rtotal += nrel[j]; }
    var img: u8[>..] = [];
    var rel: Rel[>..] = [];
    var wmax = 0.0;
    if file {
        guard flat_create(out_path("code"), total) && flat_create(out_path("rel"), rtotal * relBytes) else { abort("cannot create the output"); }
    } else {
        img.resize(total, 0);
    }
    let t2a = clock();
    for j in W { send(j, Place { file: file, base: base[j], rbase: rbase[j] }); }
    for j in W {
        let o = qget<Out>();
        wmax = max(wmax, o.ms);
        if file { continue; }
        img[base[o.k]..base[o.k] + o.code.len].move_from(o.code[..]);
        let b = base[o.k];
        for r in o.rels { rel.push(Rel { off: r.off + b, add: r.add, sym: r.sym, rtype: r.rtype, siz: r.siz }); }
    }
    let t3 = clock();
    for id in ids { thread_wait(id); }
    if file {
        guard flat_read(out_path("code"), img, 1) && flat_read(out_path("rel"), rel, relBytes) else { abort("cannot read the output back"); }
    }''')
rep('''    report(note, "join", t2, t3);''','''    report(note, if file { "join (files)" } else { "join (queue)" }, t2a, t3);
    format(note, "(slowest worker ");
    format_flt(note, wmax, 1);
    format(note, " ms)  ");''')
open(p,'w').write(s)
