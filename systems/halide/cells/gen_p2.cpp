// Problem 2 in Halide: the flood, level by level.
//
// Halide has no worklist and no loop that runs until nothing changes: a pipeline is a fixed DAG of Funcs over
// bounded domains. So one pipeline call is ONE LEVEL of a level-synchronous flood, and the driver (p2.cpp) calls
// it until the frontier is empty (then once the method pipeline, then floods again).
//
// All the state is ONE in-place u8 Func `st` (pure definition undef: the caller's buffer persists across calls).
// One byte per fact, so that every write stores the constant 1 (or goes to a scratch byte): racing writers can
// then never lose each other's facts, and the parallel schedule needs no atomics. (Attempt 1, packed bits per
// symbol with atomic(): Halide falls back to a mutex array, 10x slower even on one thread: see NOTES.md.)
// Regions, per symbol id (padded to whole blocks of BLK ids): LIVE, IFACE, EXP (expanded once as live), DONE
// (expanded as a type converted to an interface), SCR (scratch for writes that do nothing); then SIG + s (a live
// interface call of signature s), NAMED + n, REFL, CAND + m (method m's type is live and converted to an
// interface), CALLED + m, SCRM + m (scratch for the method pipeline), SUM + p * nblk + b (block b has a symbol
// in the frontier of parity p).
//
// The frontier is implicit: a symbol needs expanding if it is a live definition not yet expanded, or a live type
// converted to an interface not yet expanded as one. A level loops over the blocks in its list (`act`, made by
// the previous call) and their symbols. An update writes ONE site per iteration, so a symbol's expansion is a
// ragged RDom (phase, op, symbol in block, block): op 0 reflect, then one op per relocation (lazy: decoded from
// the rel_* columns right here) or per edge (eager: p2_edge), two per method, and last the symbol itself; each op
// is three phases (three sites): the target's block in the next list, its IFACE byte, its LIVE byte.
#include "Halide.h"
#include "join.h"
using namespace Halide;

static Expr u32(Expr e) { return cast<uint32_t>(e); }
static Expr i32(Expr e) { return cast<int32_t>(e); }
static Expr u8(Expr e) { return cast<uint8_t>(e); }
enum { fDef = 1, fText = 2, fReflect = 4, fGoType = 8, fIface = 16, fFollow = 32 };
static const int BLK = 16;

struct Layout {
    Expr nid, nsig, nstr, nmeth, nblk, nidp, LIVE, IFACE, EXP, DONE, SCR, SIG, NAMED, REFL, CAND, CALLED, SCRM, SUM, CNT, total;
    Layout(Expr nid_, Expr nsig_, Expr nstr_, Expr nmeth_) : nid(nid_), nsig(nsig_), nstr(nstr_), nmeth(nmeth_) {
        nblk = (nid + BLK - 1) / BLK, nidp = nblk * BLK;
        LIVE = 0, IFACE = nidp, EXP = 2 * nidp, DONE = 3 * nidp, SCR = 4 * nidp;
        SIG = 5 * nidp, NAMED = SIG + nsig, REFL = NAMED + nstr + 1, CAND = REFL + 1, CALLED = CAND + nmeth;
        SCRM = CALLED + nmeth, SUM = SCRM + nmeth, CNT = SUM + 2 * nblk, total = CNT + 4;
    }
};

#define SIZES                  \
    Input<int> nid{"nid"};     \
    Input<int> nsig{"nsig"};   \
    Input<int> nstr{"nstr"};   \
    Input<int> nmeth{"nmeth"};

static Expr ite(Expr c, Expr a, Expr b) {  // if_then_else: only the taken side is evaluated
    return Internal::Call::make(a.type(), Internal::Call::if_then_else, {c, a, b}, Internal::Call::PureIntrinsic);
}
static Expr bit(Expr v, int b) { return (v & u8(b)) != 0; }

// the list of blocks whose summary byte (st(base + b)) is set: a scan over the blocks, then a scatter. Parallel:
// the scan blocked by hand (scan within chunks of 2^12 in parallel, a serial scan of the chunk totals): a scan is
// a loop-carried update, which Halide runs serially as written.
// at: where its pieces are computed (root, or inside a stage of st for a level batched into the same call; then
// list and count are internal Funcs computed there too).
static void blockList(Func st, Expr base, Expr nblk, Func list, Func count, bool par, LoopLevel at = LoopLevel::root(),
                      bool internal = false) {
    Var b("b"), j("j"), ch("ch");
    Func pos("pos");
    auto flagged = [&](Expr x) { return select(x < nblk, i32(st(base + min(x, nblk - 1)) != 0), 0); };
    if (!par) {
        RDom r(0, nblk, "rb");
        pos(b) = 0;
        pos(r) = select(r > 0, pos(max(r - 1, 0)), 0) + flagged(r);
        pos.compute_at(at);
    } else {
        const int C = 1 << 12;
        Expr nch = (nblk + C - 1) / C;
        Func inc("inc"), off("off");
        RDom ri(1, C - 1, "ri");
        inc(j, ch) = flagged(ch * C + j);
        inc(ri, ch) = inc(ri - 1, ch) + flagged(ch * C + ri);
        RDom rc(1, max(nch - 1, 0), "rc");
        off(ch) = 0;
        off(rc) = off(rc - 1) + inc(C - 1, rc - 1);
        pos(b) = off(b / C) + inc(b % C, b / C);
        inc.compute_at(at).parallel(ch);
        inc.update().parallel(ch);
        off.compute_at(at);
    }
    list(b) = 0;
    RDom rl(0, nblk, "rl");
    Expr rlc = min(rl, nblk - 1);  // (the parallel split runs past the end)
    rl.where(st(base + rlc) != 0);
    list(clamp(pos(rlc) - 1, 0, nblk - 1)) = rlc;
    count() = pos(nblk - 1);
    if (par) hj::parChunks(list.update().allow_race_conditions(), rl, 1 << 14);  // distinct positions
    if (internal) list.compute_at(at), count.compute_at(at);
}

// The initial state: IFACE from the flags, the roots LIVE and their blocks in list 0.
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
        Expr id = x - L.IFACE;
        st(x) = select(x >= L.IFACE && x < L.EXP && id <= nsl && bit(fl(clamp(id, 0, nsl)), fIface), u8(1), u8(0));
        RDom r(0, roots.dim(0).extent(), 0, 2, "r");  // (root, phase)
        Expr t = unsafe_promise_clamped(i32(roots(r.x)), 0, nid - 1);
        st(select(r.y == 0, L.LIVE + t, L.SUM + t / BLK)) = u8(1);
        st.parallel(x, 1 << 16, TailStrategy::GuardWithIf);
        blockList(st, L.SUM, L.nblk, next, nnext, false);
    }
};

// One level: expand every symbol that needs it in the blocks of list `act` (parity p); the blocks of the targets
// that will need expanding go to list 1 - p.
// levels = k > 1 (ATTACKS 3, results/halide-levelfree.md): k levels in one call. A Func cannot call a Func that
// calls it, so the list of level i (built from st after level i - 1) cannot be a Func of st. It is built from
// `stIn`, which the caller binds to the same buffer as st, and computed inside level i's own stage (at an
// outermost RVar of extent 1), so it runs after level i - 1 has finished; level i's block loop runs to its count.
class Flood : public Generator<Flood> {
public:
    GeneratorParam<bool> lazy{"lazy", true};  // decode relocations here (true) or read p2_edge (false)
    GeneratorParam<bool> par{"par", false};
    GeneratorParam<int> chunk{"chunk", 16};   // blocks per parallel task
    GeneratorParam<int> levels{"levels", 1};  // levels per call
    GeneratorParam<bool> inlineInfo{"inlineInfo", false};
    // endclear: no clear pass; a block's own summary byte is cleared once its symbols are done (a 17th "symbol"),
    // and levels 2..k visit at most `cap` blocks of their list (an RDom's bounds may be params, never data); the
    // rest keep their summary byte and are picked up two levels later (nleft counts them for the caller)
    GeneratorParam<bool> endclear{"endclear", false};
    SIZES
    Input<int> parity{"parity"};
    // The blocks to visit. (A where on an outer RVar is an if around the innermost loop, re-tested for every op,
    // and not hoisted when it reads st: looping over all blocks costs every symbol's whole row every level. So the
    // block loop runs over this list, and the symbol test is folded into the op loop's bound: the only where
    // Halide turns into a loop bound is one on the innermost RVar that has a where.)
    Input<Buffer<int32_t, 1>> act{"act"};
    Input<int> nact{"nact"};
    Input<int> cap{"cap"};  // endclear: the most blocks a level after the first visits
    Input<Buffer<uint8_t, 1>> fl{"fl"};
    // lazy: the relocation columns and the 1a answer
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
    Input<Buffer<uint8_t, 1>> stIn{"stIn"};      // the same buffer as st (read only for the lists of levels 2..k)
    Output<Buffer<uint8_t, 1>> st{"st"};
    Output<Buffer<int32_t, 1>> next{"next"};      // the blocks of the next level
    Output<Buffer<int32_t, 0>> nnext{"nnext"};
    Output<Buffer<int32_t, 0>> nleft{"nleft"};   // endclear: blocks of the last level's list not visited

    Var x{"x"};

    void generate() {
        Layout L(nid, nsig, nstr, nmeth);
        Expr nsl = fl.dim(0).extent() - 1, nb = builtin.dim(0).extent(), R = rpkg.dim(0).extent();
        auto flag = [&](Expr t) { return select(t <= nsl, fl(clamp(t, 0, nsl)), u8(0)); };
        auto has = [&](Expr region, Expr id) { return st(region + id) != 0; };
        st(x) = undef<uint8_t>();
        Var v("v");
        Func stF("stF");
        stF(v) = stIn(v);
        int nu = 0;  // updates defined so far
        for (int lev = 0; lev < levels; lev++) {
        std::string sx = std::to_string(lev);
        Expr p = (parity + lev) % 2, SUMN = L.SUM + (1 - p) * L.nblk;
        Func actF("act" + sx), cnt("cnt" + sx);
        if (lev == 0) actF(v) = act(v);
        // update 0: clear the next level's block summary
        if (!endclear) {
            RDom rz(0, L.nblk, "rz" + sx);
            st(SUMN + min(rz, L.nblk - 1)) = u8(0);  // (min: the parallel split runs past the end)
            if (par) hj::parChunks(st.update(nu).allow_race_conditions(), rz, 1 << 15);  // (the clamped tail rewrites a 0)
            nu++;
        }
        const int NZ = endclear ? BLK + 1 : BLK;
        // update 1: the expansion, RDom (phase, op, symbol in block, block in the list, [the level's list])
        LoopLevel at;  // (set below, once the RDom exists)
        if (lev > 0) blockList(stF, L.SUM + p * L.nblk, L.nblk, actF, cnt, par, at, true);
        // (a where w < count stays an if in the innermost loop: the extent must be the count itself)
        RDom rk = lev == 0 ? RDom(0, 3, 0, 1 << 26, 0, NZ, 0, nact, "rk0")
                           : RDom({{0, 3}, {0, 1 << 26}, {0, NZ}, {0, endclear ? Expr(cap) : L.nblk}, {0, 1}}, "rk" + sx);
        if (lev > 0) at.set(LoopLevel(st, rk[4])), rk.where(rk.w < cnt());
        Expr ph = rk.x, op = rk.y;
        // (promises, not clamps: a clamp makes bounds inference size `info` for every block the id could be in)
        Expr b = unsafe_promise_clamped(actF(rk.w), 0, L.nblk - 1), c = b * BLK + rk.z;
        // What a symbol's ops need from the inputs, once per symbol. Indexed by the loop's own (symbol in block,
        // block in list): indexed by the id, bounds inference would size it for every block the id could be in.
        Func info("info" + sx);
        Var zv("zv"), wv("wv");
        {
            Expr cv = unsafe_promise_clamped(actF(wv), 0, L.nblk - 1) * BLK + zv;
            Expr cc = clamp(cv, 0, nsl), f = flag(cv);
            Expr nr = lazy ? select(bit(f, fFollow) && canon(cc) == u32(cv), i32(srel(cc + 1) - srel(cc)), 0) : Expr(0);
            Expr o = unsafe_promise_clamped(i32(sobj(cc)), 0, pkgOff.dim(0).extent() - 2);
            Expr base = i32(pkgOff(o)), npk = i32(pkgOff(o + 1)) - base - 5;
            info(zv, wv) = Tuple(nr, i32(xoff(cc + 1) - xoff(cc)), i32(moff(cc + 1) - moff(cc)), i32(srel(cc)), i32(xoff(cc)),
                                 i32(moff(cc)), base, npk, i32(f));
        }
        auto I = [&](int k) { return info(rk.z, rk.w)[k]; };
        Expr nr = I(0), nx = I(1), nm = I(2), rel0 = I(3), x0 = I(4), m0 = I(5), base = I(6), npk = I(7), flc = u8(I(8));
        Expr isDef = c <= nsl && bit(flc, fDef);
        Expr uii = bit(flc, fGoType) && has(L.IFACE, c);
        Expr clearZ = rk.z == BLK;  // (endclear only)
        Expr needs = !clearZ && isDef && has(L.LIVE, c) && (!has(L.EXP, c) || (uii && !has(L.DONE, c)));
        // The methods, and a last op that sets DONE, count only for a type converted to an interface (uii).
        // IFACE can arrive on c during its own expansion (another thread, or a self edge): the loop bound was
        // fixed at entry, so the ops actually run are those of the layout without them, and every iteration maps
        // its op from the uii it reads now. DONE is then never set (its op is past the bound), and if the methods
        // are cut short, EXP is not set either: c is expanded again next level. Every op is a valid inference
        // from the current state, so this is sound. (Two earlier layouts, with DONE set at the self op from the
        // uii read there, lost 6 to 2,106 live symbols in parallel runs: DONE without the method or iface work.)
        nm = select(uii, nm, 0);
        Expr nops = 2 + nr + nx + select(uii, 2 * nm + 1, 0);
        rk.where(op < select(needs, nops, clearZ, 1, 0));

        // the op's target
        Expr selfOp = op == 1 + nr + nx + 2 * nm, doneOp = uii && op == nops - 1 && !selfOp;
        Expr reflOp = !selfOp && op == 0;
        Expr isRel = !selfOp && op >= 1 && op < 1 + nr, isX = !selfOp && op >= 1 + nr && op < 1 + nr + nx;
        Expr isM = op >= 1 + nr + nx && op < 1 + nr + nx + 2 * nm;
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
        Expr ti = unsafe_promise_clamped(i32(t), 0, nid - 1), flt = flag(ti);
        Expr plainOp = (isRel || (isX && kindX == 0)) && t != 0;
        Expr ifaceOp = ((isX && kindX == 1) || (isM && mo % 2 == 0 && uii)) && t != 0;
        Expr sigOp = isX && kindX == 2, namedOp = isX && kindX == 3, candOp = isM && mo % 2 == 1 && uii;

        // the rules (p2.c), as which bytes to set; read before this op writes, each phase re-reads (a racing
        // writer of the same fact does its own pushes, so a stale read only duplicates work)
        Expr liveT = has(L.LIVE, ti), ifaceT = has(L.IFACE, ti);
        Expr setIface = select(plainOp, uii && bit(flt, fGoType) && bit(flt, fDef) && !ifaceT, ifaceOp && !ifaceT);
        Expr setLive = plainOp && !liveT;
        Expr push = setLive || (setIface && (plainOp || liveT));  // its block goes to the next list
        Expr scr = L.SCR + c;
        Expr site0 = select(push, SUMN + ti / BLK, selfOp, L.EXP + c, doneOp, L.DONE + c, reflOp && bit(flc, fReflect), L.REFL,
                            sigOp, L.SIG + i32(sigrep(clamp(i32(t), 0, nsig - 1))), namedOp, L.NAMED + clamp(i32(t), 0, nstr),
                            candOp, L.CAND + m, scr);
        Expr site1 = select(setIface, L.IFACE + ti, scr);
        Expr site2 = select(setLive || (plainOp && setIface), L.LIVE + ti, scr);
        Expr site = unsafe_promise_clamped(select(clearZ, L.SUM + p * L.nblk + b, ph == 0, site0, ph == 1, site1, site2), 0, L.total - 1);
        st(site) = select(clearZ, u8(0), u8(1));

        if (lev == 0 || !inlineInfo) info.compute_at(st, rk.z);  // (RVar names are unique per level)
        if (par) hj::parChunks(st.update(nu).allow_race_conditions(), rk.w, chunk);  // only constant stores race
        nu++;
        }
        blockList(st, L.SUM + ((parity + levels) % 2) * L.nblk, L.nblk, next, nnext, par);  // the last level's pushes
        if (endclear) {
            RDom rs(0, L.nblk, "rs");
            nleft() = sum(i32(st(L.SUM + ((parity + levels - 1) % 2) * L.nblk + rs) != 0));
        } else {
            nleft() = 0;
        }
    }
};

// When a level finds nothing: the methods something can now call. A candidate is called if reflect is live and
// it is exported, or its name is called by name, or a live signature has its (name, type). Then its type and both
// entries become LIVE (their blocks in list `parity`).
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
        Expr SUMP = L.SUM + parity * L.nblk;
        st(x) = undef<uint8_t>();
        RDom r(0, 7, 0, nmeth, "r");  // (op, method): for each of type, ifn, tfn: its block, its LIVE; then CALLED
        Expr m = min(r.y, nmeth - 1), q = r.x / 2, ph = r.x % 2;  // (min: the parallel split runs past the end)
        Expr ms = msig(m);
        Expr call = (st(L.REFL) != 0 && mexp(m) != 0) || st(L.NAMED + clamp(i32(mname(m)), 0, nstr)) != 0 ||
                    (ms >= 0 && st(L.SIG + clamp(ms, 0, nsig - 1)) != 0);
        r.where(r.x < select(st(L.CAND + m) != 0 && st(L.CALLED + m) == 0 && call, 7, 0));
        Expr t = select(q == 0, mtyp(m), q == 1, mifn(m), mtfn(m));
        Expr ti = unsafe_promise_clamped(i32(t), 0, nid - 1);
        Expr setLive = q < 3 && t != 0 && st(L.LIVE + ti) == 0;
        Expr site = select(r.x == 6, L.CALLED + m, setLive, select(ph == 0, SUMP + ti / BLK, L.LIVE + ti), L.SCRM + m);
        st(unsafe_promise_clamped(site, 0, L.total - 1)) = u8(1);
        blockList(st, SUMP, L.nblk, next, nnext, par);
        if (par) hj::parChunks(st.update(0).allow_race_conditions(), r.y, 4096);
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
