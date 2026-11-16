// Single-threaded: read raw LE u32 file, inclusive prefix sum (u64), 256-bin histogram of low 8 bits,
// print checksums in the same form as gen.c's oracle. usage: hello1 file.bin
import std;
import os;
import binary;

fn main() {
    let a = args();
    guard a.len > 1 else { print("usage: hello1 file.bin"); exit(2); }
    let t0 = clock();
    var raw: u8[>..] = [];
    if !read_file(a[1], raw) { abort("cannot read file"); }
    let t1 = clock();
    let n = raw.len / 4;
    var vals: u32[>..] = [];
    vals.resize(n, 0);
    for i in n {
        let v, ok = binary::read_u32le(raw, i * 4);
        vals[i] = v;
    }
    let t2 = clock();
    var pre: u64[>..] = [];
    pre.resize(n, 0);
    var hist: u64[256] = [0; 256];
    var p: u64 = 0;
    for i in n {
        let v = vals[i];
        p += v as u64;
        pre[i] = p;
        hist[(v & 255) as i64] += 1;
    }
    let t3 = clock();
    var ps: u64 = 0;
    for x in pre { ps += x; }
    var hs: u64 = 0;
    for b in 256 { hs = hs * 31 + hist[b] * (b + 1) as u64; }
    let t4 = clock();
    print("goose n=", n, " total=", p, " prefix_checksum=", ps, " hist_checksum=", hs);
    var note: u8[>..] = [];
    format(note, "read ");
    format_flt(note, (t1 - t0) * 1000.0, 1);
    format(note, " ms, decode ");
    format_flt(note, (t2 - t1) * 1000.0, 1);
    format(note, " ms, scan+hist ");
    format_flt(note, (t3 - t2) * 1000.0, 1);
    format(note, " ms, checksum ");
    format_flt(note, (t4 - t3) * 1000.0, 1);
    format(note, " ms, total ");
    format_flt(note, (t4 - t0) * 1000.0, 1);
    format(note, " ms\n");
    write_stderr(note);
}
