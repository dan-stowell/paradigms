// The one escape: file size and a direct read into a Goose builder (goose.h's contract: C may append whole
// elements at the stack top, moving the top and the header's count together).
#include "goose.h"
#include <fcntl.h>
#include <sys/stat.h>
#include <unistd.h>

#pragma pack(push, 1)
typedef struct { const uint8_t *data; int64_t len; } gs_slice;
#pragma pack(pop)

static int open_path(gs_slice s) {
	const uint8_t *p = s.data; int64_t n = s.len;
	char buf[4096];
	if (n <= 0 || n >= (int64_t)sizeof buf) return -1;
	memcpy(buf, p, (size_t)n);
	buf[n] = 0;
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

// appends the file's bytes to a T[>..] of elem-byte elements; false (nothing appended) if the size is not a multiple
uint8_t flat_read(gs_slice path, gs_rref out, int64_t elem) {
	int fd = open_path(path);
	if (fd < 0) return 0;
	struct stat st;
	if (fstat(fd, &st) || st.st_size % elem) { close(fd); return 0; }
	uint8_t *dst = out.stk->top;
	int64_t want = st.st_size, got = 0;
	while (got < want) {
		ssize_t k = read(fd, dst + got, (size_t)(want - got));
		if (k <= 0) { close(fd); return 0; }
		got += k;
	}
	close(fd);
	out.stk->top += want;
	out.hdr->len += want / elem;
	return 1;
}
