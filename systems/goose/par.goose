// par.goose: addressing a worker. Goose has one queue per flat type and no queue handles, so a message for
// worker j is a type of its own: Mail<u8[j+1], M>. A worker's code is generic over its tag type T and receives
// with qget<Mail<T, M>>(); a sender picks the type from j at run time with a match (eight arms: at most eight
// workers).
import std;

struct Mail<T, M> { tag: T, m: M }

fn send<M>(j: i64, m: M) {
    match j {
        0 => qput(Mail<u8[1], M> { tag: [0; 1], m: m }),
        1 => qput(Mail<u8[2], M> { tag: [0; 2], m: m }),
        2 => qput(Mail<u8[3], M> { tag: [0; 3], m: m }),
        3 => qput(Mail<u8[4], M> { tag: [0; 4], m: m }),
        4 => qput(Mail<u8[5], M> { tag: [0; 5], m: m }),
        5 => qput(Mail<u8[6], M> { tag: [0; 6], m: m }),
        6 => qput(Mail<u8[7], M> { tag: [0; 7], m: m }),
        7 => qput(Mail<u8[8], M> { tag: [0; 8], m: m }),
        _ => abort("at most 8 workers"),
    }
}

// the workers' count from the second argument (default: the machine's)
fn workers_arg() -> i64 {
    let a = args();
    var W = min(8, max(1, hardware_threads()));
    if a.len > 2 {
        let n, ok = parse_int(a[2]);
        if ok { W = n; }
    }
    guard W >= 1 && W <= 8 else { abort("1..8 workers"); }
    return W;
}

// [lo, hi) of part k when n items are cut into W equal parts
fn part_lo(n: i64, k: i64, W: i64) -> i64 { n * k / W }
