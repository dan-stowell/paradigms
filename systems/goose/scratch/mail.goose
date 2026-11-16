import std;

struct Mail<T, M> { tag: T, m: M }
struct Batch { from: i64, ids: u32[] }

fn send<M>(j: i64, m: M) {
    match j {
        0 => qput(Mail<u8[1], M> { tag: [0; 1], m: m }),
        1 => qput(Mail<u8[2], M> { tag: [0; 2], m: m }),
        _ => abort("j"),
    }
}

fn run<T>(k: i64, tag: T) {
    var ids: u32[>..] = [];
    for i in 3 + k { ids.push(i as u32); }
    send(1 - k, Batch { from: k, ids: ids[..] });
    let got = qget<Mail<T, Batch>>();
    print("worker ", k, " got from ", got.m.from, ": ", got.m.ids);
}

thread_fn worker(k: i64) {
    match k {
        0 => run(k, [0 as u8; 1]),
        1 => run(k, [0 as u8; 2]),
        _ => abort("k"),
    }
}

fn main() {
    let a = thread_spawn(worker, 0);
    let b = thread_spawn(worker, 1);
    thread_wait(a);
    thread_wait(b);
}
