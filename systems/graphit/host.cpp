// host.cpp: p2.gt's one extern (check), linked with the generated C++: compares the live slots (st[g] == 0 for
// g in 1..nslots) with oracle/p2_live.u32. Not timed; it only reads the result.
#include <cstdio>
#include <cstdint>
#include <cstring>
#include <string>
#include <vector>
extern int *st; // the GraphIt vector, defined in the generated code

static unsigned long long metaGet(const std::string &dir, const char *key) {
	FILE *f = fopen((dir + "/meta.txt").c_str(), "r");
	char k[128];
	unsigned long long v;
	while (f && fscanf(f, "%127s %llu", k, &v) == 2)
		if (!strcmp(k, key)) { fclose(f); return v; }
	fprintf(stderr, "meta: no %s\n", key);
	exit(2);
}

int check(std::string dir, int reps) {
	uint32_t nsl = (uint32_t)metaGet(dir, "nslots");
	std::vector<uint32_t> got;
	for (uint32_t g = 1; g <= nsl; g++)
		if (st[g] == 0) got.push_back(g);
	FILE *f = fopen((dir + "/oracle/p2_live.u32").c_str(), "rb");
	if (!f) { perror("oracle"); return 1; }
	std::vector<uint32_t> want;
	uint32_t x;
	while (fread(&x, 4, 1, f) == 1) want.push_back(x);
	fclose(f);
	long bad = got.size() == want.size() ? 0 : -1;
	for (size_t i = 0; bad >= 0 && i < got.size(); i++)
		if (got[i] != want[i] && bad++ < 5) printf("  live[%zu] = %u, oracle %u\n", i, got[i], want[i]);
	printf("live: %zu entries (oracle %zu), %ld differ\n%s\n", got.size(), want.size(), bad, bad ? "P2 FAIL" : "P2 OK");
	return bad != 0;
}
