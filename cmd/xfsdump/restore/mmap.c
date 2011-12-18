#include <libxfs.h>
#include <unistd.h>
#include <sys/mman.h>
#include <sys/stat.h>

/*
 * our custom mmap b/c Linux doesn't have MAP_AUTOGROW
 * and for an empty file we need to write to last byte
 * to ensure it can be accessed
 */

void *
mmap_autogrow(size_t len, int fd, off_t offset)
{
    struct stat buf;
    char nul_buffer[] = "";

    /* prealloc file if it is an empty file */
    if (fstat(fd, &buf) == -1) {;
	return (void*)MAP_FAILED;
    }
    if (buf.st_size < offset+len) {
	(void)lseek(fd, offset+len-1, SEEK_SET);
	(void)write(fd, nul_buffer, 1);
    }

    return mmap( 0, len, PROT_READ | PROT_WRITE, MAP_SHARED, fd, offset );
}
