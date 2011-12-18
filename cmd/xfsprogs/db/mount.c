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
#include "init.h"
#include "io.h"
#include "mount.h"
#include "malloc.h"
#include "data.h"

xfs_mount_t	*mp;

static void
compute_maxlevels(
	xfs_mount_t	*mp,
	int		whichfork)
{
	int		level;
	uint		maxblocks;
	uint		maxleafents;
	int		maxrootrecs;
	int		minleafrecs;
	int		minnoderecs;
	int		sz;

	maxleafents = (whichfork == XFS_DATA_FORK) ? MAXEXTNUM : MAXAEXTNUM;
	minleafrecs = mp->m_bmap_dmnr[0];
	minnoderecs = mp->m_bmap_dmnr[1];
	sz = mp->m_sb.sb_inodesize;
	maxrootrecs = (int)XFS_BTREE_BLOCK_MAXRECS(sz, xfs_bmdr, 0);
	maxblocks = (maxleafents + minleafrecs - 1) / minleafrecs;
	for (level = 1; maxblocks > 1; level++) {
		if (maxblocks <= maxrootrecs)
			maxblocks = 1;
		else
			maxblocks = (maxblocks + minnoderecs - 1) / minnoderecs;
	}
	mp->m_bm_maxlevels[whichfork] = level;
}

xfs_mount_t *
dbmount(void)
{
	void		*bufp;
	int		i;
	xfs_mount_t	*mp;
	xfs_sb_t	*sbp;

	mp = xcalloc(1, sizeof(*mp));
	bufp = NULL;
	if (read_bbs(XFS_SB_DADDR, 1, &bufp, NULL))
		return NULL;

        /* copy sb from buf to in-core, converting architecture */
        libxfs_xlate_sb(bufp, &mp->m_sb, 1, ARCH_CONVERT, XFS_SB_ALL_BITS);
	xfree(bufp);
	sbp = &mp->m_sb;
 
        if (sbp->sb_magicnum != XFS_SB_MAGIC) {
            fprintf(stderr,"%s: unexpected XFS SB magic number 0x%08x\n",
                    progname, sbp->sb_magicnum);
        }
	mp->m_blkbit_log = sbp->sb_blocklog + XFS_NBBYLOG;
	mp->m_blkbb_log = sbp->sb_blocklog - BBSHIFT;
	mp->m_agno_log = libxfs_highbit32(sbp->sb_agcount - 1) + 1;
	mp->m_agino_log = sbp->sb_inopblog + sbp->sb_agblklog;
	mp->m_litino =
		(int)(sbp->sb_inodesize -
		      (sizeof(xfs_dinode_core_t) + sizeof(xfs_agino_t)));
	mp->m_blockmask = sbp->sb_blocksize - 1;
	mp->m_blockwsize = sbp->sb_blocksize >> XFS_WORDLOG;
	mp->m_blockwmask = mp->m_blockwsize - 1;
	for (i = 0; i < 2; i++) {
		mp->m_alloc_mxr[i] =
			(uint)XFS_BTREE_BLOCK_MAXRECS(sbp->sb_blocksize,
				xfs_alloc, i == 0);
		mp->m_alloc_mnr[i] =
			(uint)XFS_BTREE_BLOCK_MINRECS(sbp->sb_blocksize,
				xfs_alloc, i == 0);
		mp->m_bmap_dmxr[i] =
			(uint)XFS_BTREE_BLOCK_MAXRECS(sbp->sb_blocksize,
				xfs_bmbt, i == 0);
		mp->m_bmap_dmnr[i] =
			(uint)XFS_BTREE_BLOCK_MINRECS(sbp->sb_blocksize,
				xfs_bmbt, i == 0);
		mp->m_inobt_mxr[i] =
			(uint)XFS_BTREE_BLOCK_MAXRECS(sbp->sb_blocksize,
				xfs_inobt, i == 0);
		mp->m_inobt_mnr[i] =
			(uint)XFS_BTREE_BLOCK_MINRECS(sbp->sb_blocksize,
				xfs_inobt, i == 0);
	}
	compute_maxlevels(mp, XFS_DATA_FORK);
	compute_maxlevels(mp, XFS_ATTR_FORK);
	mp->m_bsize = XFS_FSB_TO_BB(mp, 1);
	mp->m_ialloc_inos = (int)MAX(XFS_INODES_PER_CHUNK, sbp->sb_inopblock);
	mp->m_ialloc_blks = mp->m_ialloc_inos >> sbp->sb_inopblog;
	if (sbp->sb_rblocks) {
		mp->m_rsumlevels = sbp->sb_rextslog + 1;
		mp->m_rsumsize =
			(uint)sizeof(xfs_suminfo_t) * mp->m_rsumlevels *
			sbp->sb_rbmblocks;
		if (sbp->sb_blocksize)
			mp->m_rsumsize =
				roundup(mp->m_rsumsize, sbp->sb_blocksize);
	}
	if (XFS_SB_VERSION_HASDIRV2(sbp)) {
		mp->m_dirversion = 2;
		mp->m_dirblksize =
			1 << (sbp->sb_dirblklog + sbp->sb_blocklog);
		mp->m_dirblkfsbs = 1 << sbp->sb_dirblklog;
		mp->m_dirdatablk =
			XFS_DIR2_DB_TO_DA(mp, XFS_DIR2_DATA_FIRSTDB(mp));
		mp->m_dirleafblk =
			XFS_DIR2_DB_TO_DA(mp, XFS_DIR2_LEAF_FIRSTDB(mp));
		mp->m_dirfreeblk =
			XFS_DIR2_DB_TO_DA(mp, XFS_DIR2_FREE_FIRSTDB(mp));
	} else {
		mp->m_dirversion = 1;
		mp->m_dirblksize = sbp->sb_blocksize;
		mp->m_dirblkfsbs = 1;
	}
	return mp;
}
