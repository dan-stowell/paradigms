// Reachability (plain edges only) over the MinIO problem-2 flat files, with Differential Dataflow.
// usage: ddflow DATA_DIR [-w N]
use std::collections::HashMap;
use std::sync::{Arc, Mutex};
use std::time::Instant;

use differential_dataflow::input::Input;
use differential_dataflow::lattice::Lattice;
use differential_dataflow::operators::*;
use differential_dataflow::VecCollection;
use timely::dataflow::operators::probe::Handle;

type Node = u32;
type Edge = (Node, Node);

fn rd_u32(p: &str) -> Vec<u32> {
    let b = std::fs::read(p).unwrap();
    b.chunks_exact(4).map(|c| u32::from_le_bytes(c.try_into().unwrap())).collect()
}

fn reach<'scope, T>(edges: VecCollection<'scope, T, Edge>, roots: VecCollection<'scope, T, Node>) -> VecCollection<'scope, T, Node>
where
    T: timely::progress::Timestamp + Lattice + Ord,
{
    roots.clone().iterate(|scope, inner| {
        let roots = roots.enter(scope);
        let edges = edges.enter(scope);
        inner.map(|x| (x, ())).join_map(edges, |_k, _, d| *d).concat(roots).distinct()
    })
}

fn main() {
    let dir = std::env::args().nth(1).expect("DATA_DIR");
    let t0 = Instant::now();
    let off = rd_u32(&format!("{dir}/p2_edge_off.u32"));
    let dst = rd_u32(&format!("{dir}/p2_edge_dst.u32"));
    let kind = std::fs::read(format!("{dir}/p2_edge_kind.u8")).unwrap();
    let roots_v = rd_u32(&format!("{dir}/p2_roots.u32"));
    let oracle = rd_u32(&format!("{dir}/oracle/p2_live.u32"));
    let mut edges: Vec<Edge> = Vec::new();
    for s in 0..off.len() - 1 {
        for e in off[s] as usize..off[s + 1] as usize {
            if kind[e] == 0 {
                edges.push((s as u32, dst[e]));
            }
        }
    }
    eprintln!("read {} plain edges of {}, {} roots, {:?}", edges.len(), dst.len(), roots_v.len(), t0.elapsed());
    let edges = Arc::new(edges);
    let roots_v = Arc::new(roots_v);

    // live set maintained from the output diffs, shared across workers
    let live: Arc<Mutex<HashMap<u32, i64>>> = Arc::new(Mutex::new(HashMap::new()));
    let live2 = live.clone();

    // incremental batches: 100 evenly-spaced edges whose source is a text-ish live node are
    // picked by position; "small" = 100 edges, "big" = all out-edges of the entry root.
    let batch_small: Vec<Edge> = (0..100).map(|i| edges[(i * (edges.len() / 100)) + 7]).collect();
    let r0 = roots_v[0];
    let batch_big: Vec<Edge> = edges.iter().cloned().filter(|e| e.0 == r0).collect();

    timely::execute_from_args(std::env::args().skip(1), move |worker| {
        let idx = worker.index();
        let peers = worker.peers();
        let mut probe = Handle::new();
        let live_in = live2.clone(); let live = live2.clone();
        let (mut rin, mut ein) = worker.dataflow(|scope| {
            let (ri, r) = scope.new_collection();
            let (ei, e) = scope.new_collection();
            reach(e, r).inspect(move |(x, _t, d)| {
                *live_in.lock().unwrap().entry(*x).or_insert(0) += *d as i64;
            }).probe_with(&mut probe);
            (ri, ei)
        });

        let timer = Instant::now();
        if idx == 0 {
            for &r in roots_v.iter() {
                rin.insert(r);
            }
        }
        rin.close();
        for (i, e) in edges.iter().enumerate() {
            if i % peers == idx {
                ein.insert(*e);
            }
        }
        let mut t = 1u64;
        ein.advance_to(t);
        ein.flush();
        worker.step_while(|| probe.less_than(ein.time()));
        if idx == 0 {
            println!("full\t{:?}", timer.elapsed());
        }

        let mut step = |name: &str, batch: &Vec<Edge>, add: bool, worker: &mut timely::worker::Worker| {
            let tm = Instant::now();
            if idx == 0 {
                for e in batch {
                    if add { ein.insert(*e) } else { ein.remove(*e) }
                }
            }
            t += 1;
            ein.advance_to(t);
            ein.flush();
            worker.step_while(|| probe.less_than(ein.time()));
            if idx == 0 {
                println!("{name}\t{:?}", tm.elapsed());
            }
        };
        // report live count after each phase via a barrier-ish: only worker 0 prints (others may lag
        // by a step, so read the count after all workers passed the probe -- done by the step_while).
        let report = |label: &str| {
            if idx == 0 {
                let n = live.lock().unwrap().values().filter(|&&c| c > 0).count();
                println!("live[{label}]\t{n}");
            }
        };
        report("full");
        if idx == 0 {
            let l = live.lock().unwrap();
            let mut got: Vec<u32> = l.iter().filter(|(_, &c)| c > 0).map(|(&k, _)| k).collect();
            got.sort();
            let o: std::collections::HashSet<u32> = oracle.iter().cloned().collect();
            let sub = got.iter().filter(|x| !o.contains(x)).count();
            println!("oracle {} ; ours {} ; ours-not-in-oracle {} ; oracle-not-in-ours {}", oracle.len(), got.len(), sub, oracle.len() - (got.len() - sub));
        }
        step("remove100", &batch_small, false, worker);
        report("remove100");
        step("add100", &batch_small, true, worker);
        report("add100");
        step("removeEntryEdges", &batch_big, false, worker);
        report("removeEntryEdges");
        step("addEntryEdges", &batch_big, true, worker);
        report("addEntryEdges");
        if idx == 0 {
            println!("batch sizes: small {} big {}", batch_small.len(), batch_big.len());
        }
    })
    .unwrap();
}
