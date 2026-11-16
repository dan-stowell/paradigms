// ddflow writediff DIR_A DIR_B: write the unified problem-2 diff of B against A as flat files in DIR_B/diff/,
// so any system can apply it (README.md there says what each file is).
use std::io::Write;

use crate::facts::{self, rd_u32, Facts};

fn w<T: Copy>(dir: &str, name: &str, v: &[T]) {
    let b = unsafe { std::slice::from_raw_parts(v.as_ptr() as *const u8, std::mem::size_of_val(v)) };
    std::fs::write(format!("{dir}/{name}"), b).unwrap();
}

fn put(dir: &str, side: &str, f: &Facts) {
    w(dir, &format!("edge_{side}_src.u32"), &f.edges.iter().map(|e| e.0).collect::<Vec<u32>>());
    w(dir, &format!("edge_{side}_kind.u8"), &f.edges.iter().map(|e| e.1 .0).collect::<Vec<u8>>());
    w(dir, &format!("edge_{side}_val.u64"), &f.edges.iter().map(|e| e.1 .1).collect::<Vec<u64>>());
    w(dir, &format!("flag_{side}_id.u32"), &f.flags.iter().map(|e| e.0).collect::<Vec<u32>>());
    w(dir, &format!("flag_{side}_val.u8"), &f.flags.iter().map(|e| e.1).collect::<Vec<u8>>());
    w(dir, &format!("root_{side}.u32"), &f.roots);
    w(dir, &format!("meth_{side}_type.u32"), &f.methods.iter().map(|e| e.0).collect::<Vec<u32>>());
    w(dir, &format!("meth_{side}_name.u32"), &f.methods.iter().map(|e| e.1 .0).collect::<Vec<u32>>());
    w(dir, &format!("meth_{side}_mtyp.u32"), &f.methods.iter().map(|e| e.1 .1).collect::<Vec<u32>>());
    w(dir, &format!("meth_{side}_ifn.u32"), &f.methods.iter().map(|e| e.1 .2).collect::<Vec<u32>>());
    w(dir, &format!("meth_{side}_tfn.u32"), &f.methods.iter().map(|e| e.1 .3).collect::<Vec<u32>>());
    w(dir, &format!("meth_{side}_exported.u8"), &f.methods.iter().map(|e| e.1 .4).collect::<Vec<u8>>());
}

const README: &str = r#"# Problem 2 diff of this dataset (B) against data/minio (A), in A's id space

Written by `systems/ddflow`: `ddflow writediff /home/exedev/paradigms/data/minio <this dataset>`. Raw
little-endian arrays, no header, as in ref/flat/README.md.

**Ids.** The unified id space is A's: an A id is itself. `idmap_b.u32` (`nids_b` entries) maps every B id to its
unified id; B slots with no A counterpart get ids `nids_a .. n_unified-1`. Ids are matched by object, then by
unique (kind, name, name key, content hash) and a Myers alignment of the rest (systems/ddflow/src/facts.rs).
`strmap_b.u32` maps B's `p2_str` ids to unified string ids (A's, then new ones from `nstr_a`).

**Facts.** The same facts problem 2's rules read, as sets (deduplicated), in unified ids:
- edges `(src, kind, val)`: kind 0 plain and 1 useiface: val = target id; kind 2 ifacemethod: val =
  name string << 32 | func type id (the signature decoded through p2_sig_name/p2_sig_type); kind 3 namedmethod:
  val = name string. Edges with a zero target are not facts.
- flags `(id, bits)`: p2_flags masked to 0x1d (1 def, 4 reflect, 8 gotype, 16 iface); a change is minus old, plus new.
- roots; method-table rows `(type, name, mtyp, ifn, tfn, exported)`.

`*_minus_*`: in A, not in B. `*_plus_*`: in B, not in A. Applying minus then plus to A's facts gives B's.
`live_b.u32`: B's oracle/p2_live.u32 mapped through idmap_b, sorted (to check a result in unified ids).
`meta.txt`: the counts.
"#;

pub fn main(pos: &[String]) {
    let (da, db) = (&pos[0], &pos[1]);
    let u = facts::unify(da, db);
    let mut a = facts::load(da, None, None);
    let mut b = facts::load(db, Some(&u.idm_b), Some(&u.strm_b));
    a.sort();
    b.sort();
    let d = facts::fact_diff(&a, &b);
    let out = format!("{db}/diff");
    std::fs::create_dir_all(&out).unwrap();
    w(&out, "idmap_b.u32", &u.idm_b);
    w(&out, "strmap_b.u32", &u.strm_b);
    put(&out, "minus", &d.minus);
    put(&out, "plus", &d.plus);
    let mut lb: Vec<u32> = rd_u32(&format!("{db}/oracle/p2_live.u32")).iter().map(|&x| u.idm_b[x as usize]).collect();
    lb.sort_unstable();
    w(&out, "live_b.u32", &lb);
    let nids_a = facts::meta(da, "nids");
    let n_unified = u.idm_b.iter().copied().max().unwrap().max(nids_a as u32 - 1) + 1;
    let nstr_a = std::fs::metadata(format!("{da}/p2_str.off.u64")).unwrap().len() / 8 - 1;
    let nstr_u = u.strm_b.iter().copied().max().unwrap_or(0).max(nstr_a as u32 - 1) + 1;
    let mut m = std::fs::File::create(format!("{out}/meta.txt")).unwrap();
    writeln!(m, "nids_a {nids_a}\nnids_b {}\nn_unified {n_unified}\nnstr_a {nstr_a}\nnstr_unified {nstr_u}", u.idm_b.len()).unwrap();
    for (s, f) in [("minus", &d.minus), ("plus", &d.plus)] {
        writeln!(m, "edge_{s} {}\nflag_{s} {}\nroot_{s} {}\nmeth_{s} {}", f.edges.len(), f.flags.len(), f.roots.len(), f.methods.len()).unwrap();
    }
    writeln!(m, "live_b {}", lb.len()).unwrap();
    std::fs::write(format!("{out}/README.md"), README).unwrap();
    println!("wrote {out}: edges -{} +{}, flags -{} +{}, n_unified {n_unified}", d.minus.edges.len(), d.plus.edges.len(), d.minus.flags.len(), d.plus.flags.len());
}
