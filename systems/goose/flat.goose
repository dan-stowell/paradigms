// flat.goose: read the flat files (ref/flat/README.md) into Goose arrays.
// The one escape from the language is flatio.c: a file's size and a read() straight into a builder's
// stack top (goose.h's contract). Plain numbers are read that way; arrays of index types go through
// from_bytes, which checks every index against its array (C cannot write them: the compiler refuses).
import std;
import os;

extern fn flat_size(path: const u8[:]) -> i64;
extern fn flat_read(path: const u8[:], out: u8[>..]&, elem: i64) -> bool;
extern fn flat_read(path: const u8[:], out: u16[>..]&, elem: i64) -> bool = "flat_read";
extern fn flat_read(path: const u8[:], out: u32[>..]&, elem: i64) -> bool = "flat_read";
extern fn flat_read(path: const u8[:], out: i32[>..]&, elem: i64) -> bool = "flat_read";
extern fn flat_read(path: const u8[:], out: u64[>..]&, elem: i64) -> bool = "flat_read";
extern fn flat_read(path: const u8[:], out: i64[>..]&, elem: i64) -> bool = "flat_read";

// the data directory: the first argument
fn dir_arg() -> u8[>..] {
    let a = args();
    return str(if a.len > 1 { a[1] } else { "/home/exedev/paradigms/data/minio" });
}
var flat_dir: u8[>..] = dir_arg();

fn flat_path(name: const u8[:]) -> u8[>..] { str(flat_dir, "/", name) }

// appends the file's elements to out (sizes from the element type)
fn load(name: const u8[:], out: u8[>..]&) { guard flat_read(flat_path(name), out, 1) else { abort(str("cannot read ", name)); } }
fn load(name: const u8[:], out: u16[>..]&) { guard flat_read(flat_path(name), out, 2) else { abort(str("cannot read ", name)); } }
fn load(name: const u8[:], out: u32[>..]&) { guard flat_read(flat_path(name), out, 4) else { abort(str("cannot read ", name)); } }
fn load(name: const u8[:], out: i32[>..]&) { guard flat_read(flat_path(name), out, 4) else { abort(str("cannot read ", name)); } }
fn load(name: const u8[:], out: u64[>..]&) { guard flat_read(flat_path(name), out, 8) else { abort(str("cannot read ", name)); } }
fn load(name: const u8[:], out: i64[>..]&) { guard flat_read(flat_path(name), out, 8) else { abort(str("cannot read ", name)); } }

// the file framed as a Goose image (ULEB128 byte count, then the bytes), for from_bytes
fn load_image(name: const u8[:]) -> u8[>..] {
    let p = flat_path(name);
    var img: u8[>..] = [];
    format_uleb(img, flat_size(p));
    guard flat_read(p, img, 1) else { abort(str("cannot read ", name)); }
    return img;
}

// meta.txt: `key value` lines
fn meta(key: const u8[:]) -> i64 {
    var text: u8[>..] = [];
    guard read_file(flat_path("meta.txt"), text) else { abort("cannot read meta.txt"); }
    for line in split(text, '\n') {
        let k, v, ok = split_once(line, ' ');
        if ok && k == key {
            let n, nok = parse_int(v);
            if nok { return n; }
        }
    }
    abort(str("meta: no ", key));
    return 0;
}

fn zeros(n: i64) -> u8[>..] {
    var a: u8[>..] = [];
    a.resize(n, 0);
    return a;
}
fn load_u8(name: const u8[:]) -> u8[>..] { var a: u8[>..] = []; load(name, a); return a; }
fn load_u32(name: const u8[:]) -> u32[>..] { var a: u32[>..] = []; load(name, a); return a; }
fn load_u64(name: const u8[:]) -> u64[>..] { var a: u64[>..] = []; load(name, a); return a; }

// an array of an index type, verified against its array by from_bytes
fn load_ids<T>(name: const u8[:]) -> T[>..] {
    let a, ok = from_bytes<T[>..]>(load_image(name));
    guard ok else { abort(str(name, ": an index does not fit its array")); }
    return a;
}

fn ms(t0: f64, t1: f64) -> f64 { (t1 - t0) * 1000.0 }

fn report(note: u8[>..]&, what: const u8[:], t0: f64, t1: f64) {
    format(note, what, " ");
    format_flt(note, ms(t0, t1), 1);
    format(note, " ms  ");
}
