// Loading problem 2's flat files into plain fact vectors (the input relations of the dataflow), the id
// unification between two dumps (version A and an edited version B), and set diffs between fact vectors.
// This is file decoding only: no rule of problem 2 is applied here.
use std::collections::HashMap;

pub type Id = u32;
/// A method of a type: (name string, method func type, interface-call entry, direct entry, exported).
pub type Meth = (u32, Id, Id, Id, u8);
/// An edge's payload: (kind, value). Kind 0 plain and 1 useiface: value = target id; kind 2 ifacemethod:
/// value = name string << 32 | func type id (the signature decoded); kind 3 namedmethod: value = name string.
pub type EdgeVal = (u8, u64);

#[derive(Default, Clone)]
pub struct Facts {
    pub roots: Vec<Id>,
    pub edges: Vec<(Id, EdgeVal)>,
    pub flags: Vec<(Id, u8)>, // only the bits the rules read (1 def, 4 reflect, 8 gotype, 16 iface), nonzero
    pub methods: Vec<(Id, Meth)>,
}

pub fn rd(p: &str) -> Vec<u8> {
    std::fs::read(p).unwrap_or_else(|e| panic!("{p}: {e}"))
}
pub fn rd_u32(p: &str) -> Vec<u32> {
    rd(p).chunks_exact(4).map(|c| u32::from_le_bytes(c.try_into().unwrap())).collect()
}
pub fn rd_u64(p: &str) -> Vec<u64> {
    rd(p).chunks_exact(8).map(|c| u64::from_le_bytes(c.try_into().unwrap())).collect()
}
pub fn meta(dir: &str, key: &str) -> u64 {
    let s = std::fs::read_to_string(format!("{dir}/meta.txt")).unwrap();
    s.lines().find_map(|l| l.strip_prefix(&format!("{key} ")).map(|v| v.trim().parse().unwrap())).unwrap()
}

/// Load the P2 facts of `dir`; `idm`/`strm` map this dump's ids and method-name strings into a shared space
/// (identity when None).
pub fn load(dir: &str, idm: Option<&[u32]>, strm: Option<&[u32]>) -> Facts {
    let id = |x: u32| idm.map_or(x, |m| m[x as usize]);
    let st = |x: u32| strm.map_or(x, |m| m[x as usize]);
    let off = rd_u32(&format!("{dir}/p2_edge_off.u32"));
    let dst = rd_u32(&format!("{dir}/p2_edge_dst.u32"));
    let kind = rd(&format!("{dir}/p2_edge_kind.u8"));
    let sig_name = rd_u32(&format!("{dir}/p2_sig_name.u32"));
    let sig_type = rd_u32(&format!("{dir}/p2_sig_type.u32"));
    let fl = rd(&format!("{dir}/p2_flags.u8"));
    let moff = rd_u32(&format!("{dir}/p2_meth_off.u32"));
    let (mn, mt) = (rd_u32(&format!("{dir}/p2_meth_name.u32")), rd_u32(&format!("{dir}/p2_meth_mtyp.u32")));
    let (mi, mf) = (rd_u32(&format!("{dir}/p2_meth_ifn.u32")), rd_u32(&format!("{dir}/p2_meth_tfn.u32")));
    let mx = rd(&format!("{dir}/p2_meth_exported.u8"));
    let mut f = Facts::default();
    f.roots = rd_u32(&format!("{dir}/p2_roots.u32")).into_iter().map(id).collect();
    f.edges.reserve(dst.len());
    for s in 0..off.len() - 1 {
        for e in off[s] as usize..off[s + 1] as usize {
            let (k, t) = (kind[e], dst[e]);
            let v = match k {
                0 | 1 => {
                    if t == 0 {
                        continue;
                    }
                    id(t) as u64
                }
                2 => (st(sig_name[t as usize]) as u64) << 32 | id(sig_type[t as usize]) as u64,
                _ => st(t) as u64,
            };
            f.edges.push((id(s as u32), (k, v)));
        }
    }
    for (g, &b) in fl.iter().enumerate() {
        if b & 0x1d != 0 {
            f.flags.push((id(g as u32), b & 0x1d));
        }
    }
    for t in 0..moff.len() - 1 {
        for m in moff[t] as usize..moff[t + 1] as usize {
            f.methods.push((id(t as u32), (st(mn[m]), id(mt[m]), id(mi[m]), id(mf[m]), mx[m])));
        }
    }
    f
}

impl Facts {
    pub fn sort(&mut self) {
        fn sd<T: Ord>(v: &mut Vec<T>) {
            v.sort_unstable();
            v.dedup();
        }
        sd(&mut self.roots);
        sd(&mut self.edges);
        sd(&mut self.flags);
        sd(&mut self.methods);
    }
    pub fn len(&self) -> usize {
        self.roots.len() + self.edges.len() + self.flags.len() + self.methods.len()
    }
}

/// (minus, plus) of two sorted, deduplicated vectors: a \ b and b \ a.
pub fn diff<T: Ord + Clone>(a: &[T], b: &[T]) -> (Vec<T>, Vec<T>) {
    let (mut i, mut j, mut minus, mut plus) = (0, 0, vec![], vec![]);
    while i < a.len() || j < b.len() {
        if j == b.len() || (i < a.len() && a[i] < b[j]) {
            minus.push(a[i].clone());
            i += 1;
        } else if i == a.len() || b[j] < a[i] {
            plus.push(b[j].clone());
            j += 1;
        } else {
            i += 1;
            j += 1;
        }
    }
    (minus, plus)
}

pub struct FactDiff {
    pub minus: Facts,
    pub plus: Facts,
}
pub fn fact_diff(a: &Facts, b: &Facts) -> FactDiff {
    let (mut minus, mut plus) = (Facts::default(), Facts::default());
    (minus.roots, plus.roots) = diff(&a.roots, &b.roots);
    (minus.edges, plus.edges) = diff(&a.edges, &b.edges);
    (minus.flags, plus.flags) = diff(&a.flags, &b.flags);
    (minus.methods, plus.methods) = diff(&a.methods, &b.methods);
    FactDiff { minus, plus }
}

// ---------------------------------------------------------------- id unification
// A slot is identified across dumps by (its object's index, kind, name bytes, 128-bit name key, 128-bit content
// hash key, ordinal among equal keys in that object). The two dumps' objects are in importcfg order (sorted by
// package path); `unify` checks that the object counts agree and reports how many objects' slot lists differ.
// Linker-made ids (above nslots) are matched by position. Method-name strings are matched by content.

fn slot_keys(dir: &str) -> (Vec<u32>, Vec<u8>, Vec<u64>, Vec<u8>, Vec<u64>, Vec<u64>) {
    (
        rd_u32(&format!("{dir}/slot_obj.u32")),
        rd(&format!("{dir}/p1_kind.u8")),
        rd_u64(&format!("{dir}/name.off.u64")),
        rd(&format!("{dir}/name.bin")),
        rd_u64(&format!("{dir}/p1_namekey.u64x2")),
        rd_u64(&format!("{dir}/p1_hashkey.u64x2")),
    )
}

type SlotKey<'a> = (u32, u8, &'a [u8], u64, u64, u64, u64, u32);

fn keys_of<'a>(n: usize, k: &'a (Vec<u32>, Vec<u8>, Vec<u64>, Vec<u8>, Vec<u64>, Vec<u64>)) -> Vec<SlotKey<'a>> {
    let (obj, kind, noff, nbin, nk, hk) = k;
    let mut ord: HashMap<(u32, u8, &[u8], u64, u64, u64, u64), u32> = HashMap::new();
    let mut out = Vec::with_capacity(n + 1);
    out.push((u32::MAX, 0, &nbin[0..0], 0, 0, 0, 0, 0)); // slot 0
    for g in 1..=n {
        let name = &nbin[noff[g - 1] as usize..noff[g] as usize];
        let base = (obj[g], kind[g], name, nk[2 * g], nk[2 * g + 1], hk[2 * g], hk[2 * g + 1]);
        let c = ord.entry(base).or_insert(0);
        out.push((base.0, base.1, base.2, base.3, base.4, base.5, base.6, *c));
        *c += 1;
    }
    out
}

fn strings(dir: &str) -> Vec<Vec<u8>> {
    let off = rd_u64(&format!("{dir}/p2_str.off.u64"));
    let bin = rd(&format!("{dir}/p2_str.bin"));
    (0..off.len() - 1).map(|i| bin[off[i] as usize..off[i + 1] as usize].to_vec()).collect()
}

fn per_obj<'a, 'b>(s: &'b [SlotKey<'a>], nobj: usize) -> Vec<Vec<&'b SlotKey<'a>>> {
    let mut v: Vec<Vec<&SlotKey>> = vec![vec![]; nobj];
    for k in &s[1..] {
        v[k.0 as usize].push(k);
    }
    v
}

pub struct Unified {
    pub idm_b: Vec<u32>,  // B id -> unified id (A ids are unified ids; B-only slots get ids >= nids_a)
    pub strm_b: Vec<u32>, // B string -> unified string
    pub n_unified: u32,
    pub a_only: usize,
    pub b_only: usize,
    pub objs_differ: usize,
}

pub fn unify(da: &str, db: &str) -> Unified {
    let (na, nb) = (meta(da, "nslots") as usize, meta(db, "nslots") as usize);
    let (ida, idb) = (meta(da, "nids") as usize, meta(db, "nids") as usize);
    assert_eq!(meta(da, "nobjs"), meta(db, "nobjs"), "object counts differ: key by object index is unsound");
    assert_eq!(ida - na, idb - nb, "linker-made id counts differ");
    let (ka, kb) = (slot_keys(da), slot_keys(db));
    let (sa, sb) = (keys_of(na, &ka), keys_of(nb, &kb));
    // objects whose slot key lists differ
    let nobj = meta(da, "nobjs") as usize;
    let (oa, ob) = (per_obj(&sa, nobj), per_obj(&sb, nobj));
    let objs_differ = (0..nobj).filter(|&o| oa[o] != ob[o]).count();
    let index: HashMap<&SlotKey, u32> = sa.iter().enumerate().skip(1).map(|(g, k)| (k, g as u32)).collect();
    let mut idm_b = vec![0u32; idb];
    let mut next = ida as u32;
    let mut b_only = 0;
    let mut matched = vec![false; ida];
    for g in 1..=nb {
        idm_b[g] = match index.get(&sb[g]) {
            Some(&a) => {
                matched[a as usize] = true;
                a
            }
            None => {
                b_only += 1;
                next += 1;
                next - 1
            }
        };
    }
    for k in 0..(idb - nb - 1) {
        idm_b[nb + 1 + k] = (na + 1 + k) as u32;
        matched[na + 1 + k] = true;
    }
    let a_only = (1..=na).filter(|&g| !matched[g]).count();
    // strings
    let (stra, strb) = (strings(da), strings(db));
    let sidx: HashMap<&[u8], u32> = stra.iter().enumerate().map(|(i, s)| (s.as_slice(), i as u32)).collect();
    let mut snext = stra.len() as u32;
    let strm_b = strb
        .iter()
        .map(|s| {
            sidx.get(s.as_slice()).copied().unwrap_or_else(|| {
                snext += 1;
                snext - 1
            })
        })
        .collect();
    Unified { idm_b, strm_b, n_unified: next, a_only, b_only, objs_differ }
}
