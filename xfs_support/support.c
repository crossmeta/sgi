
#include <xfs_support/support.h>

int
is_read_only(dev_t dev)
{

	return (0);
}

int
set_blocksize(dev_t dev, int size)
{

	printf("set_blocksize: dev %x to %d ignored\n", dev, size);
	return (0);
}

