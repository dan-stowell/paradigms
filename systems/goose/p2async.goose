// p2async.goose: problem 2 (reach) on W workers that share no memory, with no supersteps (ATTACKS.md 3).
// The same owner partition and the same local rules as p2par (p2rules.goose), but nobody waits for a round:
// a worker floods its part to a local fixpoint (or until FLUSH messages for other owners are waiting), sends
// what it found for others straight to their inboxes (ids with op bits, plus any global fact it found:
// signatures, names, reflect, broadcast to every worker), takes whatever has arrived (qpoll), and goes on.
// When it has nothing to do it tells main its message counts and blocks on its inbox.
// Termination (main): every worker's latest report says idle and, for every pair (i, j), the batches i sent
// to j equal the batches j says it received from i. A worker only reports when idle and only wakes on a
// message, so a message still in flight, or one received after a report, always shows as a mismatch.
//   p2async /home/exedev/paradigms/data/minio [workers 1..8] [lazy]      P2REPS, P2FLUSH (ids; default: fixpoint)
import flat;
import p2rules;

// one inbox type per worker (queues are per type). from = -1: the start signal; stop: the flood is over.
struct Msg<T> { tag: T, from: i64, stop: bool, reflect: bool, ids: u32[], sigs: u32[], named: u32[] }
struct Ready { k: i64 }
struct Idle { k: i64, sent: i64[8], recv: i64[8], pops: i64 }
struct Result { k: i64, live: u32[], sent: i64, ids: i64, polls: i64, idles: i64, tl: f64, tf: f64, ta: f64, tw: f64 }

fn post(j: i64, from: i64, stop: bool, refl: bool, ids: u32[:], sigs: u32[:], nm: u32[:]) {
    match j {
        0 => qput(Msg<u8[1]> { tag: [0; 1], from: from, stop: stop, reflect: refl, ids: ids, sigs: sigs, named: nm }),
        1 => qput(Msg<u8[2]> { tag: [0; 2], from: from, stop: stop, reflect: refl, ids: ids, sigs: sigs, named: nm }),
        2 => qput(Msg<u8[3]> { tag: [0; 3], from: from, stop: stop, reflect: refl, ids: ids, sigs: sigs, named: nm }),
        3 => qput(Msg<u8[4]> { tag: [0; 4], from: from, stop: stop, reflect: refl, ids: ids, sigs: sigs, named: nm }),
        4 => qput(Msg<u8[5]> { tag: [0; 5], from: from, stop: stop, reflect: refl, ids: ids, sigs: sigs, named: nm }),
        5 => qput(Msg<u8[6]> { tag: [0; 6], from: from, stop: stop, reflect: refl, ids: ids, sigs: sigs, named: nm }),
        6 => qput(Msg<u8[7]> { tag: [0; 7], from: from, stop: stop, reflect: refl, ids: ids, sigs: sigs, named: nm }),
        7 => qput(Msg<u8[8]> { tag: [0; 8], from: from, stop: stop, reflect: refl, ids: ids, sigs: sigs, named: nm }),
        _ => abort("at most 8 workers"),
    }
}

var sent: i64[8] = [0; 8];       // batches sent to / received from each worker, this flood
var recv: i64[8] = [0; 8];
var nids_sent = 0;
var refl_told = false;           // reflect already broadcast
var tl = 0.0;                    // this flood's time in local work, flush, absorb, waiting
var tf = 0.0;
var ta = 0.0;
var tw = 0.0;
var checked = 0;                 // markable methods already checked against the current facts

// what this worker found for others: ids bucketed by owner (a count and a scatter), facts to everyone
fn flush(k: i64, W: i64) {
    let facts = new_sigs.len > 0 || new_named.len > 0 || (reflect && !refl_told);
    guard out.len > 0 || facts else { return; }
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
    let rf = reflect && !refl_told;
    for j in W {
        if j != k && (facts || cnt[j + 1] > cnt[j]) {
            if facts { post(j, k, false, rf, sorted[cnt[j]..cnt[j + 1]], new_sigs[..], new_named[..]); }
            else { post(j, k, false, false, sorted[cnt[j]..cnt[j + 1]], [], []); }
            sent[j] += 1;
        }
    }
    if rf { refl_told = true; }
    nids_sent += out.len;
    out.clear();
    new_sigs.clear();
    new_named.clear();
}

// a message from another worker: its ids through the local rules, its facts into the local copies
fn absorb<T>(m: Msg<T>&) {
    recv[m.from] += 1;
    var facts = false;
    if m.reflect && !reflect { reflect = true; refl_told = true; facts = true; }
    for s in m.sigs { if s < sig_seen.len && sig_seen[s] == 0 { add_sig(s); facts = true; } }
    for n in m.named { if n < named.len && named[n] == 0 { named[n] = 1; facts = true; } }
    if facts { checked = 0; }
    for x in m.ids { apply(x & idMask, x & ~idMask); }
}

// local work to a fixpoint (or until `limit` ids for others wait), the method rule included
fn local(limit: i64) {
    loop {
        drain(limit);
        if out.len >= limit { return; }
        if facts_new { checked = 0; facts_new = false; }
        let more = methods(checked);
        checked = markable.len;
        if !more && work.len == 0 { return; }
    }
}

fn run<T>(k: i64, W: i64, lz: bool, reps: i64, limit: i64, tag: T) {
    let t0 = clock();
    lazy = lz;
    load_part(k, W);
    var roots: u32[>..] = [];
    load("p2_roots.u32", roots);
    let t_loaded = clock();
    var polls = 0;
    var idles = 0;
  for rep in reps {
    if rep > 0 { reset(); }
    for j in 8 { sent[j] = 0; recv[j] = 0; }
    nids_sent = 0;
    refl_told = false;
    checked = 0;
    polls = 0;
    idles = 0;
    qput(Ready { k: k });
    // wait for the start; another worker may already have sent us ids (monotone rules: apply them now)
    loop {
        let m = qget<Msg<T>>();
        if m.from < 0 { break; }
        absorb(m);
    }
    for r in roots { if r as i64 >= lo && (r as i64) < lo + live.len { apply(r, opMark); } }
    var stop = false;
    tl = 0.0; tf = 0.0; ta = 0.0; tw = 0.0;
    while !stop {
        let c0 = clock();
        local(limit);
        let c1 = clock();
        flush(k, W);
        let c2 = clock();
        tl += c1 - c0;
        tf += c2 - c1;
        // whatever has arrived meanwhile
        var got = false;
        loop {
            let m, ok = qpoll<Msg<T>>();
            if !ok { break; }
            polls++;
            if m.stop { stop = true; break; }
            absorb(m);
            got = true;
        }
        ta += clock() - c2;
        if stop || got || work.len > 0 || out.len > 0 { continue; }
        // nothing to do: report, then sleep on the inbox
        qput(Idle { k: k, sent: sent, recv: recv, pops: 0 });
        idles++;
        let c3 = clock();
        let m = qget<Msg<T>>();
        tw += clock() - c3;
        if m.stop { stop = true; } else { absorb(m); }
    }
  }
    var mine: u32[>..] = [];
    for i in live.len { if live[i] != 0 { mine.push((lo + i) as u32); } }
    var ns = 0;
    for j in 8 { ns += sent[j]; }
    qput(Result { k: k, live: mine[..], sent: ns, ids: nids_sent, polls: polls, idles: idles, tl: tl, tf: tf, ta: ta, tw: tw });
}

thread_fn worker(k: i64, W: i64, lz: bool, reps: i64, limit: i64) {
    match k {
        0 => run(k, W, lz, reps, limit, [0 as u8; 1]),
        1 => run(k, W, lz, reps, limit, [0 as u8; 2]),
        2 => run(k, W, lz, reps, limit, [0 as u8; 3]),
        3 => run(k, W, lz, reps, limit, [0 as u8; 4]),
        4 => run(k, W, lz, reps, limit, [0 as u8; 5]),
        5 => run(k, W, lz, reps, limit, [0 as u8; 6]),
        6 => run(k, W, lz, reps, limit, [0 as u8; 7]),
        7 => run(k, W, lz, reps, limit, [0 as u8; 8]),
        _ => abort("at most 8 workers"),
    }
}

fn main() {
    let a = args();
    var W = 8;
    if a.len > 2 { let n, ok = parse_int(a[2]); if ok { W = n; } }
    guard W >= 1 && W <= 8 else { abort("1..8 workers"); }
    let lz = a.len > 3 && a[3] == "lazy";
    var reps = 5;
    var rv: u8[>..] = [];
    if env("P2REPS", rv) { let n, ok = parse_int(rv); if ok && n >= 1 { reps = n; } }
    var limit = 1 << 62;
    var fv: u8[>..] = [];
    if env("P2FLUSH", fv) { let n, ok = parse_int(fv); if ok && n >= 1 { limit = n; } }
    let t0 = clock();
    var tids: i64[>..] = [];
    for k in W { tids.push(thread_spawn(worker, k, W, lz, reps, limit)); }
    var sentm: i64[64] = [0; 64];     // sentm[i * 8 + j]: batches i sent to j (i's latest report)
    var recvm: i64[64] = [0; 64];     // recvm[j * 8 + i]: batches j received from i
    var reported: bool[8] = [false; 8];
    var best = 1.0e9;
    var reports = 0;
    var t_ready = 0.0;
  for rep in reps {
    for i in W { let r = qget<Ready>(); }
    if rep == 0 { t_ready = clock(); }
    let t_go = clock();
    for i in 64 { sentm[i] = 0; recvm[i] = 0; }
    for i in 8 { reported[i] = false; }
    reports = 0;
    for j in W { post(j, -1, false, false, [], [], []); }
    loop {
        let s = qget<Idle>();
        reports++;
        reported[s.k] = true;
        for j in 8 { sentm[s.k * 8 + j] = s.sent[j]; recvm[s.k * 8 + j] = s.recv[j]; }
        var done = true;
        for i in W { if !reported[i] { done = false; } }
        if done {
            for i in W { for j in W { if sentm[i * 8 + j] != recvm[j * 8 + i] { done = false; } } }
        }
        if done { break; }
    }
    for j in W { post(j, -1, true, false, [], [], []); }
    best = min(best, clock() - t_go);
  }
    let t_flood = clock();
    var parts: Result[>..] = [];
    for i in W { parts.push(qget<Result>()); }
    for id in tids { thread_wait(id); }
    var list: u32[>..] = [];
    var batches = 0;
    var ids = 0;
    var polls = 0;
    var idles = 0;
    for j in W { for p in parts { if p.k == j { list.append(p.live); batches += p.sent; ids += p.ids; polls += p.polls; idles += p.idles; } } }
    var want: u32[>..] = [];
    load("oracle/p2_live.u32", want);
    var bad = 0;
    if list.len != want.len { bad = 1; }
    else { for i in list.len { if list[i] != want[i] { bad++; } } }
    var note: u8[>..] = [];
    format(note, "p2async goose (", W, " workers", if lz { ", lazy decode" } else { "" }, "): ");
    report(note, "spawn+load", t0, t_ready);
    report(note, str("flood x", reps), t_ready, t_flood);
    format(note, "best flood ");
    format_flt(note, best * 1000.0, 1);
    format(note, " ms  0 supersteps, ", batches, " batches, ", ids, " ids, ", idles, " idle reports, ",
        polls, " polled, ", list.len, " live; ", bad, " differ from oracle/p2_live.u32");
    print(note);
    if env_set("P2TRACE") {
        for p in parts {
            var n2: u8[>..] = [];
            format(n2, "  worker ", p.k, ": local ");
            format_flt(n2, p.tl * 1000.0, 1);
            format(n2, " flush ");
            format_flt(n2, p.tf * 1000.0, 1);
            format(n2, " absorb ");
            format_flt(n2, p.ta * 1000.0, 1);
            format(n2, " wait ");
            format_flt(n2, p.tw * 1000.0, 1);
            format(n2, " ms, ", p.idles, " idle, ", p.sent, " batches");
            print(n2);
        }
    }
    print(if bad == 0 { "P2 OK" } else { "P2 FAIL" });
    if bad != 0 { exit(1); }
}
