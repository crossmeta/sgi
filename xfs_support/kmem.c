/*
 * Copyright (c) 2000 Silicon Graphics, Inc.  All Rights Reserved.
 * 
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of version 2 of the GNU General Public License as
 * published by the Free Software Foundation.
 * 
 * This program is distributed in the hope that it would be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.
 * 
 * Further, this software is distributed without any warranty that it is
 * free of the rightful claim of any third person regarding infringement
 * or the like.  Any license provided herein, whether implied or
 * otherwise, applies only to this software file.  Patent licenses, if
 * any, provided herein do not apply to combinations of this program with
 * other software, or any other product whatsoever.
 * 
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write the Free Software Foundation, Inc., 59
 * Temple Place - Suite 330, Boston MA 02111-1307, USA.
 * 
 * Contact information: Silicon Graphics, Inc., 1600 Amphitheatre Pkwy,
 * Mountain View, CA  94043, or:
 * 
 * http://www.sgi.com 
 * 
 * For further information regarding this notice, see: 
 * 
 * http://oss.sgi.com/projects/GenInfo/SGIGPLNoticeExplan/
 * 
 * CROSSMETA Windows porting changes.
 * Copyright (c) 2001 Supramani Sammandam.  suprasam _at_ crossmeta.org
 */

#include <ntifs.h>
#include <sys/param.h>
#include <sys/errno.h>
#include <sys/kern_svcs.h>
#include <xfs_support/kmem.h>
#include <xfs_support/debug.h>

#undef kmem_alloc
#undef kmem_free
#define	SHAKE_COUNT 16
#define MAX_SLAB_SIZE 0x20000

lock_t		kmem_lock;
STATIC int	kmem_waitcnt = 0;
event_t		kmem_freemem_wait;
int		kmem_zone_count = 32;

kmem_zone_t	zone_data;

void
kmem_init(void)
{
	LOCK_INIT(&kmem_lock);
	EVENT_INIT(&kmem_freemem_wait);

	switch (MmQuerySystemSize()) {
	case MmSmallSystem:
		kmem_zone_count = 8;
		break;
	case MmMediumSystem:
		kmem_zone_count = 16;
		break;

	case MmLargeSystem:
		kmem_zone_count = 32;
		break;
	}
}

static void
kmem_shake(int count, int line)
{
	int	shaker = 0;			/* Extreme prune */

	if (count > 1)
		shaker = (SHAKE_COUNT * 256) / count;

	/*
	 * Try pruning the i/dcache to free up some space ...
	 */
	printf("kmem_shake(%4d): pruning i/dcache @ line #%d[%d]\n",
						shaker, line, count);
#if 0

	lock_kernel();

	prune_dcache(shaker);
	prune_icache(shaker);

	unlock_kernel();
#endif
}

static void
kmem_shake_zone(kmem_zone_t *zone, int count, int line)
{
	int	shaker = 0;			/* Extreme prune */

	if (count > 1)
		shaker = (SHAKE_COUNT * 256) / count;

	/*
	 * Try pruning the i/dcache to free up some space ...
	 */
	printf("kmem_shake_zone(%4d): pruning zone 0x%p @ line #%d[%d]\n",
						shaker, zone, line, count);
#if 0
	lock_kernel();

	prune_dcache(shaker);
	prune_icache(shaker);

	unlock_kernel();
#endif
}

void *
kmem_realloc(void *ptr, size_t newsize, size_t oldsize, int flags)
{
	void *new;

	new = kmem_alloc(newsize, M_XFS, flags);
	if (ptr) {
		memcpy(new, ptr, ((oldsize < newsize) ? oldsize : newsize));
		kmem_free(ptr);
	}
	return (new);
}

#if 0
kmem_zone_t *
kmem_zone_init(int size, char *zone_name)
{
	kmem_zone_t *zp = NULL;
	caddr_t buf, p, prev;
	int n, npages, hdrsize;

	npages = kmem_zone_count;
	buf = kmem_alloc(npages * PAGE_SIZE, M_XFSZONE, KM_SLEEP);
	zp = (kmem_zone_t *)buf;
	bzero(zp, sizeof (kmem_zone_t));
	strncpy(zp->kz_name, zone_name, sizeof (zp->kz_name) - 1);
	zp->kz_unitsz = ALIGN64(size);
	zp->kz_allocsz = npages;
	p = buf + (hdrsize = ALIGN64(sizeof (kmem_zone_t)));

	/*
	 * Setup the chain of free buffers
	 */
	n = (npages * PAGE_SIZE - hdrsize) / zp->kz_unitsz;
	ASSERT(n > 0);
	prev = NULL;
	while (n-- > 0) {
		*(caddr_t *)p = zp->kz_free_buf;
		zp->kz_free_buf = p;
		p += zp->kz_unitsz;
	}
		
	LOCK_INIT(&zp->kz_mutex);
	return (zp);
}

void *
kmem_zone_alloc(kmem_zone_t *zp, int flags)
{
	int	shrink = SHAKE_COUNT;	/* # times to try to shrink cache */
	void	*ptr = NULL;
	pl_t	opl;

repeat:
	opl = LOCK(&zp->kz_mutex);
	ptr = zp->kz_free_buf;
	if (ptr)
		zp->kz_free_buf = *(caddr_t *)ptr;
	UNLOCK(&zp->kz_mutex, opl);

	if (ptr || (flags & KM_NOSLEEP))
		return (ptr);

#if 0
	/*
	 * KM_SLEEP callers don't expect a failure
	 */
	if (shrink) {
		kmem_shake_zone(zone, shrink, __LINE__);

		shrink--;
		goto repeat;
	}
#endif

	if (flags & KM_SLEEP)
		panic("kmem_zone_alloc: NULL memory on KM_SLEEP request!");

	return NULL;
}

void *
kmem_zone_zalloc(kmem_zone_t *zp, int flags)
{
	void	*ptr;

	ptr = kmem_zone_alloc(zp, flags);
	if (ptr)
		bzero(ptr, zp->kz_unitsz);
	return (ptr);

}

void
kmem_zone_free(kmem_zone_t *zp, void *ptr)
{
	pl_t opl;

	opl = LOCK(&zp->kz_mutex);
	*(caddr_t *)ptr = *(caddr_t *)zp->kz_free_buf;
	zp->kz_free_buf = ptr;
	UNLOCK(&zp->kz_mutex, opl);
}

int
kmem_zone_destroy(kmem_zone_t *zp)
{

	kmem_free(zp);
	return (0);
}

#endif
