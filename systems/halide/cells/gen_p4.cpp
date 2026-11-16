// Problem 4 in Halide: code generation by shape. Three pipelines:
//   SegHoles: the template table's own index (which holes fall in which template segment; independent of the
//             program's nodes, so a code generator would build it once with its templates)
//   Layout:   every item's length (a glue run, or one segment of a node's template) and relocation count, then an
//             exclusive scan of both: where it goes (at) and where its relocations go (rat)
//   Emit:     copy every item's bytes to at[i] (a ragged gather-copy), patch every hole (a scatter; a jump's rel32
//             is a gather of at[v]), and write every relocation to its slot (a scatter to rat[i] + rank)
// GeneratorParams pick the schedule.
#include "Halide.h"
using namespace Halide;

static Expr u32(Expr e) { return cast<uint32_t>(e); }
static Expr i32(Expr e) { return cast<int32_t>(e); }
static Expr i64(Expr e) { return cast<int64_t>(e); }
static Expr u64(Expr e) { return cast<uint64_t>(e); }
// A data-dependent index: bounds inference must bound every access; this promises the range instead of paying a
// min/max per access (the inputs' CSR arrays are well formed; p4.cpp checks the sizes it relies on).
static Expr inside(Expr e, Expr lo, Expr hi) { return unsafe_promise_clamped(e, lo, hi); }
template <typename B> static Expr at_(B &b, Expr e) { return b(inside(i32(e), 0, b.dim(0).extent() - 1)); }
enum { encInt = 1, encRel32 = 2, encReloc = 3 };

// ---------------------------------------------------------------------------------------------------------
class SegHoles : public Generator<SegHoles> {
public:
    Input<Buffer<uint32_t, 1>> tsegOff{"tsegOff"};    // ntmpl + 1
    Input<Buffer<uint32_t, 1>> tsegEnd{"tsegEnd"};    // ntseg
    Input<Buffer<uint32_t, 1>> tholeOff{"tholeOff"};  // ntmpl + 1
    Input<Buffer<uint32_t, 1>> holeAt{"holeAt"};      // nthole
    Input<Buffer<uint8_t, 1>> holeEnc{"holeEnc"};
    Output<Buffer<uint32_t, 1>> segHole0{"segHole0"};  // ntseg + 1: the holes of segment j are [segHole0[j], segHole0[j+1])
    Output<Buffer<uint32_t, 1>> holeRel{"holeRel"};    // nthole + 1: relocation holes before hole h (exclusive scan)

    Var j{"j"}, h{"h"};

    void generate() {
        Expr ntmpl = tsegOff.dim(0).extent() - 1, ntseg = tsegEnd.dim(0).extent(), nthole = holeAt.dim(0).extent();
        // the template of every segment: a ragged scatter (the CSR inverted)
        Func segT("segT");
        RDom rs(0, 1 << 16, 0, ntmpl, "rs");
        rs.where(rs.x < i32(tsegOff(rs.y + 1) - tsegOff(rs.y)));
        segT(j) = 0;
        segT(inside(i32(tsegOff(rs.y)) + rs.x, 0, ntseg - 1)) = rs.y;
        // segment j's first hole: lower_bound of its start offset among its template's holes, 16 halving steps
        Expr t = inside(segT(j), 0, ntmpl - 1);
        Expr lo = select(u32(j) == tsegOff(t), u32(0), at_(tsegEnd, max(j - 1, 0)));
        Func bs("bs");
        bs(j) = Tuple(i32(tholeOff(t)), i32(tholeOff(t + 1)));
        RDom it(0, 16, "it");
        {
            Expr a = bs(j)[0], b = bs(j)[1], mid = (a + b) / 2;
            Expr less = at_(holeAt, mid) < lo;
            Expr go = a < b && it.x >= 0;
            bs(j) = Tuple(select(go && less, mid + 1, a), select(go && !less, mid, b));
        }
        segHole0(j) = select(j >= ntseg, u32(nthole), u32(bs(min(j, ntseg - 1))[0]));
        // relocation holes before h: a scan
        RDom rh(1, nthole, "rh");
        holeRel(h) = u32(0);
        holeRel(rh) = holeRel(rh - 1) + select(holeEnc(rh - 1) == encReloc, u32(1), u32(0));

        segT.compute_root();
        bs.compute_root().parallel(j, 4096).vectorize(j, 8);
        bs.update().parallel(j, 4096).vectorize(j, 8);
        segHole0.parallel(j, 4096);
    }
};

// ---------------------------------------------------------------------------------------------------------
// Per item: (glue or node segment) -> its length, its source offset, its relocation count. Shared by Layout and
// Emit (Emit recomputes them inline: a few dependent loads per item, not per byte).
struct Item {
    Expr kind, r, t, seg, lo, len, src, h0, nh, opBase, nrel;
};
template <typename G> static Item item(G &g, Expr i) {
    Item it;
    it.kind = g.itemKind(i);
    it.r = i32(g.itemRef(i));  // a glue id or a node id (kind)
    Expr glue = it.kind == 0;
    Expr rg = clamp(it.r, 0, g.glueOff.dim(0).extent() - 2), rn = clamp(it.r, 0, g.nodeTmpl.dim(0).extent() - 1);
    it.t = inside(i32(g.nodeTmpl(rn)), 0, g.tsegOff.dim(0).extent() - 2);
    Expr s = select(glue, 0, i32(g.itemSeg(i)));
    it.seg = inside(i32(g.tsegOff(it.t)) + s, 0, g.tsegEnd.dim(0).extent() - 1);
    it.lo = select(s > 0, i32(at_(g.tsegEnd, max(it.seg - 1, 0))), 0);
    Expr nlen = i32(at_(g.tsegEnd, it.seg)) - it.lo;
    Expr glen = i32(g.glueOff(rg + 1) - g.glueOff(rg));
    it.len = select(glue, glen, nlen);
    it.h0 = i32(at_(g.segHole0, it.seg));
    it.nh = select(glue, 0, i32(at_(g.segHole0, it.seg + 1)) - it.h0);
    it.opBase = i32(at_(g.nodeOp, rn)) - i32(g.tholeOff(it.t));
    Expr nrelNode = i32(at_(g.holeRel, it.h0 + it.nh)) - i32(at_(g.holeRel, it.h0));
    it.nrel = select(glue, i32(g.glueRelOff(rg + 1) - g.glueRelOff(rg)), nrelNode);
    // one source buffer: glue.bin, then tmpl.bin at srcTmpl (p4.cpp maps them back to back)
    it.src = select(glue, i32(g.glueOff(rg)), g.srcTmpl + i32(at_(g.tmplOff, it.t)) + it.lo);
    return it;
}

#define ITEM_INPUTS                                                     \
    Input<Buffer<uint8_t, 1>> itemKind{"itemKind"};                     \
    Input<Buffer<uint32_t, 1>> itemRef{"itemRef"};                      \
    Input<Buffer<uint32_t, 1>> itemSeg{"itemSeg"};                      \
    Input<Buffer<uint64_t, 1>> glueOff{"glueOff"};                      \
    Input<Buffer<uint32_t, 1>> glueRelOff{"glueRelOff"};                \
    Input<Buffer<uint32_t, 1>> nodeTmpl{"nodeTmpl"};                    \
    Input<Buffer<uint32_t, 1>> nodeOp{"nodeOp"};                        \
    Input<Buffer<uint64_t, 1>> tmplOff{"tmplOff"};                      \
    Input<Buffer<uint32_t, 1>> tsegOff{"tsegOff"};                      \
    Input<Buffer<uint32_t, 1>> tsegEnd{"tsegEnd"};                      \
    Input<Buffer<uint32_t, 1>> tholeOff{"tholeOff"};                    \
    Input<Buffer<uint32_t, 1>> segHole0{"segHole0"};                    \
    Input<Buffer<uint32_t, 1>> holeRel{"holeRel"};                      \
    Input<int> srcTmpl{"srcTmpl"};

class Layout : public Generator<Layout> {
public:
    GeneratorParam<bool> par{"par", false};  // blocked parallel scan (a different algorithm: Halide has no scan)
    GeneratorParam<int> blk{"blk", 1 << 14};
    ITEM_INPUTS
    Input<int> nmark{"nmark"};
    Output<Func> lay{"lay", {UInt(32), UInt(32)}, 1};  // nitem + 1: (at, rat)

    Var i{"i"}, j{"j"}, b{"b"};

    void generate() {
        Expr n = itemKind.dim(0).extent();
        Func len("len");  // (length, relocations) of item i, 0 past the end
        Item it = item(*this, min(i, n - 1));
        len(i) = Tuple(select(i < n, u32(it.len), u32(0)), select(i < n, u32(it.nrel), u32(0)));
        if (!par) {
            Func sc("sc");
            RDom r(1, n, "r");
            sc(i) = Tuple(u32(0), u32(nmark));
            sc(r) = Tuple(sc(r - 1)[0] + len(r - 1)[0], sc(r - 1)[1] + len(r - 1)[1]);
            sc.compute_root();
            lay(i) = sc(i);
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
            lay.parallel(i, 1 << 16).vectorize(i, 8);
        }
    }
};

// ---------------------------------------------------------------------------------------------------------
class Emit : public Generator<Emit> {
public:
    GeneratorParam<bool> par{"par", false};    // parallel over chunks of items (racy by Halide's lights, disjoint in fact)
    GeneratorParam<int> chunk{"chunk", 4096};  // items per parallel task
    GeneratorParam<int> vec{"vec", 0};         // copy: vector width (0 scalar), predicated tail
    // patch: 0 = 8 byte stores per hole, the bytes it does not own to 8 scratch bytes (3b's form);
    //        1 = one 8-byte read-modify-write per hole (a dense vector: the bytes it does not own are written back
    //            unchanged), in two parity passes over the chunks so that no two tasks touch neighbouring bytes
    GeneratorParam<int> pform{"pform", 0};
    ITEM_INPUTS
    Input<Buffer<uint8_t, 1>> src{"src"};  // glue.bin, then tmpl.bin at srcTmpl
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
    Output<Buffer<uint8_t, 1>> code{"code"};  // code_bytes + 8 scratch bytes; updated in place (pure step undef)
    Output<Func> rel{"rel", {UInt(64), UInt(16), UInt(32), Int(64), UInt(8)}, 1};  // nrel + 1 (one scratch slot)

    Var x{"x"}, k{"k"};

    void generate() {
        Expr n = itemKind.dim(0).extent();
        Expr NC = code.dim(0).extent() - 8, NR = Func(rel).output_buffers()[0].dim(0).extent() - 1;
        Expr nfn = fnItem.dim(0).extent() - 1;
        const int C = chunk;
        Expr nChunk = (n + C - 1) / C;

        // ---- copy: (byte in item, item in chunk, chunk); the where on the byte is the inner loop's bound
        code(x) = undef<uint8_t>();
        RDom rc(0, 1 << 17, 0, C, 0, nChunk, "rc");
        Expr ic = min(rc.z * C + rc.y, n - 1);  // (min: the last chunk runs past the end; its tail repeats item n-1)
        Item ci = item(*this, ic);
        rc.where(rc.x < ci.len);
        code(inside(i32(at(ic)) + rc.x, 0, NC - 1)) = src(inside(ci.src + rc.x, 0, src.dim(0).extent() - 1));

        // ---- patch: (byte, hole in segment, item in chunk, chunk[, parity])
        RDom rp = pform == 0 ? RDom(0, 8, 0, 1 << 15, 0, C, 0, nChunk, "rp")
                             : RDom(0, 8, 0, 1 << 15, 0, C, 0, (nChunk + 1) / 2, 0, 2, "rp");
        Expr pchunk = pform == 0 ? Expr(rp[3]) : Expr(rp[3] * 2 + rp[4]);
        Expr ip = min(pchunk * C + rp.z, n - 1);
        Item pi = item(*this, ip);
        rp.where(rp.y < select(pchunk * C + rp.z < n, pi.nh, 0));
        Expr h = inside(pi.h0 + rp.y, 0, holeAt.dim(0).extent() - 1);
        Expr o = inside(pi.opBase + h, 0, opVal.dim(0).extent() - 1);
        Expr enc = holeEnc(h), w = i32(holeW(h));
        Expr P = i32(at(ip)) + i32(holeAt(h)) - pi.lo;
        Expr v = opVal(o);
        // (a real clamp: v is a jump's item only when enc says so)
        Expr val = select(enc == encRel32, i64(at(clamp(i32(v), 0, n))) + opAux(o) - i64(P + 4), v);
        Expr own = enc != encReloc && rp.x < w;  // byte rp.x of this hole is written
        Expr byte = cast<uint8_t>(val >> i64(8 * rp.x));
        if (pform == 0) {
            code(inside(select(own, P + rp.x, NC + rp.x), 0, NC + 7)) = byte;
        } else {
            Expr q = inside(P + rp.x, 0, NC + 7);
            code(q) = select(own, byte, code(q));
        }

        // ---- relocations, each into its slot
        rel(k) = Tuple(undef<uint64_t>(), undef<uint16_t>(), undef<uint32_t>(), undef<int64_t>(), undef<uint8_t>());
        // markers: (marker in function, function), at the body's start
        RDom rm(0, 1 << 16, 0, nfn, "rm");
        rm.where(rm.x < i32(fnMarkOff(rm.y + 1) - fnMarkOff(rm.y)));
        Expr m = inside(i32(fnMarkOff(rm.y)) + rm.x, 0, markOff.dim(0).extent() - 1);
        rel(inside(m, 0, NR)) = Tuple(u64(at_(at, fnItem(rm.y)) + markOff(m)), markType(m), markSym(m), markAdd(m), cast<uint8_t>(0));
        // glue relocations: (relocation in glue run, item in chunk, chunk)
        RDom rg(0, 1 << 16, 0, C, 0, nChunk, "rg");
        Expr ig = min(rg.z * C + rg.y, n - 1);
        Item gi = item(*this, ig);
        rg.where(rg.x < select(gi.kind == 0, gi.nrel, 0));
        Expr g = inside(i32(at_(glueRelOff, gi.r)) + rg.x, 0, grelOff.dim(0).extent() - 1);
        rel(inside(i32(rat(ig)) + rg.x, 0, NR)) = Tuple(u64(at(ig) + grelOff(g)), grelType(g), grelSym(g), grelAdd(g), grelSiz(g));
        // relocation holes of node segments: (hole in segment, item in chunk, chunk); its slot is its rank
        RDom rh(0, 1 << 15, 0, C, 0, nChunk, "rh");
        Expr ih = min(rh.z * C + rh.y, n - 1);
        Item hi = item(*this, ih);
        rh.where(rh.x < hi.nh);
        Expr hh = inside(hi.h0 + rh.x, 0, holeAt.dim(0).extent() - 1);
        rh.where(holeEnc(hh) == encReloc);
        Expr oh = inside(hi.opBase + hh, 0, opVal.dim(0).extent() - 1);
        Expr slot = i32(rat(ih)) + i32(at_(holeRel, hh)) - i32(at_(holeRel, hi.h0));
        Expr Ph = u64(i32(at(ih)) + i32(holeAt(hh)) - hi.lo);
        rel(inside(slot, 0, NR)) = Tuple(Ph, holeRtype(hh), u32(opVal(oh)), opAux(oh), holeW(hh));

        // ---- the schedule
        if (vec > 0) {
            RVar xo("xo"), xi("xi");
            code.update(0).allow_race_conditions().split(rc.x, xo, xi, vec, TailStrategy::Predicate).vectorize(xi);
        }
        if (pform == 0) code.update(1).unroll(rp.x);
        else code.update(1).allow_race_conditions().vectorize(rp.x);
        if (par) {
            code.update(0).allow_race_conditions().parallel(rc.z);
            if (pform == 0) code.update(1).allow_race_conditions().parallel(rp[3]);
            else code.update(1).allow_race_conditions().parallel(rp[3]);  // inside the serial parity loop rp[4]
            RVar fo("fo"), fi("fi");
            rel.update(0).allow_race_conditions().split(rm.y, fo, fi, 1024, TailStrategy::GuardWithIf).parallel(fo);
            rel.update(1).allow_race_conditions().parallel(rg.z);
            rel.update(2).allow_race_conditions().parallel(rh.z);
        }
    }
};

HALIDE_REGISTER_GENERATOR(SegHoles, segholes)
HALIDE_REGISTER_GENERATOR(Layout, layout)
HALIDE_REGISTER_GENERATOR(Emit, emit)
