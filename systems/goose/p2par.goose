// p2par.goose: problem 2 (reach) on W workers that share no memory. Each worker owns a range of ids (cut so the
// ranges hold equal numbers of edges): their live/iface marks, their edges and their methods. A worker floods
// locally; an edge to an id another worker owns becomes a message (the id with op bits) in a per-owner inbox.
// The global facts (live signatures, names called by reflection, reflect seen) go to main, which merges them
// and broadcasts them with the go/stop decision of each superstep (bulk-synchronous rounds).
//   p2par /home/exedev/paradigms/data/minio [workers 1..8] [lazy]
// lazy: relocation edges are decoded from the relocation columns as the flood reaches them (p2rules.goose)
import flat;
import p2rules;

// ---- messages (flat values). Queues are per type, so a worker's inbox is a type of its own: Inbox<Tag>.
struct Inbox<T> { tag: T, ids: u32[] }
struct Go<T> { tag: T, done: bool, reflect: bool, sigs: u32[], named: u32[] }
struct Status { k: i64, sent: i64, local: i64, reflect: bool, sigs: u32[], named: u32[] }
struct Result { k: i64, live: u32[] }


// the inbox of worker j: one type per worker
fn send(j: i64, ids: u32[:]) {
    match j {
        0 => qput(Inbox<u8[1]> { tag: [0; 1], ids: ids }),
        1 => qput(Inbox<u8[2]> { tag: [0; 2], ids: ids }),
        2 => qput(Inbox<u8[3]> { tag: [0; 3], ids: ids }),
        3 => qput(Inbox<u8[4]> { tag: [0; 4], ids: ids }),
        4 => qput(Inbox<u8[5]> { tag: [0; 5], ids: ids }),
        5 => qput(Inbox<u8[6]> { tag: [0; 6], ids: ids }),
        6 => qput(Inbox<u8[7]> { tag: [0; 7], ids: ids }),
        7 => qput(Inbox<u8[8]> { tag: [0; 8], ids: ids }),
        _ => abort("at most 8 workers"),
    }
}

fn go(j: i64, done: bool, refl: bool, sigs: u32[:], nm: u32[:]) {
    match j {
        0 => qput(Go<u8[1]> { tag: [0; 1], done: done, reflect: refl, sigs: sigs, named: nm }),
        1 => qput(Go<u8[2]> { tag: [0; 2], done: done, reflect: refl, sigs: sigs, named: nm }),
        2 => qput(Go<u8[3]> { tag: [0; 3], done: done, reflect: refl, sigs: sigs, named: nm }),
        3 => qput(Go<u8[4]> { tag: [0; 4], done: done, reflect: refl, sigs: sigs, named: nm }),
        4 => qput(Go<u8[5]> { tag: [0; 5], done: done, reflect: refl, sigs: sigs, named: nm }),
        5 => qput(Go<u8[6]> { tag: [0; 6], done: done, reflect: refl, sigs: sigs, named: nm }),
        6 => qput(Go<u8[7]> { tag: [0; 7], done: done, reflect: refl, sigs: sigs, named: nm }),
        7 => qput(Go<u8[8]> { tag: [0; 8], done: done, reflect: refl, sigs: sigs, named: nm }),
        _ => abort("at most 8 workers"),
    }
}

// messages out, bucketed by owner (a counting pass and a scatter), one inbox per other worker
fn flush(k: i64, W: i64) -> i64 {
    var cnt: i64[9] = [0; 9];
    for x in out { cnt[owner(x & idMask) + 1] += 1; }
    for j in W { cnt[j + 1] += cnt[j]; }
    var pos: i64[9] = cnt;
    var sorted: u32[>..] = [];
    sorted.resize(out.len, 0);
    for x in out {
        let j = owner(x & idMask);
        sorted[pos[j]] = x;
        pos[j] += 1;
    }
    for j in W { if j != k { send(j, sorted[cnt[j]..cnt[j + 1]]); } }
    let n = out.len;
    out.clear();
    return n;
}

fn run<T>(k: i64, W: i64, lz: bool, reps: i64, tag: T) {
    let t0 = clock();
    lazy = lz;
    load_part(k, W);
    var roots: u32[>..] = [];
    load("p2_roots.u32", roots);
    let t_loaded = clock();
    // a barrier: every worker has its part in memory before the flood starts (the warm time)
    qput(Status { k: k, sent: 0, local: 0, reflect: false, sigs: new_sigs[..], named: new_named[..] });
    var t1 = 0.0;
    var rounds = 0;
    var busy = 0.0;
    var tflush = 0.0;
    var tapply = 0.0;
  for rep in reps {
    if rep > 0 {     // the next timed flood starts from scratch: reset, then the barrier again
        reset();
        qput(Status { k: k, sent: 0, local: 0, reflect: false, sigs: new_sigs[..], named: new_named[..] });
    }
    let start = qget<Go<T>>();
    t1 = clock();
    rounds = 0;
    busy = 0.0;
    tflush = 0.0;
    tapply = 0.0;
    for r in roots { if r as i64 >= lo && (r as i64) < lo + live.len { apply(r, opMark); } }
    var checked = 0;
    loop {
        rounds++;
        let ta = clock();
        loop {
            drain(1 << 62);
            if facts_new { checked = 0; facts_new = false; }
            let more = methods(checked);
            checked = markable.len;
            if !more && work.len == 0 { break; }
        }
        let tb = clock();
        let sent = flush(k, W);
        busy += tb - ta;
        tflush += clock() - tb;
        qput(Status { k: k, sent: sent, local: 0, reflect: reflect, sigs: new_sigs[..], named: new_named[..] });
        new_sigs.clear();
        new_named.clear();
        let g = qget<Go<T>>();
        if g.done {
            for j in W - 1 { let m = qget<Inbox<T>>(); }     // this step's (empty) inboxes, so none is left for the next flood
            break;
        }
        var facts = false;
        if g.reflect && !reflect { reflect = true; facts = true; }
        for s in g.sigs { if s < sig_seen.len && sig_seen[s] == 0 { add_sig(s); facts = true; } }
        for n in g.named { if n < named.len && named[n] == 0 { named[n] = 1; facts = true; } }
        if facts { checked = 0; }
        for j in W - 1 {
            let m = qget<Inbox<T>>();
            let tc = clock();
            for x in m.ids { apply(x & idMask, x & ~idMask); }
            tapply += clock() - tc;
        }
    }
  }
    let t2 = clock();
    var mine: u32[>..] = [];
    for i in live.len { if live[i] != 0 { mine.push((lo + i) as u32); } }
    qput(Result { k: k, live: mine[..] });
    var note: u8[>..] = [];
    format(note, "  worker ", k, ": ids ", lo, "..", lo + live.len, ", ", edst.len, " edges; ");
    report(note, "load", t0, t_loaded);
    report(note, "flood", t1, t2);
    format(note, "(drain ");
    format_flt(note, busy * 1000.0, 1);
    format(note, ", flush ");
    format_flt(note, tflush * 1000.0, 1);
    format(note, ", apply inbox ");
    format_flt(note, tapply * 1000.0, 1);
    format(note, ") ", rounds, " rounds");
    print(note);
}

thread_fn worker(k: i64, W: i64, lz: bool, reps: i64) {
    match k {
        0 => run(k, W, lz, reps, [0 as u8; 1]),
        1 => run(k, W, lz, reps, [0 as u8; 2]),
        2 => run(k, W, lz, reps, [0 as u8; 3]),
        3 => run(k, W, lz, reps, [0 as u8; 4]),
        4 => run(k, W, lz, reps, [0 as u8; 5]),
        5 => run(k, W, lz, reps, [0 as u8; 6]),
        6 => run(k, W, lz, reps, [0 as u8; 7]),
        7 => run(k, W, lz, reps, [0 as u8; 8]),
        _ => abort("at most 8 workers"),
    }
}

fn main() {
    let a = args();
    var W = 8;
    if a.len > 2 { let n, ok = parse_int(a[2]); if ok { W = n; } }
    guard W >= 1 && W <= 8 else { abort("1..8 workers"); }
    let lz = a.len > 3 && a[3] == "lazy";
    var reps = 5;     // timed floods (the warm time is the best); P2REPS overrides
    var rv: u8[>..] = [];
    if env("P2REPS", rv) { let n, ok = parse_int(rv); if ok && n >= 1 { reps = n; } }
    let t0 = clock();
    var ids: i64[>..] = [];
    for k in W { ids.push(thread_spawn(worker, k, W, lz, reps)); }
    // main: the barrier of each superstep, and the global facts
    let nsig = meta("p2_sigs");
    var seen_sig: u8[>..] = zeros(nsig);
    var seen_named: u8[>..] = zeros(meta("p2_strings") + 1);
    var refl = false;
    var rounds = 0;
    var msgs = 0;
    var t_ready = 0.0;
    var best = 1.0e9;
    let trace = env_set("P2TRACE");
    var steps: u8[>..] = [];
  for rep in reps {
    for i in W { let s = qget<Status>(); }
    if rep == 0 { t_ready = clock(); }
    let t_go = clock();
    for i in seen_sig.len { seen_sig[i] = 0; }
    for i in seen_named.len { seen_named[i] = 0; }
    refl = false;
    rounds = 0;
    msgs = 0;
    steps.clear();
    for j in W { go(j, false, false, [], []); }
    loop {
        var sigs: u32[>..] = [];
        var nm: u32[>..] = [];
        var sent = 0;
        var fresh = false;
        for i in W {
            let s = qget<Status>();
            sent += s.sent;
            if s.reflect && !refl { refl = true; fresh = true; }
            for x in s.sigs { if x < seen_sig.len && seen_sig[x] == 0 { seen_sig[x] = 1; sigs.push(x); fresh = true; } }
            for x in s.named { if x < seen_named.len && seen_named[x] == 0 { seen_named[x] = 1; nm.push(x); fresh = true; } }
        }
        rounds++;
        msgs += sent;
        if trace { format(steps, " ", sent, "@"); format_flt(steps, ms(t_ready, clock()), 1); }
        let done = sent == 0 && !fresh;
        for j in W { go(j, done, refl, sigs[..], nm[..]); }
        if done { break; }
    }
    best = min(best, clock() - t_go);
  }
    let t_flood = clock();
    var parts: Result[>..] = [];      // variable-size elements, inline: walked, not indexed
    for i in W { parts.push(qget<Result>()); }
    for id in ids { thread_wait(id); }
    let t1 = clock();
    var list: u32[>..] = [];
    for j in W { for p in parts { if p.k == j { list.append(p.live); } } }
    var want: u32[>..] = [];
    load("oracle/p2_live.u32", want);
    var bad = 0;
    if list.len != want.len { bad = 1; }
    else { for i in list.len { if list[i] != want[i] { bad++; } } }
    var note: u8[>..] = [];
    format(note, "p2par goose (", W, " workers", if lz { ", lazy decode" } else { "" }, "): ");
    report(note, "spawn+load", t0, t_ready);
    report(note, str("flood x", reps), t_ready, t_flood);
    format(note, "best flood ");
    format_flt(note, best * 1000.0, 1);
    format(note, " ms  ");
    report(note, "collect", t_flood, t1);
    report(note, "total", t0, t1);
    format(note, rounds, " supersteps, ", msgs, " messages, ", list.len, " live; ", bad, " differ from oracle/p2_live.u32");
    print(note);
    if trace { print("  supersteps (messages@ms):", steps); }
    print(if bad == 0 { "P2 OK" } else { "P2 FAIL" });
    if bad != 0 { exit(1); }
}
