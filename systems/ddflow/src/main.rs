// Problem 2 (reach: cmd/link's deadcode) as one Differential Dataflow program, and its incremental relink.
//   ddflow p2   DATA_DIR [phased] [-w N]         full computation (or index, then flood), checked against oracle/p2_live.u32
//   ddflow inc  DIR_A DIR_B [REPS] [-w N]        full on A, then apply the fact diff A->B (and back) REPS times;
//                                                checks the updated live set against B's oracle (via the id map)
mod facts;

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

/// The rules of ref/flat/README.md, problem 2, as one dataflow. Returns the live set.
fn reach<'s>(
    roots: VecCollection<'s, Time, Id>,
    edges: VecCollection<'s, Time, (Id, EdgeVal)>,
    flags: VecCollection<'s, Time, (Id, u8)>,
    methods: VecCollection<'s, Time, (Id, Meth)>,
) -> VecCollection<'s, Time, Id> {
    let with = |m: u8| move |&(_, f): &(Id, u8)| f & m == m;
    let gotype = flags.clone().filter(with(DEF | GOTYPE)).map(|(s, _)| s); // a defined type descriptor
    let reflectcall = flags.clone().filter(with(DEF | REFLECT)).map(|(s, _)| s);
    let iface0 = flags.filter(with(IFACE)).map(|(s, _)| s); // converted to an interface, known from resolve
    // The static part, arranged once and shared by every round: all edges keyed by source (one index serves
    // the four edge kinds), the type->type plain edges, the method tables of types.
    let edges_by_src = edges.arrange_by_key();
    let gotype = gotype.arrange_by_self();
    let typeedge = edges_by_src
        .clone()
        .join_core(gotype.clone(), |&s, &(k, t), &()| (k == 0).then_some((t as Id, s)))
        .arrange_by_key()
        .join_core(gotype.clone(), |&t, &s, &()| Some((s, t)))
        .arrange_by_key();
    let typemethod = methods.arrange_by_key().join_core(gotype.clone(), |&t, &m, &()| Some((t, m))).arrange_by_key();
    let reflectcall = reflectcall.arrange_by_self();

    let scope = roots.inner.scope();
    scope.iterative::<u64, _, _>(|inner| {
        let step = Product::new(Default::default(), 1);
        let (live_v, live) = VecVariable::new(inner.clone(), step);
        let (iface_v, iface) = VecVariable::new(inner.clone(), step);
        let live_a = live.clone().arrange_by_self();
        // live(s) joined with every edge of s, once
        let out = live_a.clone().join_core(edges_by_src.enter(inner), |_s, &(), &(k, v)| Some((k, v)));
        let plain = out.clone().filter(|&(k, _)| k == 0).map(|(_, t)| t as Id);
        let useiface = out.clone().filter(|&(k, _)| k == 1).map(|(_, t)| t as Id);
        let sig = out.clone().filter(|&(k, _)| k == 2).map(|(_, v)| ((v >> 32) as u32, v as u32)).distinct();
        let named = out.filter(|&(k, _)| k == 3).map(|(_, n)| n as u32).distinct();
        let reflect = live_a.clone().join_core(reflectcall.enter(inner), |_, _, _| Some(())).distinct();
        // live, iface type descriptors: their type edges and their methods
        let lit = live_a
            .join_core(iface.clone().arrange_by_self(), |&s, _, _| Some(s))
            .arrange_by_self()
            .join_core(gotype.enter(inner), |&s, _, _| Some(s))
            .arrange_by_self();
        let iface_t = lit.clone().join_core(typeedge.enter(inner), |_, _, &t| Some(t));
        let cand = lit.join_core(typemethod.enter(inner), |_, _, &m| Some(m));
        // the methods something can call
        let by_reflect = cand
            .clone()
            .filter(|m| m.4 != 0)
            .map(|m| ((), m))
            .semijoin(reflect)
            .map(|(_, m)| m);
        let by_sig = cand.clone().map(|m| ((m.0, m.1), m)).semijoin(sig).map(|(_, m)| m);
        let by_name = cand.clone().map(|m| (m.0, m)).semijoin(named).map(|(_, m)| m);
        let called = by_reflect
            .concat(by_sig)
            .concat(by_name)
            .flat_map(|(_, mt, ifn, tfn, _)| [mt, ifn, tfn])
            .filter(|&x| x != 0);
        live_v.set(roots.enter(inner).concat(plain).concat(called).distinct());
        iface_v.set(
            iface0
                .enter(inner)
                .concat(useiface)
                .concat(iface_t)
                .concat(cand.map(|m| m.1))
                .filter(|&x| x != 0)
                .distinct(),
        );
        live.leave(scope)
    })
}

struct Inputs {
    roots: InputSession<Time, Id, isize>,
    edges: InputSession<Time, (Id, EdgeVal), isize>,
    flags: InputSession<Time, (Id, u8), isize>,
    methods: InputSession<Time, (Id, Meth), isize>,
}
impl Inputs {
    /// insert (sign = 1) or remove (sign = -1) this worker's share of `f`; roots optional
    fn feed(&mut self, f: &Facts, sign: isize, with_roots: bool, idx: usize, peers: usize) {
        fn share<D: Clone + Ord + std::fmt::Debug + 'static>(s: &mut InputSession<Time, D, isize>, v: &[D], sign: isize, idx: usize, peers: usize)
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
                "unify {t_unify:.0} ms: {} objects differ; slots only in A {}, only in B {}; load+sort {t_load:.0} ms",
                u.objs_differ, u.a_only, u.b_only
            );
            (a, Some(b), pos.get(2).map_or(5, |r| r.parse().unwrap()), Some(ob), Some(u))
        }
        _ => panic!("mode p2 | inc"),
    };
    let _ = ub;
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
        d
    });
    let (fa, diff) = (Arc::new(fa), Arc::new(diff));
    let snaps: Snapshots = Arc::new(Mutex::new(HashMap::new()));
    let snaps2 = snaps.clone();
    let inc = mode == "inc";
    let phased = pos.get(1).map_or(false, |p| p == "phased");
    println!("setup (read, decode{}) {:.0} ms", if inc { ", unify, diff" } else { "" }, ms(t_start));

    timely::execute_from_args(rest.into_iter(), move |worker| {
        let (idx, peers) = (worker.index(), worker.peers());
        let probe = Handle::new();
        let local: Rc<RefCell<HashMap<Id, isize>>> = Rc::new(RefCell::new(HashMap::new()));
        let local2 = local.clone();
        let mut inp = worker.dataflow(|scope| {
            let (ri, r) = scope.new_collection();
            let (ei, e) = scope.new_collection();
            let (fi, f) = scope.new_collection();
            let (mi, m) = scope.new_collection();
            reach(r, e, f, m)
                .inspect(move |(x, _t, d)| *local2.borrow_mut().entry(*x).or_insert(0) += *d)
                .probe_with(&probe);
            Inputs { roots: ri, edges: ei, flags: fi, methods: mi }
        });
        let mut t: Time = 0;
        let mut run = |inp: &mut Inputs, worker: &mut timely::worker::Worker| {
            t += 1;
            inp.advance(t);
            worker.step_while(|| probe.less_than(&t));
        };
        let snap = |label: &str| {
            let v: Vec<Id> = local.borrow().iter().filter(|(_, &c)| c > 0).map(|(&k, _)| k).collect();
            snaps2.lock().unwrap().entry(label.to_string()).or_default().extend(v);
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
                inp.feed(&fa, 1, true, idx, peers);
                run(&mut inp, worker);
                say(format!("full\t{:.1} ms", ms(tm)));
            } else {
                let tm = Instant::now();
                inp.feed(&fa, 1, false, idx, peers);
                run(&mut inp, worker);
                say(format!("index (facts, no roots)\t{:.1} ms", ms(tm)));
                let tm = Instant::now();
                for x in fa.roots.iter().skip(idx).step_by(peers) {
                    inp.roots.insert(*x);
                }
                run(&mut inp, worker);
                say(format!("flood (roots into indexed facts)\t{:.1} ms", ms(tm)));
            }
            snap("A");
        } else {
            let d = diff.as_ref().as_ref().unwrap();
            let tm = Instant::now();
            inp.feed(&fa, 1, true, idx, peers);
            run(&mut inp, worker);
            say(format!("full A\t{:.1} ms", ms(tm)));
            snap("A");
            for rep in 0..reps {
                let tm = Instant::now();
                inp.feed(&d.minus, -1, true, idx, peers);
                inp.feed(&d.plus, 1, true, idx, peers);
                run(&mut inp, worker);
                say(format!("update A->B\t{:.2} ms\trep {rep}", ms(tm)));
                if rep == 0 {
                    snap("B");
                }
                let tm = Instant::now();
                inp.feed(&d.plus, -1, true, idx, peers);
                inp.feed(&d.minus, 1, true, idx, peers);
                run(&mut inp, worker);
                say(format!("update B->A\t{:.2} ms\trep {rep}", ms(tm)));
                if rep == 0 {
                    snap("A again");
                }
            }
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
    }
    println!("{}", if ok { "P2 OK" } else { "P2 FAIL" });
    println!("e2e (process start to here) {:.0} ms", ms(t_start));
    std::process::exit(if ok { 0 } else { 1 });
}
