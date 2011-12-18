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
#include "avl.h"
#include "globals.h"
#include "agheader.h"
#include "protos.h"
#include "err_protos.h"
#include "incore.h"

void	set_mp(xfs_mount_t *mpp);
void	scan_ag(xfs_agnumber_t agno);

static void
zero_log(xfs_mount_t *mp, libxfs_init_t *args)
{
        int logdev = (mp->m_sb.sb_logstart == 0) ? args->logdev : args->ddev;
        
	libxfs_log_clear(logdev, 
		XFS_FSB_TO_DADDR(mp, mp->m_sb.sb_logstart),
		(xfs_extlen_t)XFS_FSB_TO_BB(mp, mp->m_sb.sb_logblocks),
                &mp->m_sb.sb_uuid,
                XLOG_FMT);
}

/*
 * ok, at this point, the fs is mounted but the root inode may be
 * trashed and the ag headers haven't been checked.  So we have
 * a valid xfs_mount_t and superblock but that's about it.  That
 * means we can use macros that use mount/sb fields in calculations
 * but I/O or btree routines that depend on space maps or inode maps
 * being correct are verboten.
 */

void
phase2(xfs_mount_t *mp, libxfs_init_t *args)
{
	xfs_agnumber_t		i;
	xfs_agblock_t		b;
	int			j;
	ino_tree_node_t		*ino_rec;

	/* now we can start using the buffer cache routines */
	set_mp(mp);

	/* Check whether this fs has internal or external log */
	if (mp->m_sb.sb_logstart == 0) {
		if (!args->logname) {
			fprintf (stderr,
				"This filesystem has an external log.  "
				"Specify log device with the -l option.\n");
			exit (1);
		}
		
		fprintf (stderr, "Phase 2 - using external log on %s\n", 
			 args->logname);
	} else
		fprintf (stderr, "Phase 2 - using internal log\n");

	/* Zero log if applicable */
	if (!no_modify)  {
		do_log("        - zero log...\n");
		zero_log(mp, args);
	}

	do_log("        - scan filesystem freespace and inode maps...\n");

	/*
	 * account for space used by ag headers and log if internal
	 */
	set_bmap_log(mp);
	set_bmap_fs(mp);

	bad_ino_btree = 0;

	for (i = 0; i < mp->m_sb.sb_agcount; i++)  {
		scan_ag(i);
#ifdef XR_INODE_TRACE
		print_inode_list(i);
#endif
	}

	/*
	 * make sure we know about the root inode chunk
	 */
	if ((ino_rec = find_inode_rec(0, mp->m_sb.sb_rootino)) == NULL)  {
		ASSERT(mp->m_sb.sb_rbmino == mp->m_sb.sb_rootino + 1 &&
			mp->m_sb.sb_rsumino == mp->m_sb.sb_rootino + 2);
		do_warn("root inode chunk not found\n");

		/*
		 * mark the first 3 used, the rest are free
		 */
		ino_rec = set_inode_used_alloc(0,
				(xfs_agino_t) mp->m_sb.sb_rootino);
		set_inode_used(ino_rec, 1);
		set_inode_used(ino_rec, 2);

		for (j = 3; j < XFS_INODES_PER_CHUNK; j++)
			set_inode_free(ino_rec, j);

		/*
		 * also mark blocks
		 */
		for (b = 0; b < mp->m_ialloc_blks; b++)  {
			set_agbno_state(mp, 0,
				b + XFS_INO_TO_AGBNO(mp, mp->m_sb.sb_rootino),
				XR_E_INO);
		}
	} else  {
		do_log("        - found root inode chunk\n");

		/*
		 * blocks are marked, just make sure they're in use
		 */
		if (is_inode_free(ino_rec, 0))  {
			do_warn("root inode marked free, ");
			set_inode_used(ino_rec, 0);
			if (!no_modify)
				do_warn("correcting\n");
			else
				do_warn("would correct\n");
		}

		if (is_inode_free(ino_rec, 1))  {
			do_warn("realtime bitmap inode marked free, ");
			set_inode_used(ino_rec, 1);
			if (!no_modify)
				do_warn("correcting\n");
			else
				do_warn("would correct\n");
		}

		if (is_inode_free(ino_rec, 2))  {
			do_warn("realtime summary inode marked free, ");
			set_inode_used(ino_rec, 2);
			if (!no_modify)
				do_warn("correcting\n");
			else
				do_warn("would correct\n");
		}
	}
}
