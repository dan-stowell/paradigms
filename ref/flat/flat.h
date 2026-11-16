// flat.h: read the flat files (data/README.md) by mmap. Header-only; every reference program includes it.
#define _GNU_SOURCE
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>
typedef uint8_t u8;
typedef uint16_t u16;
typedef uint32_t u32;
typedef uint64_t u64;
typedef int32_t i32;
typedef int64_t i64;

static const char *flatDir;
static size_t lastSize;
// fmap: the whole file, read-only; *n = its size in bytes
static const void *fmap(const char *name, size_t *n) {
	char p[4096];
	snprintf(p, sizeof p, "%s/%s", flatDir, name);
	int fd = open(p, O_RDONLY);
	if (fd < 0) { perror(p); exit(2); }
	struct stat st;
	fstat(fd, &st);
	lastSize = (size_t)st.st_size;
	if (n) *n = lastSize;
	if (lastSize == 0) { close(fd); return ""; }
	void *m = mmap(NULL, lastSize, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd, 0);
	if (m == MAP_FAILED) { perror(p); exit(2); }
	close(fd);
	// FLAT_HUGE=1: copy files of >= 2 MB onto 2 MB pages (MAP_HUGETLB, needs vm.nr_hugepages; falls back to the file
	// mapping): page-cache mappings are 4 KB, so a random walk over a big column misses the TLB (ATTACKS 2)
	static int huge = -1;
	if (huge < 0) huge = getenv("FLAT_HUGE") && atoi(getenv("FLAT_HUGE"));
	if (huge && lastSize >= (2u << 20)) {
		size_t hn = (lastSize + (2u << 20) - 1) & ~(size_t)((2u << 20) - 1);
		void *h = mmap(NULL, hn, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
		if (h != MAP_FAILED) { memcpy(h, m, lastSize); munmap(m, lastSize); return h; }
	}
	return m;
}
#define FMAP(T, name, cnt) ((const T *)fmap(name, cnt))
static u64 metaGet(const char *key) {
	char p[4096], k[128];
	unsigned long long v;
	snprintf(p, sizeof p, "%s/meta.txt", flatDir);
	FILE *f = fopen(p, "r");
	if (!f) { perror(p); exit(2); }
	while (fscanf(f, "%127s %llu", k, &v) == 2)
		if (strcmp(k, key) == 0) { fclose(f); return v; }
	fprintf(stderr, "meta: no %s\n", key);
	exit(2);
}
static double now(void) {
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec + t.tv_nsec * 1e-9;
}
static inline u64 mix64(u64 h) {
	h ^= h >> 30;
	h *= 0xBF58476D1CE4E5B9ull;
	h ^= h >> 27;
	h *= 0x94D049BB133111EBull;
	return h ^ (h >> 31);
}
// compare n u32s with an oracle file; prints the first few differences; returns the number that differ
static long check32(const char *what, const u32 *got, size_t n, const char *oracle) {
	size_t m;
	const u32 *want = FMAP(u32, oracle, &m);
	if (m != n * 4) { printf("%s: size %zu, oracle %zu\n", what, n, m / 4); return -1; }
	long bad = 0;
	for (size_t i = 0; i < n; i++)
		if (got[i] != want[i] && bad++ < 5) printf("  %s[%zu] = %u, oracle %u\n", what, i, got[i], want[i]);
	printf("%s: %zu entries, %ld differ from %s\n", what, n, bad, oracle);
	return bad;
}
