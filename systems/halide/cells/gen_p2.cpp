// Problem 2 in Halide: the flood, level by level.
//
// Halide has no worklist and no loop that runs until nothing changes: a pipeline is a fixed DAG of Funcs over
// bounded domains. So one pipeline call is ONE LEVEL of a level-synchronous flood, and the driver (p2.cpp) calls
// it until the frontier is empty (then once the method pipeline, then floods again). All the state is ONE
// in-place u8 Func `st` (its pure definition is undef: the caller's buffer persists across calls), with sites:
//   [0, nblk * BLK)     per symbol id (padded to whole blocks): LIVE 1, IFACE 2, DONE 4 (expanded as a type converted to an interface),
//                       FR0 8, FR1 16 (in the frontier of an even / odd level)
//   SIG + s             signature s seen (a live interface method call)
//   NAMED + n           method name n called by reflection-like code
//   REFL                reflect's Method is live
//   CAND + m            method m: 1 candidate (its type is live and converted to an interface), 2 called
//   SUM + p * nblk + b  block b (BLK ids) has a symbol in frontier p: the level loop skips empty blocks
// An update definition writes ONE site per iteration, so a symbol's expansion is a ragged RDom over
// (op, symbol in block, block), every op one site: op 0 notes reflect, then
// then one op per relocation (lazy: decoded from the rel_* columns right here) or per edge (eager: p2_edge),
// two per method, and last the symbol itself. Each op runs as two iterations (phase): phase 0 sets the target's bits, phase 1 marks the target's
// block in the next frontier's summary if the target is in it.
// Parallel: iterations race on shared sites, so the update is atomic() (a compare-and-swap per site write).
#include "Halide.h"
#include "join.h"
using namespace Halide;

static Expr u32(Expr e) { return cast<uint32_t>(e); }
static Expr i32(Expr e) { return cast<int32_t>(e); }
static Expr u8(Expr e) { return cast<uint8_t>(e); }
enum { LIVE = 1, IFACE = 2, DONE = 4, FR0 = 8, FR1 = 16 };
enum { fDef = 1, fText = 2, fReflect = 4, fGoType = 8, fIface = 16, fFollow = 32 };
static const int BLK = 16;

// the site layout, from the sizes
struct Layout {
    Expr nid, nsig, nstr, nmeth, nblk, SIG, NAMED, REFL, CAND, SUM, total;
    Layout(Expr nid_, Expr nsig_, Expr nstr_, Expr nmeth_) : nid(nid_), nsig(nsig_), nstr(nstr_), nmeth(nmeth_) {
        nblk = (nid + BLK - 1) / BLK;
        SIG = nblk * BLK, NAMED = SIG + nsig, REFL = NAMED + nstr + 1, CAND = REFL + 1, SUM = CAND + nmeth;
        total = SUM + 2 * nblk;
    }
};

// the sizes every pipeline takes
#define SIZES                                  \
    Input<int> nid{"nid"};                     \
    Input<int> nsig{"nsig"};                   \
    Input<int> nstr{"nstr"};                   \
    Input<int> nmeth{"nmeth"};

static Expr ite(Expr c, Expr a, Expr b) {  // if_then_else: only the taken side is evaluated
    return Internal::Call::make(a.type(), Internal::Call::if_then_else, {c, a, b}, Internal::Call::PureIntrinsic);
}
static Expr bit(Expr v, int b) { return (v & u8(b)) != 0; }
// the frontier bit of parity p (0 or 1)
static Expr frBit(Expr p) { return u8(select(p == 0, FR0, FR1)); }

// the list of blocks whose summary byte (st(base + b)) is set: a serial scan over the few thousand blocks,
// then a scatter
static void blockList(Func st, Expr base, Expr nblk, Func list, Func count) {
    Var b("b");
    RDom r(0, nblk, "rb");
    Func pos("pos");
    pos(b) = 0;
    pos(r) = select(r > 0, pos(max(r - 1, 0)), 0) + i32(st(base + r) != 0);
    pos.compute_root();
    list(b) = 0;
    RDom rl(0, nblk, "rl");
    rl.where(st(base + rl) != 0);
    list(clamp(pos(rl) - 1, 0, nblk - 1)) = rl;
    count() = pos(nblk - 1);
}

static void scheduleUpdate(Stage s, RVar outer, bool par, int chunk) {
    if (!par) return;
    s.atomic(true);  // (true: no associativity proof: the update reads other sites; Halide uses a CAS loop)
    hj::parChunks(s, outer, chunk);
}

// The initial state: IFACE from the flags, the roots LIVE and in frontier 0.
class Init : public Generator<Init> {
public:
    SIZES
    Input<Buffer<uint8_t, 1>> fl{"fl"};
    Input<Buffer<uint32_t, 1>> roots{"roots"};
    Output<Buffer<uint8_t, 1>> st{"st"};
    Output<Buffer<int32_t, 1>> next{"next"};
    Output<Buffer<int32_t, 0>> nnext{"nnext"};
    Var x{"x"};
    void generate() {
        Layout L(nid, nsig, nstr, nmeth);
        Expr nsl = fl.dim(0).extent() - 1;
        st(x) = select(x <= nsl && bit(fl(min(x, nsl)), fIface), u8(IFACE), u8(0));
        RDom r(0, roots.dim(0).extent(), 0, 2, "r");  // (root, phase): the root's bits, then its block
        Expr t = unsafe_promise_clamped(i32(roots(r.x)), 0, nid - 1);
        Expr site = select(r.y == 0, t, L.SUM + t / BLK);
        st(site) = select(r.y == 0, st(site) | u8(LIVE | FR0), u8(1));
        st.parallel(x, 1 << 16, TailStrategy::GuardWithIf);
        blockList(st, L.SUM, L.nblk, next, nnext);
    }
};

// One level: expand every symbol in frontier `parity`.
class Flood : public Generator<Flood> {
public:
    GeneratorParam<bool> lazy{"lazy", true};  // decode relocations here (true) or read p2_edge (false)
    GeneratorParam<bool> par{"par", false};
    GeneratorParam<int> chunk{"chunk", 4};    // blocks per parallel task
    SIZES
    Input<int> parity{"parity"};
    // the blocks with a symbol in frontier `parity` (made by the previous call). A where on an outer RVar is an
    // if around the innermost loop, re-tested for every op, and Halide does not hoist it (it reads st, which the
    // loop writes): looping over all blocks costs every symbol's whole row every level. So the block loop runs
    // over this list, and the symbol test is folded into the op loop's bound (the only where Halide turns into
    // a bound is one on the innermost RVar).
    Input<Buffer<int32_t, 1>> act{"act"};
    Input<int> nact{"nact"};
    Input<Buffer<uint8_t, 1>> fl{"fl"};
    // lazy: the relocation columns and the 1a answer; the edges that are not relocations (xoff/xdst/xkind)
    Input<Buffer<uint32_t, 1>> canon{"canon"};
    Input<Buffer<uint32_t, 1>> builtin{"builtin"};
    Input<Buffer<uint32_t, 1>> srel{"srel"};
    Input<Buffer<uint32_t, 1>> sobj{"sobj"};
    Input<Buffer<uint32_t, 1>> pkgOff{"pkgOff"};
    Input<Buffer<uint32_t, 1>> pkg{"pkg"};
    Input<Buffer<uint32_t, 1>> rpkg{"rpkg"};
    Input<Buffer<uint32_t, 1>> ridx{"ridx"};
    Input<Buffer<uint16_t, 1>> rtype{"rtype"};
    // eager: p2_edge; lazy: the edges that are not relocations, in the same shape
    Input<Buffer<uint32_t, 1>> xoff{"xoff"};
    Input<Buffer<uint32_t, 1>> xdst{"xdst"};
    Input<Buffer<uint8_t, 1>> xkind{"xkind"};
    Input<Buffer<uint32_t, 1>> moff{"moff"};
    Input<Buffer<uint32_t, 1>> mtyp{"mtyp"};
    Input<Buffer<uint32_t, 1>> sigrep{"sigrep"};  // a signature's representative (equal (name, type) pairs)
    Output<Buffer<uint8_t, 1>> st{"st"};
    Output<Buffer<int32_t, 1>> next{"next"};      // the blocks in the next frontier (act of the next call)
    Output<Buffer<int32_t, 0>> nnext{"nnext"};    // how many

    Var x{"x"};

    void generate() {
        Layout L(nid, nsig, nstr, nmeth);
        Expr nsl = fl.dim(0).extent() - 1, nb = builtin.dim(0).extent(), R = rpkg.dim(0).extent();
        Expr p = parity, frC = frBit(p), frN = frBit(1 - p);
        auto flag = [&](Expr t) { return select(t <= nsl, fl(clamp(t, 0, nsl)), u8(0)); };

        st(x) = undef<uint8_t>();
        // update 0: clear the next frontier's block summary
        RDom rz(0, L.nblk, "rz");
        st(L.SUM + (1 - p) * L.nblk + rz) = u8(0);

        // update 1: the expansion. RDom (phase, op, symbol in block, block in the list).
        RDom rk(0, 2, 0, 1 << 26, 0, BLK, 0, nact, "rk");
        Expr ph = rk.x, op = rk.y;
        // (promises, not clamps: a clamp makes bounds inference size info for every possible block)
        Expr b = unsafe_promise_clamped(act(rk.w), 0, L.nblk - 1), c = b * BLK + rk.z;
        Expr cs = clamp(c, 0, nsl);
        // what the symbol's ops need from the inputs, once per symbol (computed at the symbol loop: inline, every
        // op would reload it)
        // (indexed by the loop's own (symbol in block, block in list): indexed by the id, bounds inference would size
        // it for every block the data-dependent id could be in)
        Func info("info");
        Var zv("zv"), wv("wv");
        {
            Expr cv = unsafe_promise_clamped(act(wv), 0, L.nblk - 1) * BLK + zv;
            Expr cc = clamp(cv, 0, nsl), f = flag(cv);
            Expr nr = lazy ? select(bit(f, fFollow) && canon(cc) == u32(cv), i32(srel(cc + 1) - srel(cc)), 0) : Expr(0);
            Expr o = unsafe_promise_clamped(i32(sobj(cc)), 0, pkgOff.dim(0).extent() - 2);
            Expr base = i32(pkgOff(o)), npk = i32(pkgOff(o + 1)) - base - 5;
            info(zv, wv) = Tuple(nr, i32(xoff(cc + 1) - xoff(cc)), i32(moff(cc + 1) - moff(cc)), i32(srel(cc)), i32(xoff(cc)),
                             i32(moff(cc)), base, npk, i32(f));
        }
        Expr nr = info(rk.z, rk.w)[0], nx = info(rk.z, rk.w)[1], nm = info(rk.z, rk.w)[2], rel0 = info(rk.z, rk.w)[3];
        Expr x0 = info(rk.z, rk.w)[4], m0 = info(rk.z, rk.w)[5], base = info(rk.z, rk.w)[6], npk = info(rk.z, rk.w)[7];
        Expr flc = u8(info(rk.z, rk.w)[8]);
        Expr stc = st(c);
        Expr uii = bit(flc, fGoType) && bit(stc, IFACE);
        Expr proceed = c <= nsl && bit(flc, fDef) && !(uii && bit(stc, DONE));
        Expr nops = 2 + nr + nx + 2 * nm;
        rk.where(op < select((stc & frC) != 0, nops, 0));  // the only where: Halide makes it the op loop's bound

        // ops: 0 reflect, then the relocations, the other edges, two per method, and last the symbol itself (clear
        // its frontier bit, set DONE). atomic() refuses an update whose written SITE depends on the Func's own
        // values, so the op layout and every site come from the inputs only; the state (uii, proceed, the
        // target's bits) only decides the VALUE written, and an op that should do nothing rewrites its site.
        Expr selfOp = op == nops - 1, reflOp = !selfOp && op == 0;
        Expr isRel = !selfOp && op >= 1 && op < 1 + nr, isX = !selfOp && op >= 1 + nr && op < 1 + nr + nx;
        Expr isM = !selfOp && op >= 1 + nr + nx;
        Expr tRel = u32(0);
        if (lazy) {
            Expr r = unsafe_promise_clamped(rel0 + op - 1, 0, R - 1);
            Expr ty = rtype(r);
            // a relocation the flood follows: not weak, and not one of the types the method rules handle (26 is
            // R_METHODOFF: on MinIO all three of a method's relocations are type 26, so hlink's "skip the next
            // two" never skips anything else, checked in p2.cpp)
            Expr follow = (ty & 32768) == 0 && ty != 22 && ty != 23 && ty != 24 && ty != 25 && ty != 26 && ty != 102;
            Expr pk = rpkg(r), idx = ridx(r), sp = pk - Expr((uint32_t)2147483643u);
            Expr tt = select(sp < 5, u32(npk) + sp, pk);
            Expr pb = pkg(unsafe_promise_clamped(base + i32(tt), 0, pkg.dim(0).extent() - 1));
            Expr valid = follow && tt < u32(npk + 5) && pb != Expr((uint32_t)0xFFFFFFFFu);
            Expr s = select(sp == 1, select(idx < u32(nb), builtin(clamp(i32(idx), 0, nb - 1)), u32(0)), pb + idx);
            tRel = select(valid && s != 0, canon(unsafe_promise_clamped(i32(s), 0, nsl)), u32(0));
        }
        Expr e = unsafe_promise_clamped(x0 + op - 1 - nr, 0, xdst.dim(0).extent() - 1);
        Expr mo = op - 1 - nr - nx, m = unsafe_promise_clamped(m0 + mo / 2, 0, nmeth - 1);
        Expr kindX = i32(xkind(e));
        // (if_then_else, not select: select evaluates both sides, so every op would decode a relocation)
        Expr t = ite(isRel, tRel, ite(isX, xdst(e), ite(isM && mo % 2 == 0, mtyp(m), u32(0))));
        Expr tk = select(isRel, 0, isX, kindX, 1);  // a method's type: as useiface
        Expr ti = unsafe_promise_clamped(i32(t), 0, nid - 1);
        Expr stt = st(ti), flt = flag(ti);
        Expr plainBits = select(uii && bit(flt, fGoType) && bit(flt, fDef) && !bit(stt, IFACE), u8(IFACE | LIVE) | frN,
                                !bit(stt, LIVE), u8(LIVE) | frN, u8(0));
        Expr ifaceBits = select(!bit(stt, IFACE), u8(IFACE) | select(bit(stt, LIVE), frN, u8(0)), u8(0));
        Expr nodeOp = (isRel || isX || isM) && t != 0 && (tk == 0 || tk == 1) && !(isM && mo % 2 == 1);
        Expr act_ = proceed && (!isM || uii);  // does this op change anything
        Expr sigOp = isX && kindX == 2, namedOp = isX && kindX == 3, candOp = isM && mo % 2 == 1;
        Expr sigSite = L.SIG + i32(sigrep(clamp(i32(t), 0, nsig - 1))), namedSite = L.NAMED + clamp(i32(t), 0, nstr);
        // phase 0: (site, its new value)
        Expr site0 = select(selfOp, c, reflOp, L.REFL, nodeOp, ti, sigOp, sigSite, namedOp, namedSite,
                            candOp, L.CAND + m, c);
        // phase 1: a node op marks the target's block if the target is in the next frontier; else rewrite c
        Expr mark = act_ && nodeOp && (stt & frN) != 0;
        Expr site1 = select(nodeOp, L.SUM + (1 - p) * L.nblk + ti / BLK, c);
        Expr site = unsafe_promise_clamped(select(ph == 0, site0, site1), 0, L.total - 1);
        Expr old = st(site);
        Expr new0 = select(selfOp, (old & ~frC) | select(uii, u8(DONE), u8(0)),
                           !act_, old,
                           reflOp, old | select(bit(flc, fReflect), u8(1), u8(0)),
                           nodeOp, old | select(tk == 0, plainBits, ifaceBits),
                           sigOp || namedOp || candOp, old | u8(1),
                           old);
        Expr new1 = select(mark, u8(1), old);
        st(site) = select(ph == 0, new0, new1);

        info.compute_at(st, rk.z);
        st.update(1).unroll(rk.x);  // the two phases, straight-line: LLVM shares the decode between them
        scheduleUpdate(st.update(1), rk.w, par, chunk);
        blockList(st, L.SUM + (1 - p) * L.nblk, L.nblk, next, nnext);

    }
};

// When the frontier is empty: the methods something can now call. A candidate is called if reflect is live and
// it is exported, or its name is called by name, or a live signature has its (name, type). Then its type and
// both entries become LIVE in frontier `parity`.
class Methods : public Generator<Methods> {
public:
    GeneratorParam<bool> par{"par", false};
    SIZES
    Input<int> parity{"parity"};
    Input<Buffer<uint32_t, 1>> mname{"mname"};
    Input<Buffer<uint32_t, 1>> mtyp{"mtyp"};
    Input<Buffer<uint32_t, 1>> mifn{"mifn"};
    Input<Buffer<uint32_t, 1>> mtfn{"mtfn"};
    Input<Buffer<uint8_t, 1>> mexp{"mexp"};
    Input<Buffer<int32_t, 1>> msig{"msig"};  // the representative signature with the method's (name, type), or -1
    Output<Buffer<uint8_t, 1>> st{"st"};
    Output<Buffer<int32_t, 1>> next{"next"};
    Output<Buffer<int32_t, 0>> nnext{"nnext"};
    Var x{"x"};
    void generate() {
        Layout L(nid, nsig, nstr, nmeth);
        Expr frP = frBit(parity);
        st(x) = undef<uint8_t>();
        RDom r(0, 8, 0, nmeth, "r");  // (op * 2 + phase, method): ops 0-2 its type and entries, op 3 "called"
        Expr m = min(r.y, nmeth - 1), op = r.x / 2, ph = r.x % 2;  // (min: the parallel split runs past the end)
        Expr cm = st(L.CAND + m);
        Expr ms = msig(m);
        Expr call = (st(L.REFL) != 0 && mexp(m) != 0) || st(L.NAMED + clamp(i32(mname(m)), 0, nstr)) != 0 ||
                    (ms >= 0 && st(L.SIG + clamp(ms, 0, nsig - 1)) != 0);
        r.where(cm == 1 && call);
        Expr t = select(op == 0, mtyp(m), op == 1, mifn(m), mtfn(m));
        Expr ti = unsafe_promise_clamped(i32(t), 0, nid - 1);
        Expr node = op < 3 && t != 0;
        Expr stt = st(ti);
        Expr mark = node && (stt & frP) != 0;
        Expr site = unsafe_promise_clamped(select(ph == 0, select(node, ti, L.CAND + m),
                                                  select(node, L.SUM + parity * L.nblk + ti / BLK, L.CAND + m)),
                                           0, L.total - 1);
        Expr old = st(site);
        Expr v0 = select(node, old | select(!bit(old, LIVE), u8(LIVE) | frP, u8(0)), op == 3, old | u8(2), old);
        st(site) = select(ph == 0, v0, select(mark, u8(1), old));  // (sites from the inputs only: see Flood)
        blockList(st, L.SUM + parity * L.nblk, L.nblk, next, nnext);
        scheduleUpdate(st.update(0), r.y, par, 4096);
    }
};

// Once per flood: signatures with equal (name, type) share a representative, and each method finds the
// representative of its (name, type), by join.h's hash join.
class Prep : public Generator<Prep> {
public:
    GeneratorParam<bool> par{"par", false};
    Input<Buffer<uint32_t, 1>> sname{"sname"};
    Input<Buffer<uint32_t, 1>> stype{"stype"};
    Input<Buffer<uint32_t, 1>> mname{"mname"};
    Input<Buffer<uint32_t, 1>> mtyp{"mtyp"};
    Output<Buffer<uint32_t, 1>> sigrep{"sigrep"};
    Output<Buffer<int32_t, 1>> msig{"msig"};
    Var j{"j"};
    void generate() {
        Expr ns = sname.dim(0).extent(), nm = mname.dim(0).extent(), M = ns + nm;
        Func k1("k1"), k2("k2"), creates("creates");
        k1(j) = hj::u64e(select(j < ns, sname(clamp(j, 0, ns - 1)), mname(clamp(j - ns, 0, nm - 1))));
        k2(j) = hj::u64e(select(j < ns, stype(clamp(j, 0, ns - 1)), mtyp(clamp(j - ns, 0, nm - 1))));
        creates(j) = u8(j < ns);
        k1.compute_root(), k2.compute_root(), creates.compute_root();
        hj::Join J = hj::hashJoin("sig", k1, k2, creates, M, 14, 6, par);
        sigrep(j) = u32(J.rep(j));
        msig(j) = J.rep(ns + j);
    }
};

HALIDE_REGISTER_GENERATOR(Init, init)
HALIDE_REGISTER_GENERATOR(Flood, flood)
HALIDE_REGISTER_GENERATOR(Methods, methods)
HALIDE_REGISTER_GENERATOR(Prep, prep)
