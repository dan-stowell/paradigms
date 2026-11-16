// Asynchronous P2: same transfer rules and serial method phase as p2par.c.
// Build: gcc -O2 -march=native -pthread -o build-g/p2async ref/flat/p2async.c
#include "flat.h"
#include <pthread.h>

enum { fDef = 1, fText = 2, fReflect = 4, fGoType = 8, fIface = 16 };
static u8 *live, *iface, *ifaceDone;
static inline int tas(u8 *p) {
	int first = __atomic_exchange_n(p, 1, __ATOMIC_RELAXED) == 0;
	// live/iface is a two-flag handshake: exchange then load the other flag.
	// x86 locked xchg orders store->load; weak machines need a fence so both
	// sides cannot miss the other's transition. All flag accesses stay relaxed.
#if !defined(__x86_64__) && !defined(__i386__)
	__atomic_thread_fence(__ATOMIC_SEQ_CST);
#endif
	return first;
}
// shared flags inside the parallel flood: relaxed atomic loads and stores (plain ones are data races, even same-value)
#define LD(x) __atomic_load_n(&(x), __ATOMIC_RELAXED)
#define ST1(x) __atomic_store_n(&(x), 1, __ATOMIC_RELAXED)

typedef struct { u32 *a; size_t n, cap; } vec;
static inline void push(vec *v, u32 x) {
	if (v->n == v->cap) { v->cap = v->cap ? 2 * v->cap : 4096; v->a = realloc(v->a, 4 * v->cap); }
	v->a[v->n++] = x;
}

// Each published chunk and each worker holding private work owns one credit.
// Publish adds a credit before exposing work; taking a whole chunk transfers it.
// A worker retires its credit only after its private stack is empty. Thus zero
// means no queued OR executing work, including children not yet published.
typedef struct { pthread_mutex_t lock; vec stack; size_t available; } mailbox;
static mailbox *boxes;
static size_t pending;
static void publish(vec *local, int w) {
	mailbox *b = &boxes[w];
	if (local->n <= 64 || __atomic_load_n(&b->available, __ATOMIC_RELAXED)) return;
	pthread_mutex_lock(&b->lock);
	if (!b->stack.n) {
		size_t n = local->n / 2;
		if (n > 256) n = 256;
		__atomic_fetch_add(&pending, 1, __ATOMIC_RELAXED);
		while (n--) push(&b->stack, local->a[--local->n]);
		__atomic_store_n(&b->available, b->stack.n, __ATOMIC_RELAXED);
	}
	pthread_mutex_unlock(&b->lock);
}
static int take(vec *local, int victim) {
	mailbox *b = &boxes[victim];
	if (!__atomic_load_n(&b->available, __ATOMIC_RELAXED)) return 0;
	pthread_mutex_lock(&b->lock);
	int got = b->stack.n != 0;
	if (got) {
		vec tmp = *local; *local = b->stack; b->stack = tmp;
		__atomic_store_n(&b->available, 0, __ATOMIC_RELAXED);
	}
	pthread_mutex_unlock(&b->lock);
	return got;
}

typedef struct {
	int T;
	vec *next, *meth;
	int *refl;
	u32 nsl;
	const u8 *fl, *ek;
	const u32 *eoff, *ed, *moff, *mTyp;
	u8 *sigSeen, *named;
} context;
typedef struct { context *ctx; int w; u64 items, steals; } worker;
static void *flood(void *opaque) {
	worker *arg = opaque;
	int w = arg->w;
	context *ctx = arg->ctx;
	vec *nx = &ctx->next[w];
	int credit = 0;
	for (;;) {
		if (!nx->n) {
			if (credit) {
				__atomic_fetch_sub(&pending, 1, __ATOMIC_ACQ_REL);
				credit = 0;
			}
			for (int v = 0; v < ctx->T; v++) {
				int victim = (w + v) % ctx->T;
				if (take(nx, victim)) {
					credit = 1;
					arg->steals += victim != w;
					break;
				}
			}
			if (!credit) {
				if (!__atomic_load_n(&pending, __ATOMIC_ACQUIRE)) break;
				// No spinning barrier: let oversubscribed workers run.
				struct timespec delay = {0, 50000};
				nanosleep(&delay, NULL);
				continue;
			}
		}
		if (ctx->T > 1) publish(nx, w);
		u32 c = nx->a[--nx->n];
		arg->items++;
		if (c > ctx->nsl || !(ctx->fl[c] & fDef)) continue;
		if (ctx->fl[c] & fReflect) ctx->refl[w] = 1;
		int uii = (ctx->fl[c] & fGoType) && __atomic_load_n(&iface[c], __ATOMIC_RELAXED);
		if (uii && !tas(&ifaceDone[c])) continue;
		for (u32 e = ctx->eoff[c]; e < ctx->eoff[c + 1]; e++) {
			u32 t = ctx->ed[e];
			switch (ctx->ek[e]) {
			case 0:
				if (uii && (ctx->fl[t] & fGoType) && (ctx->fl[t] & fDef) && !LD(iface[t]) && tas(&iface[t])) {
					tas(&live[t]);
					push(nx, t); // (re)flood it as a type converted to an interface
				} else if (!LD(live[t]) && tas(&live[t])) push(nx, t);
				break;
			case 1:
				if (!LD(iface[t]) && tas(&iface[t]) && LD(live[t])) push(nx, t);
				break;
			case 2: if (!LD(ctx->sigSeen[t])) ST1(ctx->sigSeen[t]); break;
			case 3: if (!LD(ctx->named[t])) ST1(ctx->named[t]); break;
			}
		}
		if (uii)
			for (u32 m = ctx->moff[c]; m < ctx->moff[c + 1]; m++) {
				u32 t = ctx->mTyp[m];
				if (t && !LD(iface[t]) && tas(&iface[t]) && LD(live[t])) push(nx, t);
				push(&ctx->meth[w], m);
			}
	}
	return NULL;
}

int main(int argc, char **argv) {
	flatDir = argc > 1 ? argv[1] : "data/minio";
	u32 N = (u32)metaGet("nids"), nsl = (u32)metaGet("nslots");
	size_t nroots, nsig, nstr, nmeth;
	const u8 *fl = FMAP(u8, "p2_flags.u8", NULL), *ek = FMAP(u8, "p2_edge_kind.u8", NULL);
	const u32 *eoff = FMAP(u32, "p2_edge_off.u32", NULL), *ed = FMAP(u32, "p2_edge_dst.u32", NULL);
	const u32 *sigName = FMAP(u32, "p2_sig_name.u32", &nsig), *sigType = FMAP(u32, "p2_sig_type.u32", NULL);
	const u32 *moff = FMAP(u32, "p2_meth_off.u32", NULL), *mName = FMAP(u32, "p2_meth_name.u32", &nmeth);
	const u32 *mTyp = FMAP(u32, "p2_meth_mtyp.u32", NULL), *mIfn = FMAP(u32, "p2_meth_ifn.u32", NULL), *mTfn = FMAP(u32, "p2_meth_tfn.u32", NULL);
	const u8 *mExp = FMAP(u8, "p2_meth_exported.u8", NULL);
	const u32 *roots = FMAP(u32, "p2_roots.u32", &nroots);
	FMAP(u64, "p2_str.off.u64", &nstr);
	nroots /= 4, nsig /= 4, nmeth /= 4, nstr = nstr / 8 - 1;
	int T = getenv("OMP_NUM_THREADS") ? atoi(getenv("OMP_NUM_THREADS")) : (int)sysconf(_SC_NPROCESSORS_ONLN);
	if (T < 1 || T > 1024) { fprintf(stderr, "invalid thread count\n"); return 2; }
	live = malloc(N), iface = malloc(N), ifaceDone = malloc(N);
	u8 *sigSeen = malloc(nsig), *named = malloc(nstr + 1);
	boxes = calloc(T, sizeof(*boxes));
	for (int w = 0; w < T; w++) pthread_mutex_init(&boxes[w].lock, NULL);
	vec *next = calloc(T, sizeof(vec)), *meth = calloc(T, sizeof(vec)), fr = {0}, mk = {0};
	int *refl = calloc(T, sizeof(int));
	context ctx = {T, next, meth, refl, nsl, fl, ek, eoff, ed, moff, mTyp, sigSeen, named};
	worker *workers = calloc(T, sizeof(*workers));
	pthread_t *threads = calloc(T, sizeof(*threads));

	u64 hs = 1;
	while (hs < 4 * nsig + 64) hs <<= 1;
	u64 *sigTab = malloc(hs * 8);
	double best = 1e9, total = 0;
	u64 items = 0, steals = 0;
	size_t nlive = 0;
	int rounds = 0;
	for (int rep = 0; rep < (argc > 2 ? atoi(argv[2]) : 7); rep++) {
		memset(live, 0, N); memset(ifaceDone, 0, N); memset(sigSeen, 0, nsig); memset(named, 0, nstr + 1);
		memset(sigTab, 0, hs * 8); memset(refl, 0, T * sizeof(int));
		for (u32 g = 0; g < N; g++) iface[g] = g <= nsl && (fl[g] & fIface);
		double t0 = now();
		fr.n = mk.n = 0;
		rounds = 0;
		items = steals = 0;
		int reflectSeen = 0;
		for (size_t i = 0; i < nroots; i++) if (tas(&live[roots[i]])) push(&fr, roots[i]);
		for (;;) {
			rounds++;

			pending = 0;
			for (size_t i = 0; i < fr.n; i++) push(&boxes[i % T].stack, fr.a[i]);
			fr.n = 0;
			for (int w = 0; w < T; w++) {
				boxes[w].available = boxes[w].stack.n;
				pending += boxes[w].stack.n != 0;
			}
			for (int w = 0; w < T; w++) {
				workers[w] = (worker){.ctx = &ctx, .w = w};
				if (w && pthread_create(&threads[w], NULL, flood, &workers[w])) {
					fprintf(stderr, "pthread_create failed\n"); exit(2);
				}
			}
			flood(&workers[0]);
			// One completion/join phase per round; pthread_join blocks, not spins.
			for (int w = 0; w < T; w++) {
				if (w) pthread_join(threads[w], NULL);
				items += workers[w].items;
				steals += workers[w].steals;
			}

			for (int w = 0; w < T; w++) {
				for (size_t k = 0; k < meth[w].n; k++) push(&mk, meth[w].a[k]);
				meth[w].n = 0;
				reflectSeen |= refl[w];
			}

			// signatures seen this round into the table, then the methods something can call
			for (u32 s = 0; s < nsig; s++) {
				if (sigSeen[s] != 1) continue;
				sigSeen[s] = 2;
				u64 k = (u64)sigName[s] << 32 | sigType[s];
				for (u64 h = mix64(k) & (hs - 1);; h = (h + 1) & (hs - 1)) {
					if (sigTab[h] == 0) { sigTab[h] = k + 1; break; }
					if (sigTab[h] == k + 1) break;
				}
			}
			size_t keep = 0;
			for (size_t i = 0; i < mk.n; i++) {
				u32 m = mk.a[i];
				int call = (reflectSeen && mExp[m]) || named[mName[m]];
				if (!call) {
					u64 k = (u64)mName[m] << 32 | mTyp[m];
					for (u64 h = mix64(k) & (hs - 1); sigTab[h]; h = (h + 1) & (hs - 1))
						if (sigTab[h] == k + 1) { call = 1; break; }
				}
				if (call) {
					u32 ts[3] = {mTyp[m], mIfn[m], mTfn[m]};
					for (int d = 0; d < 3; d++) if (ts[d] && tas(&live[ts[d]])) push(&fr, ts[d]);
				} else mk.a[keep++] = m;
			}
			mk.n = keep;
			if (fr.n == 0) break;
		}
		double t1 = now();
		total += t1 - t0;
		if (t1 - t0 < best) best = t1 - t0;
	}
	nlive = 0;
	for (u32 g = 1; g < N; g++) nlive += live[g];
	u32 *list = malloc(4 * nlive), k = 0;
	for (u32 g = 1; g < N; g++) if (live[g]) list[k++] = g;
	printf("p2async (%d threads): flood best %.3f ms, mean %.3f ms, total %.6f s, %zu live, %d rounds, %d barriers, %llu items, %llu steals\n", T, best * 1e3, total * 1e3 / (argc > 2 ? atoi(argv[2]) : 7), total, nlive, rounds, rounds, (unsigned long long)items, (unsigned long long)steals);
	long bad = check32("live", list, nlive, "oracle/p2_live.u32");
	printf(bad ? "P2 FAIL\n" : "P2 OK\n");
	return bad != 0;
}
