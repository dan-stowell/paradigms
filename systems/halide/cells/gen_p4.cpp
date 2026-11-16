// Problem 4 in Halide: code generation by shape. Three pipelines:
//   Templates: the template table's own index, per template segment (where its bytes start, its first hole, its
//              first relocation hole) and the relocation holes as a list; it depends on the templates only, so a
//              code generator builds it once with its template library
//   Layout:    every item's length (a glue run, or one segment of a node's template) and relocation count, then an
//              exclusive scan of both: where it goes (at) and where its relocations go (rat)
//   Emit:      copy every item's bytes to at[i] (a ragged copy), patch every hole (a scatter; a jump's rel32 is a
//              gather of at[v]), and write every relocation into its slot (a scatter to rat[i] + rank)
// GeneratorParams pick the schedule (and, where said, the form of an update).
#include "Halide.h"
using namespace Halide;

static Expr u32(Expr e) { return cast<uint32_t>(e); }
static Expr i32(Expr e) { return cast<int32_t>(e); }
static Expr i64(Expr e) { return cast<int64_t>(e); }
static Expr u64(Expr e) { return cast<uint64_t>(e); }
// A data-dependent index: bounds inference must bound every access; this promises the range instead of paying a
// min/max per access (the CSR arrays are well formed; p4.cpp checks the sizes it relies on).
static Expr inside(Expr e, Expr lo, Expr hi) { return unsafe_promise_clamped(e, lo, hi); }
template <typename B> static Expr at_(B &b, Expr e) { return b(inside(i32(e), 0, b.dim(0).extent() - 1)); }
enum { encInt = 1, encRel32 = 2, encReloc = 3 };

// ---------------------------------------------------------------------------------------------------------
class Templates : public Generator<Templates> {
public:
    Input<Buffer<uint64_t, 1>> tmplOff{"tmplOff"};    // ntmpl + 1
    Input<Buffer<uint32_t, 1>> tsegOff{"tsegOff"};    // ntmpl + 1
    Input<Buffer<uint32_t, 1>> tsegEnd{"tsegEnd"};    // ntseg
    Input<Buffer<uint32_t, 1>> tholeOff{"tholeOff"};  // ntmpl + 1
    Input<Buffer<uint32_t, 1>> holeAt{"holeAt"};      // nthole
    Input<Buffer<uint8_t, 1>> holeEnc{"holeEnc"};
    Input<int> srcTmpl{"srcTmpl"};                    // where tmpl.bin starts in the copy's source buffer
    // ntseg + 1, per segment j: (lo, its start in the template; src, its bytes in the source buffer;
    // h0, its holes are [h0[j], h0[j+1]); r0, its relocation holes are relHole[r0[j] .. r0[j+1]))
    Output<Func> seg{"seg", {UInt(32), UInt(32), UInt(32), UInt(32)}, 1};
    Output<Buffer<uint32_t, 1>> relHole{"relHole"};  // the relocation holes, in hole order

    Var j{"j"}, h{"h"};

    void generate() {
        Expr ntmpl = tsegOff.dim(0).extent() - 1, ntseg = tsegEnd.dim(0).extent(), nthole = holeAt.dim(0).extent();
        // the template of every segment: a ragged scatter (the CSR inverted)
        Func segT("segT");
        RDom rs(0, 1 << 16, 0, ntmpl, "rs");
        rs.where(rs.x < i32(tsegOff(rs.y + 1) - tsegOff(rs.y)));
        segT(j) = 0;
        segT(inside(i32(tsegOff(rs.y)) + rs.x, 0, ntseg - 1)) = rs.y;
        Expr jc = min(j, ntseg - 1);
        Expr t = inside(segT(jc), 0, ntmpl - 1);
        Expr lo = select(u32(jc) == tsegOff(t), u32(0), at_(tsegEnd, max(jc - 1, 0)));
        // segment j's first hole: lower_bound of its start among its template's holes, 16 halving steps
        Func bs("bs");
        bs(j) = Tuple(i32(tholeOff(t)), i32(tholeOff(t + 1)));
        RDom it(0, 16, "it");
        {
            Expr a = bs(j)[0], b = bs(j)[1], mid = (a + b) / 2;
            Expr less = at_(holeAt, mid) < lo;
            Expr go = a < b && it.x >= 0;
            bs(j) = Tuple(select(go && less, mid + 1, a), select(go && !less, mid, b));
        }
        // relocation holes before h: an exclusive scan
        Func holeRel("holeRel");
        RDom rh(1, nthole, "rh");
        holeRel(h) = u32(0);
        holeRel(rh) = holeRel(rh - 1) + select(holeEnc(rh - 1) == encReloc, u32(1), u32(0));
        Expr h0 = select(j >= ntseg, u32(nthole), u32(bs(jc)[0]));
        seg(j) = Tuple(lo, u32(srcTmpl) + u32(at_(tmplOff, t)) + lo, h0, holeRel(inside(i32(h0), 0, nthole)));
        // the list: relocation hole h goes to slot holeRel(h)
        RDom rr(0, nthole, "rr");
        rr.where(holeEnc(rr) == encReloc);
        relHole(h) = u32(0);
        relHole(inside(i32(holeRel(rr)), 0, nthole - 1)) = u32(rr);

        segT.compute_root();
        holeRel.compute_root();
        // (split explicitly: parallel(j, 4096).vectorize(j, 8) vectorizes the OUTER loop, which kept the name j,
        // and the pure and update steps then disagreed on the tail: lanes read uninitialized bs, a crash 1 run in N)
        Var jo("jo"), ji("ji");
        bs.compute_root().split(j, jo, ji, 4096, TailStrategy::GuardWithIf).parallel(jo).vectorize(ji, 8, TailStrategy::GuardWithIf);
        bs.update().split(j, jo, ji, 4096, TailStrategy::GuardWithIf).parallel(jo).vectorize(ji, 8, TailStrategy::GuardWithIf);
        seg.parallel(j, 4096);
    }
};

// ---------------------------------------------------------------------------------------------------------
// An item, as the pipelines see it. A node segment item's template data comes from Templates' seg table.
struct Item {
    Expr glue, r, j, len, nrel, src, h0, nh, r0, lo, opBase, g0;
};
template <typename G> static Item item(G &g, Expr i) {
    Item it;
    it.glue = g.itemKind(i) == 0;
    it.r = i32(g.itemRef(i));  // a glue id or a node id
    // Both sides of every select are evaluated: the side an item is not on reads entry 0 (a cached line) instead
    // of chasing node -> template -> segment from a glue id (four random misses per glue item)
    Expr rg = select(it.glue, clamp(it.r, 0, g.glueOff.dim(0).extent() - 2), 0);
    Expr rn = select(it.glue, 0, clamp(it.r, 0, g.nodeTmpl.dim(0).extent() - 1));
    Expr t = inside(i32(g.nodeTmpl(rn)), 0, g.tsegOff.dim(0).extent() - 2);
    Expr s = select(it.glue, 0, i32(g.itemSeg(i)));
    it.j = inside(i32(g.tsegOff(t)) + s, 0, g.tsegEnd.dim(0).extent() - 1);
    it.lo = i32(g.segLo(it.j));
    it.len = select(it.glue, i32(g.glueOff(rg + 1) - g.glueOff(rg)), i32(g.tsegEnd(it.j)) - it.lo);
    it.h0 = i32(g.segH0(it.j));
    it.nh = select(it.glue, 0, i32(g.segH0(it.j + 1)) - it.h0);
    it.r0 = i32(g.segR0(it.j));
    it.g0 = i32(g.glueRelOff(rg));
    it.nrel = select(it.glue, i32(g.glueRelOff(rg + 1)) - it.g0, i32(g.segR0(it.j + 1)) - it.r0);
    it.src = select(it.glue, i32(g.glueOff(rg)), i32(g.segSrc(it.j)));
    it.opBase = i32(at_(g.nodeOp, rn)) - i32(g.tholeOff(t));
    return it;
}

#define ITEM_INPUTS                                      \
    Input<Buffer<uint8_t, 1>> itemKind{"itemKind"};      \
    Input<Buffer<uint32_t, 1>> itemRef{"itemRef"};       \
    Input<Buffer<uint32_t, 1>> itemSeg{"itemSeg"};       \
    Input<Buffer<uint64_t, 1>> glueOff{"glueOff"};       \
    Input<Buffer<uint32_t, 1>> glueRelOff{"glueRelOff"}; \
    Input<Buffer<uint32_t, 1>> nodeTmpl{"nodeTmpl"};     \
    Input<Buffer<uint32_t, 1>> nodeOp{"nodeOp"};         \
    Input<Buffer<uint32_t, 1>> tsegOff{"tsegOff"};       \
    Input<Buffer<uint32_t, 1>> tsegEnd{"tsegEnd"};       \
    Input<Buffer<uint32_t, 1>> tholeOff{"tholeOff"};     \
    Input<Buffer<uint32_t, 1>> segLo{"segLo"};           \
    Input<Buffer<uint32_t, 1>> segSrc{"segSrc"};         \
    Input<Buffer<uint32_t, 1>> segH0{"segH0"};           \
    Input<Buffer<uint32_t, 1>> segR0{"segR0"};

class Layout : public Generator<Layout> {
public:
    GeneratorParam<bool> par{"par", false};  // blocked parallel scan (a different algorithm: Halide has no scan)
    GeneratorParam<int> blk{"blk", 1 << 14};
    GeneratorParam<bool> lenRoot{"lenRoot", false};  // the item lengths computed first (vectorized gathers), then scanned
    ITEM_INPUTS
    Input<int> nmark{"nmark"};
    Output<Func> lay{"lay", {UInt(32), UInt(32)}, 1};  // nitem + 1: (at, rat)

    Var i{"i"}, j{"j"}, b{"b"};

    void generate() {
        Expr n = itemKind.dim(0).extent();
        Func len("len");  // (length, relocations) of item i, 0 past the end
        Item it = item(*this, min(i, n - 1));
        len(i) = Tuple(select(i < n, u32(it.len), u32(0)), select(i < n, u32(it.nrel), u32(0)));
        if (lenRoot) {
            Var io("io"), ii("ii");
            len.compute_root().split(i, io, ii, 1 << 14, TailStrategy::GuardWithIf).vectorize(ii, 16, TailStrategy::GuardWithIf);
            if (par) len.parallel(io);
        }
        if (!par) {
            // the scan, straight into the output
            RDom r(1, n, "r");
            lay(i) = Tuple(u32(0), u32(nmark));
            lay(r) = Tuple(lay(r - 1)[0] + len(r - 1)[0], lay(r - 1)[1] + len(r - 1)[1]);
        } else {
            // blocks of blk items: each block's exclusive scan (parallel over blocks), a serial scan of the block
            // totals, then the sum
            const int B = blk;
            Expr nB = n / B + 1;  // blocks over [0, n]
            Func inb("inb"), off("off");
            RDom ri(1, B - 1, "ri");
            inb(j, b) = Tuple(u32(0), u32(0));
            inb(ri, b) = Tuple(inb(ri - 1, b)[0] + len(b * B + ri - 1)[0], inb(ri - 1, b)[1] + len(b * B + ri - 1)[1]);
            RDom rb(1, nB - 1, "rb");
            off(b) = Tuple(u32(0), u32(nmark));
            Expr last = rb * B - 1;  // the last item of block rb - 1
            off(rb) = Tuple(off(rb - 1)[0] + inb(B - 1, rb - 1)[0] + len(last)[0], off(rb - 1)[1] + inb(B - 1, rb - 1)[1] + len(last)[1]);
            lay(i) = Tuple(off(i / B)[0] + inb(i % B, i / B)[0], off(i / B)[1] + inb(i % B, i / B)[1]);
            inb.compute_root().parallel(b);
            inb.update().parallel(b);
            off.compute_root();
            Var io("io"), ii("ii");
            lay.split(i, io, ii, 1 << 16, TailStrategy::GuardWithIf).parallel(io).vectorize(ii, 8, TailStrategy::GuardWithIf);
        }
    }
};

// ---------------------------------------------------------------------------------------------------------
class Emit : public Generator<Emit> {
public:
    GeneratorParam<bool> par{"par", false};    // parallel over chunks of items (racy by Halide's lights, disjoint in fact)
    GeneratorParam<int> chunk{"chunk", 4096};  // items per parallel task
    // copy: 0 = byte by byte, exact; W > 0 = W-byte vectors, read-modify-write (the bytes past the item's end are
    // written back unchanged), in two parity passes over the chunks so that no two tasks touch neighbouring bytes
    GeneratorParam<int> vec{"vec", 0};
    // patch: 0 = 8 byte stores per hole, the bytes it does not own to 8 scratch bytes (3b's form);
    //        1 = one 8-byte read-modify-write per hole (a dense vector), in two parity passes like the copy
    GeneratorParam<int> pform{"pform", 0};
    // copy form: 0 = each item copies its bytes (a ragged scatter, above); 1 = each output byte fetches its own (a
    // gather, the pure definition): per tile of output bytes, the items starting in it mark (source - destination)
    // at their first byte, a running fill carries it to every byte, and code(x) = src(x + fill(x))
    GeneratorParam<int> gform{"gform", 0};
    GeneratorParam<int> tile{"tile", 4096};
    // the per-item facts the three item loops need (source, hole range, operand base, relocation base): 0 = derived
    // inline in each loop (item -> node -> template -> segment, a dozen dependent loads, three times);
    // 1 = computed once into a table (compute_root), which the loops then read in order
    GeneratorParam<int> info{"info", 0};
    GeneratorParam<int> only{"only", 0};
    // loop fusion (compute_with) at the chunk level: 1 = each chunk's relocations right after its patches
    GeneratorParam<int> fuse{"fuse", 0};  // diagnostics (wrong output): 1 = the copy only, 2 = copy and patch, no relocations
    ITEM_INPUTS
    Input<Buffer<uint32_t, 1>> relHole{"relHole"};
    Input<Buffer<uint8_t, 1>> src{"src"};  // glue.bin, then tmpl.bin (segSrc points into it)
    Input<Buffer<uint32_t, 1>> holeAt{"holeAt"};
    Input<Buffer<uint8_t, 1>> holeW{"holeW"};
    Input<Buffer<uint8_t, 1>> holeEnc{"holeEnc"};
    Input<Buffer<uint16_t, 1>> holeRtype{"holeRtype"};
    Input<Buffer<int64_t, 1>> opVal{"opVal"};
    Input<Buffer<int64_t, 1>> opAux{"opAux"};
    Input<Buffer<uint32_t, 1>> grelOff{"grelOff"};
    Input<Buffer<uint8_t, 1>> grelSiz{"grelSiz"};
    Input<Buffer<uint16_t, 1>> grelType{"grelType"};
    Input<Buffer<uint32_t, 1>> grelSym{"grelSym"};
    Input<Buffer<int64_t, 1>> grelAdd{"grelAdd"};
    Input<Buffer<uint32_t, 1>> fnItem{"fnItem"};
    Input<Buffer<uint32_t, 1>> fnMarkOff{"fnMarkOff"};
    Input<Buffer<uint32_t, 1>> markOff{"markOff"};
    Input<Buffer<uint16_t, 1>> markType{"markType"};
    Input<Buffer<uint32_t, 1>> markSym{"markSym"};
    Input<Buffer<int64_t, 1>> markAdd{"markAdd"};
    Input<Buffer<uint32_t, 1>> at{"at"};    // nitem + 1 (Layout)
    Input<Buffer<uint32_t, 1>> rat{"rat"};  // nitem + 1
    Output<Buffer<uint8_t, 1>> code{"code"};  // code_bytes + 64 scratch bytes; updated in place (pure step undef)
    Output<Func> rel{"rel", {UInt(64), UInt(16), UInt(32), Int(64), UInt(8)}, 1};  // nrel

    Var x{"x"}, k{"k"};

    // The RDom of a per-item update: (inner..., item in chunk, chunk[, parity]); returns the item and whether it
    // is a real one (the last chunk runs past the end; its tail repeats item n-1 with no work)
    struct Over { RDom r; Expr i, real; int ic; };
    Over over(Region inner, bool parity, const std::string &name) {
        Expr n = itemKind.dim(0).extent();
        const int C = chunk;
        Expr nChunk = (n + C - 1) / C;
        Region d = inner;
        d.push_back(Range(0, C));
        if (parity) d.push_back(Range(0, (nChunk + 1) / 2)), d.push_back(Range(0, 2));
        else d.push_back(Range(0, nChunk));
        Over o;
        o.r = RDom(d, name);
        int ic = (int)inner.size();
        Expr c = parity ? Expr(o.r[ic + 1] * 2 + o.r[ic + 2]) : Expr(o.r[ic + 1]);
        Expr ii = c * C + o.r[ic];
        o.real = ii < n;
        o.i = min(ii, n - 1);
        o.ic = ic;
        return o;
    }

    void generate() {
        Expr n = itemKind.dim(0).extent();
        Expr NC = code.dim(0).extent() - 64, NR = Func(rel).output_buffers()[0].dim(0).extent();
        Expr nfn = fnItem.dim(0).extent() - 1;
        Expr nsrc = src.dim(0).extent();

        // ---- the per-item facts (info 2: one copy of the Func per loop, each computed at its loop's item level)
        Var iv("iv");
        auto mkInfo = [&](const std::string &name) {
            Func inf(name);
            Item d = item(*this, iv);
            inf(iv) = Tuple(d.src, i32(at(iv)) - d.lo, d.h0, d.nh, d.opBase, select(d.glue, d.g0, d.r0), cast<uint8_t>(d.glue));
            return inf;
        };
        Func inf = mkInfo("info"), infC = info == 2 ? mkInfo("infoC") : inf, infP = info == 2 ? mkInfo("infoP") : inf,
             infR = info == 2 ? mkInfo("infoR") : inf;
        struct I { Expr src, pb, h0, nh, ob, rel0, glue, at, len; };
        auto I_ = [&](Expr i, Func inf) {
            I r;
            r.src = inf(i)[0], r.pb = inf(i)[1], r.h0 = inf(i)[2], r.nh = inf(i)[3], r.ob = inf(i)[4], r.rel0 = inf(i)[5];
            r.glue = inf(i)[6] != 0;
            r.at = i32(at(i)), r.len = i32(at(i + 1) - at(i));
            return r;
        };

        // ---- copy
        Over oc;
        Func mark("mark"), fill("fill"), first("first");
        Var xi("xi"), tl("tl");
        if (gform == 1) {
            const int T = tile;
            Expr ntile = (NC + 64 + T - 1) / T;
            // the item holding the first byte of tile tl: the last item with at <= tl * T (zero-length items share
            // their successor's at, so this is never one of them); 23 halving steps
            Func bs("bsi");
            bs(tl) = Tuple(0, n - 1);
            RDom it(0, 23, "it");
            {
                Expr a = bs(tl)[0], b = bs(tl)[1], mid = (a + b + 1) / 2;
                Expr le = i64(at(inside(mid, 0, n))) <= i64(tl) * T;
                Expr go = a < b && it.x >= 0;
                bs(tl) = Tuple(select(go && le, mid, a), select(go && !le, mid - 1, b));
            }
            first(tl) = inside(bs(min(tl, ntile))[0], 0, n - 1);
            auto delta = [&](Expr i) { i = inside(i, 0, n - 1); return I_(i, inf).src - i32(at(i)); };
            const int NONE = -(1 << 30);
            mark(xi, tl) = NONE;
            RDom ri(1, 1 << 22, "ri");
            Expr f0 = first(tl), im = min(f0 + ri, n - 1);
            ri.where(ri <= first(tl + 1) - f0);
            Expr pos = i32(at(inside(im, 0, n - 1))) - tl * T;  // in [0, T]: an item starting at the next tile's first byte goes to T
            mark(inside(pos, 0, T), tl) = delta(im);
            fill(xi, tl) = select(mark(xi, tl) != NONE, mark(xi, tl), delta(f0));
            RDom rf(1, T - 1, "rf");
            fill(rf, tl) = select(mark(rf, tl) != NONE, mark(rf, tl), fill(rf - 1, tl));
            code(x) = src(inside(x + fill(x % T, x / T), 0, nsrc - 1));
        } else {
            code(x) = undef<uint8_t>();
        }
        if (gform == 1) {
        } else if (vec == 0) {  // (byte, item, chunk): the where on the byte is the inner loop's bound
            oc = over({Range(0, 1 << 24)}, false, "rc");  // (an item is < 16 MB: a whole function, with P4_TOPK)
            I ci = I_(oc.i, infC);
            oc.r.where(oc.r[0] < select(oc.real, ci.len, 0));
            code(inside(i32(at(oc.i)) + oc.r[0], 0, NC - 1)) = src(inside(ci.src + oc.r[0], 0, nsrc - 1));
        } else {         // (lane, word, item, chunk, parity): the where on the word is its bound
            const int W = vec;
            oc = over({Range(0, W), Range(0, (1 << 24) / W)}, true, "rc");
            I ci = I_(oc.i, infC);
            Expr len = select(oc.real, ci.len, 0);
            oc.r.where(oc.r[1] * W < len);
            Expr b = oc.r[1] * W + oc.r[0];
            Expr q = inside(i32(at(oc.i)) + b, 0, NC + 63);
            code(q) = select(b < len, src(inside(ci.src + b, 0, nsrc - 1)), code(q));
        }

        // ---- patch: (byte, hole in segment, item, chunk[, parity])
        Over op = over({Range(0, 8), Range(0, 1 << 15)}, pform == 1, "rp");
        if (only == 1) op.r.where(op.r[1] < 0);
        {
            I pi = I_(op.i, infP);
            op.r.where(op.r[1] < select(op.real, pi.nh, 0));
            Expr h = inside(pi.h0 + op.r[1], 0, holeAt.dim(0).extent() - 1);
            Expr o = inside(pi.ob + h, 0, opVal.dim(0).extent() - 1);
            Expr enc = holeEnc(h), w = i32(holeW(h)), bx = op.r[0];
            Expr P = pi.pb + i32(holeAt(h));
            Expr v = opVal(o);
            // (both sides of a select are evaluated: a hole that is not a jump reads at[0], not at[its operand],
            // which is a random cache miss for an immediate)
            Expr tgt = select(enc == encRel32, clamp(i32(v), 0, n), 0);
            Expr val = select(enc == encRel32, i64(at(tgt)) + opAux(o) - i64(P + 4), v);
            Expr own = enc != encReloc && bx < w;  // byte bx of this hole is written
            Expr byte = cast<uint8_t>(val >> i64(8 * bx));
            if (pform == 0) {
                code(inside(select(own, P + bx, NC + bx), 0, NC + 63)) = byte;
            } else {
                Expr q = inside(P + bx, 0, NC + 63);
                code(q) = select(own, byte, code(q));
            }
        }

        // ---- relocations, each into its slot
        rel(k) = Tuple(undef<uint64_t>(), undef<uint16_t>(), undef<uint32_t>(), undef<int64_t>(), undef<uint8_t>());
        // markers: (marker in function, function in chunk, chunk), at the body's start
        const int FC = 1024;
        RDom rm(0, 1 << 16, 0, FC, 0, (nfn + FC - 1) / FC, "rm");
        Expr f = min(rm.z * FC + rm.y, nfn - 1);
        rm.where(rm.x < select(rm.z * FC + rm.y < nfn, i32(fnMarkOff(f + 1) - fnMarkOff(f)), 0));
        if (only) rm.where(rm.x < 0);
        Expr m = inside(i32(fnMarkOff(f)) + rm.x, 0, markOff.dim(0).extent() - 1);
        rel(inside(m, 0, NR - 1)) = Tuple(u64(at_(at, fnItem(f)) + markOff(m)), markType(m), markSym(m), markAdd(m), cast<uint8_t>(0));
        // the item's relocations: (relocation of the item, item, chunk). A glue run's are its own list; a node
        // segment's are its template's relocation holes in the segment (relHole), their symbols its operands
        Over orl = over({Range(0, 1 << 16)}, false, "rl");
        if (only) orl.r.where(orl.r[0] < 0);
        {
            I li = I_(orl.i, infR);
            Expr kx = orl.r[0];
            orl.r.where(kx < select(orl.real, i32(rat(orl.i + 1) - rat(orl.i)), 0));
            // (both branches of the selects below are evaluated: the side the item is not on reads entry 0)
            Expr g = select(li.glue, inside(li.rel0 + kx, 0, grelOff.dim(0).extent() - 1), 0);
            Expr hh = select(li.glue, 0, inside(i32(at_(relHole, li.rel0 + kx)), 0, holeAt.dim(0).extent() - 1));
            Expr oh = select(li.glue, 0, inside(li.ob + hh, 0, opVal.dim(0).extent() - 1));
            Expr Pn = li.pb + i32(holeAt(hh)), Pg = li.at + i32(grelOff(g));
            Expr slot = inside(i32(rat(orl.i)) + kx, 0, NR - 1);
            rel(slot) = Tuple(u64(select(li.glue, Pg, Pn)), select(li.glue, grelType(g), holeRtype(hh)),
                              select(li.glue, grelSym(g), u32(opVal(oh))), select(li.glue, grelAdd(g), opAux(oh)),
                              select(li.glue, grelSiz(g), holeW(hh)));
        }

        // ---- the schedule
        int pc = oc.ic + 1, pp = op.ic + 1;  // the chunk RVar of each update
        int pu = gform == 1 ? 0 : 1;          // the patch's update
        if (info == 2) {
            if (gform == 0) infC.compute_at(code, oc.r[oc.ic]);
            infP.compute_at(code, op.r[op.ic]);  // (RVar names are unique: no stage index needed)
            infR.compute_at(rel, orl.r[orl.ic]);
        } else if (info == 1) {
            // vectorized: 16 items' dependent loads (node -> template -> segment) issued side by side as gathers
            Var io("io"), ii("ii");
            inf.compute_root().split(iv, io, ii, 1 << 14, TailStrategy::GuardWithIf).vectorize(ii, 16, TailStrategy::GuardWithIf);
            if (par) inf.parallel(io);
        }
        if (gform == 1) {
            Var xt("xt"), xv("xv");
            code.split(x, xt, xv, tile).vectorize(xv, 32);
            if (par) code.parallel(xt);
            mark.compute_at(code, xt).vectorize(xi, 32);
            fill.compute_at(code, xt).vectorize(xi, 32);
            first.compute_root();
        }
        if (vec > 0 && gform == 0) code.update(0).allow_race_conditions().vectorize(oc.r[0]);
        if (pform == 0) code.update(pu).unroll(op.r[0]);
        else code.update(pu).allow_race_conditions().vectorize(op.r[0]);
        if (par) {
            // (with parity, the chunk RVar is parallel inside the serial parity loop)
            if (gform == 0) code.update(0).allow_race_conditions().parallel(oc.r[pc]);
            code.update(pu).allow_race_conditions().parallel(op.r[pp]);
            rel.update(0).allow_race_conditions().parallel(rm.z);
            rel.update(1).allow_race_conditions().parallel(orl.r[orl.ic + 1]);
        }
        // (compute_with matches the fused loops by name: the chunk RVars are renamed to the patch's)
        if (fuse >= 1) rel.update(1).rename(orl.r[orl.ic + 1], RVar(op.r[pp].name())).compute_with(code.update(pu), op.r[pp]);
        // (fusing the copy and the patch, two updates of one Func, is refused: "Cannot schedule code.update(1) to be
        // computed with code.s1")
    }
};

HALIDE_REGISTER_GENERATOR(Templates, templates)
HALIDE_REGISTER_GENERATOR(Layout, layout)
HALIDE_REGISTER_GENERATOR(Emit, emit)
