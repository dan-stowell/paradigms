// Problem 3 in Halide: 3a layout (Layout generator) and 3b relocate (Relocate generator).
// Each algorithm is written once; GeneratorParams pick the schedule (and, where said, a variant of the algorithm).
#include "Halide.h"
using namespace Halide;

static const int nB = 16, nC = 13, nK = nB * nC; // buckets, alignment classes, (bucket, class) keys per unit
static const int bText = 1, bTextFIPS = 2, bTypelink = 6;
static const int unitMax = 8192;                 // a unit is at most 8192 slots, so at most 8192 placed symbols

static Expr u32(Expr e) { return cast<uint32_t>(e); }
static Expr i32(Expr e) { return cast<int32_t>(e); }
static Expr rnd(Expr v, Expr a) { return (v + a - 1) & ~(a - 1); }  // a is a power of two

// The typelink comparator: type strings bytewise, shorter first, then id. Halide has no data-dependent loop
// inside an expression, so a variable-length string compare is an extern C function (tl_less, in p3.cpp).
static Expr tl_less(Expr a, Expr b) {
    return Internal::Call::make(Int(32), "tl_less", {i32(a), i32(b)}, Internal::Call::Extern) != 0;
}

class Layout : public Generator<Layout> {
public:
    // csize schedule: 0 serial scatter, 1 atomic() parallel scatter, 2 rfactor over chunks, 3 by unit (parallel)
    GeneratorParam<int> csched{"csched", 0};
    // cursor walk: 0 serial over units, 1 parallel over units
    GeneratorParam<int> wsched{"wsched", 0};
    GeneratorParam<int> chunks{"chunks", 8};
    GeneratorParam<int> nChunk{"nChunk", 3};  // typelink sort: 8-byte prefix keys compared before the extern

    Input<Buffer<uint8_t, 1>> bucket{"bucket"};
    Input<Buffer<uint8_t, 1>> cls{"cls"};
    Input<Buffer<uint32_t, 1>> size{"size"};
    Input<Buffer<uint32_t, 1>> unit{"unit"};
    Input<Buffer<uint32_t, 1>> bstart{"bstart"};
    Input<Buffer<uint32_t, 1>> bmin{"bmin"};
    Input<Buffer<uint32_t, 1>> tlstr{"tlstr"};
    Input<Buffer<uint64_t, 1>> soff{"soff"};
    Input<Buffer<uint8_t, 1>> sbin{"sbin"};
    Input<int> nunits{"nunits"};
    Input<int> ntl{"ntl"};
    Input<uint32_t> minFunc{"minFunc"};
    Output<Buffer<uint32_t, 1>> addr{"addr"};

    Var i{"i"}, b{"b"}, c{"c"}, u{"u"}, x{"x"}, k{"k"};

    void generate() {
        Expr L = bucket.dim(0).extent();
        auto B = [&](Expr j) { return i32(bucket(j)); };
        auto C = [&](Expr j) { return i32(cls(j)); };

        // occupied size of each symbol
        Func occ("occ");
        Expr s = size(i);
        Expr isText = B(i) == bText || B(i) == bTextFIPS;
        occ(i) = rnd(select(isText, max(s, minFunc), s), u32(1) << u32(C(i)));

        // symbols per unit (units are contiguous in the list): a histogram, then an exclusive scan
        Func ulen("ulen"), ustart("ustart");
        RDom r(0, L, "r");
        ulen(u) = 0;
        ulen(clamp(i32(unit(r)), 0, nunits - 1)) += 1;
        RDom ru(0, nunits, "ru");
        ustart(u) = 0;
        ustart(ru) = select(ru == 0, 0, ustart(max(ru - 1, 0)) + ulen(max(ru - 1, 0)));

        // 1. bytes per (class, bucket, unit), typelinks excluded
        Func csize("csize");
        csize(c, b, u) = u32(0);
        Expr contrib = select(B(r) == bTypelink, u32(0), occ(r));
        RDom rl(0, unitMax, "rl");
        if (csched < 3) {
            csize(clamp(C(r), 0, nC - 1), clamp(B(r), 0, nB - 1), clamp(i32(unit(r)), 0, nunits - 1)) += contrib;
        } else {
            // the same sum, split by unit: unit u's symbols are [ustart(u), ustart(u) + ulen(u))
            rl.where(rl < ulen(u));
            Expr j = clamp(ustart(u) + rl, 0, L - 1);
            csize(clamp(C(j), 0, nC - 1), clamp(B(j), 0, nB - 1), u) += select(B(j) == bTypelink, u32(0), occ(j));
        }

        // 2. the walk over units, per bucket: align the unit's block, then its classes from 12 down to 0
        RDom rc(0, nC, "rc");
        Func tot("tot"), top("top"), endv("endv");
        tot(b, u) = sum(csize(rc, b, u));
        top(b, u) = maximum(select(csize(rc, b, u) != 0, rc.x, -1));
        Expr al = select(top(b, ru) >= 0, max(bmin(b), u32(1) << u32(max(top(b, ru), 0))), u32(1));
        endv(b, u) = u32(0);
        endv(b, ru) = rnd(select(ru == 0, bstart(b), endv(b, max(ru - 1, 0))), al) + tot(b, ru);
        Func base("base");  // the first address of class c of bucket b in unit u
        base(c, b, u) = endv(b, u) - tot(b, u) + sum(select(rc > c, csize(rc, b, u), u32(0)));

        // 3. the bump cursor, per unit: each symbol takes its (bucket, class) cursor, then moves it by its size.
        //    A Halide update writes ONE site per iteration, so "read cursor, store address, bump cursor" is two
        //    iterations of a phase RVar (rw.x) over one Func W whose sites are [0, nK) cursors, [nK, ...) addresses.
        Func W("W");
        W(x, u) = undef<uint32_t>();
        RDom rk(0, nK, "rk");
        W(rk, u) = base(rk % nC, rk / nC, u);
        RDom rw(0, 2, 0, unitMax, "rw");
        rw.where(rw.y < ulen(u));
        Expr j = clamp(ustart(u) + rw.y, 0, L - 1);
        Expr key = clamp(B(j) * nC + C(j), 0, nK - 1);
        W(select(rw.x == 0, nK + rw.y, key), u) = W(key, u) + select(rw.x == 0, u32(0), occ(j));

        // 4. typelinks: compact their indices, sort by type string (merge sort, merge path by binary search),
        //    then a serial walk: round up to the class, advance by size (not occupied)
        Func tpos("tpos"), tl0("tl0");
        tpos(i) = 0;
        tpos(r) = select(r == 0, 0, tpos(max(r - 1, 0))) + select(B(r) == bTypelink, 1, 0);
        tl0(k) = 0;
        RDom rt(0, L, "rt");
        rt.where(B(rt) == bTypelink);
        tl0(clamp(tpos(rt) - 1, 0, ntl - 1)) = rt;
        // sort keys of each typelink position p: its type string's first 8*nChunk bytes as big-endian u64s;
        // only a tie on all of them calls the extern comparator (behind an if_then_else, so it is not evaluated
        // otherwise)
        Func pk("pk");
        Var jc("jc");
        {
            Expr sid = clamp(i32(tlstr(clamp(tl0(k), 0, L - 1))), 0, soff.dim(0).extent() - 2);
            Expr o = soff(sid), n = soff(sid + 1) - o;
            Expr v = cast<uint64_t>(0);
            for (int t = 0; t < 8; t++) {
                Expr at = cast<uint64_t>(jc * 8 + t);
                Expr byte = select(at < n, cast<uint64_t>(sbin(clamp(i32(o + at), 0, sbin.dim(0).extent() - 1))), cast<uint64_t>(0));
                v = v | (byte << (56 - 8 * t));
            }
            pk(jc, k) = v;
        }
        auto less = [&](Expr pa, Expr pb) {
            pa = clamp(pa, 0, ntl - 1), pb = clamp(pb, 0, ntl - 1);
            Expr lt = Internal::const_false(), eq = Internal::const_true();
            for (int c = nChunk - 1; c >= 0; c--) lt = pk(c, pa) < pk(c, pb) || (pk(c, pa) == pk(c, pb) && lt);
            for (int c = 0; c < nChunk; c++) eq = eq && pk(c, pa) == pk(c, pb);
            return Internal::Call::make(Bool(), Internal::Call::if_then_else, std::vector<Expr>{eq, tl_less(tl0(pa), tl0(pb)), lt},
                                        Internal::Call::PureIntrinsic);
        };
        Func pos0("pos0");
        pos0(k) = k;
        std::vector<Func> stages{pos0};
        for (int w = 1, lg = 0; w < (1 << 16); w *= 2, lg++) {  // runs up to 2^16 > the typelink count (checked in p3.cpp)
            Func cur = stages.back(), nxt("msort" + std::to_string(w)), bs("bsearch" + std::to_string(w));
            Expr base0 = k - k % (2 * w), d = k - base0;
            Expr lenA = clamp(ntl - base0, 0, w), lenB = clamp(ntl - base0 - w, 0, w);
            // the merge path: lo = how many of run A come before output position d
            bs(k) = Tuple(max(0, d - lenB), min(d, lenA));
            RDom it(0, lg + 1, "it");  // a range of at most w + 1 values: lg + 1 halving steps
            Expr lo = bs(k)[0], hi = bs(k)[1], mid = (lo + hi) / 2;
            Expr aBeforeB = !less(cur(clamp(base0 + w + d - mid - 1, 0, ntl - 1)), cur(clamp(base0 + mid, 0, ntl - 1)));
            Expr go = lo < hi && it.x >= 0;  // (it.x makes this an update over the RDom it)
            bs(k) = Tuple(select(go && aBeforeB, mid + 1, lo), select(go && !aBeforeB, mid, hi));
            Expr ia = bs(k)[0], ib = d - ia;
            Expr va = cur(clamp(base0 + ia, 0, ntl - 1)), vb = cur(clamp(base0 + w + ib, 0, ntl - 1));
            nxt(k) = select(ia < lenA && (ib >= lenB || !less(vb, va)), va, vb);
            bs.compute_at(nxt, k);
            nxt.compute_root().parallel(k, 1024);
            stages.push_back(nxt);
        }
        pk.compute_root().parallel(k, 1024);
        Func tl("tl");  // symbol index of the k-th typelink in sorted order
        tl(k) = tl0(clamp(stages.back()(k), 0, ntl - 1));
        tl.compute_root();
        Func tva("tva");  // the address of the k-th typelink in sorted order
        RDom rs(0, ntl, "rs");
        tva(k) = u32(0);
        Expr ts = clamp(tl(rs), 0, L - 1);
        Expr prevEnd = select(rs == 0, bstart(bTypelink), tva(max(rs - 1, 0)) + size(clamp(tl(max(rs - 1, 0)), 0, L - 1)));
        tva(rs) = rnd(prevEnd, u32(1) << u32(C(ts)));

        // 5. the address of every symbol: from its unit's W, then the typelinks over it
        addr(i) = u32(0);
        RDom ra(0, unitMax, 0, nunits, "ra");
        ra.where(ra.x < ulen(ra.y));
        addr(clamp(ustart(ra.y) + ra.x, 0, L - 1)) = W(nK + ra.x, ra.y);
        addr(clamp(tl(rs), 0, L - 1)) = tva(rs);

        // ---- schedule
        ulen.compute_root();
        ustart.compute_root();
        tpos.compute_root();
        tl0.compute_root();
        tva.compute_root();
        occ.compute_root().parallel(i, 8192, TailStrategy::GuardWithIf).vectorize(i, 16, TailStrategy::GuardWithIf);
        csize.compute_root();
        if (csched == 1) {
            csize.update().atomic().parallel(r);
        } else if (csched == 2) {
            RVar ro("ro"), ri("ri");
            Var ch("ch");
            csize.update().split(r, ro, ri, (L + chunks - 1) / chunks);
            Func part = csize.update().rfactor(ro, ch);
            part.compute_root().update().parallel(ch);
        } else if (csched == 3) {
            csize.update().parallel(u);
        }
        tot.compute_root();
        top.compute_root();
        endv.compute_root().update().parallel(b);
        base.compute_root();
        // W lives per unit: computed inside addr's loop over units (its storage is one unit's sites)
        W.compute_at(addr, ra.y).store_in(MemoryType::Stack);
        if (wsched == 1) addr.update(0).allow_race_conditions().parallel(ra.y);  // units write disjoint ranges
        addr.compute_root();
    }
};

// 3b. Copy every placed symbol's bytes into the image, then patch its relocations.
// Data-dependent indices carry unsafe_promise_clamped (bounds inference must bound every access; a real clamp
// would cost a min/max per byte and hide the dense ramp from the vectorizer).
static Expr inside(Expr e, Expr lo, Expr hi) { return unsafe_promise_clamped(e, lo, hi); }

class Relocate : public Generator<Relocate> {
public:
    // relocation loop: 0 by symbol, 1 by equal chunks of relocations (position space: each relocation finds its
    // symbol by binary search in rstart)
    GeneratorParam<int> rform{"rform", 0};
    GeneratorParam<bool> par{"par", false};     // parallel over symbols / chunks (racy by Halide's lights, disjoint in fact)
    GeneratorParam<int> vec{"vec", 0};          // vector width of the byte copy (0: scalar)
    GeneratorParam<int> chunk{"chunk", 16384};  // relocations per chunk (rform 1)

    Input<Buffer<uint8_t, 1>> bucket{"bucket"};
    Input<Buffer<uint32_t, 1>> addr{"addr"};
    Input<Buffer<uint64_t, 1>> doff{"doff"};
    Input<Buffer<uint8_t, 1>> dbin{"dbin"};
    Input<Buffer<uint32_t, 1>> rstart{"rstart"};
    Input<Buffer<uint32_t, 1>> roff{"roff"};
    Input<Buffer<uint8_t, 1>> rsiz{"rsiz"};
    Input<Buffer<uint8_t, 1>> rkind{"rkind"};
    Input<Buffer<int64_t, 1>> radd{"radd"};
    Input<Buffer<uint32_t, 1>> rtgt{"rtgt"};
    Input<Buffer<uint32_t, 1>> addrAll{"addrAll"};
    Input<Buffer<uint32_t, 1>> sect{"sect"};  // the 8 section bases, ascending
    Input<uint32_t> imageBase{"imageBase"};
    Input<uint32_t> unreach{"unreach"};
    Input<uint32_t> mapinit{"mapinit"};
    // image: updated in place (its pure step is undef: the caller hands a zeroed buffer), plus 8 scratch bytes
    Output<Buffer<uint8_t, 1>> image{"image"};
    Output<Buffer<int32_t, 0>> nerr{"nerr"};

    Var x{"x"}, q{"q"};

    void generate() {
        Expr L = bucket.dim(0).extent();
        Expr R = roff.dim(0).extent();
        Expr NI = image.dim(0).extent() - 8;  // the image, then 8 scratch bytes
        auto hasBytes = [&](Expr i) { Expr bb = bucket(i); return bb < 13 || bb > 15; };  // not bss, noptrbss, coverage

        // ---- the algorithm
        image(x) = undef<uint8_t>();
        // copy: RDom over (byte, symbol); the where clause makes the byte loop ragged
        RDom rc(0, 1 << 24, 0, L, "rc");  // the data of one symbol is < 16 MB
        Expr dlen = i32(doff(rc.y + 1) - doff(rc.y));
        rc.where(rc.x < dlen);
        rc.where(hasBytes(rc.y));
        image(inside(i32(addr(rc.y) - imageBase) + rc.x, 0, NI - 1)) = dbin(inside(i32(doff(rc.y)) + rc.x, 0, dbin.dim(0).extent() - 1));

        // the value relocation rel writes, for a symbol at address a: 8 kinds as one select chain
        struct Patch { Expr v, nbytes, write, err; };
        auto patch = [&](Expr rel, Expr a) {
            Expr tg = rtgt(rel), kind = i32(rkind(rel)), sz = i32(rsiz(rel)), add = radd(rel);
            Expr ta = addrAll(inside(i32(tg), 0, addrAll.dim(0).extent() - 1));
            Expr P = a + roff(rel);
            Expr missing = ta == 0 && tg != 0;
            Expr t2 = select(kind == 2 && missing, unreach, kind == 6 && missing, mapinit, ta);
            Expr sb = sect(0);
            for (int s = 1; s < 8; s++) sb = select(t2 >= sect(s), sect(s), sb);
            Expr pc = cast<int64_t>(P) + sz;
            Expr v = select(kind == 1 || kind == 2, cast<int64_t>(t2) + add,
                            kind == 4 && ta == 0, Expr((int64_t)0xFFFFFFFFll),
                            kind == 3 || kind == 4, cast<int64_t>(t2 - sb) + add,
                            kind >= 5 && kind <= 7, select(tg == 0, add - pc, cast<int64_t>(t2) + add - pc),
                            add);
            Expr bad = ((kind == 1 || kind == 2 || (kind >= 5 && kind <= 7)) && t2 == 0 && tg != 0) || kind < 1 || kind > 8 ||
                       !(sz == 1 || sz == 2 || sz == 4 || sz == 8);
            return Patch{v, select(kind == 4 && ta == 0, 4, sz), !bad && !(kind == 3 && ta == 0), bad};
        };

        // the symbol of relocation q: upper_bound(rstart, q) - 1, by a binary search of 20 halving steps
        Func owner("owner");
        owner(q) = Tuple(0, L);
        RDom it(0, 20, "it");
        {
            Expr lo = owner(q)[0], hi = owner(q)[1], mid = (lo + hi + 1) / 2;
            Expr le = i32(rstart(clamp(mid, 0, L))) <= q;
            Expr go = lo < hi && it.x >= 0;  // (it.x makes this an update over it)
            owner(q) = Tuple(select(go && le, mid, lo), select(go && !le, mid - 1, hi));
        }

        // relocations: an RDom over (byte of the value, relocation, ...); each byte is one store
        RDom rr;
        Expr rel, sym;
        if (rform == 0) {  // (byte, relocation within the symbol, symbol)
            rr = RDom(0, 8, 0, 1 << 24, 0, L, "rr");
            sym = rr.z;
            rel = inside(i32(rstart(sym)) + rr.y, 0, R - 1);
            // (bss symbols have no bytes: an empty range, so Halide can still make the where a loop bound)
            rr.where(rr.y < select(hasBytes(sym), i32(rstart(sym + 1) - rstart(sym)), 0));
        } else {           // (byte, relocation within the chunk, chunk)
            rr = RDom(0, 8, 0, chunk, 0, (R + chunk - 1) / chunk, "rr");
            rel = inside(rr.z * chunk + rr.y, 0, R - 1);
            rr.where(rel < R);
            sym = inside(owner(rel)[0], 0, L - 1);
        }
        Patch pt = patch(rel, addr(sym));
        if (rform == 1) rr.where(hasBytes(sym));
        // Halide turns a where into a loop bound only on the innermost RVar that has one: a where on rr.x
        // (the byte count) leaves rr.y's where an if inside a loop to 2^24. So the byte loop always runs 8 bytes,
        // and the bytes a relocation does not write go to 8 scratch bytes past the image's end.
        Expr at = select(pt.write && rr.x < pt.nbytes, i32(addr(sym) + roff(rel) - imageBase) + rr.x, NI + rr.x);
        image(inside(at, 0, NI + 7)) = cast<uint8_t>(pt.v >> cast<int64_t>(8 * rr.x));

        // errors, over the relocations of the symbols that have bytes
        Func errs("errs");  // per symbol, then summed
        Var i("i");
        RDom re(0, 1 << 24, "re");
        re.where(re.x < i32(rstart(i + 1) - rstart(i)));
        Patch pe = patch(inside(i32(rstart(i)) + re.x, 0, R - 1), addr(i));
        errs(i) = select(hasBytes(i), sum(select(pe.err, 1, 0)), 0);
        RDom rs(0, L, "rs");
        nerr() = sum(errs(rs));

        // ---- the schedule
        if (vec > 0) {
            RVar xo("xo"), xi("xi");
            // even a vector copy of contiguous bytes needs the race override: the store index is data-dependent
            image.update(0).allow_race_conditions().split(rc.x, xo, xi, vec, TailStrategy::GuardWithIf).vectorize(xi);
        }
        image.update(1).unroll(rr.x);  // the 8 byte stores of one relocation, straight-line
        if (rform == 1) {
            owner.compute_at(image, rr.z);  // one chunk's owners at a time, the 20 halving steps in cache
            owner.vectorize(q, 8, TailStrategy::GuardWithIf);
            owner.update().vectorize(q, 8, TailStrategy::GuardWithIf);
        }
        errs.compute_root();
        if (par) {
            errs.parallel(i, 1024, TailStrategy::GuardWithIf);
            // symbols write disjoint byte ranges: Halide cannot prove it (data-dependent stores), we can
            image.update(0).allow_race_conditions().parallel(rc.y);
            image.update(1).allow_race_conditions().parallel(rr.z);
        }
    }
};

HALIDE_REGISTER_GENERATOR(Layout, layout)
HALIDE_REGISTER_GENERATOR(Relocate, relocate)
