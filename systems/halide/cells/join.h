// join.h: a hash join in Halide, written as rounds of scatter-min and gather.
//
// Halide has no hash table: an insert with linear probing is a loop whose trip count depends on the data, and
// an update definition writes one site per iteration. So each probe becomes a ROUND over all the entries still
// looking for their group: every entry that may create a group scatters its index into bucket h_r(key) with
// min (a commutative update, so Halide can run it atomically in parallel); then every looking entry gathers the
// bucket's owner and compares keys. Equal keys hash alike, so a key's members all settle in the same round; a
// key that lost its bucket to another key tries again with a new hash in a smaller table. The group of an entry
// is the index of the entry that claimed its bucket (its representative). After `rounds` rounds, `left` counts
// the creating entries that never settled (it must be 0; the driver checks).
#pragma once
#include "Halide.h"

namespace hj {
using namespace Halide;

inline Expr u64e(Expr e) { return cast<uint64_t>(e); }
inline Expr mix64(Expr h) {  // flat.h's mix64
    h = h ^ (h >> 30);
    h = h * u64e(Expr((uint64_t)0xBF58476D1CE4E5B9ull));
    h = h ^ (h >> 27);
    h = h * u64e(Expr((uint64_t)0x94D049BB133111EBull));
    return h ^ (h >> 31);
}

// parallel over chunks of an RVar (parallel(r) alone makes one task per iteration)
inline void parChunks(Stage s, RVar r, int n = 1 << 14) {
    RVar o(r.name() + "o"), i(r.name() + "i");
    s.split(r, o, i, n, TailStrategy::GuardWithIf).parallel(o);
}

struct Join {
    Func rep;    // rep(j): the representative entry of j's group, or -1 (a looking entry whose key no creator has)
    Func left;   // left(): creating entries with no group after the last round (must be 0)
    std::vector<Func> claims, reps;  // per round, for scheduling
};

// Entries j in [0, M). k1(j), k2(j): the key. creates(j) (u8, nonzero): the entry may create a group (else it only looks one
// up). Round r uses a table of 2^(logT - r) buckets (at least 2^10).
inline Join hashJoin(const std::string &name, Func k1, Func k2, Func creates, Expr M, int logT, int rounds, bool par) {
    Var j("j"), h("h");
    Join out;
    Func prev;  // rep after the previous round
    for (int r = 0; r < rounds; r++) {
        int lg = std::max(logT - r, 10);
        Expr salt = u64e(Expr((uint64_t)(0x9E3779B97F4A7C15ull * (uint64_t)(r + 1))));
        auto bucket = [&](Expr e) { return cast<int32_t>(mix64(k1(e) ^ mix64(k2(e) + salt)) >> (64 - lg)); };
        Expr looking = r == 0 ? Internal::const_true() : prev(j) < 0;

        Func claim(name + "_claim" + std::to_string(r));
        RDom rj(0, M, name + "_rj" + std::to_string(r));
        Expr lookingR = r == 0 ? Internal::const_true() : prev(rj) < 0;
        rj.where(creates(rj) != 0 && lookingR);
        claim(h) = M;  // M: empty
        Expr b = clamp(bucket(rj), 0, (1 << lg) - 1);
        claim(b) = min(claim(b), rj);

        Func rep(name + "_rep" + std::to_string(r));
        Expr own = claim(clamp(bucket(j), 0, (1 << lg) - 1));
        Expr ownC = clamp(own, 0, M - 1);
        Expr hit = own < M && k1(ownC) == k1(j) && k2(ownC) == k2(j);
        rep(j) = r == 0 ? select(hit, own, -1) : select(!looking, prev(j), hit, own, -1);

        claim.compute_root();
        rep.compute_root();
        if (par) {
            claim.parallel(h, 1 << 14, TailStrategy::GuardWithIf);
            parChunks(claim.update().atomic(), rj);
            rep.parallel(j, 1 << 14, TailStrategy::GuardWithIf);
        }
        out.claims.push_back(claim);
        out.reps.push_back(rep);
        prev = rep;
    }
    out.rep = prev;
    RDom rl(0, M, name + "_rl");
    out.left = Func(name + "_left");
    out.left() = sum(select(creates(rl) != 0 && prev(rl) < 0, 1, 0));
    return out;
}
}  // namespace hj
