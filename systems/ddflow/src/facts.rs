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
// A slot is identified across dumps by (its object, kind, name bytes, 128-bit name key, 128-bit content hash
// key, ordinal among equal keys in that object). Both dumps list objects in importcfg order (sorted by package
// path) and the object counts must agree. An object whose slot list is identical maps by position; only the
// objects that differ are matched key by key. Linker-made ids (above nslots) map by position. Method-name
// strings map by content.

pub struct Slots {
    kind: Vec<u8>,
    noff: Vec<u64>,
    nbin: Vec<u8>,
    nk: Vec<u64>,
    hk: Vec<u64>,
    rank: Vec<u32>,
    base: Vec<u32>, // per object, plus nslots+1 at the end
}
impl Slots {
    pub fn load(dir: &str) -> Slots {
        let mut base = rd_u32(&format!("{dir}/obj_base.u32"));
        base.push(meta(dir, "nslots") as u32 + 1);
        Slots {
            kind: rd(&format!("{dir}/p1_kind.u8")),
            noff: rd_u64(&format!("{dir}/name.off.u64")),
            nbin: rd(&format!("{dir}/name.bin")),
            nk: rd_u64(&format!("{dir}/p1_namekey.u64x2")),
            hk: rd_u64(&format!("{dir}/p1_hashkey.u64x2")),
            rank: rd_u32(&format!("{dir}/p1_rank.u32")),
            base,
        }
    }
    pub fn name(&self, g: u32) -> &[u8] {
        let g = g as usize;
        &self.nbin[self.noff[g - 1] as usize..self.noff[g] as usize]
    }
    fn key(&self, g: u32) -> (u8, &[u8], u64, u64, u64, u64, u32) {
        let i = g as usize;
        (self.kind[i], self.name(g), self.nk[2 * i], self.nk[2 * i + 1], self.hk[2 * i], self.hk[2 * i + 1], if self.name(g).is_empty() { self.rank[i] } else { 0 })
    }
}

fn strings(dir: &str) -> Vec<Vec<u8>> {
    let off = rd_u64(&format!("{dir}/p2_str.off.u64"));
    let bin = rd(&format!("{dir}/p2_str.bin"));
    (0..off.len() - 1).map(|i| bin[off[i] as usize..off[i + 1] as usize].to_vec()).collect()
}

/// The matched index pairs of a shortest edit script between a and b (Myers' O(ND) diff).
fn myers<T: Eq>(a: &[T], b: &[T]) -> Vec<(usize, usize)> {
    let (n, m) = (a.len() as isize, b.len() as isize);
    let off = (n + m + 1) as usize;
    let mut v = vec![0isize; 2 * off + 1];
    let mut trace: Vec<Vec<isize>> = vec![]; // trace[d] = v[-d..=d] before step d
    let at = |k: isize| (k + off as isize) as usize;
    let mut dmax = 0;
    'outer: for d in 0..=(n + m) {
        trace.push(v[at(-d)..=at(d)].to_vec());
        let mut k = -d;
        while k <= d {
            let mut x = if k == -d || (k != d && v[at(k - 1)] < v[at(k + 1)]) { v[at(k + 1)] } else { v[at(k - 1)] + 1 };
            let mut y = x - k;
            while x < n && y < m && a[x as usize] == b[y as usize] {
                x += 1;
                y += 1;
            }
            v[at(k)] = x;
            if x >= n && y >= m {
                dmax = d;
                break 'outer;
            }
            k += 2;
        }
    }
    let (mut x, mut y, mut pairs) = (n, m, vec![]);
    for d in (1..=dmax).rev() {
        let tv = &trace[d as usize];
        let g = |k: isize| tv[(k + d) as usize];
        let k = x - y;
        let pk = if k == -d || (k != d && g(k - 1) < g(k + 1)) { k + 1 } else { k - 1 };
        let px = g(pk);
        let py = px - pk;
        let (sx, sy) = if pk == k + 1 { (px, py + 1) } else { (px + 1, py) }; // after the edit step
        while x > sx && y > sy {
            x -= 1;
            y -= 1;
            pairs.push((x as usize, y as usize));
        }
        x = px;
        y = py;
    }
    while x > 0 && y > 0 {
        x -= 1;
        y -= 1;
        pairs.push((x as usize, y as usize));
    }
    pairs
}

pub struct Unified {
    pub idm_b: Vec<u32>,  // B id -> unified id (A ids are unified ids; B-only slots get ids from nids_a up)
    pub strm_b: Vec<u32>, // B string -> unified string
    pub a_only: Vec<u32>, // A slots with no B counterpart
    pub b_only: Vec<u32>, // B slots (B ids) with no A counterpart
    pub objs_differ: Vec<usize>,
    pub sa: Slots,
    pub sb: Slots,
}

pub fn unify(da: &str, db: &str) -> Unified {
    let (na, nb) = (meta(da, "nslots") as usize, meta(db, "nslots") as usize);
    let (ida, idb) = (meta(da, "nids") as usize, meta(db, "nids") as usize);
    let nobj = meta(da, "nobjs") as usize;
    assert_eq!(nobj as u64, meta(db, "nobjs"), "object counts differ: matching by object index is unsound");
    assert_eq!(ida - na, idb - nb, "linker-made id counts differ");
    let (sa, sb) = (Slots::load(da), Slots::load(db));
    let mut idm_b = vec![0u32; idb];
    let (mut next, mut a_only, mut b_only, mut objs_differ) = (ida as u32, vec![], vec![], vec![]);
    for o in 0..nobj {
        let (a0, a1, b0, b1) = (sa.base[o], sa.base[o + 1], sb.base[o], sb.base[o + 1]);
        if a1 - a0 == b1 - b0 && (0..a1 - a0).all(|j| sa.key(a0 + j) == sb.key(b0 + j)) {
            for j in 0..a1 - a0 {
                idm_b[(b0 + j) as usize] = a0 + j;
            }
            continue;
        }
        objs_differ.push(o);
        // 1. a key that occurs once in each version (a named symbol, a content hash) is the same symbol,
        //    wherever the compiler put it; 2. the rest (unnamed symbols: DWARF ranges, ...; duplicate keys) are
        //    aligned in order, as a text diff aligns lines (Myers), between and around those.
        let ka: Vec<_> = (a0..a1).map(|g| sa.key(g)).collect();
        let kb: Vec<_> = (b0..b1).map(|g| sb.key(g)).collect();
        let mut ma = vec![false; ka.len()];
        let mut mb = vec![false; kb.len()];
        let mut count: HashMap<_, (u32, u32, usize)> = HashMap::new();
        for (i, k) in ka.iter().enumerate() {
            let e = count.entry(*k).or_insert((0, 0, 0));
            e.0 += 1;
            e.2 = i;
        }
        for k in &kb {
            count.entry(*k).or_insert((0, 0, 0)).1 += 1;
        }
        for (j, k) in kb.iter().enumerate() {
            let &(ca, cb, i) = &count[k];
            if ca == 1 && cb == 1 && (!k.1.is_empty() || k.4 != 0) {
                ma[i] = true;
                mb[j] = true;
                idm_b[(b0 as usize) + j] = a0 + i as u32;
            }
        }
        let ra: Vec<usize> = (0..ka.len()).filter(|&i| !ma[i]).collect();
        let rb: Vec<usize> = (0..kb.len()).filter(|&j| !mb[j]).collect();
        let (xa, xb): (Vec<_>, Vec<_>) = (ra.iter().map(|&i| ka[i]).collect(), rb.iter().map(|&j| kb[j]).collect());
        for (i, j) in myers(&xa, &xb) {
            let (i, j) = (ra[i], rb[j]);
            ma[i] = true;
            mb[j] = true;
            idm_b[(b0 as usize) + j] = a0 + i as u32;
        }
        for j in 0..kb.len() {
            if !mb[j] {
                b_only.push(b0 + j as u32);
                idm_b[(b0 as usize) + j] = next;
                next += 1;
            }
        }
        a_only.extend((0..ka.len()).filter(|&i| !ma[i]).map(|i| a0 + i as u32));
    }
    for k in 0..(idb - nb - 1) {
        idm_b[nb + 1 + k] = (na + 1 + k) as u32;
    }
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
    Unified { idm_b, strm_b, a_only, b_only, objs_differ, sa, sb }
}

impl Unified {
    /// a readable name for a unified id
    pub fn name(&self, u: u32) -> String {
        let na = self.sa.base[self.sa.base.len() - 1];
        let show = |sl: &Slots, g: u32, pre: &str| {
            let n = String::from_utf8_lossy(sl.name(g)).to_string();
            if n.is_empty() { format!("{pre}(unnamed, kind {}, id {u})", sl.kind[g as usize]) } else { format!("{pre}{n}") }
        };
        if u < na {
            show(&self.sa, u, "")
        } else if let Some(g) = self.b_only.iter().find(|&&g| self.idm_b[g as usize] == u) {
            show(&self.sb, *g, "(B) ")
        } else {
            format!("(linker-made {u})")
        }
    }
}
