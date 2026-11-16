// flatio.c: the one escape from Goose. Goose's file API reads a whole file into a u8[>..] through a 64 KB
// bounce buffer and has no file size, no offset read and no way to see bytes as u32s. These read a file (or
// elements [first, first+count) of it) straight into a builder's stack top: goose.h's contract (C may append
// whole elements at the stack top, moving the top and the header's count together).
#define _GNU_SOURCE
#include "goose.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#pragma pack(push, 1)
typedef struct { const uint8_t *data; int64_t len; } gs_slice;
#pragma pack(pop)

static int open_path(gs_slice s) {
	char buf[4096];
	if (s.len <= 0 || s.len >= (int64_t)sizeof buf) return -1;
	memcpy(buf, s.data, (size_t)s.len);
	buf[s.len] = 0;
	return open(buf, O_RDONLY);
}

int64_t flat_size(gs_slice path) {
	int fd = open_path(path);
	if (fd < 0) return -1;
	struct stat st;
	int r = fstat(fd, &st);
	close(fd);
	return r ? -1 : (int64_t)st.st_size;
}

static uint8_t read_at(int fd, gs_rref out, int64_t at, int64_t want, int64_t elem) {
	uint8_t *dst = out.stk->top;
	int64_t got = 0;
	while (got < want) {
		ssize_t k = pread(fd, dst + got, (size_t)(want - got), at + got);
		if (k <= 0) { close(fd); return 0; }
		got += k;
	}
	close(fd);
	out.stk->top += want;
	out.hdr->len += want / elem;
	return 1;
}

// appends the whole file to a T[>..] of elem-byte elements; false (nothing appended) if its size is not a multiple
uint8_t flat_read(gs_slice path, gs_rref out, int64_t elem) {
	int fd = open_path(path);
	if (fd < 0) return 0;
	struct stat st;
	if (fstat(fd, &st) || st.st_size % elem) { close(fd); return 0; }
	return read_at(fd, out, 0, st.st_size, elem);
}

// appends elements [first, first + count) of the file
uint8_t flat_read_range(gs_slice path, gs_rref out, int64_t elem, int64_t first, int64_t count) {
	int fd = open_path(path);
	if (fd < 0) return 0;
	struct stat st;
	if (fstat(fd, &st) || first < 0 || count < 0 || (first + count) * elem > st.st_size) { close(fd); return 0; }
	return read_at(fd, out, first * elem, count * elem, elem);
}

// ---- the output's pages (ATTACKS.md 2). Goose's data stacks are 4 KB-page reservations (the runtime even
// madvises MADV_NOHUGEPAGE on a stack's first 2 MB) and the language has no say over page size. These two
// reach under it: they replace the pages above a builder's stack top, where its next n bytes will go, with
// fresh mappings at the same addresses (MAP_FIXED, as the runtime itself does to discard pages on macOS).
#include <sys/mman.h>
#include <sys/resource.h>

#define PAGE4K ((uintptr_t)4096)
#define PAGE2M ((uintptr_t)2 << 20)
static uintptr_t up(uintptr_t x, uintptr_t a) { return (x + a - 1) & ~(a - 1); }

static int fresh4k(uintptr_t s, uintptr_t e) {
	if (e <= s) return 1;
	return mmap((void *)s, e - s, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED | MAP_NORESERVE,
	            -1, 0) != MAP_FAILED;
}

// fresh, never-touched pages for the next n bytes above out's stack top: 4 KB pages, or (huge) 2 MB hugetlb pages
// from the first 2 MB boundary on. Returns the bytes on huge pages (0 for 4 KB), -1 if the OS refused.
int64_t fresh_pages(gs_rref out, int64_t n, uint8_t huge) {
	uintptr_t top = (uintptr_t)out.stk->top, s = up(top, PAGE4K), e = up(top + (uintptr_t)n, PAGE4K);
	if (!huge) return fresh4k(s, e) ? 0 : -1;
	uintptr_t hs = up(top, PAGE2M), he = up(top + (uintptr_t)n, PAGE2M);
	if (!fresh4k(s, hs < e ? hs : e)) return -1;
	if (he <= hs) return 0;
	if (mmap((void *)hs, he - hs, PROT_READ | PROT_WRITE, MAP_PRIVATE | MAP_ANONYMOUS | MAP_FIXED | MAP_HUGETLB,
	         -1, 0) == MAP_FAILED) {
		fresh4k(hs, he);       // no huge pages to give: the range is plain memory again
		return -1;
	}
	return (int64_t)(he - hs);
}

// minor page faults so far: the process, or the calling thread (a Goose worker is a thread)
int64_t minor_faults(uint8_t thread) {
	struct rusage r;
	getrusage(thread ? RUSAGE_THREAD : RUSAGE_SELF, &r);
	return (int64_t)r.ru_minflt;
}

// ---- the output file (problem 4): workers that share no memory still share the file system. Main creates the
// file at its final size; each worker writes its own bytes at its own offset (pwrite), as a linker's workers write
// their ranges of the output file.
static int open_flags(gs_slice s, int flags) {
	char buf[4096];
	if (s.len <= 0 || s.len >= (int64_t)sizeof buf) return -1;
	memcpy(buf, s.data, (size_t)s.len);
	buf[s.len] = 0;
	return open(buf, flags, 0644);
}

uint8_t flat_create(gs_slice path, int64_t size) {
	int fd = open_flags(path, O_WRONLY | O_CREAT | O_TRUNC);
	if (fd < 0) return 0;
	int r = ftruncate(fd, size);
	close(fd);
	return r == 0;
}

// writes a slice of elem-byte elements at byte offset at of an existing file
uint8_t flat_write_at(gs_slice path, gs_slice data, int64_t elem, int64_t at) {
	int fd = open_flags(path, O_WRONLY);
	if (fd < 0) return 0;
	int64_t want = data.len * elem, done = 0;
	while (done < want) {
		ssize_t k = pwrite(fd, data.data + done, (size_t)(want - done), at + done);
		if (k <= 0) { close(fd); return 0; }
		done += k;
	}
	close(fd);
	return 1;
}

// the same for a slice of another element type (a Goose slice is passed by value: one C name per type)
uint8_t flat_write_rels(gs_slice path, gs_slice data, int64_t elem, int64_t at) { return flat_write_at(path, data, elem, at); }
