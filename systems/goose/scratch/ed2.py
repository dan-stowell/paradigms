import sys
p=sys.argv[1]; s=open(p).read()
i=s.index('''    for id in ids { thread_wait(id); }
    if file {''')
tail=s[i:]
s=s[:i]
new_main_tail='''    for id in ids { thread_wait(id); }
    var note: u8[>..] = [];
    format(note, "p4 goose (", W, " workers, x", reps, "): ");
    report(note, "spawn+load", t0, t1);
    spread(note, "rep wall", wall);
    spread(note, "layout", lay);
    spread(note, "emit", emt);
    report(note, if file { "join (files)" } else { "join (queue)" }, t2a, t3);
    format(note, "(slowest worker ");
    format_flt(note, wmax, 1);
    format(note, " ms)  ", total, " bytes, ", pieces, " pieces, ", rtotal, " relocations");
    print(note);
    guard !env_set("P4_NOCHECK") else { return; }      // e2e without the check: the output is the files
    if file {
        guard flat_read(out_path("code"), img, 1) && flat_read(out_path("rel"), rel, relBytes) else { abort("cannot read the output back"); }
    }
    let bad = nerr + check(img, rel);
    print(if bad == 0 { "P4 OK" } else { "P4 FAIL" });
    if bad != 0 { exit(1); }
}
'''
check='''
// the code byte for byte against oracle/p4_code.bin, the relocations as a sorted list against oracle/p4_rel_*
fn check(img: u8[:], rel: Rel[>..]&) -> i64 {
    var bad = 0;
    var want: u8[>..] = [];
    load("oracle/p4_code.bin", want);
    let wfn = load_u64("oracle/p4_fn_off.u64");
    if img.len != want.len { print("code: ", img.len, " bytes, oracle ", want.len); bad++; }
    else if img != want[..] {
        var nf = 0;
        for g in wfn.len - 1 { if img[wfn[g]..wfn[g + 1]] != want[wfn[g]..wfn[g + 1]] { if nf < 5 { print("  function ", g, " differs"); } nf++; } }
        print("code: ", nf, " functions differ");
        bad++;
    }
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
    print("code: ", img.len, " bytes; relocations: ", rel.len, ", oracle ", wr.len, ", ", rbad, " differ");
    return bad + rbad;
}
'''
j=s.index('fn main() {')
s=s[:j]+check.lstrip('\n')+'\n'+s[j:]+new_main_tail
open(p,'w').write(s)
