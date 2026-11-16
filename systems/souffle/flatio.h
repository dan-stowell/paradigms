// flatio.h: binary column input/output for compiled Soufflé programs (the flat files of ref/flat/README.md).
//
// A .dl file names its inputs and outputs with IO=flat directives, for example
//   .input  edge(IO=flat, csr="p2_edge_off.u32", cols="p2_edge_dst.u32,p2_edge_kind.u8")
//   .input  flag(IO=flat, index="true", cols="p2_flags.u8")
//   .output live(IO=flat, cols="live.u32", exitafter="1")   // exitafter=N: _exit after the N-th flat output
// Input tuples: [the CSR row (csr=)] [the element's position (index=true) | its position in its row (pos=true)],
// then one value per column (a .u64x2 file gives two). File names are relative to -F (the fact dir).
// Output: column j of the relation (in its first index's order) goes to the j-th file ("-" skips it).
//
// Soufflé's own reader interface is one virtual call and one heap tuple per row, then an unhinted insert;
// so build.sh rewrites the generated code's `getReader(...)->readAll(*rel)` into `flatio::readAll(..., *rel)`,
// which has the relation's concrete type: it builds the tuples from the mmap'd columns in parallel, sorts them
// in each index's order and bulk-loads every B-tree index (btree_set::load, linear time). Relations that are
// not B-trees (brie, eqrel) get a parallel hinted insert. Non-flat IO falls through to Soufflé's IOSystem.
#pragma once
#include <fcntl.h>
#include <omp.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <unistd.h>

#include <algorithm>
#include <chrono>
#include <cstdio>
#include <cstring>
#include <parallel/algorithm>
#include <string>
#include <vector>

namespace flatio {
using namespace souffle;
using Clock = std::chrono::steady_clock;
inline double msSince(Clock::time_point a) { return std::chrono::duration<double, std::milli>(Clock::now() - a).count(); }

struct Stats {
	Clock::time_point start = Clock::now();
	double load = 0, write = 0, lastWrite = 0; // lastWrite: ms from start to the end of the last output
	size_t tuples = 0;
	// compute = everything up to the last output that is not reading or writing; teardown = freeing the relations
	~Stats() { report(); }
	void report() {
		double total = msSince(start);
		if (getenv("FLATIO_QUIET")) return;
		fprintf(stderr, "flatio: load %.1f ms (%zu tuples), compute %.1f ms, write %.1f ms, teardown %.1f ms, threads %d\n", load,
		        tuples, lastWrite - load - write, write, total - lastWrite, omp_get_max_threads());
	}
};
inline Stats stats;

struct Col {
	const uint8_t* p = nullptr;
	size_t n = 0;     // elements
	int size = 4;     // bytes per element
	int width = 1;    // values per element (2 for .u64x2)
	bool sgn = false; // signed
	RamDomain get(size_t i, int j) const {
		const uint8_t* q = p + (i * width + j) * size;
		switch (size) {
		case 1: return *q;
		case 2: { uint16_t v; memcpy(&v, q, 2); return v; }
		case 4: {
			if (sgn) { int32_t v; memcpy(&v, q, 4); return v; }
			uint32_t v; memcpy(&v, q, 4); return v;
		}
		default: { int64_t v; memcpy(&v, q, 8); return v; }
		}
	}
};

inline const uint8_t* mapFile(const std::string& path, size_t& bytes) {
	int fd = open(path.c_str(), O_RDONLY);
	if (fd < 0) throw std::runtime_error("flatio: cannot open " + path);
	struct stat st;
	fstat(fd, &st);
	bytes = st.st_size;
	void* m = bytes ? mmap(nullptr, bytes, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd, 0) : nullptr;
	close(fd);
	if (m == MAP_FAILED) throw std::runtime_error("flatio: cannot map " + path);
	return (const uint8_t*)m;
}

inline std::string factDir = "."; // the last input's fact dir (writers see only the output dir)

// a string table P.off.u64 + P.bin, named "P.str": per string id, its Soufflé symbol (encoded serially)
inline Col openStrings(const std::string& dir, const std::string& name, SymbolTable& st) {
	std::string base = name.substr(0, name.size() - 4);
	size_t ob, bb;
	const uint64_t* off = (const uint64_t*)mapFile(dir + "/" + base + ".off.u64", ob);
	const char* bin = (const char*)mapFile(dir + "/" + base + ".bin", bb);
	static std::vector<std::vector<int64_t>> keep;
	keep.emplace_back(ob / 8 - 1);
	auto& ids = keep.back();
	for (size_t i = 0; i < ids.size(); i++) ids[i] = st.encode(std::string(bin + off[i], off[i + 1] - off[i]));
	Col c;
	c.p = (const uint8_t*)ids.data(), c.n = ids.size(), c.size = 8;
	return c;
}

// "P.key8": per string id of the table P, its first 8 bytes as a big-endian number (zero-padded), so that numeric
// order is bytewise order on the 8-byte prefixes
inline Col openPrefixKeys(const std::string& dir, const std::string& name) {
	std::string base = name.substr(0, name.size() - 5);
	size_t ob, bb;
	const uint64_t* off = (const uint64_t*)mapFile(dir + "/" + base + ".off.u64", ob);
	const uint8_t* bin = mapFile(dir + "/" + base + ".bin", bb);
	static std::vector<std::vector<int64_t>> keep;
	keep.emplace_back(ob / 8 - 1);
	auto& k = keep.back();
	for (size_t i = 0; i < k.size(); i++) {
		uint64_t v = 0;
		for (size_t j = 0; j < 8; j++) v = v << 8 | (off[i] + j < off[i + 1] ? bin[off[i] + j] : 0);
		k[i] = (int64_t)v;
	}
	Col c;
	c.p = (const uint8_t*)k.data(), c.n = k.size(), c.size = 8;
	return c;
}

inline Col openCol(const std::string& dir, const std::string& name) {
	Col c;
	size_t bytes;
	c.p = mapFile(dir + "/" + name, bytes);
	std::string sfx = name.substr(name.rfind('.') + 1);
	if (sfx == "u8") c.size = 1;
	else if (sfx == "u16") c.size = 2;
	else if (sfx == "u32") c.size = 4;
	else if (sfx == "i32") c.size = 4, c.sgn = true;
	else if (sfx == "i64" || sfx == "u64") c.size = 8;
	else if (sfx == "u64x2") c.size = 8, c.width = 2;
	else throw std::runtime_error("flatio: unknown column type " + name);
	c.n = bytes / (c.size * c.width);
	return c;
}

inline std::vector<std::string> split(const std::string& s) {
	std::vector<std::string> out;
	size_t a = 0;
	while (a <= s.size()) {
		size_t b = s.find(',', a);
		if (b == std::string::npos) b = s.size();
		if (b > a) out.push_back(s.substr(a, b - a));
		a = b + 1;
	}
	return out;
}
inline std::string opt(const std::map<std::string, std::string>& d, const std::string& k) {
	auto it = d.find(k);
	return it == d.end() ? "" : it->second;
}

// the comparator of a B-tree index type, if it is one
template <class T>
struct btreeOf { static constexpr bool ok = false; };
template <class K, class C, class A, unsigned B, class S, class W, class U>
struct btreeOf<btree_set<K, C, A, B, S, W, U>> { static constexpr bool ok = true; using cmp = C; };
template <class K, class C, class A, unsigned B, class S, class W, class U>
struct btreeOf<btree_multiset<K, C, A, B, S, W, U>> { static constexpr bool ok = true; using cmp = C; };

// does relation type R have index member ind_k? (C++17: Soufflé's headers need make_span, gone in C++20)
#define FLATIO_HAS(k)                                                                         \
	template <class R, class = void>                                                          \
	struct has_ind_##k : std::false_type {};                                                  \
	template <class R>                                                                        \
	struct has_ind_##k<R, std::void_t<decltype(std::declval<R&>().ind_##k)>> : std::true_type {};
FLATIO_HAS(0) FLATIO_HAS(1) FLATIO_HAS(2) FLATIO_HAS(3) FLATIO_HAS(4) FLATIO_HAS(5) FLATIO_HAS(6) FLATIO_HAS(7) FLATIO_HAS(8)
#undef FLATIO_HAS
template <class T>
using bare = std::remove_cv_t<std::remove_reference_t<T>>;

// sort a copy of v in the index's order and bulk-load it
template <class Ind, class V>
void loadIndex(Ind& ind, const V& v) {
	using C = typename btreeOf<Ind>::cmp;
	C c;
	if (std::is_sorted(v.begin(), v.end(), [&](const auto& a, const auto& b) { return c.less(a, b); })) {
		Ind t = Ind::load(v.begin(), v.end());
		ind.swap(t);
		return;
	}
	V w(v);
	__gnu_parallel::sort(w.begin(), w.end(), [&](const auto& a, const auto& b) { return c.less(a, b); });
	Ind t = Ind::load(w.begin(), w.end());
	ind.swap(t);
}

template <class R, class V>
void bulkLoad(R& rel, V& v) {
	if constexpr (has_ind_0<R>::value) {
		// set semantics: drop duplicate tuples (an index can be a partial order, a multiset, so use the whole
		// tuple), then load every index in its own order
		if (!std::is_sorted(v.begin(), v.end())) __gnu_parallel::sort(v.begin(), v.end());
		v.erase(std::unique(v.begin(), v.end()), v.end());
#define FLATIO_IND(k)                                                                                    \
	if constexpr (has_ind_##k<R>::value) {                                                              \
		static_assert(btreeOf<bare<decltype(rel.ind_##k)>>::ok, "flatio: non-btree index");               \
		loadIndex(rel.ind_##k, v);                                                                    \
	}
		FLATIO_IND(0) FLATIO_IND(1) FLATIO_IND(2) FLATIO_IND(3) FLATIO_IND(4) FLATIO_IND(5) FLATIO_IND(6) FLATIO_IND(7)
		static_assert(!has_ind_8<R>::value, "flatio: more than 8 indexes");
#undef FLATIO_IND
	} else {
#pragma omp parallel
		{
			auto h = rel.createContext();
#pragma omp for schedule(static)
			for (size_t i = 0; i < v.size(); i++) rel.insert(v[i], h);
		}
	}
}

template <class R>
void readAll(std::map<std::string, std::string> d, SymbolTable& st, RecordTable& rt, R& rel) {
	if (opt(d, "IO") != "flat") {
		IOSystem::getInstance().getReader(d, st, rt)->readAll(rel);
		return;
	}
	auto t0 = Clock::now();
	using T = typename R::t_tuple;
	constexpr size_t arity = std::tuple_size<T>::value;
	std::string dir = opt(d, "fact-dir");
	factDir = dir;
	if (opt(d, "meta") == "true") { // meta.txt: (key, value) lines
		if constexpr (arity == 2) {
			FILE* f = fopen((dir + "/meta.txt").c_str(), "r");
			if (!f) throw std::runtime_error("flatio: no meta.txt in " + dir);
			char k[256];
			unsigned long long x;
			while (fscanf(f, "%255s %llu", k, &x) == 2) rel.insert(T{st.encode(k), (RamDomain)x});
			fclose(f);
		}
		stats.load += msSince(t0);
		return;
	}
	// words8="P": the string table P as (string id, word index, word): each string's bytes as big-endian 8-byte
	// words, the last one zero-padded, so numeric order on words is bytewise order (a string of n bytes has
	// ceil(n / 8) words). The bytes as numbers, nothing more: no comparison or order is computed here.
	if (!opt(d, "words8").empty()) {
		if constexpr (arity == 3) {
			std::string base = opt(d, "words8");
			size_t ob, bb;
			const uint64_t* off = (const uint64_t*)mapFile(dir + "/" + base + ".off.u64", ob);
			const uint8_t* bin = mapFile(dir + "/" + base + ".bin", bb);
			std::vector<T> v;
			for (size_t s = 0; s + 1 < ob / 8; s++)
				for (uint64_t a = off[s], c = 0; a < off[s + 1]; a += 8, c++) {
					uint64_t w = 0;
					for (size_t j = 0; j < 8; j++) w = w << 8 | (a + j < off[s + 1] ? bin[a + j] : 0);
					v.push_back(T{(RamDomain)s, (RamDomain)c, (RamDomain)w});
				}
			bulkLoad(rel, v);
			stats.tuples += v.size();
		} else throw std::runtime_error("flatio: words8 needs arity 3");
		stats.load += msSince(t0);
		return;
	}
	std::vector<Col> cols;
	size_t nv = 0;
	for (auto& f : split(opt(d, "cols")))
		cols.push_back(f.size() > 4 && f.substr(f.size() - 4) == ".str" ? openStrings(dir, f, st)
		               : f.size() > 5 && f.substr(f.size() - 5) == ".key8" ? openPrefixKeys(dir, f)
		                                                                    : openCol(dir, f)),
		    nv += cols.back().width;
	// leading columns: the CSR row; then the element's position (index=true) or its position in its row (pos=true)
	std::string csr = opt(d, "csr");
	bool byRow = !csr.empty(), byIndex = opt(d, "index") == "true", byPos = opt(d, "pos") == "true";
	size_t lead = byRow + byIndex + byPos;
	if (nv + lead != arity) throw std::runtime_error("flatio: " + opt(d, "name") + ": columns do not match arity");
	Col off;
	if (byRow) off = openCol(dir, csr);
	size_t n = byRow ? (size_t)off.get(off.n - 1, 0) : cols.empty() ? 0 : cols[0].n;
	for (auto& c : cols)
		if (c.n != n) throw std::runtime_error("flatio: " + opt(d, "name") + ": columns differ in length");
	std::vector<T> v(n);
	auto fill = [&](size_t i, size_t row, size_t first) {
		T& t = v[i];
		size_t k = 0;
		if (byRow) t[k++] = row;
		if (byIndex) t[k++] = i;
		if (byPos) t[k++] = i - first;
		for (auto& c : cols)
			for (int j = 0; j < c.width; j++) t[k++] = c.get(i, j);
	};
	if (byRow) {
#pragma omp parallel for schedule(dynamic, 4096)
		for (size_t r = 0; r < off.n - 1; r++)
			for (size_t i = off.get(r, 0), e = off.get(r + 1, 0); i < e; i++) fill(i, r, off.get(r, 0));
	} else {
#pragma omp parallel for schedule(static)
		for (size_t i = 0; i < n; i++) fill(i, 0, 0);
	}
	bulkLoad(rel, v);
	stats.tuples += v.size();
	stats.load += msSince(t0);
	if (getenv("FLATIO_VERBOSE")) fprintf(stderr, "flatio: %s %zu tuples %.1f ms\n", opt(d, "name").c_str(), v.size(), msSince(t0));
}

inline uint64_t metaGet(const std::string& dir, const char* key) {
	FILE* f = fopen((dir + "/meta.txt").c_str(), "r");
	if (!f) throw std::runtime_error("flatio: no meta.txt in " + dir);
	char k[256];
	unsigned long long x;
	while (fscanf(f, "%255s %llu", k, &x) == 2)
		if (!strcmp(k, key)) { fclose(f); return x; }
	fclose(f);
	throw std::runtime_error(std::string("flatio: meta has no ") + key);
}

// image="NAME": the relation is (position, width, value), the patches of problem 3b. Assemble the output image
// (C, not Datalog: Soufflé has no bytes): copy every placed symbol's bytes to its address (addr=, default
// oracle/p3_addr.u32; not buckets 13-15), write each patch's low `width` bytes little-endian, write NAME.
// FLATIO_HUGE=1: the image on 2 MB pages (MAP_HUGETLB). FLATIO_IMG_REPS=n: assemble n times into a fresh mapping
// and report min/median/max ms and minor faults per assembly (ATTACKS 2).
template <class R>
void writeImage(std::map<std::string, std::string> d, const R& rel) {
	auto t0 = Clock::now();
	const std::string& fd = factDir;
	size_t L = metaGet(fd, "p3_placed"), nimg = metaGet(fd, "image_bytes"), base = metaGet(fd, "imageBase"), b;
	std::string an = opt(d, "addr").empty() ? "oracle/p3_addr.u32" : opt(d, "addr");
	const uint32_t* addr = (const uint32_t*)mapFile(fd + "/" + an, b);
	const uint8_t* bucket = mapFile(fd + "/p3_bucket.u8", b);
	const uint64_t* doff = (const uint64_t*)mapFile(fd + "/p3_data.off.u64", b);
	const uint8_t* dbin = mapFile(fd + "/p3_data.bin", b);
	std::vector<typename R::t_tuple> all(rel.begin(), rel.end());
	double toVec = msSince(t0);
	int reps = getenv("FLATIO_IMG_REPS") ? atoi(getenv("FLATIO_IMG_REPS")) : 1;
	std::vector<double> ms;
	std::vector<long> flt;
	uint8_t* img = nullptr;
	size_t mapped = 0;
	for (int rep = 0; rep < reps; rep++) {
		if (img) munmap(img, mapped);
		struct rusage r0, r1;
		getrusage(RUSAGE_SELF, &r0);
		auto t1 = Clock::now();
		size_t hp = 2u << 20;
		mapped = (nimg + hp - 1) / hp * hp;
		img = (uint8_t*)MAP_FAILED;
		if (getenv("FLATIO_HUGE")) img = (uint8_t*)mmap(nullptr, mapped, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
		if (img == MAP_FAILED) img = (uint8_t*)mmap(nullptr, mapped, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
#pragma omp parallel for schedule(dynamic, 1024)
		for (size_t i = 0; i < L; i++)
			if (bucket[i] < 13 && doff[i + 1] > doff[i]) memcpy(img + addr[i] - base, dbin + doff[i], doff[i + 1] - doff[i]);
#pragma omp parallel for schedule(static)
		for (size_t i = 0; i < all.size(); i++) {
			uint64_t v = (uint64_t)all[i][2];
			memcpy(img + (uint64_t)all[i][0] - base, &v, (size_t)all[i][1]); // little-endian: the low bytes
		}
		ms.push_back(msSince(t1));
		getrusage(RUSAGE_SELF, &r1);
		flt.push_back(r1.ru_minflt - r0.ru_minflt);
	}
	auto t2 = Clock::now();
	std::string path = (opt(d, "output-dir").empty() ? std::string(".") : opt(d, "output-dir")) + "/" + opt(d, "image");
	FILE* f = fopen(path.c_str(), "wb");
	if (!f || fwrite(img, 1, nimg, f) != nimg) throw std::runtime_error("flatio: cannot write " + path);
	fclose(f);
	double fileMs = msSince(t2);
	std::vector<double> sorted = ms;
	std::sort(sorted.begin(), sorted.end());
	fprintf(stderr, "flatio: image %zu patches, assemble min %.1f median %.1f max %.1f ms over %d (%s pages, %ld minor faults), tuples->vector %.1f ms, file %.1f ms\n",
	        all.size(), sorted[0], sorted[sorted.size() / 2], sorted.back(), reps, getenv("FLATIO_HUGE") ? "2 MB" : "4 KB", flt.back(), toVec, fileMs);
}

template <class R>
void writeAll(std::map<std::string, std::string> d, SymbolTable& st, RecordTable& rt, const R& rel) {
	if (opt(d, "IO") != "flat") {
		IOSystem::getInstance().getWriter(d, st, rt)->writeAll(rel);
		return;
	}
	auto t0 = Clock::now();
	using T = typename R::t_tuple;
	constexpr size_t arity = std::tuple_size<T>::value;
	std::string dir = opt(d, "output-dir");
	if (dir.empty()) dir = ".";
	auto names = split(opt(d, "cols"));
	if (!opt(d, "image").empty()) {
		if constexpr (arity == 3) writeImage(d, rel);
		names.clear();
	} else if (names.size() != arity) throw std::runtime_error("flatio: " + opt(d, "name") + ": output columns do not match arity");
	std::vector<T> all;
	if (!names.empty()) all.assign(rel.begin(), rel.end());
	for (size_t j = 0; j < names.size(); j++) {
		if (names[j] == "-") continue;
		Col c = {};
		std::string sfx = names[j].substr(names[j].rfind('.') + 1);
		c.size = sfx == "u8" ? 1 : sfx == "u16" ? 2 : sfx == "u32" || sfx == "i32" ? 4 : 8;
		std::vector<uint8_t> buf(all.size() * c.size);
		for (size_t i = 0; i < all.size(); i++) memcpy(&buf[i * c.size], &all[i][j], c.size); // little-endian
		std::string path = dir + "/" + names[j];
		FILE* f = fopen(path.c_str(), "wb");
		if (!f) throw std::runtime_error("flatio: cannot write " + path);
		fwrite(buf.data(), 1, buf.size(), f);
		fclose(f);
	}
	stats.write += msSince(t0);
	stats.lastWrite = msSince(stats.start);
	static int written = 0;
	written++;
	if (!getenv("FLATIO_NOEXIT") && !opt(d, "exitafter").empty() && written >= std::stoi(opt(d, "exitafter"))) { // the last output: exit, do not free every relation
		stats.report();
		fflush(stdout);
		_exit(0);
	}
}
} // namespace flatio
