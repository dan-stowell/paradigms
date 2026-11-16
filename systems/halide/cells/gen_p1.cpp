// Problem 1 in Halide: 1a the join by name (Resolve), 1b relocation targets (RelTarget), 1c the content-hash join
// (ContentJoin). The joins are join.h's rounds of scatter-min + gather; GeneratorParam par picks the schedule.
#include "Halide.h"
#include "join.h"
using namespace Halide;
using hj::u64e;

static Expr u32(Expr e) { return cast<uint32_t>(e); }
static Expr i32(Expr e) { return cast<int32_t>(e); }
static const uint32_t rankDef = 1u << 29;

// The ids of the slots with keep(g), g in [1, N], in id order: a scan, then a scatter to position scan(g) - 1.
// A scan is an update with a loop-carried dependence, which Halide runs serially; the parallel schedule is a
// different algorithm (Halide cannot derive it): blocks of 2^14 slots, each block's scan (parallel over blocks),
// a serial scan of the block totals, then the scatter.
static Func compact(const std::string &name, Func keep, Expr N, Expr M, bool par) {
    Var g("g"), j("j"), b("b");
    Func ids(name + "_ids"), pos(name + "_pos");
    if (!par) {
        Func inc(name + "_inc");
        RDom rs(1, N, name + "_rs");
        inc(g) = 0;
        inc(rs) = inc(rs - 1) + i32(keep(rs));
        inc.compute_root();
        pos(g) = inc(g) - 1;
    } else {
        const int B = 1 << 14;
        Expr nB = (N + B) / B;  // blocks over [0, N]
        Func inb(name + "_inb"), tot(name + "_tot"), off(name + "_off");
        RDom ri(1, B - 1, name + "_ri");
        auto kp = [&](Expr x) { return select(x >= 1 && x <= N, i32(keep(min(x, N))), 0); };
        inb(j, b) = kp(b * B + j);  // j: position in block b
        inb(ri, b) = inb(ri - 1, b) + kp(b * B + ri);
        tot(b) = inb(B - 1, b);
        RDom rb(1, nB - 1, name + "_rb");
        off(b) = 0;
        off(rb) = off(rb - 1) + tot(rb - 1);
        pos(g) = off(g / B) + inb(g % B, g / B) - 1;
        inb.compute_root().parallel(b);
        inb.update().parallel(b);
        off.compute_root();
    }
    RDom rk(1, N, name + "_rk");
    rk.where(keep(min(rk, N)) != 0);  // (min: the parallel split runs past the end by a tail)
    ids(j) = 0;
    ids(clamp(pos(rk), 0, M - 1)) = rk;
    ids.compute_root();
    if (par) hj::parChunks(ids.update().allow_race_conditions(), rk);  // distinct positions
    Func idc(name + "_idc");  // (inlined) the ids are slot ids: tells bounds inference so
    idc(j) = unsafe_promise_clamped(ids(j), 1, N);
    return idc;
}

class Resolve : public Generator<Resolve> {
public:
    GeneratorParam<bool> par{"par", false};
    GeneratorParam<int> logT{"logT", 20};
    GeneratorParam<int> rounds{"rounds", 8};

    Input<Buffer<uint8_t, 1>> kind{"kind"};
    Input<Buffer<uint8_t, 1>> flags{"flags"};
    Input<Buffer<uint64_t, 2>> key{"key"};    // (2, nslots + 1)
    Input<Buffer<uint32_t, 1>> rank{"rank"};
    Input<Buffer<uint64_t, 2>> want{"want"};  // (2, nwant)
    Input<Buffer<uint64_t, 2>> bkey{"bkey"};  // (2, nbuiltin)
    Input<int> nkeyed{"nkeyed"};              // slots in the join (kinds 4, 5 with a key, and the kind 1 candidates)
    Output<Buffer<uint32_t, 1>> canon{"canon"};
    Output<Buffer<uint32_t, 1>> builtin{"builtin"};
    Output<Buffer<int32_t, 0>> check{"check"};  // entries that found no group: 0

    Var g{"g"}, j{"j"}, i{"i"};

    void generate() {
        Expr N = kind.dim(0).extent() - 1, nw = want.dim(1).extent(), nb = bkey.dim(1).extent();
        Expr M1 = nkeyed, M = M1 + nw + nb;
        Func keyed("keyed");
        keyed(g) = cast<uint8_t>((kind(g) == 1 || kind(g) == 4 || kind(g) == 5) && key(0, g) != 0);
        Func ids = compact("keyed", keyed, N, M1, par);

        // the entries: keyed slots, then the wanted names (they create a group, with value 0), then the builtins
        // (they only look one up)
        auto sel = [&](Expr jj, int c) {
            Expr js = clamp(jj, 0, M1 - 1), jw = clamp(jj - M1, 0, nw - 1), jb = clamp(jj - M1 - nw, 0, nb - 1);
            return select(jj < M1, key(c, ids(js)), jj < M1 + nw, want(c, jw), bkey(c, jb));
        };
        Func k1("k1"), k2("k2"), creates("creates");
        k1(j) = sel(j, 0);
        k2(j) = sel(j, 1);
        creates(j) = cast<uint8_t>(select(j < M1, kind(ids(clamp(j, 0, M1 - 1))) != 1, j < M1 + nw));
        k1.compute_root(), k2.compute_root(), creates.compute_root();
        hj::Join J = hj::hashJoin("name", k1, k2, creates, M, logT, rounds, par);

        // the winner of each group: the maximum of rank << 32 | ~id over its slots
        Func win("win");
        RDom rw(0, M1, "rw");
        rw.where(J.rep(rw) >= 0);
        Expr id = ids(rw);
        win(j) = u64e(0);
        Expr at = clamp(J.rep(rw), 0, M - 1);
        win(at) = max(win(at), u64e(rank(id)) << 32 | u64e(~u32(id)));

        // canonical ids: itself (0 for a debug-only reference), the winner for a slot in a group
        canon(g) = select((flags(g) & 2) != 0 || g == 0, u32(0), u32(g));
        RDom rc(0, M1, "rc");
        rc.where(J.rep(rc) >= 0);
        canon(unsafe_promise_clamped(ids(rc), 0, N)) = ~u32(win(clamp(J.rep(rc), 0, M - 1)));
        Expr jb = M1 + nw + i, rb = J.rep(jb);
        Expr v = win(clamp(rb, 0, M - 1));
        builtin(i) = select(rb >= 0 && v != 0 && ((v >> 32) & rankDef) != 0, ~u32(v), u32(0));
        check() = J.left();

        // schedule
        win.compute_root();
        if (par) {
            k1.parallel(j, 1 << 14, TailStrategy::GuardWithIf);
            k2.parallel(j, 1 << 14, TailStrategy::GuardWithIf);
            creates.parallel(j, 1 << 14, TailStrategy::GuardWithIf);
            win.parallel(j, 1 << 14, TailStrategy::GuardWithIf);
            hj::parChunks(win.update().atomic(), rw);
            canon.parallel(g, 1 << 16, TailStrategy::GuardWithIf);
            hj::parChunks(canon.update().allow_race_conditions(), rc);  // ids are distinct
        }
    }
};

// 1b. For relocation q of slot g (in object o): its target through o's package table, then the canonical id.
// A ragged update: (relocation within the slot, slot), the slot's range [srel(g), srel(g+1)).
class RelTarget : public Generator<RelTarget> {
public:
    GeneratorParam<bool> par{"par", false};

    Input<Buffer<uint32_t, 1>> sobj{"sobj"};
    Input<Buffer<uint32_t, 1>> srel{"srel"};
    Input<Buffer<uint32_t, 1>> pkgOff{"pkgOff"};
    Input<Buffer<uint32_t, 1>> pkg{"pkg"};
    Input<Buffer<uint32_t, 1>> rpkg{"rpkg"};
    Input<Buffer<uint32_t, 1>> ridx{"ridx"};
    Input<Buffer<uint32_t, 1>> canon{"canon"};
    Input<Buffer<uint32_t, 1>> builtin{"builtin"};
    Output<Buffer<uint32_t, 1>> tgt{"tgt"};

    Var q{"q"};

    void generate() {
        Expr N = sobj.dim(0).extent() - 1, R = rpkg.dim(0).extent(), nb = builtin.dim(0).extent();
        RDom rr(0, 1 << 24, 1, N, "rr");
        Expr g = min(rr.y, N);  // (the parallel split runs the RDom past its end by a tail)
        rr.where(rr.x < i32(srel(g + 1) - srel(g)));
        Expr r = unsafe_promise_clamped(i32(srel(g)) + rr.x, 0, R - 1);
        Expr o = unsafe_promise_clamped(i32(sobj(g)), 0, pkgOff.dim(0).extent() - 2), base = i32(pkgOff(o)), npk = i32(pkgOff(o + 1)) - base - 5;
        Expr p = rpkg(r), idx = ridx(r), sp = p - Expr((uint32_t)2147483643u);
        Expr t = select(sp < 5, u32(npk) + sp, p);
        Expr pb = pkg(unsafe_promise_clamped(base + i32(t), 0, pkg.dim(0).extent() - 1));
        Expr valid = t < u32(npk + 5) && pb != Expr((uint32_t)0xFFFFFFFFu);
        Expr s = select(sp == 1, select(idx < u32(nb), builtin(clamp(i32(idx), 0, nb - 1)), u32(0)), pb + idx);
        tgt(q) = undef<uint32_t>();  // every relocation belongs to one slot's range
        tgt(r) = select(valid && s != 0, canon(unsafe_promise_clamped(i32(s), 0, N)), u32(0));
        if (par) {
            RVar go("go"), gi("gi");
            tgt.update().allow_race_conditions().split(rr.y, go, gi, 4096, TailStrategy::GuardWithIf).parallel(go);
        }
    }
};

// 1c. Content-addressed copies with mark 1 or 2: kind 2 grouped by k1, kind 3 by (k1, k2). The winner is the
// maximum of (mark 1) << 63 | size << 32 | ~id for kind 2, (mark 1) << 63 | ~id for kind 3.
class ContentJoin : public Generator<ContentJoin> {
public:
    GeneratorParam<bool> par{"par", false};
    GeneratorParam<int> logT{"logT", 20};
    GeneratorParam<int> rounds{"rounds", 8};

    Input<Buffer<uint8_t, 1>> kind{"kind"};
    Input<Buffer<uint8_t, 1>> mark{"mark"};
    Input<Buffer<uint64_t, 2>> hkey{"hkey"};  // (2, nslots + 1)
    Input<Buffer<uint32_t, 1>> canon{"canon"};
    Input<int> ncopies{"ncopies"};
    Output<Buffer<uint32_t, 1>> out{"out"};
    Output<Buffer<int32_t, 0>> check{"check"};  // entries that found no group, sizes >= 2^31: 0

    Var g{"g"}, j{"j"};

    void generate() {
        Expr N = kind.dim(0).extent() - 1, M = ncopies;
        Func copy("copy");
        copy(g) = cast<uint8_t>((kind(g) == 2 || kind(g) == 3) && mark(g) != 0);
        Func ids = compact("copy", copy, N, M, par);
        Func k1("k1"), k2("k2"), all("all");
        Expr id = ids(j);
        k1(j) = hkey(0, id);
        // the kind is part of the key: kind 3's k2 is 4 hash bytes (< 2^32), kind 2 gets 2^40
        k2(j) = select(kind(id) == 3, hkey(1, id), u64e(1) << 40);
        all(j) = cast<uint8_t>(1);
        k1.compute_root(), k2.compute_root();
        hj::Join J = hj::hashJoin("hash", k1, k2, all, M, logT, rounds, par);

        auto score = [&](Expr e) {
            Expr s = ids(e);
            Expr m1 = select(mark(s) == 1, u64e(1) << 63, u64e(0));
            return m1 | select(kind(s) == 2, hkey(1, s) << 32, u64e(0)) | u64e(~u32(s));
        };
        Func win("win");
        RDom rw(0, M, "rw");
        win(j) = u64e(0);
        Expr at = clamp(J.rep(rw), 0, M - 1);
        win(at) = max(win(at), score(rw));

        out(g) = canon(g);
        RDom ro(0, M, "ro");
        Expr s = ids(ro), w = win(clamp(J.rep(ro), 0, M - 1));
        Expr wid = ~u32(w), wmark1 = (w >> 63) != 0;
        Expr keepSelf = kind(s) == 2 && mark(s) == 2 && hkey(1, s) > hkey(1, unsafe_promise_clamped(i32(wid), 0, N));
        ro.where(wmark1 && !keepSelf);
        out(unsafe_promise_clamped(s, 0, N)) = wid;
        RDom rs(0, M, "rs");
        Expr big = kind(ids(rs)) == 2 && hkey(1, ids(rs)) >= (u64e(1) << 31);
        check() = J.left() + sum(select(big, 1, 0));

        win.compute_root();
        if (par) {
            k1.parallel(j, 1 << 14, TailStrategy::GuardWithIf);
            k2.parallel(j, 1 << 14, TailStrategy::GuardWithIf);
            win.parallel(j, 1 << 14, TailStrategy::GuardWithIf);
            hj::parChunks(win.update().atomic(), rw);
            out.parallel(g, 1 << 16, TailStrategy::GuardWithIf);
            hj::parChunks(out.update().allow_race_conditions(), ro);  // ids are distinct
        }
    }
};

// The sizes the joins need before they start (Halide bounds every Func by expressions of its inputs, so a count
// computed in a pipeline cannot size a later stage of the same pipeline): slots in the join by name, and
// content-addressed copies with a mark.
class Counts : public Generator<Counts> {
public:
    Input<Buffer<uint8_t, 1>> kind{"kind"};
    Input<Buffer<uint64_t, 2>> key{"key"};
    Input<Buffer<uint8_t, 1>> mark{"mark"};
    Output<Buffer<int32_t, 0>> nkeyed{"nkeyed"};
    Output<Buffer<int32_t, 0>> ncopies{"ncopies"};
    void generate() {
        Expr N = kind.dim(0).extent() - 1;
        RDom r(1, N, "r");
        Expr k = kind(r);
        nkeyed() = sum(select((k == 1 || k == 4 || k == 5) && key(0, r) != 0, 1, 0));
        ncopies() = sum(select((k == 2 || k == 3) && mark(r) != 0, 1, 0));
    }
};

HALIDE_REGISTER_GENERATOR(Counts, counts)
HALIDE_REGISTER_GENERATOR(Resolve, resolve)
HALIDE_REGISTER_GENERATOR(RelTarget, reltarget)
HALIDE_REGISTER_GENERATOR(ContentJoin, contentjoin)
