// gb.h: the little that every GraphBLAS cell shares: the flat-file reader (ref/flat/flat.h), an error check,
// and the loader helpers that turn the flat files' CSR rows into (row, col) tuples. The loaders are the only
// plain-C loops in the cells; every rule is a GraphBLAS call.
#include "../../ref/flat/flat.h"
#include <GraphBLAS.h>

#define OK(x) do { GrB_Info _i = (x); if (_i < GrB_SUCCESS) { \
	fprintf(stderr, "%s:%d: %s -> %d\n", __FILE__, __LINE__, #x, (int)_i); exit(3); } } while (0)

typedef GrB_Index ix;
static GrB_Scalar TRUE_;
static double cpuNow(void) { struct timespec t; clock_gettime(CLOCK_PROCESS_CPUTIME_ID, &t); return t.tv_sec + t.tv_nsec * 1e-9; }
static int nthreads(void) { return getenv("GB_THREADS") ? atoi(getenv("GB_THREADS")) : 1; }
static void gbInit(void) {
	OK(GrB_init(GrB_NONBLOCKING));
	OK(GrB_Global_set_INT32(GrB_GLOBAL, nthreads(), GxB_NTHREADS));
	if (getenv("GB_BURBLE")) OK(GrB_Global_set_INT32(GrB_GLOBAL, 1, GxB_BURBLE));
	OK(GrB_Scalar_new(&TRUE_, GrB_BOOL));
	OK(GrB_Scalar_setElement_BOOL(TRUE_, 1));
}
// an iso-true boolean matrix from tuples (duplicates collapse)
static GrB_Matrix boolMatrix(ix nr, ix nc, const ix *Ix, const ix *J, ix n) {
	GrB_Matrix A;
	OK(GrB_Matrix_new(&A, GrB_BOOL, nr, nc));
	OK(GxB_Matrix_build_Scalar(A, Ix, J, TRUE_, n));
	OK(GrB_Matrix_wait(A, GrB_MATERIALIZE));
	return A;
}
static GrB_Vector boolVector(ix n, const ix *Ix, ix k) {
	GrB_Vector v;
	OK(GrB_Vector_new(&v, GrB_BOOL, n));
	OK(GxB_Vector_build_Scalar(v, Ix, TRUE_, k));
	OK(GrB_Vector_wait(v, GrB_MATERIALIZE));
	return v;
}
static ix nv(GrB_Vector v) { ix n; OK(GrB_Vector_nvals(&n, v)); return n; }
static ix nm(GrB_Matrix a) { ix n; OK(GrB_Matrix_nvals(&n, a)); return n; }
// the sorted indices of a vector's entries, as u32
static u32 *indices32(GrB_Vector v, ix *n) {
	*n = nv(v);
	ix *Ix = malloc(8 * (*n + 1)), k = *n;
	OK(GrB_Vector_extractTuples_BOOL(Ix, NULL, &k, v));
	u32 *o = malloc(4 * (*n + 1));
	for (ix i = 0; i < k; i++) o[i] = (u32)Ix[i];
	free(Ix);
	return o;
}
static void writeFile(const char *path, const void *p, size_t n) {
	FILE *f = fopen(path, "wb");
	if (!f || fwrite(p, 1, n, f) != n) { perror(path); exit(2); }
	fclose(f);
}
