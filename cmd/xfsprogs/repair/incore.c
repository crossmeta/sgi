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
 */

#include <libxfs.h>
#include <malloc.h>
#include "avl.h"
#include "globals.h"
#include "incore.h"
#include "agheader.h"
#include "protos.h"
#include "err_protos.h"

/*
 * push a block allocation record onto list.  assumes list
 * if set to NULL if empty.
 */
void
record_allocation(ba_rec_t *addr, ba_rec_t *list)
{
	addr->next = list;
	list = addr;

	return;
}

void
free_allocations(ba_rec_t *list)
{
	ba_rec_t *current = list;

	while (list != NULL)  {
		list = list->next;
		free(current);
		current = list;
	}

	return;
}

/* ba bmap setupstuff.  setting/getting state is in incore.h  */

void
setup_bmap(xfs_agnumber_t agno, xfs_agblock_t numblocks, xfs_drtbno_t rtblocks)
{
	int i;
	xfs_drfsbno_t size;

        ba_bmap = (__uint64_t**)malloc(agno*sizeof(__uint64_t *));
        if (!ba_bmap)  {
		do_error("couldn't allocate block map pointers\n");
		return;
	}
	for (i = 0; i < agno; i++)  {
                int size;
                
                size = roundup(numblocks * (NBBY/XR_BB),sizeof(__uint64_t));
                
                ba_bmap[i] = (__uint64_t*)memalign(sizeof(__uint64_t), size);
                if (!ba_bmap[i]) {
			do_error("couldn't allocate block map, size = %d\n",
				numblocks);
			return;
		}
		bzero(ba_bmap[i], size);
	}

	if (rtblocks == 0)  {
		rt_ba_bmap = NULL;
		return;
	}

	size = roundup(rtblocks * (NBBY/XR_BB), sizeof(__uint64_t));

        rt_ba_bmap=(__uint64_t*)memalign(sizeof(__uint64_t), size);
	if (!rt_ba_bmap) {
			do_error(
			"couldn't allocate real-time block map, size = %llu\n",
				rtblocks);
			return;
	}

	/*
	 * start all real-time as free blocks
	 */
	set_bmap_rt(rtblocks);

	return;
}

/* ARGSUSED */
void
teardown_rt_bmap(xfs_mount_t *mp)
{
	if (rt_ba_bmap != NULL)  {
		free(rt_ba_bmap);
		rt_ba_bmap = NULL;
	}

	return;
}

/* ARGSUSED */
void
teardown_ag_bmap(xfs_mount_t *mp, xfs_agnumber_t agno)
{
	ASSERT(ba_bmap[agno] != NULL);

	free(ba_bmap[agno]);
	ba_bmap[agno] = NULL;

	return;
}

/* ARGSUSED */
void
teardown_bmap_finish(xfs_mount_t *mp)
{
	free(ba_bmap);
	ba_bmap = NULL;

	return;
}

void
teardown_bmap(xfs_mount_t *mp)
{
	xfs_agnumber_t i;

	for (i = 0; i < mp->m_sb.sb_agcount; i++)  {
		teardown_ag_bmap(mp, i);
	}

	teardown_rt_bmap(mp);
	teardown_bmap_finish(mp);

	return;
}

/*
 * block map initialization routines -- realtime, log, fs
 */
void
set_bmap_rt(xfs_drtbno_t num)
{
	xfs_drtbno_t j;
	xfs_drtbno_t size;

	/*
	 * for now, initialize all realtime blocks to be free
	 * (state == XR_E_FREE)
	 */
	size = howmany(num * (NBBY/XR_BB), sizeof(__uint64_t));

	for (j = 0; j < size; j++)
		rt_ba_bmap[j] = 0x2222222222222222;
	
	return;
}

void
set_bmap_log(xfs_mount_t *mp)
{
	xfs_dfsbno_t	logend, i;

	if (mp->m_sb.sb_logstart == 0)
		return;

	logend = mp->m_sb.sb_logstart + mp->m_sb.sb_logblocks;

	for (i = mp->m_sb.sb_logstart; i < logend ; i++)  {
		set_fsbno_state(mp, i, XR_E_INUSE_FS);
	}

	return;
}

void
set_bmap_fs(xfs_mount_t *mp)
{
	xfs_agnumber_t	i;
	xfs_agblock_t	j;
	xfs_agblock_t	end;

	/*
	 * AG header is 4 sectors
	 */
	end = howmany(4 * mp->m_sb.sb_sectsize, mp->m_sb.sb_blocksize);

	for (i = 0; i < mp->m_sb.sb_agcount; i++)
		for (j = 0; j < end; j++)
			set_agbno_state(mp, i, j, XR_E_INUSE_FS);

	return;
}

#if 0
void
set_bmap_fs_bt(xfs_mount_t *mp)
{
	xfs_agnumber_t	i;
	xfs_agblock_t	j;
	xfs_agblock_t	begin;
	xfs_agblock_t	end;

	begin = bnobt_root;
	end = inobt_root + 1;

	for (i = 0; i < mp->m_sb.sb_agcount; i++)  {
		/*
		 * account for btree roots
		 */
		for (j = begin; j < end; j++)
			set_agbno_state(mp, i, j, XR_E_INUSE_FS);
	}

	return;
}
#endif

void
incore_init(xfs_mount_t *mp)
{
	int agcount = mp->m_sb.sb_agcount;
	extern void incore_ino_init(xfs_mount_t *);
	extern void incore_ext_init(xfs_mount_t *);

	/* init block alloc bmap */

	setup_bmap(agcount, mp->m_sb.sb_agblocks, mp->m_sb.sb_rextents);
	incore_ino_init(mp);
	incore_ext_init(mp);

	/* initialize random globals now that we know the fs geometry */

	inodes_per_block = mp->m_sb.sb_inopblock;

	return;
}

#if defined(XR_BMAP_TRACE) || defined(XR_BMAP_DBG)
int
get_agbno_state(xfs_mount_t *mp, xfs_agnumber_t agno,
		xfs_agblock_t ag_blockno)
{
	__uint64_t *addr;

	addr = ba_bmap[(agno)] + (ag_blockno)/XR_BB_NUM;

	return((*addr >> (((ag_blockno)%XR_BB_NUM)*XR_BB)) & XR_BB_MASK);
}

void set_agbno_state(xfs_mount_t *mp, xfs_agnumber_t agno,
	xfs_agblock_t ag_blockno, int state)
{
	__uint64_t *addr;

	addr = ba_bmap[(agno)] + (ag_blockno)/XR_BB_NUM;

	*addr = (((*addr) &
	  (~((__uint64_t) XR_BB_MASK << (((ag_blockno)%XR_BB_NUM)*XR_BB)))) |
	 (((__uint64_t) (state)) << (((ag_blockno)%XR_BB_NUM)*XR_BB)));
}

int
get_fsbno_state(xfs_mount_t *mp, xfs_dfsbno_t blockno)
{
	return(get_agbno_state(mp, XFS_FSB_TO_AGNO(mp, blockno),
			XFS_FSB_TO_AGBNO(mp, blockno)));
}

void
set_fsbno_state(xfs_mount_t *mp, xfs_dfsbno_t blockno, int state)
{
	set_agbno_state(mp, XFS_FSB_TO_AGNO(mp, blockno),
		XFS_FSB_TO_AGBNO(mp, blockno), state);

	return;
}
#endif
