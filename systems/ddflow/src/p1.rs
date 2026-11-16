// Problem 1a+1b (resolve: the join by name, then relocation targets) as a dataflow, to measure what an
// incremental resolve costs. The rules are ref/flat/README.md's (p1.c), with one change of representation that
// incrementality needs: a winner's tie-break "lowest id" is taken as "lowest (object, index in object)", which
// orders exactly like the dump's ids but does not move when another object grows.
use std::collections::HashMap;
use std::sync::{Arc, Mutex};
use std::time::Instant;

use differential_dataflow::input::{Input, InputSession};
use differential_dataflow::VecCollection;
use timely::dataflow::operators::probe::Handle;

use crate::facts::{diff, meta, rd, rd_u32, rd_u64, Id};

type Time = u64;
type Diff = i32;
type Key = (u64, u64);
/// a member of a name group: (rank, !object, !index in object, id); the maximum is the winner
type Member = (u32, u32, u32, Id);
type Reloc = (Id, u32); // (source, its relocation number)
const RANK_DEF: u32 = 1 << 29;
const BUILTIN: u64 = 1 << 32;

#[derive(Default, Clone)]
pub struct P1Facts {
    named: Vec<(Key, Member)>,  // kinds 4 and 5 with a key
    cand: Vec<(Key, Member)>,   // kind 1 with a key: joins only a group that exists
    want: Vec<Key>,             // names the linker looks up
    bkey: Vec<(Key, u32)>,      // builtin i's key
    plain: Vec<(Id, Id)>,       // slots outside the join: (id, its canonical id: itself, or 0 for a debug-only reference)
    relocs: Vec<(Reloc, u64)>,  // raw target: a slot id, or BUILTIN | i
}

pub fn load(dir: &str, idm: Option<&[u32]>) -> P1Facts {
    let id = |x: u32| idm.map_or(x, |m| m[x as usize]);
    let n = meta(dir, "nslots") as usize;
    let kind = rd(&format!("{dir}/p1_kind.u8"));
    let fl = rd(&format!("{dir}/p1_flags.u8"));
    let nk = rd_u64(&format!("{dir}/p1_namekey.u64x2"));
    let rank = rd_u32(&format!("{dir}/p1_rank.u32"));
    let obj = rd_u32(&format!("{dir}/slot_obj.u32"));
    let base = rd_u32(&format!("{dir}/obj_base.u32"));
    let srel = rd_u32(&format!("{dir}/slot_rel.u32"));
    let (rpkg, ridx) = (rd_u32(&format!("{dir}/rel_pkg.u32")), rd_u32(&format!("{dir}/rel_idx.u32")));
    let (poff, pkg) = (rd_u32(&format!("{dir}/obj_pkg_off.u32")), rd_u32(&format!("{dir}/obj_pkg.u32")));
    let want = rd_u64(&format!("{dir}/p1_want.u64x2"));
    let bk = rd_u64(&format!("{dir}/p1_builtin.u64x2"));
    let mut f = P1Facts::default();
    f.want = want.chunks_exact(2).map(|c| (c[0], c[1])).collect();
    f.bkey = bk.chunks_exact(2).enumerate().map(|(i, c)| ((c[0], c[1]), i as u32)).collect();
    for g in 1..=n {
        let (k, o) = ((nk[2 * g], nk[2 * g + 1]), obj[g]);
        let m: Member = (rank[g], !o, !(g as u32 - base[o as usize]), id(g as u32));
        match kind[g] {
            4 | 5 if k.0 != 0 => f.named.push((k, m)),
            1 if k.0 != 0 => f.cand.push((k, m)),
            _ => f.plain.push((id(g as u32), if fl[g] & 2 != 0 { 0 } else { id(g as u32) })),
        }
        let pb = &pkg[poff[o as usize] as usize..poff[o as usize + 1] as usize];
        let npk = pb.len() as u32 - 5;
        for r in srel[g] as usize..srel[g + 1] as usize {
            let (p, idx) = (rpkg[r], ridx[r]);
            let sp = p.wrapping_sub(2147483643);
            let t = if sp < 5 { npk + sp } else { p };
            if t < npk + 5 && pb[t as usize] != u32::MAX {
                let raw = if sp == 1 { BUILTIN | idx as u64 } else { id(pb[t as usize] + idx) as u64 };
                f.relocs.push(((id(g as u32), (r - srel[g] as usize) as u32), raw));
            }
        }
    }
    f
}

impl P1Facts {
    fn sort(&mut self) {
        self.named.sort_unstable();
        self.cand.sort_unstable();
        self.want.sort_unstable();
        self.want.dedup();
        self.bkey.sort_unstable();
        self.plain.sort_unstable();
        self.relocs.sort_unstable();
    }
    fn len(&self) -> usize {
        self.named.len() + self.cand.len() + self.want.len() + self.bkey.len() + self.plain.len() + self.relocs.len()
    }
}

/// 1a and 1b: (canonical id of every slot, nonzero), (canonical target of every relocation, nonzero)
#[allow(clippy::type_complexity)]
fn resolve<'s>(
    named: VecCollection<'s, Time, (Key, Member), Diff>,
    cand: VecCollection<'s, Time, (Key, Member), Diff>,
    want: VecCollection<'s, Time, Key, Diff>,
    bkey: VecCollection<'s, Time, (Key, u32), Diff>,
    plain: VecCollection<'s, Time, (Id, Id), Diff>,
    relocs: VecCollection<'s, Time, (Reloc, u64), Diff>,
) -> (VecCollection<'s, Time, (Id, Id), Diff>, VecCollection<'s, Time, (Reloc, Id), Diff>) {
    // the groups: every name defined or referenced by name, and every name the linker wants
    let groups = named.clone().map(|(k, _)| k).concat(want).distinct_core::<Diff>();
    let joined = cand.clone().semijoin(groups.clone());
    let alone = cand.antijoin(groups).map(|(_, m)| (m.3, m.3)); // a candidate without a group is itself
    let members = named.concat(joined);
    let winner = members.clone().reduce(|_k, ms, out| out.push((*ms.last().unwrap().0, 1 as Diff)));
    let canon_named = members.map(|(k, m)| (k, m.3)).join_map(winner.clone(), |_, &g, w| (g, w.3));
    let canon = canon_named.concat(alone).concat(plain).filter(|&(_, c)| c != 0);
    let builtin = bkey.join_map(winner, |_, &i, &w| (i, w)).filter(|(_, w)| w.0 & RANK_DEF != 0).map(|(i, w)| (i as u64 | BUILTIN, w.3));
    // 1b: a relocation's target is its raw target's canonical id
    let by_raw = relocs.map(|(r, t)| (t, r));
    let to_slot = by_raw.clone().filter(|(t, _)| t & BUILTIN == 0).join_map(canon.clone().map(|(g, c)| (g as u64, c)), |_, &r, &c| (r, c));
    let to_builtin = by_raw.filter(|(t, _)| t & BUILTIN != 0).join_map(builtin, |_, &r, &c| (r, c));
    (canon, to_slot.concat(to_builtin))
}

struct In {
    named: InputSession<Time, (Key, Member), Diff>,
    cand: InputSession<Time, (Key, Member), Diff>,
    want: InputSession<Time, Key, Diff>,
    bkey: InputSession<Time, (Key, u32), Diff>,
    plain: InputSession<Time, (Id, Id), Diff>,
    relocs: InputSession<Time, (Reloc, u64), Diff>,
}
impl In {
    fn feed(&mut self, f: &P1Facts, sign: Diff, idx: usize, peers: usize) {
        fn share<D: differential_dataflow::Data>(s: &mut InputSession<Time, D, Diff>, v: &[D], sign: Diff, idx: usize, peers: usize) {
            for x in v.iter().skip(idx).step_by(peers) {
                s.update(x.clone(), sign);
            }
        }
        share(&mut self.named, &f.named, sign, idx, peers);
        share(&mut self.cand, &f.cand, sign, idx, peers);
        share(&mut self.want, &f.want, sign, idx, peers);
        share(&mut self.bkey, &f.bkey, sign, idx, peers);
        share(&mut self.plain, &f.plain, sign, idx, peers);
        share(&mut self.relocs, &f.relocs, sign, idx, peers);
    }
    fn advance(&mut self, t: Time) {
        self.named.advance_to(t);
        self.cand.advance_to(t);
        self.want.advance_to(t);
        self.bkey.advance_to(t);
        self.plain.advance_to(t);
        self.relocs.advance_to(t);
        self.named.flush();
        self.cand.flush();
        self.want.flush();
        self.bkey.flush();
        self.plain.flush();
        self.relocs.flush();
    }
}

type Snap = Arc<Mutex<HashMap<String, (Vec<(Id, Id)>, Vec<(Reloc, Id)>)>>>;

/// oracles of a dump, in unified ids: (slot, canon) and (relocation, target), nonzero only, sorted
fn oracle(dir: &str, idm: Option<&[u32]>) -> (Vec<(Id, Id)>, Vec<(Reloc, Id)>) {
    let id = |x: u32| idm.map_or(x, |m| m[x as usize]);
    let canon = rd_u32(&format!("{dir}/oracle/p1_canon.u32"));
    let tgt = rd_u32(&format!("{dir}/oracle/p1_reltarget.u32"));
    let srel = rd_u32(&format!("{dir}/slot_rel.u32"));
    let mut c: Vec<(Id, Id)> = (1..canon.len()).filter(|&g| canon[g] != 0).map(|g| (id(g as u32), id(canon[g]))).collect();
    let mut r = vec![];
    for g in 1..srel.len() - 1 {
        for e in srel[g] as usize..srel[g + 1] as usize {
            if tgt[e] != 0 {
                r.push(((id(g as u32), (e - srel[g] as usize) as u32), id(tgt[e])));
            }
        }
    }
    c.sort_unstable();
    r.sort_unstable();
    (c, r)
}

pub fn main(pos: &[String], rest: Vec<String>) {
    let t0 = Instant::now();
    let u = crate::facts::unify(&pos[0], &pos[1]);
    let mut a = load(&pos[0], None);
    let mut b = load(&pos[1], Some(&u.idm_b));
    a.sort();
    b.sort();
    let (oa, ob) = (oracle(&pos[0], None), oracle(&pos[1], Some(&u.idm_b)));
    let reps: usize = pos.get(2).map_or(3, |r| r.parse().unwrap());
    let d = |x: &P1Facts, y: &P1Facts| {
        let mut m = P1Facts::default();
        let mut p = P1Facts::default();
        (m.named, p.named) = diff(&x.named, &y.named);
        (m.cand, p.cand) = diff(&x.cand, &y.cand);
        (m.want, p.want) = diff(&x.want, &y.want);
        (m.bkey, p.bkey) = diff(&x.bkey, &y.bkey);
        (m.plain, p.plain) = diff(&x.plain, &y.plain);
        (m.relocs, p.relocs) = diff(&x.relocs, &y.relocs);
        (m, p)
    };
    let (minus, plus) = d(&a, &b);
    println!(
        "p1 facts: A {} ; diff: named -{} +{}, cand -{} +{}, plain -{} +{}, relocs -{} +{}, want -{} +{}; setup {:.0} ms",
        a.len(),
        minus.named.len(),
        plus.named.len(),
        minus.cand.len(),
        plus.cand.len(),
        minus.plain.len(),
        plus.plain.len(),
        minus.relocs.len(),
        plus.relocs.len(),
        minus.want.len(),
        plus.want.len(),
        t0.elapsed().as_secs_f64() * 1e3
    );
    let (a, minus, plus) = (Arc::new(a), Arc::new(minus), Arc::new(plus));
    let snaps: Snap = Arc::new(Mutex::new(HashMap::new()));
    let snaps2 = snaps.clone();
    timely::execute_from_args(rest.into_iter(), move |worker| {
        let (idx, peers) = (worker.index(), worker.peers());
        let probe = Handle::new();
        let cmap: std::rc::Rc<std::cell::RefCell<HashMap<(Id, Id), Diff>>> = Default::default();
        let rmap: std::rc::Rc<std::cell::RefCell<HashMap<(Reloc, Id), Diff>>> = Default::default();
        let (c2, r2) = (cmap.clone(), rmap.clone());
        let mut inp = worker.dataflow(|scope| {
            let (ni, n) = scope.new_collection();
            let (ci, c) = scope.new_collection();
            let (wi, w) = scope.new_collection();
            let (bi, bk) = scope.new_collection();
            let (pi, p) = scope.new_collection();
            let (ri, r) = scope.new_collection();
            let (canon, tgt) = resolve(n, c, w, bk, p, r);
            canon.consolidate().inspect(move |(x, _, d)| *c2.borrow_mut().entry(*x).or_insert(0) += *d).probe_with(&probe);
            tgt.consolidate().inspect(move |(x, _, d)| *r2.borrow_mut().entry(*x).or_insert(0) += *d).probe_with(&probe);
            In { named: ni, cand: ci, want: wi, bkey: bi, plain: pi, relocs: ri }
        });
        let mut t: Time = 0;
        let mut run = |inp: &mut In, worker: &mut timely::worker::Worker| {
            t += 1;
            inp.advance(t);
            worker.step_while(|| probe.less_than(&t));
        };
        let snap = |label: &str| {
            let c: Vec<_> = cmap.borrow().iter().filter(|(_, &n)| n > 0).map(|(&k, _)| k).collect();
            let r: Vec<_> = rmap.borrow().iter().filter(|(_, &n)| n > 0).map(|(&k, _)| k).collect();
            let mut s = snaps2.lock().unwrap();
            let e = s.entry(label.to_string()).or_default();
            e.0.extend(c);
            e.1.extend(r);
        };
        let say = |s: String| {
            if idx == 0 {
                println!("{s}")
            }
        };
        let tm = Instant::now();
        inp.feed(&a, 1, idx, peers);
        run(&mut inp, worker);
        say(format!("p1 full A\t{:.1} ms", tm.elapsed().as_secs_f64() * 1e3));
        snap("A");
        for rep in 0..reps {
            let tm = Instant::now();
            inp.feed(&minus, -1, idx, peers);
            inp.feed(&plus, 1, idx, peers);
            run(&mut inp, worker);
            say(format!("p1 update A->B\t{:.2} ms\trep {rep}", tm.elapsed().as_secs_f64() * 1e3));
            if rep == 0 {
                snap("B");
            }
            let tm = Instant::now();
            inp.feed(&plus, -1, idx, peers);
            inp.feed(&minus, 1, idx, peers);
            run(&mut inp, worker);
            say(format!("p1 update B->A\t{:.2} ms\trep {rep}", tm.elapsed().as_secs_f64() * 1e3));
        }
    })
    .unwrap();
    let mut s = snaps.lock().unwrap();
    let mut ok = true;
    for (label, o) in [("A", &oa), ("B", &ob)] {
        let e = s.get_mut(label).unwrap();
        e.0.sort_unstable();
        e.1.sort_unstable();
        let (cm, cp) = diff(&e.0, &o.0);
        let (rm, rp) = diff(&e.1, &o.1);
        let good = cm.is_empty() && cp.is_empty() && rm.is_empty() && rp.is_empty();
        println!(
            "check p1 {label}: canon {} (extra {}, missing {}), reloc targets {} (extra {}, missing {}) -> {}",
            e.0.len(),
            cm.len(),
            cp.len(),
            e.1.len(),
            rm.len(),
            rp.len(),
            if good { "OK" } else { "FAIL" }
        );
        ok &= good;
    }
    let (ca, cb) = (&s["A"].0, &s["B"].0);
    let (m, p) = diff(ca, cb);
    let (rm, rp) = diff(&s["A"].1, &s["B"].1);
    println!("canon pairs changed -{} +{}; reloc targets changed -{} +{}", m.len(), p.len(), rm.len(), rp.len());
    println!("{}", if ok { "P1 OK" } else { "P1 FAIL" });
    std::process::exit(if ok { 0 } else { 1 });
}
