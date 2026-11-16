import std;

var live: u8[>..] = [];
type Sym = u32 in live;

struct Tag0 { x: u8 }
struct Tag1 { x: u8 }
struct Inbox<T> { tag: T, ids: u32[] }

thread_fn worker(k: i64) {
    for i in 10 { live.push(0); }
    let s = 3 as Sym;
    live[s] = 1;
    let m = qget<Inbox<Tag1>>();
    print("worker ", k, " got ", m.ids.len, " live ", live.len);
}

fn main() {
    let id = thread_spawn(worker, 1);
    var ids: u32[>..] = [1, 2, 3];
    qput(Inbox<Tag0> { tag: Tag0 { 0 }, ids: ids[..] });
    qput(Inbox<Tag1> { tag: Tag1 { 0 }, ids: ids[..] });
    thread_wait(id);
    let m = qget<Inbox<Tag0>>();
    print("main got ", m.ids.len);
}
