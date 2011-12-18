#include <xfs.h>
#include <stdarg.h>

int64_t
atoll(char *s)
{

	return (atol(s));
}


int64_t
strtoll(const char *s, char **ptr, int base)
{

	return ((int64_t)strtol(s, ptr, base));
}

#if 0
int
ftruncate64(int fd, int64_t size)
{

	fprintf(stderr, "ftruncate64: bug bug!!!\n");
	return (0);
}

int
fstat64(int fd, struct stat *sb)
{
	bzero(sb, sizeof (*sb));
	fprintf(stderr, "fstat64: bug bug!!!\n");
	return (0);
}
#endif


void CDECL
xfs_fs_cmn_err(int lvl, void *mp, char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

void CDECL
printk(char *fmt, ...)
{
	va_list ap;

	va_start(ap, fmt);
	vfprintf(stderr, fmt, ap);
	va_end(ap);
}

void *
memalign(int align_size, size_t size)
{

	return (libxfs_malloc(size));
}
