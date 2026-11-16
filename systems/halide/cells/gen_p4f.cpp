// Problem 4 in Halide over the flattened emit stream (ATTACKS 4 step 1; ref/p4/p4runs.c D, Futhark's 4D).
// The item -> template -> segment -> hole chain is resolved once, before the emit (p4f.cpp's prep, in C, timed
// apart): a blob that is the image with every patched field zeroed, one patch record per (4-byte part of an)
// integer or rel32 hole, and the relocations complete. The emit is then three flat loops with no lookups:
//   copy:     code(x) = blob(x)                         (a pure, vectorized Func)
//   patch:    for each record, its <= 4 bytes             (a scatter; a rel32 gathers at[its target item])
//   relocs:   rel(k) = the prepared columns              (a pure copy, as Futhark's 4D and p4runs collect them)
// With H1 (codex-holes) the integers are baked into the blob by the prep and only rel32 records are left.
#include "Halide.h"
using namespace Halide;

static Expr i32(Expr e) { return cast<int32_t>(e); }
static Expr i64(Expr e) { return cast<int64_t>(e); }
static Expr inside(Expr e, Expr lo, Expr hi) { return unsafe_promise_clamped(e, lo, hi); }
enum { encInt = 1, encRel32 = 2 };

class EmitFlat : public Generator<EmitFlat> {
public:
    GeneratorParam<bool> par{"par", false};
    GeneratorParam<int> chunk{"chunk", 1 << 14};  // records per parallel task
    // patch: 0 = 4 byte stores per record, the bytes past its width to 4 scratch bytes past the code (3b's form);
    //        1 = 4-byte records only (H1: rel32 only): one dense 4-lane store each
    GeneratorParam<int> pform{"pform", 0};
    GeneratorParam<bool> doRel{"doRel", true};  // the relocation copy (false: its time measured apart)
    Input<Buffer<uint8_t, 1>> blob{"blob"};     // code_bytes
    Input<Buffer<int64_t, 1>> at{"at"};         // nitem + 1: every item's place (the rel32 targets)
    Input<Buffer<int32_t, 1>> pp{"pp"};         // per record: absolute position
    Input<Buffer<uint8_t, 1>> pn{"pn"};         //   width (1..4)
    Input<Buffer<uint8_t, 1>> pe{"pe"};         //   encInt / encRel32
    Input<Buffer<int64_t, 1>> pv{"pv"};         //   integer (already shifted for an 8-byte hole's high half) / target item
    Input<Buffer<int64_t, 1>> pa{"pa"};         //   rel32: addend
    Input<Buffer<uint64_t, 1>> ro{"ro"};        // the relocations
    Input<Buffer<uint8_t, 1>> rs{"rs"};
    Input<Buffer<uint16_t, 1>> rt{"rt"};
    Input<Buffer<uint32_t, 1>> ry{"ry"};
    Input<Buffer<int64_t, 1>> ra{"ra"};
    Output<Buffer<uint8_t, 1>> code{"code"};    // code_bytes + 64 (scratch past the end)
    Output<Func> rel{"rel", {UInt(64), UInt(16), UInt(32), Int(64), UInt(8)}, 1};

    Var x{"x"}, k{"k"}, xo{"xo"}, xi{"xi"};

    void generate() {
        Expr NC = blob.dim(0).extent(), NP = pp.dim(0).extent(), na = at.dim(0).extent();
        code(x) = select(x < NC, blob(min(x, NC - 1)), cast<uint8_t>(0));
        // patch: (byte, record in chunk, chunk)
        const int C = chunk;
        RDom r(0, 4, 0, C, 0, (NP + C - 1) / C, "r");
        Expr j = r.z * C + r.y, real = j < NP, jc = min(j, NP - 1);
        Expr P = pp(jc), n = select(real, i32(pn(jc)), 0), e = pe(jc), v = pv(jc);
        // (both sides of a select are evaluated: an integer record reads at[0], not at[its value])
        Expr tgt = select(e == encRel32, inside(i32(v), 0, na - 1), 0);
        Expr val = select(e == encRel32, at(tgt) + pa(jc) - i64(P + 4), v);
        Expr byte = cast<uint8_t>(val >> i64(8 * r.x));
        if (pform == 0) {
            code(inside(select(r.x < n, P + r.x, NC + r.x), 0, NC + 63)) = byte;
        } else {
            r.where(real);
            code(inside(P + r.x, 0, NC + 63)) = byte;
        }
        if (doRel)
            rel(k) = Tuple(ro(k), rt(k), ry(k), ra(k), rs(k));
        else
            rel(k) = Tuple(cast<uint64_t>(0), cast<uint16_t>(0), cast<uint32_t>(0), cast<int64_t>(0), cast<uint8_t>(0));

        // schedule
        code.split(x, xo, xi, 1 << 16, TailStrategy::GuardWithIf).vectorize(xi, 64, TailStrategy::GuardWithIf);
        if (pform == 0) code.update(0).unroll(r.x);
        else code.update(0).allow_race_conditions().vectorize(r.x);
        Func(rel).split(k, xo, xi, 1 << 14, TailStrategy::GuardWithIf).vectorize(xi, 8, TailStrategy::GuardWithIf);
        if (par) {
            code.parallel(xo);
            code.update(0).allow_race_conditions().parallel(r.z);  // records write disjoint bytes (scratch aside)
            Func(rel).parallel(xo);
        }
    }
};

HALIDE_REGISTER_GENERATOR(EmitFlat, emitflat)
