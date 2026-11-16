#include "Halide.h"
using namespace Halide;

// (a) gather: out[i] = table[idx[i]]
class Gather : public Generator<Gather> {
public:
    Input<Buffer<uint32_t, 1>> idx{"idx"};
    Input<Buffer<uint32_t, 1>> table{"table"};
    Output<Buffer<uint32_t, 1>> out{"out"};
    Var i{"i"}, io{"io"};
    void generate() {
        out(i) = table(clamp(cast<int>(idx(i)), 0, table.dim(0).extent() - 1));  // clamp is required: data-dependent index => unbounded access otherwise
    }
    void schedule() {
        out.split(i, io, i, 1 << 16).parallel(io);
    }
};

// (b) histogram: hist[idx[r] % nbins] += 1, an RDom update with a data-dependent store.
//  variant 0: default (serial: Halide cannot prove the update parallel-safe)
//  variant 1: update().atomic().parallel(r): shared bins, atomic adds
//  variant 2: rfactor: per-chunk private histograms (parallel over chunks), then merge
class Hist : public Generator<Hist> {
public:
    GeneratorParam<int> variant{"variant", 0};
    GeneratorParam<int> nbins{"nbins", 1 << 20};
    GeneratorParam<int> chunk{"chunk", 6250000};
    Input<Buffer<uint32_t, 1>> idx{"idx"};
    Output<Buffer<uint32_t, 1>> hist{"hist"};
    void generate() {
        Var b, u;
        RDom r(0, idx.dim(0).extent());
        hist(b) = cast<uint32_t>(0);
        hist(cast<int>(idx(r) % cast<uint32_t>(nbins))) += cast<uint32_t>(1);
        if (variant == 1) {
            hist.update().atomic().parallel(r.x);
        } else if (variant == 2) {
            RVar ro, ri;
            hist.update().split(r.x, ro, ri, chunk);
            Func inter = hist.update().rfactor(ro, u);
            inter.compute_root().update().parallel(u);
            inter.compute_root().parallel(b);
        }
        // bounds of the output are not inferable from the scatter; pin them
        hist.dim(0).set_bounds(0, nbins);
    }
};

// (c) inclusive prefix sum. Scan = RDom update with a loop-carried dependence.
//  variant 0: out(r) = out(r-1) + in(r), serial.
//  variant 1: blocked three-phase scan written by hand (scan in blocks in parallel,
//             scan block totals serially, add block offsets).
class Scan : public Generator<Scan> {
public:
    GeneratorParam<int> variant{"variant", 0};
    GeneratorParam<int> blk{"blk", 100000};
    Input<Buffer<uint32_t, 1>> in{"in"};
    Output<Buffer<uint32_t, 1>> out{"out"};
    void generate() {
        Var x, c;
        if (variant == 0) {
            RDom r(1, in.dim(0).extent() - 1);
            out(x) = in(x);
            out(r) = out(r - 1) + in(r);
        } else {
            RDom ri(1, blk - 1);
            Func local("local");  // per-block inclusive scan
            local(x, c) = in(c * blk + x);
            local(ri, c) = local(ri - 1, c) + in(c * blk + ri);
            Func total("total");  // inclusive scan of block totals
            RDom rc(1, in.dim(0).extent() / blk - 1);
            total(c) = local(blk - 1, c);
            total(rc) = total(rc - 1) + local(blk - 1, rc);
            out(x) = local(x % blk, x / blk) + select(x / blk > 0, total(max(x / blk - 1, 0)), cast<uint32_t>(0));
            local.compute_root().parallel(c);
            local.update().parallel(c);
            total.compute_root();
            Var xo, xi;
            out.split(x, xo, xi, blk).parallel(xo);
        }
    }
};

HALIDE_REGISTER_GENERATOR(Gather, gather)
HALIDE_REGISTER_GENERATOR(Hist, hist)
HALIDE_REGISTER_GENERATOR(Scan, scan)
