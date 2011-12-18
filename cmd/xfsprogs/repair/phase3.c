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
#include "incore.h"
#include "protos.h"
#include "err_protos.h"
#include "dinode.h"

/*
 * walks an unlinked list, returns 1 on an error (bogus pointer) or
 * I/O error
 */
int
walk_unlinked_list(xfs_mount_t *mp, xfs_agnumber_t agno, xfs_agino_t start_ino)
{
	xfs_buf_t *bp;
	xfs_dinode_t *dip;
	xfs_agino_t current_ino = start_ino;
	xfs_agblock_t agbno;
	int state;

	while (current_ino != NULLAGINO)  {
		if (!verify_aginum(mp, agno, current_ino))
			return(1);
		if ((bp = get_agino_buf(mp, agno, current_ino, &dip)) == NULL)
			return(1);
		/*
		 * if this looks like a decent inode, then continue
		 * following the unlinked pointers.  If not, bail.
		 */
		if (verify_dinode(mp, dip, agno, current_ino) == 0)  {
			/*
			 * check if the unlinked list points to an unknown
			 * inode.  if so, put it on the uncertain inode list
			 * and set block map appropriately.
			 */
			if (find_inode_rec(agno, current_ino) == NULL)  {
				add_aginode_uncertain(agno, current_ino, 1);
				agbno = XFS_AGINO_TO_AGBNO(mp, current_ino);

				switch (state = get_agbno_state(mp,
							agno, agbno))  {
				case XR_E_UNKNOWN:
				case XR_E_FREE:
				case XR_E_FREE1:
					set_agbno_state(mp, agno, agbno,
						XR_E_INO);
					break;
				case XR_E_BAD_STATE:
					do_error(
						"bad state in block map %d\n",
						state);
					abort();
					break;
				default:
					/*
					 * the block looks like inodes
					 * so be conservative and try
					 * to scavenge what's in there.
					 * if what's there is completely
					 * bogus, it'll show up later
					 * and the inode will be trashed
					 * anyway, hopefully without
					 * losing too much other data
					 */
					set_agbno_state(mp, agno, agbno,
						XR_E_INO);
					break;
				}
			}
			current_ino = dip->di_next_unlinked;
		} else  {
			current_ino = NULLAGINO;;
		}
		libxfs_putbuf(bp);
	}

	return(0);
}

void
process_agi_unlinked(xfs_mount_t *mp, xfs_agnumber_t agno)
{
	xfs_agnumber_t i;
	xfs_buf_t *bp;
	xfs_agi_t *agip;
	int err = 0;
	int agi_dirty = 0;

	bp = libxfs_readbuf(mp->m_dev, XFS_AG_DADDR(mp, agno, XFS_AGI_DADDR),
				mp->m_sb.sb_sectsize/BBSIZE, 0);
	if (!bp) {
		do_error("cannot read agi block %lld for ag %u\n",
			XFS_AG_DADDR(mp, agno, XFS_AGI_DADDR), agno);
		exit(1);
	}

	agip = XFS_BUF_TO_AGI(bp);

	ASSERT(no_modify || INT_GET(agip->agi_seqno, ARCH_CONVERT) == agno);

	for (i = 0; i < XFS_AGI_UNLINKED_BUCKETS; i++)  {
		if (INT_GET(agip->agi_unlinked[i], ARCH_CONVERT) != NULLAGINO)  {
			err += walk_unlinked_list(mp, agno,
						INT_GET(agip->agi_unlinked[i], ARCH_CONVERT));
			/*
			 * clear the list
			 */
			if (!no_modify)  {
				INT_SET(agip->agi_unlinked[i], ARCH_CONVERT, NULLAGINO);
				agi_dirty = 1;
			}
		}
	}

	if (err)
		do_warn("error following ag %d unlinked list\n", agno);

	ASSERT(agi_dirty == 0 || agi_dirty && !no_modify);

	if (agi_dirty && !no_modify)
		libxfs_writebuf(bp, 0);
	else
		libxfs_putbuf(bp);
}

void
phase3(xfs_mount_t *mp)
{
	int i, j;

	printf("Phase 3 - for each AG...\n");
	if (!no_modify)
		printf("        - scan and clear agi unlinked lists...\n");
	else
		printf("        - scan (but don't clear) agi unlinked lists...\n");

	/*
	 * first, let's look at the possibly bogus inodes
	 */
	for (i = 0; i < mp->m_sb.sb_agcount; i++)  {
		/*
		 * walk unlinked list to add more potential inodes to list
		 */
		process_agi_unlinked(mp, i);
		check_uncertain_aginodes(mp, i);
	}

	/* ok, now that the tree's ok, let's take a good look */

	printf(
	    "        - process known inodes and perform inode discovery...\n");

	for (i = 0; i < mp->m_sb.sb_agcount; i++)  {
		do_log("        - agno = %d\n", i);
		/*
		 * turn on directory processing (inode discovery) and 
		 * attribute processing (extra_attr_check)
		 */
		process_aginodes(mp, i, 1, 0, 1);
	}

	/*
	 * process newly discovered inode chunks
	 */
	printf("        - process newly discovered inodes...\n");
	do  {
		/*
		 * have to loop until no ag has any uncertain
		 * inodes
		 */
		j = 0;
		for (i = 0; i < mp->m_sb.sb_agcount; i++)  {
			j += process_uncertain_aginodes(mp, i);
#ifdef XR_INODE_TRACE
			fprintf(stderr,
				"\t\t phase 3 - process_uncertain_inodes returns %d\n", j);
#endif
		}
	} while (j != 0);
}

