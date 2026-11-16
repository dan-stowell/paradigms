import std;
import os;

let D = "/home/exedev/paradigms/data/minio/";

extern fn flat_size(path: const u8[:]) -> i64;
extern fn flat_read(path: const u8[:], out: u8[>..]&, elem: i64) -> bool;
extern fn flat_read(path: const u8[:], out: u32[>..]&, elem: i64) -> bool = "flat_read";
extern fn flat_read_ids(path: const u8[:], out: SlotId[>..]&, elem: i64) -> bool = "flat_read";

var live: u8[>..] = [];
type SlotId = u32 in live;

fn main() {
    for i in 5205435 { live.push(0); }
    let p = str(D, "p2_edge_dst.u32");
    // A: stdlib read_file, prefix, copy, from_bytes
    let t0 = clock();
    var raw: u8[>..] = [];
    if !read_file(p, raw) { abort("read"); }
    var img: u8[>..] = [];
    format_uleb(img, raw.len);
    img.append(raw);
    var a, ok = from_bytes<SlotId[>..]>(img);
    let t1 = clock();
    // B: size, prefix, direct read, from_bytes (verified index type)
    var img2: u8[>..] = [];
    format_uleb(img2, flat_size(p));
    if !flat_read(p, img2, 1) { abort("read"); }
    var b, ok2 = from_bytes<SlotId[>..]>(img2);
    let t2 = clock();
    // C: direct read into u32[>..]
    var c: u32[>..] = [];
    if !flat_read(p, c, 4) { abort("read"); }
    let t3 = clock();
    var s: u64 = 0;
    for x in a { s += x; }
    for x in b { s += x; }
    for x in c { s += x; }
    print(ok, ok2, " n=", a.len, " ", b.len, " ", c.len, " sum=", s);
    print("A ", (t1-t0)*1000.0, " B ", (t2-t1)*1000.0, " C ", (t3-t2)*1000.0);
}
