// Multi-threaded (shared-nothing): main reads the file, copies one chunk per worker through a typed
// queue (a flat value, one memcpy); each worker decodes u32s, builds its LOCAL inclusive prefix array
// (u64) and a 256-bin histogram, and sends back (chunk total, sum of local prefixes, histogram).
// Main folds: global prefix checksum = sum over chunks of base*len + local_prefix_sum, where
// base = total of all earlier chunks. (Goose has no shared memory, so the global prefix array is not
// materialized across workers; see NOTES.md.)
// usage: hello8 file.bin [threads]
import std;
import os;
import binary;

struct Chunk  { id: i64, data: u8[] }
struct Result { id: i64, len: i64, total: u64, lps: u64, hist: u64[256] }

thread_fn worker() {
    let c = qget<Chunk>();
    let n = c.data.len / 4;
    var vals: u32[>..] = [];
    vals.resize(n, 0);
    for i in n {
        let v, ok = binary::read_u32le(c.data, i * 4);
        vals[i] = v;
    }
    var pre: u64[>..] = [];
    pre.resize(n, 0);
    var hist: u64[256] = [0; 256];
    var p: u64 = 0;
    for i in n {
        let v = vals[i];
        p += v;
        pre[i] = p;
        hist[(v & 255) as i64] += 1;
    }
    var lps: u64 = 0;
    for x in pre { lps += x; }
    qput(Result { id: c.id, len: n, total: p, lps: lps, hist: hist });
}

fn main() {
    let a = args();
    guard a.len > 1 else { print("usage: hello8 file.bin [threads]"); exit(2); }
    var nt: i64 = max(1, hardware_threads());
    if a.len > 2 { nt = a[2][0] as i64 - 48; }   // one-char thread count
    let t0 = clock();
    var raw: u8[>..] = [];
    if !read_file(a[1], raw) { abort("cannot read file"); }
    let t1 = clock();
    let n = raw.len / 4;
    let per = (n + nt - 1) / nt;
    var ids: i64[>..] = [];
    for k in nt {
        let lo = min(k * per, n) * 4;
        let hi = min((k + 1) * per, n) * 4;
        qput(Chunk { id: k, data: raw[lo..hi] });
        ids.push(thread_spawn(worker));
    }
    let t2 = clock();
    var res: Result[>..] = [];
    res.resize(nt, Result { id: 0, len: 0, total: 0, lps: 0, hist: [0; 256] });
    for k in nt {
        let r = qget<Result>();
        res[r.id] = r;
    }
    for id in ids { thread_wait(id); }
    let t3 = clock();
    var base: u64 = 0;
    var ps: u64 = 0;
    var hist: u64[256] = [0; 256];
    for k in nt {
        ps += base * (res[k].len as u64) + res[k].lps;
        base += res[k].total;
        for b in 256 { hist[b] += res[k].hist[b]; }
    }
    var hs: u64 = 0;
    for b in 256 { hs = hs * 31 + hist[b] * (b + 1) as u64; }
    let t4 = clock();
    print("goose n=", n, " total=", base, " prefix_checksum=", ps, " hist_checksum=", hs);
    var note: u8[>..] = [];
    format(note, "threads ", nt, ": read ");
    format_flt(note, (t1 - t0) * 1000.0, 1);
    format(note, " ms, send+spawn ");
    format_flt(note, (t2 - t1) * 1000.0, 1);
    format(note, " ms, workers+collect ");
    format_flt(note, (t3 - t2) * 1000.0, 1);
    format(note, " ms, fold ");
    format_flt(note, (t4 - t3) * 1000.0, 1);
    format(note, " ms, total ");
    format_flt(note, (t4 - t0) * 1000.0, 1);
    format(note, " ms\n");
    write_stderr(note);
}
