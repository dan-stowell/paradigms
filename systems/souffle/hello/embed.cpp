// Embedding host: loads edges from a raw binary file (u32 pairs) straight into Soufflé's relations,
// bypassing TSV parsing; then runs the generated program and reads `total` via the C++ API.
// usage: reach_emb <mode> <threads> [dir]
//   mode 1: public API  Relation::insert(tuple)           (serial, unsorted input)
//   mode 2: internal btree insert with per-thread hint ctx (parallel, unsorted input)
//   mode 4: parallel sort + btree_set::load (linear-time bulk build)
//   mode 3: sort pairs (parallel) then btree insert(first,last) range  (serial bulk load)
// Needs private->public to reach rel_edge (generated class keeps relations private).
#include <algorithm>
#include <parallel/algorithm>
#include <any>
#include <array>
#include <map>
#include <memory>
#include <set>
#include <mutex>
#include <thread>
#include <unordered_map>
#include <numeric>
#include <optional>
#include <variant>
#include <iomanip>
#include <cassert>
#include <cmath>
#include <limits>
#include <deque>
#include <list>
#include <atomic>
#include <chrono>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <fstream>
#include <functional>
#include <iostream>
#include <regex>
#include <sstream>
#include <string>
#include <vector>
#include <fcntl.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <unistd.h>
#include <omp.h>
#define private public
#include "reach_emb.cpp"
#undef private

using Clock = std::chrono::steady_clock;
static double ms(Clock::time_point a, Clock::time_point b) {
  return std::chrono::duration<double, std::milli>(b - a).count();
}

int main(int argc, char** argv) {
  int mode = argc > 1 ? atoi(argv[1]) : 1;
  int nt = argc > 2 ? atoi(argv[2]) : 1;
  std::string dir = argc > 3 ? argv[3] : "data";
  omp_set_num_threads(nt);
  auto t0 = Clock::now();
  std::string path = dir + "/edge.bin";
  int fd = open(path.c_str(), O_RDONLY);
  struct stat st; fstat(fd, &st);
  size_t n = st.st_size / 8;
  const uint32_t* e = (const uint32_t*)mmap(nullptr, st.st_size, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd, 0);
  auto t1 = Clock::now();

  souffle::Sf_reach_emb prog;
  prog.setNumThreads(nt);
  // roots: first 100 lines of root.facts (text, tiny)
  {
    std::ifstream f(dir + "/root.facts");
    unsigned r;
    while (f >> r) {
      prog.rel_root_fa3590f20c4d7282->insert(souffle::Tuple<souffle::RamDomain, 1>{{(souffle::RamDomain)r}});
    }
  }
  auto& rel = *prog.rel_edge_04d25f5060e9043b;
  std::vector<std::array<uint32_t, 2>> sorted;
  if (mode == 1) {
    souffle::Relation* R = prog.getRelation("edge");
    for (size_t i = 0; i < n; i++) {
      souffle::tuple t(R);
      t << e[2 * i] << e[2 * i + 1];
      R->insert(t);
    }
  } else if (mode == 2) {
#pragma omp parallel
    {
      auto h = rel.createContext();
#pragma omp for schedule(static)
      for (size_t i = 0; i < n; i++) rel.insert(souffle::Tuple<souffle::RamDomain, 2>{{(souffle::RamDomain)e[2 * i], (souffle::RamDomain)e[2 * i + 1]}}, h);
    }
  } else if (mode == 4) {
    // parallel sort of packed u64 keys, then linear-time btree bulk load (btree_set::load)
    std::vector<uint64_t> k(n);
#pragma omp parallel for
    for (size_t i = 0; i < n; i++) k[i] = ((uint64_t)e[2 * i] << 32) | e[2 * i + 1];
    __gnu_parallel::sort(k.begin(), k.end());
    k.erase(std::unique(k.begin(), k.end()), k.end());
    std::vector<souffle::Tuple<souffle::RamDomain, 2>> v(k.size());
#pragma omp parallel for
    for (size_t i = 0; i < k.size(); i++) v[i] = {{(souffle::RamDomain)(k[i] >> 32), (souffle::RamDomain)(uint32_t)k[i]}};
    rel.ind_0 = std::remove_reference_t<decltype(rel.ind_0)>::load(v.begin(), v.end());
  } else {
    sorted.resize(n);
    memcpy(sorted.data(), e, n * 8);
    std::sort(sorted.begin(), sorted.end());  // serial; see NOTES for parallel variant
    sorted.erase(std::unique(sorted.begin(), sorted.end()), sorted.end());
    auto h = rel.createContext();
    for (auto& p : sorted) rel.insert(souffle::Tuple<souffle::RamDomain, 2>{{(souffle::RamDomain)p[0], (souffle::RamDomain)p[1]}}, h);
  }
  auto t2 = Clock::now();
  prog.runAll("", "", false, false);  // performIO=false: no file load, no output print
  auto t3 = Clock::now();
  souffle::Relation* T = prog.getRelation("total");
  long total = -1;
  for (auto& x : *T) { total = x[0]; }
  printf("mode=%d threads=%d edges=%zu total=%ld | open+mmap %.0f ms, insert %.0f ms, eval %.0f ms\n", mode, nt, n, total,
         ms(t0, t1), ms(t1, t2), ms(t2, t3));
}
