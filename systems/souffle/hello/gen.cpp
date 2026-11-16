// gen: 2M nodes, 10M random edges, 100 roots -> data/edge.bin (u32 pairs), data/edge.facts (TSV), data/root.facts
#include <cstdio>
#include <cstdint>
#include <random>
#include <vector>
int main(int argc, char** argv) {
  uint32_t N = 2000000; uint64_t E = 10000000; int R = 100;
  const char* d = argc > 1 ? argv[1] : "data";
  std::mt19937_64 g(42); char p[512];
  std::vector<uint32_t> v(2 * E);
  for (uint64_t i = 0; i < 2 * E; i++) v[i] = g() % N;
  snprintf(p, 512, "%s/edge.bin", d); FILE* f = fopen(p, "wb"); fwrite(v.data(), 4, 2 * E, f); fclose(f);
  snprintf(p, 512, "%s/edge.facts", d); f = fopen(p, "w");
  for (uint64_t i = 0; i < E; i++) fprintf(f, "%u\t%u\n", v[2 * i], v[2 * i + 1]);
  fclose(f);
  snprintf(p, 512, "%s/root.facts", d); f = fopen(p, "w");
  for (int i = 0; i < R; i++) fprintf(f, "%u\n", (uint32_t)(g() % N));
  fclose(f);
}
