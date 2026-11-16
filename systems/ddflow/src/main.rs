// Problem 2 (reach: cmd/link's deadcode) as one Differential Dataflow program, and its incremental relink.
//   ddflow p2   DATA_DIR [phased] [-w N]         full computation (or index, then flood), checked against oracle/p2_live.u32
//   ddflow inc  DIR_A DIR_B [REPS] [-w N]        full on A, then apply the fact diff A->B (and back) REPS times;
//                                                checks the updated live set against B's oracle (via the id map)
//   ddflow p1inc DIR_A DIR_B [REPS] [-w N]       the same for problem 1a+1b (resolve), checked against both oracles
mod facts;
mod p1;

use std::cell::RefCell;
use std::collections::HashMap;
use std::rc::Rc;
use std::sync::{Arc, Mutex};
use std::time::Instant;

use differential_dataflow::input::{Input, InputSession};
use differential_dataflow::operators::iterate::VecVariable;
use differential_dataflow::VecCollection;
use timely::dataflow::operators::probe::Handle;
use timely::order::Product;

use facts::{EdgeVal, Facts, Id, Meth};

const DEF: u8 = 1;
const REFLECT: u8 = 4;
const GOTYPE: u8 = 8;
const IFACE: u8 = 16;

type Time = u64;
type Diff = i32;
thread_local!(static ROUNDS: std::cell::Cell<u64> = const { std::cell::Cell::new(0) });

/// The rules of ref/flat/README.md, problem 2, as one dataflow. Returns the live set.
fn reach<'s>(
    roots: VecCollection<'s, Time, Id, Diff>,
    edges: VecCollection<'s, Time, (Id, EdgeVal), Diff>,
    flags: VecCollection<'s, Time, (Id, u8), Diff>,
    methods: VecCollection<'s, Time, (Id, Meth), Diff>,
) -> VecCollection<'s, Time, Id, Diff> {
    let with = |m: u8| move |&(_, f): &(Id, u8)| f & m == m;
    let gotype = flags.clone().filter(with(DEF | GOTYPE)).map(|(s, _)| s); // a defined type descriptor
    let reflectcall = flags.clone().filter(with(DEF | REFLECT)).map(|(s, _)| s);
    let iface0 = flags.filter(with(IFACE)).map(|(s, _)| s); // converted to an interface, known from resolve
    // The static part, arranged once and shared by every round. iface(t) is only ever read for type
    // descriptors (gotype), so it is kept for those alone: every source of iface facts is filtered by gotype
    // here, outside the loop, instead of joining with gotype in every round.
    let gotype = gotype.arrange_by_self();
    let kind = |k: u8| move |(s, (ek, v)): (Id, EdgeVal)| (ek == k).then_some((s, v));
    let plain = edges.clone().flat_map(kind(0)).map(|(s, t)| (s, t as Id)).arrange_by_key();
    let useiface = edges
        .clone()
        .flat_map(kind(1))
        .map(|(s, t)| (t as Id, s))
        .arrange_by_key()
        .join_core(gotype.clone(), |&t, &s, &()| Some((s, t)))
        .arrange_by_key();
    let ifacemethod = edges.clone().flat_map(kind(2)).map(|(s, v)| (s, ((v >> 32) as u32, v as u32))).arrange_by_key();
    let namedmethod = edges.flat_map(kind(3)).map(|(s, n)| (s, n as u32)).arrange_by_key();
    let typeedge = plain
        .clone()
        .join_core(gotype.clone(), |&s, &t, &()| Some((t, s)))
        .arrange_by_key()
        .join_core(gotype.clone(), |&t, &s, &()| Some((s, t)))
        .arrange_by_key();
    let typemethod = methods.arrange_by_key().join_core(gotype.clone(), |&t, &m, &()| Some((t, m)));
    let methodtype = typemethod // (t, mtyp) for the method types that are type descriptors
        .clone()
        .map(|(t, m)| (m.1, t))
        .arrange_by_key()
        .join_core(gotype.clone(), |&mt, &t, &()| Some((t, mt)))
        .arrange_by_key();
    let typemethod = typemethod.arrange_by_key();
    let iface0 = iface0.arrange_by_self().join_core(gotype, |&t, &(), &()| Some(t));
    let reflectcall = reflectcall.arrange_by_self();

    let scope = roots.inner.scope();
    scope.iterative::<u32, _, _>(|inner| {
        let step = Product::new(Default::default(), 1);
        let (live_v, live) = VecVariable::new(inner.clone(), step);
        let (iface_v, iface) = VecVariable::new(inner.clone(), step); // iface type descriptors
        let live_a = live.clone().arrange_by_self();
        let targets = live_a.clone().join_core(plain.enter(inner), |_, &(), &t| Some(t));
        let useiface = live_a.clone().join_core(useiface.enter(inner), |_, &(), &t| Some(t));
        let sig = live_a.clone().join_core(ifacemethod.enter(inner), |_, &(), &x| Some(x)).distinct_core::<Diff>();
        let named = live_a.clone().join_core(namedmethod.enter(inner), |_, &(), &n| Some(n)).distinct_core::<Diff>();
        let reflect = live_a.clone().join_core(reflectcall.enter(inner), |_, _, _| Some(())).distinct_core::<Diff>();
        // live iface type descriptors: their type edges and their methods
        let lit = live_a.join_core(iface.arrange_by_self(), |&s, _, _| Some(s)).arrange_by_self();
        let iface_t = lit.clone().join_core(typeedge.enter(inner), |_, _, &t| Some(t));
        let iface_m = lit.clone().join_core(methodtype.enter(inner), |_, _, &t| Some(t));
        let cand = lit.join_core(typemethod.enter(inner), |_, _, &m| Some(m));
        // the methods something can call
        let by_reflect = cand.clone().filter(|m| m.4 != 0).map(|m| ((), m)).semijoin(reflect).map(|(_, m)| m);
        let by_sig = cand.clone().map(|m| ((m.0, m.1), m)).semijoin(sig).map(|(_, m)| m);
        let by_name = cand.map(|m| (m.0, m)).semijoin(named).map(|(_, m)| m);
        let called = by_reflect
            .concat(by_sig)
            .concat(by_name)
            .flat_map(|(_, mt, ifn, tfn, _)| [mt, ifn, tfn])
            .filter(|&x| x != 0);
        live_v.set(roots.enter(inner).concat(targets).concat(called).distinct_core::<Diff>());
        iface_v.set(iface0.enter(inner).concat(useiface).concat(iface_t).concat(iface_m).distinct_core::<Diff>());
        if std::env::var("DD_ROUNDS").is_ok() {
            // diagnostic: the number of rounds the loop took (the largest inner time of a live change)
            live.clone().inspect_batch(|_, d| {
                ROUNDS.with(|r| r.set(d.iter().fold(r.get(), |m, x| m.max(x.1.inner as u64))));
            });
        }
        live.leave(scope)
    })
}

/// One instance of the dataflow on a worker: its inputs, its probe, and this worker's share of the live set
/// (accumulated from the output's changes).
struct Dataflow {
    inp: Inputs,
    probe: Handle<Time>,
    live: Rc<RefCell<HashMap<Id, Diff>>>,
    t: Time,
}
impl Dataflow {
    fn new(worker: &mut timely::worker::Worker) -> Dataflow {
        let probe = Handle::new();
        let live: Rc<RefCell<HashMap<Id, Diff>>> = Rc::new(RefCell::new(HashMap::new()));
        let l2 = live.clone();
        let inp = worker.dataflow(|scope| {
            let (ri, r) = scope.new_collection();
            let (ei, e) = scope.new_collection();
            let (fi, f) = scope.new_collection();
            let (mi, m) = scope.new_collection();
            reach(r, e, f, m)
                .inspect(move |(x, _t, d)| *l2.borrow_mut().entry(*x).or_insert(0) += *d)
                .probe_with(&probe);
            Inputs { roots: ri, edges: ei, flags: fi, methods: mi }
        });
        Dataflow { inp, probe, live, t: 0 }
    }
    /// close the current time and run until the output has caught up with it
    fn run(&mut self, worker: &mut timely::worker::Worker) {
        self.t += 1;
        self.inp.advance(self.t);
        let (p, t) = (&self.probe, self.t);
        worker.step_while(|| p.less_than(&t));
    }
    fn live(&self) -> Vec<Id> {
        self.live.borrow().iter().filter(|(_, &c)| c > 0).map(|(&k, _)| k).collect()
    }
}

struct Inputs {
    roots: InputSession<Time, Id, Diff>,
    edges: InputSession<Time, (Id, EdgeVal), Diff>,
    flags: InputSession<Time, (Id, u8), Diff>,
    methods: InputSession<Time, (Id, Meth), Diff>,
}
impl Inputs {
    /// insert (sign = 1) or remove (sign = -1) this worker's share of `f`; roots optional
    fn feed(&mut self, f: &Facts, sign: Diff, with_roots: bool, idx: usize, peers: usize) {
        fn share<D: Clone + Ord + std::fmt::Debug + 'static>(s: &mut InputSession<Time, D, Diff>, v: &[D], sign: Diff, idx: usize, peers: usize)
        where
            D: differential_dataflow::Data,
        {
            for x in v.iter().skip(idx).step_by(peers) {
                s.update(x.clone(), sign);
            }
        }
        share(&mut self.edges, &f.edges, sign, idx, peers);
        share(&mut self.flags, &f.flags, sign, idx, peers);
        share(&mut self.methods, &f.methods, sign, idx, peers);
        if with_roots {
            share(&mut self.roots, &f.roots, sign, idx, peers);
        }
    }
    fn advance(&mut self, t: Time) {
        self.roots.advance_to(t);
        self.edges.advance_to(t);
        self.flags.advance_to(t);
        self.methods.advance_to(t);
        self.roots.flush();
        self.edges.flush();
        self.flags.flush();
        self.methods.flush();
    }
}

type Snapshots = Arc<Mutex<HashMap<String, Vec<Id>>>>;

fn check(label: &str, got: &mut Vec<Id>, want: &[Id]) -> bool {
    got.sort_unstable();
    let (extra, missing) = facts::diff(got, want);
    let ok = extra.is_empty() && missing.is_empty();
    println!("check {label}: {} live, oracle {}, extra {}, missing {} -> {}", got.len(), want.len(), extra.len(), missing.len(), if ok { "OK" } else { "FAIL" });
    ok
}

fn ms(t: Instant) -> f64 {
    t.elapsed().as_secs_f64() * 1e3
}

fn main() {
    let t_start = Instant::now();
    let args: Vec<String> = std::env::args().collect();
    let mode = args[1].clone();
    let pos: Vec<String> = args[2..].iter().take_while(|a| !a.starts_with('-')).cloned().collect();
    let rest: Vec<String> = args[2 + pos.len()..].to_vec();
    if mode == "p1inc" {
        return p1::main(&pos, rest);
    }
    let (fa, fb, reps, oracle_b, ub) = match mode.as_str() {
        "p2" => {
            let f = facts::load(&pos[0], None, None);
            (f, None, 0, None, None)
        }
        "inc" => {
            let t = Instant::now();
            let u = facts::unify(&pos[0], &pos[1]);
            let t_unify = ms(t);
            let t = Instant::now();
            let mut a = facts::load(&pos[0], None, None);
            let mut b = facts::load(&pos[1], Some(&u.idm_b), Some(&u.strm_b));
            a.sort();
            b.sort();
            let t_load = ms(t);
            let ob: Vec<Id> = {
                let mut v: Vec<Id> = facts::rd_u32(&format!("{}/oracle/p2_live.u32", pos[1])).iter().map(|&x| u.idm_b[x as usize]).collect();
                v.sort_unstable();
                v
            };
            println!(
                "unify {t_unify:.0} ms: objects that differ {:?}; slots only in A {}, only in B {}; load+sort {t_load:.0} ms",
                u.objs_differ,
                u.a_only.len(),
                u.b_only.len()
            );
            for &g in &u.a_only {
                println!("  only in A: {}", u.name(g));
            }
            for &g in &u.b_only {
                println!("  only in B: {}", u.name(u.idm_b[g as usize]));
            }
            (a, Some(b), pos.get(2).map_or(5, |r| r.parse().unwrap()), Some(ob), Some(u))
        }
        _ => panic!("mode p2 | inc"),
    };
    let oracle_a = facts::rd_u32(&format!("{}/oracle/p2_live.u32", pos[0]));
    let diff = fb.as_ref().map(|b| {
        let t = Instant::now();
        let d = facts::fact_diff(&fa, b);
        println!(
            "diff {:.0} ms: edges -{} +{}, flags -{} +{}, methods -{} +{}, roots -{} +{} (of {} A facts)",
            ms(t),
            d.minus.edges.len(),
            d.plus.edges.len(),
            d.minus.flags.len(),
            d.plus.flags.len(),
            d.minus.methods.len(),
            d.plus.methods.len(),
            d.minus.roots.len(),
            d.plus.roots.len(),
            fa.len()
        );
        // symbols whose facts changed (as a source of an edge, a flag, a type of a method, a root)
        let mut syms: Vec<Id> = Vec::new();
        for f in [&d.minus, &d.plus] {
            syms.extend(f.edges.iter().map(|e| e.0));
            syms.extend(f.flags.iter().map(|e| e.0));
            syms.extend(f.methods.iter().map(|e| e.0));
            syms.extend(f.roots.iter().copied());
        }
        syms.sort_unstable();
        syms.dedup();
        println!("symbols with changed facts: {}", syms.len());
        let u = ub.as_ref().unwrap();
        for f in [("-", &d.minus), ("+", &d.plus)] {
            for e in &f.1.edges {
                let t = if e.1 .0 < 2 { u.name(e.1 .1 as u32) } else { format!("{:#x}", e.1 .1) };
                println!("  edge {} {} -> kind {} {}", f.0, u.name(e.0), e.1 .0, t);
            }
            for e in &f.1.flags {
                println!("  flag {} {} = {}", f.0, u.name(e.0), e.1);
            }
        }
        d
    });
    let (fa, diff, fb2) = (Arc::new(fa), Arc::new(diff), Arc::new(fb));
    let snaps: Snapshots = Arc::new(Mutex::new(HashMap::new()));
    let snaps2 = snaps.clone();
    let inc = mode == "inc";
    let phased = pos.get(1).map_or(false, |p| p == "phased");
    println!("setup (read, decode{}) {:.0} ms", if inc { ", unify, diff" } else { "" }, ms(t_start));

    timely::execute_from_args(rest.into_iter(), move |worker| {
        let (idx, peers) = (worker.index(), worker.peers());
        let mut df = Dataflow::new(worker);
        let snap = |df: &Dataflow, label: &str| {
            snaps2.lock().unwrap().entry(label.to_string()).or_default().extend(df.live());
        };
        let say = |s: String| {
            if idx == 0 {
                println!("{s}")
            }
        };
        if !inc {
            // full: every fact and the roots at once (warm: the facts are in memory), or by phase: index the
            // facts first (no roots: nothing is live), then the flood alone (the roots into the indexed facts)
            if !phased {
                let tm = Instant::now();
                df.inp.feed(&fa, 1, true, idx, peers);
                df.run(worker);
                say(format!("full\t{:.1} ms\t{} rounds", ms(tm), ROUNDS.with(|r| r.get())));
            } else {
                let tm = Instant::now();
                df.inp.feed(&fa, 1, false, idx, peers);
                df.run(worker);
                say(format!("index (facts, no roots)\t{:.1} ms", ms(tm)));
                let tm = Instant::now();
                for x in fa.roots.iter().skip(idx).step_by(peers) {
                    df.inp.roots.update(*x, 1);
                }
                df.run(worker);
                say(format!("flood (roots into indexed facts)\t{:.1} ms", ms(tm)));
            }
            snap(&df, "A");
        } else {
            let d = diff.as_ref().as_ref().unwrap();
            let tm = Instant::now();
            df.inp.feed(&fa, 1, true, idx, peers);
            df.run(worker);
            say(format!("full A\t{:.1} ms", ms(tm)));
            snap(&df, "A");
            for rep in 0..reps {
                let tm = Instant::now();
                df.inp.feed(&d.minus, -1, true, idx, peers);
                df.inp.feed(&d.plus, 1, true, idx, peers);
                df.run(worker);
                say(format!("update A->B\t{:.2} ms\trep {rep}", ms(tm)));
                if rep == 0 {
                    snap(&df, "B");
                }
                let tm = Instant::now();
                df.inp.feed(&d.plus, -1, true, idx, peers);
                df.inp.feed(&d.minus, 1, true, idx, peers);
                df.run(worker);
                say(format!("update B->A\t{:.2} ms\trep {rep}", ms(tm)));
                if rep == 0 {
                    snap(&df, "A again");
                }
            }
            // from scratch on B (unified ids), in a second dataflow of its own
            let mut df2 = Dataflow::new(worker);
            let tm = Instant::now();
            df2.inp.feed(fb2.as_ref().as_ref().unwrap(), 1, true, idx, peers);
            df2.run(worker);
            say(format!("full B (from scratch)\t{:.1} ms", ms(tm)));
            snap(&df2, "B scratch");
        }
    })
    .unwrap();
    let mut s = snaps.lock().unwrap();
    let mut ok = check("A", s.get_mut("A").unwrap(), &oracle_a);
    if let Some(ob) = oracle_b {
        ok &= check("A->B vs B oracle (unified ids)", s.get_mut("B").unwrap(), &ob);
        ok &= check("B->A vs A oracle", s.get_mut("A again").unwrap(), &oracle_a);
        let (mut a, mut b) = (s["A"].clone(), s["B"].clone());
        a.sort_unstable();
        b.sort_unstable();
        let (m, p) = facts::diff(&a, &b);
        println!("live ids changed: -{} +{}", m.len(), p.len());
        let u = ub.as_ref().unwrap();
        for x in &m {
            println!("  live - {}", u.name(*x));
        }
        for x in &p {
            println!("  live + {}", u.name(*x));
        }
        let mut bs = s["B scratch"].clone();
        bs.sort_unstable();
        let same = bs == b;
        println!("check A->B update vs B from scratch (same process, unified ids): {}", if same { "OK" } else { "FAIL" });
        ok &= same;
        ok &= check("B from scratch vs B oracle", &mut bs, &ob);
    }
    println!("{}", if ok { "P2 OK" } else { "P2 FAIL" });
    println!("e2e (process start to here) {:.0} ms", ms(t_start));
    std::process::exit(if ok { 0 } else { 1 });
}
