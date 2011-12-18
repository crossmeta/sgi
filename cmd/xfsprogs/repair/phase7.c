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
#include "versions.h"

/* dinoc is a pointer to the IN-CORE dinode core */
void
set_nlinks(xfs_dinode_core_t	*dinoc,
		xfs_ino_t	ino,
		__uint32_t	nrefs,
		int		*dirty)
{
	if (!no_modify)  {
		if (INT_GET(dinoc->di_nlink, ARCH_NOCONVERT) != nrefs)  {
			*dirty = 1;
			do_warn("resetting inode %llu nlinks from %d to %d\n",
					ino, INT_GET(dinoc->di_nlink, ARCH_NOCONVERT), nrefs);

			if (nrefs > XFS_MAXLINK_1)  {
				ASSERT(fs_inode_nlink);
				do_warn(
"nlinks %d will overflow v1 ino, ino %llu will be converted to version 2\n",
					nrefs, ino);

			}
			INT_SET(dinoc->di_nlink, ARCH_NOCONVERT, nrefs);
		}
	} else  {
		if (INT_GET(dinoc->di_nlink, ARCH_NOCONVERT) != nrefs)
			do_warn(
			"would have reset inode %llu nlinks from %d to %d\n",
				ino, INT_GET(dinoc->di_nlink, ARCH_NOCONVERT), nrefs);
	}
}

void
phase7(xfs_mount_t *mp)
{
	ino_tree_node_t		*irec;
	xfs_inode_t		*ip;
	xfs_trans_t		*tp;
	int			i;
	int			j;
	int			error;
	int			dirty;
	xfs_ino_t		ino;
	__uint32_t		nrefs;

	if (!no_modify)
		printf("Phase 7 - verify and correct link counts...\n");
	else
		printf("Phase 7 - verify link counts...\n");

	tp = libxfs_trans_alloc(mp, XFS_TRANS_REMOVE);

	error = libxfs_trans_reserve(tp, (no_modify ? 0 : 10),
		XFS_REMOVE_LOG_RES(mp), 0, XFS_TRANS_PERM_LOG_RES,
		XFS_REMOVE_LOG_COUNT);

	ASSERT(error == 0);

	/*
	 * for each ag, look at each inode 1 at a time using the
	 * sim code.  if the number of links is bad, reset it,
	 * log the inode core, commit the transaction, and
	 * allocate a new transaction
	 */
	for (i = 0; i < glob_agcount; i++)  {
		irec = findfirst_inode_rec(i);

		while (irec != NULL)  {
			for (j = 0; j < XFS_INODES_PER_CHUNK; j++)  {
				ASSERT(is_inode_confirmed(irec, j));

				if (is_inode_free(irec, j))
					continue;

				ASSERT(no_modify || is_inode_reached(irec, j));
				ASSERT(no_modify ||
						is_inode_referenced(irec, j));

				nrefs = num_inode_references(irec, j);

				ino = XFS_AGINO_TO_INO(mp, i,
					irec->ino_startnum + j);

				error = libxfs_trans_iget(mp, tp, ino, 0, &ip);

				if (error)  {
					if (!no_modify)
						do_error(
					"couldn't map inode %llu, err = %d\n",
							ino, error);
					else  {
						do_warn(
	"couldn't map inode %llu, err = %d, can't compare link counts\n",
							ino, error);
						continue;
					}
				}

				dirty = 0;

				/*
				 * compare and set links for all inodes
				 * but the lost+found inode.  we keep
				 * that correct as we go.
				 */
				if (ino != orphanage_ino)
					set_nlinks(&ip->i_d, ino, nrefs,
							&dirty);
				
				if (!dirty)  {
					libxfs_trans_iput(tp, ip, 0);
				} else  {
					libxfs_trans_log_inode(tp, ip,
							XFS_ILOG_CORE);
					/*
					 * no need to do a bmap finish since
					 * we're not allocating anything
					 */
					ASSERT(error == 0);
					error = libxfs_trans_commit(tp,
						XFS_TRANS_RELEASE_LOG_RES|
						XFS_TRANS_SYNC, NULL);

					ASSERT(error == 0);

					tp = libxfs_trans_alloc(mp,
							XFS_TRANS_REMOVE);

					error = libxfs_trans_reserve(tp,
						(no_modify ? 0 : 10),
						XFS_REMOVE_LOG_RES(mp),
						0, XFS_TRANS_PERM_LOG_RES,
						XFS_REMOVE_LOG_COUNT);
					ASSERT(error == 0);
				}
			}
			irec = next_ino_rec(irec);
		}
	}

	/* 
	 * always have one unfinished transaction coming out
	 * of the loop.  cancel it.
	 */
	libxfs_trans_cancel(tp, XFS_TRANS_RELEASE_LOG_RES);
}
