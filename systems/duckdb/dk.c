// dk.c: a thin host for DuckDB's C API. It does I/O only: it shows the flat files to DuckDB as Arrow arrays
// (the raw little-endian columns already are Arrow buffers, so this is zero-copy: no parsing, no conversion),
// runs a SQL script statement by statement with a clock on each, and writes query results out as raw arrays
// or into an image file. All of the problems' logic is in the .sql files.
//
//   dk <flatdir> <threads> script.sql...
//
// Script directives (a line starting with '.', everything else is SQL; a statement ends with ';' at line end):
//   .table NAME ROWS col=file[@skip] ...   an Arrow view NAME_v over the files, then CREATE TABLE NAME AS FROM NAME_v.
//                                           ROWS: a number or a meta.txt key, optionally +k / -k. col=# is the row index
//                                           (u32 0..ROWS-1). file@1 starts at element 1 (the "next" of a CSR offset)
//   .view NAME ROWS col=file ...            only the Arrow view (scan it once: an Arrow stream is consumed by a scan)
//   .cell NAME                              start a named section; its statement times are summed and printed
//   .dump FILE QUERY;                       write column 0 of each row, raw, little-endian, at its own width
//   .repeat / .until QUERY;                 run the statements between them until QUERY's first value is 0
//   .image SIZE / .blob QUERY; / .int QUERY; / .imgout FILE
//                                           an image buffer; .blob rows (pos, bytes) are copied to pos; .int rows
//                                           (pos, width, value) are written little-endian; .imgout writes it
// meta.txt's keys are also DuckDB variables: getvariable('nslots').
#define _GNU_SOURCE
#include "duckdb.h"
#include <fcntl.h>
#include <stdint.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/mman.h>
#include <sys/resource.h>
#include <sys/stat.h>
#include <time.h>
#include <unistd.h>

// ---- the Arrow C data interface (https://arrow.apache.org/docs/format/CDataInterface.html)
struct ArrowSchema {
	const char *format, *name, *metadata;
	int64_t flags, n_children;
	struct ArrowSchema **children, *dictionary;
	void (*release)(struct ArrowSchema *);
	void *private_data;
};
struct ArrowArray {
	int64_t length, null_count, offset, n_buffers, n_children;
	const void **buffers;
	struct ArrowArray **children, *dictionary;
	void (*release)(struct ArrowArray *);
	void *private_data;
};
struct ArrowArrayStream {
	int (*get_schema)(struct ArrowArrayStream *, struct ArrowSchema *);
	int (*get_next)(struct ArrowArrayStream *, struct ArrowArray *);
	const char *(*get_last_error)(struct ArrowArrayStream *);
	void (*release)(struct ArrowArrayStream *);
	void *private_data;
};

static const char *flatDir;
static double now(void) {
	struct timespec t;
	clock_gettime(CLOCK_MONOTONIC, &t);
	return t.tv_sec + t.tv_nsec * 1e-9;
}
static const void *fmap(const char *name, size_t *n) {
	char p[4096];
	snprintf(p, sizeof p, "%s/%s", flatDir, name);
	int fd = open(p, O_RDONLY);
	if (fd < 0) { perror(p); exit(2); }
	struct stat st;
	fstat(fd, &st);
	*n = (size_t)st.st_size;
	if (*n == 0) { close(fd); return ""; }
	void *m = mmap(NULL, *n, PROT_READ, MAP_PRIVATE | MAP_POPULATE, fd, 0);
	if (m == MAP_FAILED) { perror(p); exit(2); }
	close(fd);
	return m;
}

// ---- meta.txt
static char metaK[64][64];
static unsigned long long metaV[64];
static int nmeta;
static void readMeta(void) {
	char p[4096];
	snprintf(p, sizeof p, "%s/meta.txt", flatDir);
	FILE *f = fopen(p, "r");
	if (!f) { perror(p); exit(2); }
	while (nmeta < 64 && fscanf(f, "%63s %llu", metaK[nmeta], &metaV[nmeta]) == 2) nmeta++;
	fclose(f);
}
static long long rowsOf(const char *s) {
	char k[64];
	long long add = 0;
	const char *pm = strpbrk(s, "+-");
	size_t n = pm ? (size_t)(pm - s) : strlen(s);
	if (pm) add = atoll(pm);
	snprintf(k, sizeof k, "%.*s", (int)n, s);
	if (k[0] >= '0' && k[0] <= '9') return atoll(k) + add;
	for (int i = 0; i < nmeta; i++)
		if (!strcmp(metaK[i], k)) return (long long)metaV[i] + add;
	fprintf(stderr, "meta: no %s\n", k);
	exit(2);
}

// ---- a table of columns, served as a stream of record batches (struct arrays) that all point into the mmaps
enum { BATCH = 122880, STANDARD_VECTOR_SIZE_DK = 2048 }; // DuckDB's row group size: one batch per row group
typedef struct {
	char name[64];
	const char *fmt; // Arrow format of the column
	const void *data, *bin; // data: values (or u64 offsets for strings); bin: string bytes
	struct ArrowArray *lchild, *lchildp[1]; // a list column (a CSR): the struct array of all its values
} col;
typedef struct {
	int ncol;
	col c[32];
	int64_t rows, next;
	struct ArrowSchema top, kids[32], grandkids[32], *kidp[32], *gkp[32][1];
} table;
static void noRelS(struct ArrowSchema *s) { s->release = NULL; }
static void noRelA(struct ArrowArray *a) { a->release = NULL; }
static void freeBatch(struct ArrowArray *a) {
	free(a->private_data);
	a->release = NULL;
}
static int getSchema(struct ArrowArrayStream *st, struct ArrowSchema *out) {
	table *t = st->private_data;
	*out = t->top;
	out->release = noRelS;
	return 0;
}
static int getNext(struct ArrowArrayStream *st, struct ArrowArray *out) {
	table *t = st->private_data;
	memset(out, 0, sizeof *out);
	if (t->next >= t->rows) return 0; // release == NULL: the end of the stream
	int64_t off = t->next, len = t->rows - off < BATCH ? t->rows - off : BATCH;
	t->next += len;
	// one allocation per batch: the struct's children, their buffers, and the u64x2 lists' children
	struct mem {
		struct ArrowArray kid[32];
		struct ArrowArray *kidp[32];
		const void *buf[32][3], *top[1];
	} *m = calloc(1, sizeof *m);
	for (int i = 0; i < t->ncol; i++) {
		col *c = &t->c[i];
		struct ArrowArray *a = &m->kid[i];
		a->length = len, a->offset = off, a->release = noRelA;
		m->buf[i][0] = NULL;
		if (c->lchild) { // the offsets are the CSR file itself; the values array is shared by every batch
			a->n_buffers = 2, a->n_children = 1;
			m->buf[i][1] = c->data;
			a->children = c->lchildp;
		} else if (c->bin) {
			a->n_buffers = 3;
			m->buf[i][1] = c->data, m->buf[i][2] = c->bin;
		} else {
			a->n_buffers = 2;
			m->buf[i][1] = c->data;
		}
		a->buffers = m->buf[i];
		m->kidp[i] = a;
	}
	m->top[0] = NULL;
	*out = (struct ArrowArray){.length = len, .n_buffers = 1, .n_children = t->ncol, .buffers = m->top, .children = m->kidp,
	                           .release = freeBatch, .private_data = m};
	return 0;
}
static const char *lastErr(struct ArrowArrayStream *st) { (void)st; return "dk stream"; }
static void relStream(struct ArrowArrayStream *st) { st->release = NULL; }

static duckdb_connection con;
static void die(const char *what, const char *err) {
	fprintf(stderr, "%s: %s\n", what, err ? err : "?");
	exit(1);
}
// a primitive column: FILE (its suffix is its type), FILE@k (from element k), FILE.u64x2:lo / :hi (one half, copied:
// Arrow has no strided arrays). *n: its element count
static const void *loadPrim(const char *spec, const char **fmt, size_t *n) {
	char file[512];
	snprintf(file, sizeof file, "%s", spec);
	long skip = 0;
	char *at = strchr(file, '@');
	if (at) *at = 0, skip = atol(at + 1);
	char *half = strstr(file, ":lo") ? strstr(file, ":lo") : strstr(file, ":hi");
	if (half) {
		int hi = half[1] == 'h';
		*half = 0;
		const uint64_t *src = fmap(file, n);
		*n /= 16;
		uint64_t *dst = malloc(8 * *n);
		for (size_t k = 0; k < *n; k++) dst[k] = src[2 * k + hi];
		*fmt = "L";
		return dst;
	}
	const char *suf = strrchr(file, '.') + 1;
	int es = 0;
	if (!strcmp(suf, "u8")) *fmt = "C", es = 1;
	else if (!strcmp(suf, "u16")) *fmt = "S", es = 2;
	else if (!strcmp(suf, "u32")) *fmt = "I", es = 4;
	else if (!strcmp(suf, "i32")) *fmt = "i", es = 4;
	else if (!strcmp(suf, "i64")) *fmt = "l", es = 8;
	else if (!strcmp(suf, "u64")) *fmt = "L", es = 8;
	else if (!strcmp(suf, "u64x2")) *fmt = "d:38,0", es = 16; // a 128-bit key: read as DECIMAL(38,0), i.e. a HUGEINT (k2 << 64 | k1)
	else die("type", file);
	const char *d = fmap(file, n);
	*n = *n / es - skip;
	return d + skip * es;
}

// .table / .view: words[0] = NAME, words[1] = ROWS, then col=file
static void makeView(char **w, int nw) {
	table *t = calloc(1, sizeof *t);
	t->rows = strcmp(w[1], "*") ? rowsOf(w[1]) : -1; // *: as many as the first file has
	t->top = (struct ArrowSchema){.format = "+s", .name = "", .n_children = nw - 2, .children = t->kidp, .release = noRelS};
	for (int i = 2; i < nw; i++) {
		col *c = &t->c[t->ncol];
		struct ArrowSchema *s = &t->kids[t->ncol];
		char *eq = strchr(w[i], '=');
		snprintf(c->name, sizeof c->name, "%.*s", (int)(eq - w[i]), w[i]);
		char file[512];
		snprintf(file, sizeof file, "%s", eq + 1);
		long skip = 0;
		size_t n;
		char *br = strchr(file, '[');
		if (br) { // name=OFF.u32[f1=file,f2=file,...]: a list of structs over a CSR (Arrow's list: offsets + values)
			*br = 0;
			char *fields = br + 1;
			fields[strcspn(fields, "]")] = 0;
			c->data = (const char *)fmap(file, &n) + skip * 4;
			n = n / 4 - skip;
			if (t->rows < 0) t->rows = (int64_t)n - 1;
			if ((int64_t)n < t->rows + 1) { fprintf(stderr, "%s: %zu offsets < %lld rows + 1\n", file, n, (long long)t->rows); exit(2); }
			int nf = 0;
			struct ArrowSchema *fs = calloc(16, sizeof *fs), **fsp = calloc(16, sizeof *fsp), *ss = calloc(1, sizeof *ss), **ssp = calloc(1, sizeof *ssp);
			struct ArrowArray *fa = calloc(16, sizeof *fa), **fap = calloc(16, sizeof *fap), *sa = calloc(1, sizeof *sa);
			const void **fbuf = calloc(32, sizeof *fbuf), **sbuf = calloc(1, sizeof *sbuf);
			int64_t total = -1;
			char *sv;
			for (char *f = strtok_r(fields, ",", &sv); f; f = strtok_r(NULL, ",", &sv), nf++) {
				char *e = strchr(f, '=');
				*e = 0;
				size_t fn;
				const char *ffmt;
				if (!strcmp(e + 1, "#")) { // the value's index in the whole values array (a relocation's number)
					fn = ((const uint32_t *)c->data)[n - 1];
					uint32_t *iota = malloc(4 * fn);
					for (size_t r = 0; r < fn; r++) iota[r] = (uint32_t)r;
					fbuf[2 * nf + 1] = iota, ffmt = "I";
				} else fbuf[2 * nf + 1] = loadPrim(e + 1, &ffmt, &fn);
				if (total < 0 || (int64_t)fn < total) total = (int64_t)fn;
				fs[nf] = (struct ArrowSchema){.format = ffmt, .name = strdup(f), .release = noRelS};
				fsp[nf] = &fs[nf];
				fa[nf] = (struct ArrowArray){.n_buffers = 2, .buffers = &fbuf[2 * nf], .release = noRelA};
				fap[nf] = &fa[nf];
			}
			for (int k = 0; k < nf; k++) fa[k].length = total;
			*ss = (struct ArrowSchema){.format = "+s", .name = "", .n_children = nf, .children = fsp, .release = noRelS};
			*ssp = ss;
			*sa = (struct ArrowArray){.length = total, .n_buffers = 1, .buffers = sbuf, .n_children = nf, .children = fap, .release = noRelA};
			c->lchild = sa, c->lchildp[0] = sa, c->fmt = "+l";
			s->n_children = 1, s->children = ssp;
		} else if (!strcmp(file, "#")) {
			if (t->rows < 0) die("# needs a row count", w[0]);
			uint32_t *iota = malloc(4 * t->rows);
			for (int64_t r = 0; r < t->rows; r++) iota[r] = (uint32_t)r;
			c->data = iota, c->fmt = "I";
		} else if (strstr(file, ".off.u64")) { // strings: P.off.u64 + P.bin is Arrow's large binary
			char bin[512];
			snprintf(bin, sizeof bin, "%.*s.bin", (int)(strstr(file, ".off.u64") - file), file);
			size_t nb;
			c->data = fmap(file, &n), c->bin = fmap(bin, &nb), c->fmt = "Z";
			n = n / 8 - 1; // count+1 offsets
			if (t->rows < 0) t->rows = (int64_t)n;
		} else {
			c->data = loadPrim(w[i] + (eq + 1 - w[i]), &c->fmt, &n);
			if (t->rows < 0) t->rows = (int64_t)n;
			if ((int64_t)n < t->rows) { fprintf(stderr, "%s: %zu elements < %lld rows\n", file, n, (long long)t->rows); exit(2); }
		}
		s->format = c->fmt, s->name = c->name, s->flags = 0, s->release = noRelS;
		t->kidp[t->ncol] = s;
		t->ncol++;
	}
	struct ArrowArrayStream *st = calloc(1, sizeof *st);
	*st = (struct ArrowArrayStream){getSchema, getNext, lastErr, relStream, t};
	char vn[128];
	snprintf(vn, sizeof vn, "%s_v", w[0]);
	if (duckdb_arrow_scan(con, vn, (duckdb_arrow_stream)st) != DuckDBSuccess) die("arrow_scan", vn);
}

// ---- running statements
static double cellMs[64], cellRep[64][256];
static long cellFlt[64];
static char cellName[64][64];
static int ncell = -1, cellCount; // ncell: the current cell (-1: none)
static double loadMs, stmtMs[4096];
static int curStmt = -1;
static int quiet;
static duckdb_result run(const char *sql, int keep) {
	duckdb_result r;
	struct rusage ru0, ru1;
	getrusage(RUSAGE_SELF, &ru0);
	double t0 = now();
	if (duckdb_query(con, sql, &r) != DuckDBSuccess) {
		fprintf(stderr, "SQL error: %s\n  in: %.300s\n", duckdb_result_error(&r), sql);
		exit(1);
	}
	double ms = (now() - t0) * 1e3;
	getrusage(RUSAGE_SELF, &ru1);
	if (ncell >= 0) cellMs[ncell] += ms, cellFlt[ncell] += ru1.ru_minflt - ru0.ru_minflt;
	if (curStmt >= 0) stmtMs[curStmt] += ms;
	if (!quiet) {
		const char *p = sql;
		while (*p == ' ' || *p == '\n' || *p == '\t') p++;
		int n = (int)strcspn(p, "\n");
		printf("  %9.2f ms  %.*s\n", ms, n > 90 ? 90 : n, p);
	}
	if (!keep) duckdb_destroy_result(&r);
	return r;
}
static uint64_t firstValue(const char *sql) {
	duckdb_result r = run(sql, 1);
	uint64_t v = (uint64_t)duckdb_value_int64(&r, 0, 0);
	duckdb_destroy_result(&r);
	return v;
}
static int widthOf(duckdb_type ty) {
	switch (ty) {
	case DUCKDB_TYPE_BOOLEAN: case DUCKDB_TYPE_TINYINT: case DUCKDB_TYPE_UTINYINT: return 1;
	case DUCKDB_TYPE_SMALLINT: case DUCKDB_TYPE_USMALLINT: return 2;
	case DUCKDB_TYPE_INTEGER: case DUCKDB_TYPE_UINTEGER: return 4;
	case DUCKDB_TYPE_BIGINT: case DUCKDB_TYPE_UBIGINT: return 8;
	default: return 0;
	}
}
// .dump: column 0, raw, at its own width (a NULL writes 0)
static void dump(const char *file, const char *sql) {
	duckdb_result r = run(sql, 1);
	int w = widthOf(duckdb_column_type(&r, 0));
	if (!w) die("dump: column type", file);
	FILE *f = fopen(file, "wb");
	if (!f) { perror(file); exit(2); }
	size_t rows = 0;
	duckdb_data_chunk ch;
	while ((ch = duckdb_fetch_chunk(r))) {
		idx_t n = duckdb_data_chunk_get_size(ch);
		duckdb_vector v = duckdb_data_chunk_get_vector(ch, 0);
		uint8_t *d = duckdb_vector_get_data(v);
		uint64_t *valid = duckdb_vector_get_validity(v);
		if (valid)
			for (idx_t i = 0; i < n; i++)
				if (!duckdb_validity_row_is_valid(valid, i)) memset(d + i * w, 0, w);
		fwrite(d, w, n, f);
		rows += n;
		duckdb_destroy_data_chunk(&ch);
	}
	fclose(f);
	duckdb_destroy_result(&r);
	printf("  dumped %zu rows to %s\n", rows, file);
}
static uint8_t *img;
static size_t imgSize, imgMapped;
// a column of a chunk as u64s (any unsigned/signed integer width)
static void col64(duckdb_vector v, idx_t n, uint64_t *out) {
	duckdb_logical_type lt = duckdb_vector_get_column_type(v);
	int w = widthOf(duckdb_get_type_id(lt));
	duckdb_destroy_logical_type(&lt);
	void *d = duckdb_vector_get_data(v);
	switch (w) {
	case 1: for (idx_t i = 0; i < n; i++) out[i] = ((uint8_t *)d)[i]; break;
	case 2: for (idx_t i = 0; i < n; i++) out[i] = ((uint16_t *)d)[i]; break;
	case 4: for (idx_t i = 0; i < n; i++) out[i] = ((uint32_t *)d)[i]; break;
	case 8: memcpy(out, d, 8 * n); break;
	default: die("image: column type", "");
	}
}
// .blob rows (pos, bytes) / .int rows (pos, width, value), pos relative to the image start. This is the one
// place dk writes on the query's behalf: SQL has no "store at byte offset", so the rows are the scatter's
// instructions and this loop is a memcpy per row.
static void scatter(const char *sql, int ints) {
	duckdb_result r = run(sql, 1);
	double t0 = now();
	struct rusage ru0, ru1;
	getrusage(RUSAGE_SELF, &ru0);
	duckdb_data_chunk ch;
	uint64_t pos[STANDARD_VECTOR_SIZE_DK], wv[STANDARD_VECTOR_SIZE_DK], val[STANDARD_VECTOR_SIZE_DK];
	while ((ch = duckdb_fetch_chunk(r))) {
		idx_t n = duckdb_data_chunk_get_size(ch);
		col64(duckdb_data_chunk_get_vector(ch, 0), n, pos);
		if (ints) {
			col64(duckdb_data_chunk_get_vector(ch, 1), n, wv);
			col64(duckdb_data_chunk_get_vector(ch, 2), n, val);
			for (idx_t i = 0; i < n; i++) {
				if (pos[i] + wv[i] > imgSize) die("image", "int out of range");
				memcpy(img + pos[i], &val[i], wv[i]);
			}
		} else {
			duckdb_string_t *s = (duckdb_string_t *)duckdb_vector_get_data(duckdb_data_chunk_get_vector(ch, 1));
			for (idx_t i = 0; i < n; i++) {
				uint32_t len = duckdb_string_t_length(s[i]);
				if (pos[i] + len > imgSize) die("image", "blob out of range");
				memcpy(img + pos[i], duckdb_string_t_data(&s[i]), len);
			}
		}
		duckdb_destroy_data_chunk(&ch);
	}
	duckdb_destroy_result(&r);
	getrusage(RUSAGE_SELF, &ru1);
	if (ncell >= 0) cellMs[ncell] += (now() - t0) * 1e3, cellFlt[ncell] += ru1.ru_minflt - ru0.ru_minflt;
}

// ---- the script
static char *readFile(const char *p) {
	FILE *f = fopen(p, "rb");
	if (!f) { perror(p); exit(2); }
	fseek(f, 0, SEEK_END);
	long n = ftell(f);
	fseek(f, 0, SEEK_SET);
	char *s = malloc(n + 1);
	if (fread(s, 1, n, f) != (size_t)n) exit(2);
	s[n] = 0;
	fclose(f);
	return s;
}
typedef struct {
	char *dir, *arg;
	int line;
} stmt; // dir == NULL: SQL; else a directive with its argument
static stmt prog[4096];
static int nprog;
static void parse(char *s) {
	char *sql = NULL;
	size_t sqlLen = 0;
	for (char *line = strtok(s, "\n"); line; line = strtok(NULL, "\n")) {
		char *p = line;
		while (*p == ' ' || *p == '\t') p++;
		if (!sql && *p == '.') {
			char *sp = strchr(p, ' ');
			stmt *st = &prog[nprog++];
			st->dir = strndup(p + 1, sp ? (size_t)(sp - p - 1) : strlen(p + 1));
			st->arg = strdup(sp ? sp + 1 : "");
			// a directive that takes a query continues to the ';'
			if ((!strcmp(st->dir, "dump") || !strcmp(st->dir, "until") || !strcmp(st->dir, "blob") || !strcmp(st->dir, "int") || !strcmp(st->dir, "show")) &&
			    !strchr(st->arg, ';')) {
				sqlLen = strlen(st->arg);
				sql = realloc(st->arg, sqlLen + 2), sql[sqlLen++] = '\n', sql[sqlLen] = 0, st->arg = sql;
				st->line = -1; // collecting
			}
			continue;
		}
		if (!sql && (*p == 0 || (p[0] == '-' && p[1] == '-'))) continue;
		size_t n = strlen(line);
		if (!sql) { sql = malloc(1), sql[0] = 0, sqlLen = 0; prog[nprog++] = (stmt){NULL, sql, 0}; }
		sql = realloc(sql, sqlLen + n + 2);
		memcpy(sql + sqlLen, line, n);
		sqlLen += n;
		sql[sqlLen++] = '\n', sql[sqlLen] = 0;
		prog[nprog - 1].arg = sql;
		// a statement ends at a ';' that ends a line (comments after it are allowed)
		char *e = line + n;
		while (e > line && (e[-1] == ' ' || e[-1] == '\t')) e--;
		char *cm = strstr(line, "--");
		if (cm) { e = cm; while (e > line && (e[-1] == ' ' || e[-1] == '\t')) e--; }
		if (e > line && e[-1] == ';') sql = NULL;
	}
}
static int split(char *s, char **w) {
	int n = 0;
	for (char *t = strtok(s, " \t\n"); t && n < 40; t = strtok(NULL, " \t\n")) w[n++] = t;
	return n;
}
static void exec(int from, int to) {
	for (int i = from; i < to; i++) {
		stmt *st = &prog[i];
		curStmt = i;
		if (!st->dir) { run(st->arg, 0); continue; }
		char *d = st->dir, *a = strdup(st->arg), *w[40];
		if (!strcmp(d, "table") || !strcmp(d, "view")) {
			char nm[128];
			int nw = split(a, w);
			snprintf(nm, sizeof nm, "%s", w[0]);
			double t0 = now();
			makeView(w, nw);
			if (!strcmp(d, "table")) {
				char q[512];
				snprintf(q, sizeof q, "CREATE OR REPLACE TABLE %s AS FROM %s_v", nm, nm);
				int saved = ncell;
				ncell = -1;
				run(q, 0);
				ncell = saved;
			}
			loadMs += (now() - t0) * 1e3;
		} else if (!strcmp(d, "cell")) {
			for (char *e = a + strlen(a); e > a && (e[-1] == ' ' || e[-1] == '\n'); e--) e[-1] = 0;
			for (ncell = 0; ncell < cellCount && strcmp(cellName[ncell], a); ncell++) {}
			if (ncell == cellCount) cellCount++;
			snprintf(cellName[ncell], 64, "%s", a);
			if (!quiet) printf("-- %s\n", a);
		} else if (!strcmp(d, "nocell")) {
			ncell = -1;
		} else if (!strcmp(d, "dump")) {
			int saved = ncell;
			ncell = -1;
			char *sp = strchr(a, ' ');
			*sp = 0;
			dump(a, sp + 1);
			ncell = saved;
		} else if (!strcmp(d, "repeat")) {
			int depth = 1, j = i + 1;
			for (; j < to; j++) {
				if (prog[j].dir && !strcmp(prog[j].dir, "repeat")) depth++;
				if (prog[j].dir && !strcmp(prog[j].dir, "until") && --depth == 0) break;
			}
			int rounds = 0, q = quiet;
			for (;;) {
				rounds++;
				exec(i + 1, j);
				quiet = 1; // print the first round only
				uint64_t v = firstValue(prog[j].arg);
				if (v == 0) break;
			}
			quiet = q;
			printf("  (%d rounds)\n", rounds);
			i = j;
		} else if (!strcmp(d, "image")) {
			imgSize = (size_t)rowsOf(a);
			if (img) munmap(img, imgMapped);
			// a fresh anonymous mapping each time (its page faults land in the cell that writes it); DK_THP=1: on
			// 2 MB huge pages (MAP_HUGETLB, needs vm.nr_hugepages), ATTACKS 2
			size_t hp = 2u << 20;
			imgMapped = (imgSize + hp - 1) / hp * hp;
			img = MAP_FAILED;
			if (getenv("DK_THP")) img = mmap(NULL, imgMapped, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_HUGETLB, -1, 0);
			if (img == MAP_FAILED) img = mmap(NULL, imgMapped, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS, -1, 0);
			if (img == MAP_FAILED) die("image", "mmap");
		} else if (!strcmp(d, "blob") || !strcmp(d, "int")) {
			scatter(a, d[0] == 'i');
		} else if (!strcmp(d, "imgout")) {
			for (char *e = a + strlen(a); e > a && (e[-1] == ' ' || e[-1] == '\n'); e--) e[-1] = 0;
			FILE *f = fopen(a, "wb");
			fwrite(img, 1, imgSize, f);
			fclose(f);
		} else if (!strcmp(d, "echo")) {
			printf("%s\n", a);
		} else if (!strcmp(d, "show")) { // print a small result
			duckdb_result r = run(a, 1);
			idx_t nc = duckdb_column_count(&r), nr = duckdb_row_count(&r);
			for (idx_t y = 0; y < nr && y < 50; y++) {
				printf("    ");
				for (idx_t x = 0; x < nc; x++) {
					char *v = duckdb_value_varchar(&r, x, y);
					printf("%s%s", x ? " | " : "", v ? v : "NULL");
					duckdb_free(v);
				}
				printf("\n");
			}
			duckdb_destroy_result(&r);
		} else {
			die("unknown directive", d);
		}
		free(a);
	}
}

int main(int argc, char **argv) {
	if (argc < 4) { fprintf(stderr, "usage: dk flatdir threads script.sql...\n"); return 2; }
	double t0 = now();
	setvbuf(stdout, NULL, _IOLBF, 0);
	flatDir = argv[1];
	readMeta();
	duckdb_database db;
	if (duckdb_open(NULL, &db) != DuckDBSuccess) die("open", "");
	if (duckdb_connect(db, &con) != DuckDBSuccess) die("connect", "");
	char q[256];
	snprintf(q, sizeof q, "SET threads = %d", atoi(argv[2]));
	quiet = 1;
	run(q, 0);
	for (int i = 0; i < nmeta; i++) {
		snprintf(q, sizeof q, "SET VARIABLE %s = %llu::UBIGINT", metaK[i], metaV[i]);
		run(q, 0);
	}
	snprintf(q, sizeof q, "SET VARIABLE flatdir = '%s'", flatDir);
	run(q, 0);
	quiet = getenv("DK_QUIET") != NULL;
	// dk flat threads once.sql... [-- repeated.sql...]: the scripts after "--" run DK_REPS times (default 1), so a
	// cell can be timed over many in-process runs (CHARTER: make the work meaty, report the range)
	int reps = getenv("DK_REPS") ? atoi(getenv("DK_REPS")) : 1, a = 3;
	if (reps < 1) reps = 1;
	if (reps > 256) reps = 256;
	for (; a < argc && strcmp(argv[a], "--"); a++) {
		int from = nprog;
		parse(readFile(argv[a]));
		exec(from, nprog);
	}
	int from = nprog;
	for (a++; a < argc; a++) parse(readFile(argv[a]));
	int nrep = 0, before0[64] = {0};
	long flt0[64] = {0};
	if (nprog > from) {
		for (int rep = 0; rep < reps; rep++) {
			double before[64];
			for (int c = 0; c < 64; c++) before[c] = cellMs[c];
			if (rep) quiet = 1;
			exec(from, nprog);
			for (int c = 0; c < cellCount; c++) cellRep[c][rep] = cellMs[c] - before[c];
			if (!rep) for (int c = 0; c < cellCount; c++) before0[c] = before[c] != cellMs[c], flt0[c] = cellFlt[c];
		}
		nrep = reps;
	}
	printf("load (arrow views + CREATE TABLE): %.1f ms\n", loadMs);
	for (int c = 0; c < cellCount; c++) {
		if (nrep > 1 && before0[c]) {
			double *x = cellRep[c];
			for (int i = 1; i < nrep; i++) for (int j = i; j > 0 && x[j] < x[j - 1]; j--) { double y = x[j]; x[j] = x[j - 1]; x[j - 1] = y; }
			printf("cell %s: min %.1f median %.1f max %.1f ms over %d reps; %ld minor faults in rep 1, %ld per later rep\n", cellName[c], x[0], x[nrep / 2],
			       x[nrep - 1], nrep, flt0[c], (cellFlt[c] - flt0[c]) / (nrep - 1));
		} else printf("cell %s: %.1f ms; %ld minor faults\n", cellName[c], cellMs[c], cellFlt[c]);
	}
	if (getenv("DK_STMTS")) // every statement's total time over all rounds and reps
		for (int i = 0; i < nprog; i++)
			if (stmtMs[i] >= 1) {
				const char *p = prog[i].arg;
				while (*p == ' ' || *p == '\n') p++;
				printf("  stmt %9.1f ms  %s%.*s\n", stmtMs[i], prog[i].dir ? "." : "", (int)(strcspn(p, "\n") > 80 ? 80 : strcspn(p, "\n")), p);
			}
	printf("total process: %.1f ms\n", (now() - t0) * 1e3);
	return 0;
}
