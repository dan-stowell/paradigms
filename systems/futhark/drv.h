// drv.h: what every Futhark driver shares. The drivers only load the flat files (mmap, via ref/flat/flat.h),
// hand them to the generated library, time the entry points and check the outputs; all compute is in .fut.
// Include after the generated header.
#include "../../ref/flat/flat.h"
#include <sys/resource.h>

static struct futhark_context *ctx;
static int threads = 0, reps = 5;

static void fut_init(int argc, char **argv) {
	flatDir = argc > 1 ? argv[1] : "/home/exedev/paradigms/data/minio";
	threads = argc > 2 ? atoi(argv[2]) : 0;
	reps = argc > 3 ? atoi(argv[3]) : 5;
	struct futhark_context_config *cfg = futhark_context_config_new();
#ifdef MULTICORE
	if (threads > 0) futhark_context_config_set_num_threads(cfg, threads);
#endif
	const char *tune = getenv("FUT_TUNING");
	if (tune && *tune) {
		FILE *f = fopen(tune, "r");
		char k[256];
		long v;
		while (f && fscanf(f, "%255[^=]=%ld\n", k, &v) == 2)
			if (futhark_context_config_set_tuning_param(cfg, k, v)) fprintf(stderr, "tuning: no %s\n", k);
		if (f) fclose(f);
	}
	if (getenv("FUT_PROFILE")) futhark_context_config_set_profiling(cfg, 1);
	ctx = futhark_context_new(cfg);
}
// FUT_PROFILE=1: the runtime's per-kernel report (JSON) to stderr
static void fut_report(void) {
	if (getenv("FUT_PROFILE")) {
		char *r = futhark_context_report(ctx);
		fputs(r, stderr);
		free(r);
	}
}
static void fut_check(int err, const char *what) {
	if (err) {
		fprintf(stderr, "%s: %s\n", what, futhark_context_get_error(ctx));
		exit(1);
	}
}
#define SYNC() fut_check(futhark_context_sync(ctx), "sync")
// a flat file as a Futhark array (copied into Futhark's memory: futhark_new_* copies on c/multicore)
#define LOADER(T, CT)                                                                                        \
	static struct futhark_##T##_1d *load_##T(const char *file) {                                             \
		size_t n;                                                                                            \
		const void *p = fmap(file, &n);                                                                      \
		struct futhark_##T##_1d *a = futhark_new_##T##_1d(ctx, (const CT *)p, (int64_t)(n / sizeof(CT)));   \
		if (n) munmap((void *)p, n);                                                                         \
		return a;                                                                                            \
	}
#define LOAD(T, file) load_##T(file)

static int cmpd(const void *a, const void *b) { double x = *(const double *)a, y = *(const double *)b; return (x > y) - (x < y); }
// time `body` reps times; prints every time, then min / median / max
#define TIMEIT(name, body)                                                                                   \
	do {                                                                                                     \
		double ts_[1000];                                                                                    \
		int n_ = reps < 1000 ? reps : 1000;                                                                  \
		struct rusage ru0_, ru1_;                                                                            \
		printf("%-14s", name);                                                                               \
		getrusage(RUSAGE_SELF, &ru0_);                                                                       \
		for (int rep_ = 0; rep_ < n_; rep_++) {                                                              \
			double t0_ = now();                                                                              \
			body;                                                                                            \
			SYNC();                                                                                          \
			ts_[rep_] = (now() - t0_) * 1e3;                                                                 \
			printf(" %.1f", ts_[rep_]);                                                                      \
		}                                                                                                    \
		getrusage(RUSAGE_SELF, &ru1_);                                                                       \
		qsort(ts_, n_, sizeof(double), cmpd);                                                                \
		printf("  | min %.2f med %.2f max %.2f ms (threads %d) minflt/rep %ld\n", ts_[0], ts_[n_ / 2], ts_[n_ - 1], threads, \
		       (ru1_.ru_minflt - ru0_.ru_minflt) / n_); \
	} while (0)
