// flatio.c: the one escape from Goose. Goose's file API reads a whole file into a u8[>..] through a 64 KB
// bounce buffer and has no file size, no offset read and no way to see bytes as u32s. These read a file (or
// elements [first, first+count) of it) straight into a builder's stack top: goose.h's contract (C may append
// whole elements at the stack top, moving the top and the header's count together).
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
