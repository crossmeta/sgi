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
#include "dir.h"
#include "dir2.h"
#include "dinode.h"
#include "scan.h"
#include "versions.h"
#include "attr_repair.h"
#include "bmap.h"

/*
 * inode clearing routines
 */

/*
 * return the offset into the inode where the attribute fork starts
 */
/* ARGSUSED */
int
calc_attr_offset(xfs_mount_t *mp, xfs_dinode_t *dino)
{
	xfs_dinode_core_t	*dinoc = &dino->di_core;
	int			offset = ((__psint_t) &dino->di_u)
						- (__psint_t)dino;

	/*
	 * don't worry about alignment when calculating offset
	 * because the data fork is already 8-byte aligned
	 */
	switch (dinoc->di_format)  {
	case XFS_DINODE_FMT_DEV:
		offset += sizeof(dev_t);
		break;
	case XFS_DINODE_FMT_LOCAL:
		offset += INT_GET(dinoc->di_size, ARCH_CONVERT);
		break;
	case XFS_DINODE_FMT_UUID:
		offset += sizeof(uuid_t);
		break;
	case XFS_DINODE_FMT_EXTENTS:
		offset += INT_GET(dinoc->di_nextents, ARCH_CONVERT) * sizeof(xfs_bmbt_rec_32_t);
		break;
	case XFS_DINODE_FMT_BTREE:
		offset += INT_GET(dino->di_u.di_bmbt.bb_numrecs, ARCH_CONVERT) * sizeof(xfs_bmbt_rec_32_t);
		break;
	default:
		do_error("Unknown inode format.\n");
		abort();
		break;
	}

	return(offset);
}

/* ARGSUSED */
int
clear_dinode_attr(xfs_mount_t *mp, xfs_dinode_t *dino, xfs_ino_t ino_num)
{
	xfs_dinode_core_t *dinoc = &dino->di_core;

	ASSERT(dinoc->di_forkoff != 0);

	if (!no_modify)
		fprintf(stderr, "clearing inode %I64u attributes \n", ino_num);
	else
		fprintf(stderr, "would have cleared inode %I64u attributes\n",
			ino_num);

	if (INT_GET(dinoc->di_anextents, ARCH_CONVERT) != 0)  {
		if (no_modify)
			return(1);
		INT_ZERO(dinoc->di_anextents, ARCH_CONVERT);
	}

	if (dinoc->di_aformat != XFS_DINODE_FMT_EXTENTS)  {
		if (no_modify)
			return(1);
		dinoc->di_aformat = XFS_DINODE_FMT_EXTENTS;
	}

	/* get rid of the fork by clearing forkoff */

	/* Originally, when the attr repair code was added, the fork was cleared
	 * by turning it into shortform status.  This meant clearing the
	 * hdr.totsize/count fields and also changing aformat to LOCAL
	 * (vs EXTENTS).  Over various fixes, the aformat and forkoff have
	 * been updated to not show an attribute fork at all, however.
	 * It could be possible that resetting totsize/count are not needed,
	 * but just to be safe, leave it in for now. 
	 */

	if (!no_modify) {
		xfs_attr_shortform_t *asf = (xfs_attr_shortform_t *)
				XFS_DFORK_APTR_ARCH(dino, ARCH_CONVERT);
		INT_SET(asf->hdr.totsize, ARCH_CONVERT,
			sizeof(xfs_attr_sf_hdr_t));
		INT_SET(asf->hdr.count, ARCH_CONVERT, 0);
		dinoc->di_forkoff = 0;  /* got to do this after asf is set */
	}

	/*
	 * always returns 1 since the fork gets zapped
	 */
	return(1);
}

/* ARGSUSED */
int
clear_dinode_core(xfs_dinode_core_t *dinoc, xfs_ino_t ino_num)
{
	int dirty = 0;

	if (INT_GET(dinoc->di_magic, ARCH_CONVERT) != XFS_DINODE_MAGIC)  {
		dirty = 1;

		if (no_modify)
			return(1);

		INT_SET(dinoc->di_magic, ARCH_CONVERT, XFS_DINODE_MAGIC);
	}

	if (!XFS_DINODE_GOOD_VERSION(dinoc->di_version) ||
	    (!fs_inode_nlink && dinoc->di_version > XFS_DINODE_VERSION_1))  {
		dirty = 1;

		if (no_modify)
			return(1);

		dinoc->di_version = (fs_inode_nlink) ? XFS_DINODE_VERSION_2
						: XFS_DINODE_VERSION_1;
	}

	if (INT_GET(dinoc->di_mode, ARCH_CONVERT) != 0)  {
		dirty = 1;

		if (no_modify)
			return(1);

		INT_ZERO(dinoc->di_mode, ARCH_CONVERT);
	}

	if (INT_GET(dinoc->di_flags, ARCH_CONVERT) != 0)  {
		dirty = 1;

		if (no_modify)
			return(1);

		INT_ZERO(dinoc->di_flags, ARCH_CONVERT);
	}

	if (INT_GET(dinoc->di_dmevmask, ARCH_CONVERT) != 0)  {
		dirty = 1;

		if (no_modify)
			return(1);

		INT_ZERO(dinoc->di_dmevmask, ARCH_CONVERT);
	}

	if (dinoc->di_forkoff != 0)  {
		dirty = 1;

		if (no_modify)
			return(1);

		dinoc->di_forkoff = 0;
	}

	if (dinoc->di_format != XFS_DINODE_FMT_EXTENTS)  {
		dirty = 1;

		if (no_modify)
			return(1);

		dinoc->di_format = XFS_DINODE_FMT_EXTENTS;
	}

	if (dinoc->di_aformat != XFS_DINODE_FMT_EXTENTS)  {
		dirty = 1;

		if (no_modify)
			return(1);

		dinoc->di_aformat = XFS_DINODE_FMT_EXTENTS;
	}

	if (INT_GET(dinoc->di_size, ARCH_CONVERT) != 0)  {
		dirty = 1;

		if (no_modify)
			return(1);

		INT_ZERO(dinoc->di_size, ARCH_CONVERT);
	}

	if (INT_GET(dinoc->di_nblocks, ARCH_CONVERT) != 0)  {
		dirty = 1;

		if (no_modify)
			return(1);

		INT_ZERO(dinoc->di_nblocks, ARCH_CONVERT);
	}

	if (INT_GET(dinoc->di_onlink, ARCH_CONVERT) != 0)  {
		dirty = 1;

		if (no_modify)
			return(1);

		INT_ZERO(dinoc->di_onlink, ARCH_CONVERT);
	}

	if (INT_GET(dinoc->di_nextents, ARCH_CONVERT) != 0)  {
		dirty = 1;

		if (no_modify)
			return(1);

		INT_ZERO(dinoc->di_nextents, ARCH_CONVERT);
	}

	if (INT_GET(dinoc->di_anextents, ARCH_CONVERT) != 0)  {
		dirty = 1;

		if (no_modify)
			return(1);

		INT_ZERO(dinoc->di_anextents, ARCH_CONVERT);
	}

	if (dinoc->di_version > XFS_DINODE_VERSION_1 &&
			INT_GET(dinoc->di_nlink, ARCH_CONVERT) != 0)  {
		dirty = 1;

		if (no_modify)
			return(1);

		INT_ZERO(dinoc->di_nlink, ARCH_CONVERT);
	}

	return(dirty);
}

/* ARGSUSED */
int
clear_dinode_unlinked(xfs_mount_t *mp, xfs_dinode_t *dino)
{

	if (dino->di_next_unlinked != NULLAGINO)  {
		if (!no_modify)
			dino->di_next_unlinked = NULLAGINO;
		return(1);
	}

	return(0);
}

/*
 * this clears the unlinked list too so it should not be called
 * until after the agi unlinked lists are walked in phase 3.
 * returns > zero if the inode has been altered while being cleared
 */
int
clear_dinode(xfs_mount_t *mp, xfs_dinode_t *dino, xfs_ino_t ino_num)
{
	int dirty;

	dirty = clear_dinode_core(&dino->di_core, ino_num);
	dirty += clear_dinode_unlinked(mp, dino);

	/* and clear the forks */

	if (dirty && !no_modify)
		bzero(&dino->di_u, XFS_LITINO(mp));

	return(dirty);
}


/*
 * misc. inode-related utility routines
 */

/*
 * returns 0 if inode number is valid, 1 if bogus
 */
int
verify_inum(xfs_mount_t		*mp,
		xfs_ino_t	ino)
{
	xfs_agnumber_t	agno;
	xfs_agino_t	agino;
	xfs_agblock_t	agbno;
	xfs_sb_t	*sbp = &mp->m_sb;;

	/* range check ag #, ag block.  range-checking offset is pointless */

	agno = XFS_INO_TO_AGNO(mp, ino);
	agino = XFS_INO_TO_AGINO(mp, ino);
	agbno = XFS_AGINO_TO_AGBNO(mp, agino);

	if (ino == 0 || ino == NULLFSINO)
		return(1);

	if (ino != XFS_AGINO_TO_INO(mp, agno, agino))
		return(1);

	if (agno >= sbp->sb_agcount ||
		(agno < sbp->sb_agcount && agbno >= sbp->sb_agblocks) ||
		(agno == sbp->sb_agcount && agbno >= sbp->sb_dblocks -
				(sbp->sb_agcount-1) * sbp->sb_agblocks) ||
		(agbno == 0))
		return(1);

	return(0);
}

/*
 * have a separate routine to ensure that we don't accidentally
 * lose illegally set bits in the agino by turning it into an FSINO
 * to feed to the above routine
 */
int
verify_aginum(xfs_mount_t	*mp,
		xfs_agnumber_t	agno,
		xfs_agino_t	agino)
{
	xfs_agblock_t	agbno;
	xfs_sb_t	*sbp = &mp->m_sb;;

	/* range check ag #, ag block.  range-checking offset is pointless */

	if (agino == 0 || agino == NULLAGINO)
		return(1);

	/*
	 * agino's can't be too close to NULLAGINO because the min blocksize
	 * is 9 bits and at most 1 bit of that gets used for the inode offset
	 * so if the agino gets shifted by the # of offset bits and compared
	 * to the legal agbno values, a bogus agino will be too large.  there
	 * will be extra bits set at the top that shouldn't be set.
	 */
	agbno = XFS_AGINO_TO_AGBNO(mp, agino);

	if (agno >= sbp->sb_agcount ||
		(agno < sbp->sb_agcount && agbno >= sbp->sb_agblocks) ||
		(agno == sbp->sb_agcount && agbno >= sbp->sb_dblocks -
				(sbp->sb_agcount-1) * sbp->sb_agblocks) ||
		(agbno == 0))
		return(1);

	return(0);
}

/*
 * return 1 if block number is good, 0 if out of range
 */
int
verify_dfsbno(xfs_mount_t	*mp,
		xfs_dfsbno_t	fsbno)
{
	xfs_agnumber_t	agno;
	xfs_agblock_t	agbno;
	xfs_sb_t	*sbp = &mp->m_sb;;

	/* range check ag #, ag block.  range-checking offset is pointless */

	agno = XFS_FSB_TO_AGNO(mp, fsbno);
	agbno = XFS_FSB_TO_AGBNO(mp, fsbno);

	if (agno >= sbp->sb_agcount ||
		(agno < sbp->sb_agcount && agbno >= sbp->sb_agblocks) ||
		(agno == sbp->sb_agcount && agbno >= sbp->sb_dblocks -
				(sbp->sb_agcount-1) * sbp->sb_agblocks))
		return(0);

	return(1);
}

int
verify_agbno(xfs_mount_t	*mp,
		xfs_agnumber_t	agno,
		xfs_agblock_t	agbno)
{
	xfs_sb_t	*sbp = &mp->m_sb;;

	/* range check ag #, ag block.  range-checking offset is pointless */

	if (agno >= sbp->sb_agcount ||
		(agno < sbp->sb_agcount && agbno >= sbp->sb_agblocks) ||
		(agno == sbp->sb_agcount && agbno >= sbp->sb_dblocks -
				(sbp->sb_agcount-1) * sbp->sb_agblocks))
		return(0);

	return(1);
}

void
convert_extent(
	xfs_bmbt_rec_32_t	*rp,
	xfs_dfiloff_t		*op,	/* starting offset (blockno in file) */
	xfs_dfsbno_t		*sp,	/* starting block (fs blockno) */
	xfs_dfilblks_t		*cp,	/* blockcount */
	int			*fp)	/* extent flag */
{
	xfs_bmbt_irec_t irec, *s = &irec;
	/* Just use the extent parsing routine from the kernel */
	libxfs_bmbt_get_all((xfs_bmbt_rec_t *)rp, s);

	if (fs_has_extflgbit)  {
		if (s->br_state == XFS_EXT_UNWRITTEN) {
			*fp = 1;
		} else {
			*fp = 0;
		}
	} else  {
		*fp = 0;
	}
	*op = s->br_startoff;
	*sp = s->br_startblock;
	*cp = s->br_blockcount;
}

/*
 * return address of block fblock if it's within the range described
 * by the extent list.  Otherwise, returns a null address.
 */
/* ARGSUSED */
xfs_dfsbno_t
get_bmbt_reclist(
	xfs_mount_t		*mp,
	xfs_bmbt_rec_32_t	*rp,
	int			numrecs,
	xfs_dfiloff_t		fblock)
{
	int			i;
	xfs_dfilblks_t		cnt;
	xfs_dfiloff_t		off_bno;
	xfs_dfsbno_t		start;
	int			flag;

	for (i = 0; i < numrecs; i++, rp++) {
		convert_extent(rp, &off_bno, &start, &cnt, &flag);
		if (off_bno >= fblock && off_bno + cnt < fblock)
			return(start + fblock - off_bno);
	}

	return(NULLDFSBNO);
}

/*
 * return 1 if inode should be cleared, 0 otherwise
 * if check_dups should be set to 1, that implies that
 * the primary purpose of this call is to see if the
 * file overlaps with any duplicate extents (in the
 * duplicate extent list).
 */
/* ARGSUSED */
int
process_bmbt_reclist_int(
	xfs_mount_t		*mp,
	xfs_bmbt_rec_32_t	*rp,
	int			numrecs,
	int			type,
	xfs_ino_t		ino,
	xfs_drfsbno_t		*tot,
	blkmap_t		**blkmapp,
	xfs_dfiloff_t		*first_key,
	xfs_dfiloff_t		*last_key,
	int			check_dups,
	int			whichfork)
{
	xfs_dfsbno_t		b;
	xfs_drtbno_t		ext;
	xfs_dfilblks_t		c;		/* count */
	xfs_dfilblks_t		cp = 0;		/* prev count */
	xfs_dfsbno_t		s;		/* start */
	xfs_dfsbno_t		sp = 0;		/* prev start */
	xfs_dfiloff_t		o = 0;		/* offset */
	xfs_dfiloff_t		op = 0;		/* prev offset */
	char			*ftype;
	char			*forkname;
	int			i;
	int			state;
	int			flag;		/* extent flag */

	if (whichfork == XFS_DATA_FORK)
		forkname = "data";
	else
		forkname = "attr";

	if (type == XR_INO_RTDATA)
		ftype = "real-time";
	else
		ftype = "regular";

	for (i = 0; i < numrecs; i++, rp++) {
		convert_extent(rp, &o, &s, &c, &flag);
		if (i == 0)
			*last_key = *first_key = o;
		else
			*last_key = o;
		if (i > 0 && op + cp > o)  {
			do_warn(
"bmap rec out of order, inode %llu entry %d [o s c] [%llu %llu %llu], %d [%llu %llu %llu]\n",
				ino, i, o, s, c, i-1, op, sp, cp);
			return(1);
		}
		op = o;
		cp = c;
		sp = s;

		/*
		 * check numeric validity of the extent
		 */
		if (c == 0)  {
			do_warn(
		"zero length extent (off = %llu, fsbno = %llu) in ino %llu\n",
				o, s, ino);
			return(1);
		}
		if (type == XR_INO_RTDATA) {
			if (s >= mp->m_sb.sb_rblocks)  {
				do_warn(
"inode %llu - bad rt extent starting block number %llu, offset %llu\n",
					ino, s, o);
				return(1);
			}
			if (s + c - 1 >= mp->m_sb.sb_rblocks)  {
				do_warn(
"inode %llu - bad rt extent last block number %llu, offset %llu\n",
					ino, s + c - 1, o);
				return(1);
			}
			if (s + c - 1 < s)  {
				do_warn(
"inode %llu - bad rt extent overflows - start %llu, end %llu, offset %llu\n",
					ino, s, s + c - 1, o);
				return(1);
			}
		} else  {
			if (!verify_dfsbno(mp, s))  {
				do_warn(
"inode %llu - bad extent starting block number %llu, offset %llu\n",
					ino, s, o);
				return(1);
			}
			if (!verify_dfsbno(mp, s + c - 1))  {
				do_warn(
"inode %llu - bad extent last block number %llu, offset %llu\n",
					ino, s + c - 1, o);
				return(1);
			}
			if (s + c - 1 < s)  {
				do_warn(
"inode %llu - bad extent overflows - start %llu, end %llu, offset %llu\n",
					ino, s, s + c - 1, o);
				return(1);
			}
			if (o >= fs_max_file_offset)  {
				do_warn(
"inode %llu - extent offset too large - start %llu, count %llu, offset %llu\n",
					ino, s, c, o);
				return(1);
			}
		}

		/*
		 * realtime file data fork
		 */
		if (type == XR_INO_RTDATA && whichfork == XFS_DATA_FORK)  {
			/*
			 * XXX - verify that the blocks listed in the record
			 * are multiples of an extent
			 */
			if (s % mp->m_sb.sb_rextsize != 0 ||
					c % mp->m_sb.sb_rextsize != 0)  {
				do_warn(
"malformed rt inode extent [%llu %llu] (fs rtext size = %u)\n",
					s, c, mp->m_sb.sb_rextsize);
				return(1);
			}

			/*
			 * XXX - set the appropriate number of extents
			 */
			for (b = s; b < s + c; b += mp->m_sb.sb_rextsize)  {
				ext = (xfs_drtbno_t) b / mp->m_sb.sb_rextsize;

				if (check_dups == 1)  {
					if (search_rt_dup_extent(mp, ext))  {
						do_warn(
"data fork in rt ino %llu claims dup rt extent, off - %llu, start - %llu, count %llu\n",
							ino, o, s, c);
						return(1);
					}
					continue;
				}

				state = get_rtbno_state(mp, ext);

				switch (state)  {
				case XR_E_FREE:
/* XXX - turn this back on after we
	run process_rtbitmap() in phase2
					do_warn(
			"%s fork in rt ino %llu claims free rt block %llu\n",
						forkname, ino, ext);
*/
					/* fall through ... */
				case XR_E_UNKNOWN:
					set_rtbno_state(mp, ext, XR_E_INUSE);
					break;
				case XR_E_BAD_STATE:
					do_error(
				"bad state in rt block map %llu\n", ext);
					abort();
					break;
				case XR_E_FS_MAP:
				case XR_E_INO:
				case XR_E_INUSE_FS:
					do_error(
	"%s fork in rt inode %llu found metadata block %llu in %s bmap\n",
						forkname, ino, ext, ftype);
				case XR_E_INUSE:
				case XR_E_MULT:
					set_rtbno_state(mp, ext, XR_E_MULT);
					do_warn(
			"%s fork in rt inode %llu claims used rt block %llu\n",
						forkname, ino, ext);
					return(1);
				case XR_E_FREE1:
				default:
					do_error(
				"illegal state %d in %s block map %llu\n",
						state, ftype, b);
				}
			}

			/*
			 * bump up the block counter
			 */
			*tot += c;

			/*
			 * skip rest of loop processing since that's
			 * all for regular file forks and attr forks
			 */
			continue;
		}

	
		/*
		 * regular file data fork or attribute fork
		 */
		if (blkmapp && *blkmapp)
			blkmap_set_ext(blkmapp, o, s, c);
		for (b = s; b < s + c; b++)  {
			if (check_dups == 1)  {
				/*
				 * if we're just checking the bmap for dups,
				 * return if we find one, otherwise, continue
				 * checking each entry without setting the
				 * block bitmap
				 */
				if (search_dup_extent(mp,
						    XFS_FSB_TO_AGNO(mp, b),
						    XFS_FSB_TO_AGBNO(mp, b)))  {
					do_warn(
"%s fork in ino %llu claims dup extent, off - %llu, start - %llu, cnt %llu\n",
						forkname, ino, o, s, c);
					return(1);
				}
				continue;
			}

			/* FIX FOR BUG 653709 -- EKN 
			 * realtime attribute fork, should be valid block number
	 		 * in regular data space, not realtime partion.
			 */
		        if (type == XR_INO_RTDATA && whichfork == XFS_ATTR_FORK) {
			  if (mp->m_sb.sb_agcount < XFS_FSB_TO_AGNO(mp, b))
				return(1);
			}	
		
			state = get_fsbno_state(mp, b);
			switch (state)  {
			case XR_E_FREE:
			case XR_E_FREE1:
				do_warn(
				"%s fork in ino %llu claims free block %llu\n",
					forkname, ino, (__uint64_t) b);
				/* fall through ... */
			case XR_E_UNKNOWN:
				set_fsbno_state(mp, b, XR_E_INUSE);
				break;
			case XR_E_BAD_STATE:
				do_error("bad state in block map %llu\n", b);
				abort();
				break;
			case XR_E_FS_MAP:
			case XR_E_INO:
			case XR_E_INUSE_FS:
				do_warn(
				"%s fork in inode %llu claims metadata block %llu\n",
					forkname, ino, (__uint64_t) b);
				return(1);
			case XR_E_INUSE:
			case XR_E_MULT:
				set_fsbno_state(mp, b, XR_E_MULT);
				do_warn(
				"%s fork in %s inode %llu claims used block %llu\n",
					forkname, ftype, ino, (__uint64_t) b);
				return(1);
			default:
				do_error("illegal state %d in block map %llu\n",
					state, b);
				abort();
			}
		}
		*tot += c;
	}

	return(0);
}

/*
 * return 1 if inode should be cleared, 0 otherwise, sets block bitmap
 * as a side-effect
 */
int
process_bmbt_reclist(
	xfs_mount_t		*mp,
	xfs_bmbt_rec_32_t	*rp,
	int			numrecs,
	int			type,
	xfs_ino_t		ino,
	xfs_drfsbno_t		*tot,
	blkmap_t		**blkmapp,
	xfs_dfiloff_t		*first_key,
	xfs_dfiloff_t		*last_key,
	int			whichfork)
{
	return(process_bmbt_reclist_int(mp, rp, numrecs, type, ino, tot,
					blkmapp, first_key, last_key, 0,
					whichfork));
}

/*
 * return 1 if inode should be cleared, 0 otherwise, does not set
 * block bitmap
 */
int
scan_bmbt_reclist(
	xfs_mount_t		*mp,
	xfs_bmbt_rec_32_t	*rp,
	int			numrecs,
	int			type,
	xfs_ino_t		ino,
	xfs_drfsbno_t		*tot,
	int			whichfork)
{
	xfs_dfiloff_t		first_key = 0;
	xfs_dfiloff_t		last_key = 0;

	return(process_bmbt_reclist_int(mp, rp, numrecs, type, ino, tot,
					NULL, &first_key, &last_key, 1,
					whichfork));
}

/*
 * these two are meant for routines that read and work with inodes
 * one at a time where the inodes may be in any order (like walking
 * the unlinked lists to look for inodes).  the caller is responsible
 * for writing/releasing the buffer.
 */
xfs_buf_t *
get_agino_buf(xfs_mount_t	 *mp,
		xfs_agnumber_t	agno,
		xfs_agino_t	agino,
		xfs_dinode_t	**dipp)
{
	ino_tree_node_t *irec;
	xfs_buf_t *bp;
	int size;

	if ((irec = find_inode_rec(agno, agino)) == NULL)
		return(NULL);
	
	size = XFS_FSB_TO_BB(mp, MAX(1, XFS_INODES_PER_CHUNK/inodes_per_block));
	bp = libxfs_readbuf(mp->m_dev, XFS_AGB_TO_DADDR(mp, agno,
		XFS_AGINO_TO_AGBNO(mp, irec->ino_startnum)), size, 0);
	if (!bp) {
		do_warn("cannot read inode (%u/%u), disk block %lld\n",
			agno, irec->ino_startnum,
			XFS_AGB_TO_DADDR(mp, agno,
				XFS_AGINO_TO_AGBNO(mp, irec->ino_startnum)));
		return(NULL);
	}

	*dipp = XFS_MAKE_IPTR(mp, bp, agino -
		XFS_OFFBNO_TO_AGINO(mp, XFS_AGINO_TO_AGBNO(mp,
						irec->ino_startnum),
		0));

	return(bp);
}

/*
 * these next routines return the filesystem blockno of the
 * block containing the block "bno" in the file whose bmap
 * tree (or extent list) is rooted by "rootblock".
 *
 * the next routines are utility routines for the third
 * routine, get_bmapi().
 */
/* ARGSUSED */
xfs_dfsbno_t
getfunc_extlist(xfs_mount_t		*mp,
		xfs_ino_t		ino,
		xfs_dinode_t		*dip,
		xfs_dfiloff_t		bno,
		int			whichfork)
{
	xfs_dfiloff_t		fbno;
	xfs_dfilblks_t		bcnt;
	xfs_dfsbno_t		fsbno;
	xfs_dfsbno_t		final_fsbno = NULLDFSBNO;
	xfs_bmbt_rec_32_t	*rootblock = (xfs_bmbt_rec_32_t *)
						XFS_DFORK_PTR_ARCH(dip, whichfork, ARCH_CONVERT);
	xfs_extnum_t		nextents = XFS_DFORK_NEXTENTS_ARCH(dip, whichfork, ARCH_CONVERT);
	int			i;
	int			flag;

	for (i = 0; i < nextents; i++)  {
		convert_extent(rootblock + i, &fbno, &fsbno, &bcnt, &flag);

		if (fbno <= bno && bno < fbno + bcnt)  {
			final_fsbno = bno - fbno + fsbno;
			break;
		}
	}

	return(final_fsbno);
}

xfs_dfsbno_t
getfunc_btree(xfs_mount_t		*mp,
		xfs_ino_t		ino,
		xfs_dinode_t		*dip,
		xfs_dfiloff_t		bno,
		int			whichfork)
{
	int			i;
	int			prev_level;
	int			flag;
	int			found;
	xfs_bmbt_rec_32_t	*rec;
	xfs_bmbt_ptr_t		*pp;
	xfs_bmbt_key_t		*key;
	xfs_bmdr_key_t		*rkey;
	xfs_bmdr_ptr_t		*rp;
	xfs_dfiloff_t		fbno;
	xfs_dfsbno_t		fsbno;
	xfs_dfilblks_t		bcnt;
	xfs_buf_t		*bp;
	xfs_dfsbno_t		final_fsbno = NULLDFSBNO;
	xfs_bmbt_block_t	*block;
	xfs_bmdr_block_t	*rootblock = (xfs_bmdr_block_t *)
			XFS_DFORK_PTR_ARCH(dip, whichfork, ARCH_CONVERT);

	ASSERT(rootblock->bb_level != 0);
	/*
	 * deal with root block, it's got a slightly different
	 * header structure than interior nodes.  We know that
	 * a btree should have at least 2 levels otherwise it
	 * would be an extent list.
	 */
	rkey = XFS_BTREE_KEY_ADDR(
			XFS_DFORK_SIZE_ARCH(dip, mp, whichfork, ARCH_CONVERT),
			xfs_bmdr, rootblock, 1,
			XFS_BTREE_BLOCK_MAXRECS(XFS_DFORK_SIZE_ARCH(dip,
						mp, whichfork, ARCH_CONVERT),
			xfs_bmdr, 1));
	rp = XFS_BTREE_PTR_ADDR(
			XFS_DFORK_SIZE_ARCH(dip, mp, whichfork, ARCH_CONVERT),
			xfs_bmdr, rootblock, 1,
			XFS_BTREE_BLOCK_MAXRECS(XFS_DFORK_SIZE_ARCH(dip,
						mp, whichfork, ARCH_CONVERT),
			xfs_bmdr, 1));
	for (found = -1, i = 0; i < rootblock->bb_numrecs - 1; i++)  {
		if (rkey[i].br_startoff <= bno
				&& bno < rkey[i+1].br_startoff)  {
			found = i;
			break;
		}
	}
	if (i == rootblock->bb_numrecs - 1 && bno >= rkey[i].br_startoff)
		found = i;

	ASSERT(found != -1);

	fsbno = INT_GET(rp[found], ARCH_CONVERT);

	ASSERT(verify_dfsbno(mp, fsbno));

	bp = libxfs_readbuf(mp->m_dev, XFS_FSB_TO_DADDR(mp, fsbno),
				XFS_FSB_TO_BB(mp, 1), 0);
	if (!bp) {
		do_error("cannot read bmap block %llu\n", fsbno);
		return(NULLDFSBNO);
	}
	block = XFS_BUF_TO_BMBT_BLOCK(bp);

	/*
	 * ok, now traverse any interior btree nodes
	 */
	prev_level = rootblock->bb_level;

	while (INT_GET(block->bb_level, ARCH_CONVERT) > 0)  {
		ASSERT(INT_GET(block->bb_level, ARCH_CONVERT) < prev_level);

		prev_level = INT_GET(block->bb_level, ARCH_CONVERT);

		if (INT_GET(block->bb_numrecs, ARCH_CONVERT) >
						mp->m_bmap_dmxr[1]) {
			do_warn("# of bmap records in inode %llu exceeds max "
				"(%u, max - %u)\n",
				ino, INT_GET(block->bb_numrecs, ARCH_CONVERT),
				mp->m_bmap_dmxr[1]);
			libxfs_putbuf(bp);
			return(NULLDFSBNO);
		}
		if (verbose && INT_GET(block->bb_numrecs, ARCH_CONVERT) <
						mp->m_bmap_dmnr[1]) {
			do_warn("- # of bmap records in inode %llu < than min "
				"(%u, min - %u), proceeding ...\n",
				ino, INT_GET(block->bb_numrecs, ARCH_CONVERT),
				mp->m_bmap_dmnr[1]);
		}
		key = XFS_BTREE_KEY_ADDR(mp->m_sb.sb_blocksize,
			xfs_bmbt, block, 1, mp->m_bmap_dmxr[1]);
		pp = XFS_BTREE_PTR_ADDR(mp->m_sb.sb_blocksize,
			xfs_bmbt, block, 1, mp->m_bmap_dmxr[1]);
		for (	found = -1, i = 0;
			i < INT_GET(block->bb_numrecs, ARCH_CONVERT) - 1;
			i++) {
			if (INT_GET(key[i].br_startoff, ARCH_CONVERT) <= bno &&
			    bno < INT_GET(key[i+1].br_startoff, ARCH_CONVERT)) {
				found = i;
				break;
			}
		}
		if (i == INT_GET(block->bb_numrecs, ARCH_CONVERT) - 1 &&
			bno >= INT_GET(key[i].br_startoff, ARCH_CONVERT))
			found = i;

		ASSERT(found != -1);
		fsbno = INT_GET(pp[found], ARCH_CONVERT);

		ASSERT(verify_dfsbno(mp, fsbno));

		/*
		 * release current btree block and read in the
		 * next btree block to be traversed
		 */
		libxfs_putbuf(bp);
		bp = libxfs_readbuf(mp->m_dev, XFS_FSB_TO_DADDR(mp, fsbno),
					XFS_FSB_TO_BB(mp, 1), 0);
		if (!bp) {
			do_error("cannot read bmap block %llu\n", fsbno);
			return(NULLDFSBNO);
		}
		block = XFS_BUF_TO_BMBT_BLOCK(bp);
	}

	/*
	 * current block must be a leaf block
	 */
	ASSERT(INT_GET(block->bb_level, ARCH_CONVERT) == 0);
	if (INT_GET(block->bb_numrecs, ARCH_CONVERT) > mp->m_bmap_dmxr[0]) {
		do_warn("# of bmap records in inode %llu greater than max "
			"(%u, max - %u)\n",
			ino, INT_GET(block->bb_numrecs, ARCH_CONVERT),
			mp->m_bmap_dmxr[0]);
		libxfs_putbuf(bp);
		return(NULLDFSBNO);
	}
	if (verbose && INT_GET(block->bb_numrecs, ARCH_CONVERT) <
					mp->m_bmap_dmnr[0])
		do_warn("- # of bmap records in inode %llu < min "
			"(%u, min - %u), continuing...\n",
			ino, INT_GET(block->bb_numrecs, ARCH_CONVERT),
			mp->m_bmap_dmnr[0]);

	rec = (xfs_bmbt_rec_32_t *)XFS_BTREE_REC_ADDR(mp->m_sb.sb_blocksize,
		xfs_bmbt, block, 1, mp->m_bmap_dmxr[0]);
	for (i = 0; i < INT_GET(block->bb_numrecs, ARCH_CONVERT); i++)  {
		convert_extent(rec + i, &fbno, &fsbno, &bcnt, &flag);

		if (fbno <= bno && bno < fbno + bcnt)  {
			final_fsbno = bno - fbno + fsbno;
			break;
		}
	}
	libxfs_putbuf(bp);

	if (final_fsbno == NULLDFSBNO)
		do_warn("could not map block %llu\n", bno);

	return(final_fsbno);
}

/*
 * this could be smarter.  maybe we should have an open inode
 * routine that would get the inode buffer and return back
 * an inode handle.  I'm betting for the moment that this
 * is used only by the directory and attribute checking code
 * and that the avl tree find and buffer cache search are
 * relatively cheap.  If they're too expensive, we'll just
 * have to fix this and add an inode handle to the da btree
 * cursor.
 *
 * caller is responsible for checking doubly referenced blocks
 * and references to holes
 */
xfs_dfsbno_t
get_bmapi(xfs_mount_t *mp, xfs_dinode_t *dino_p,
		xfs_ino_t ino_num, xfs_dfiloff_t bno, int whichfork)
{
	xfs_dfsbno_t		fsbno;

	switch (XFS_DFORK_FORMAT_ARCH(dino_p, whichfork, ARCH_CONVERT)) {
	case XFS_DINODE_FMT_EXTENTS:
		fsbno = getfunc_extlist(mp, ino_num, dino_p, bno, whichfork);
		break;
	case XFS_DINODE_FMT_BTREE:
		fsbno = getfunc_btree(mp, ino_num, dino_p, bno, whichfork); 
		break;
	case XFS_DINODE_FMT_LOCAL:
		do_error("get_bmapi() called for local inode %llu\n", ino_num);
		fsbno = NULLDFSBNO;
		break;
	default:
		/*
		 * shouldn't happen
		 */
		do_error("bad inode format for inode %llu\n", ino_num);
		fsbno = NULLDFSBNO;
	}

	return(fsbno);
}

/*
 * higher level inode processing stuff starts here:
 * first, one utility routine for each type of inode
 */

/*
 * return 1 if inode should be cleared, 0 otherwise
 */
/* ARGSUSED */
int
process_btinode(
	xfs_mount_t		*mp,
	xfs_agnumber_t		agno,
	xfs_agino_t		ino,
	xfs_dinode_t		*dip,
	int			type,
	int			*dirty,
	xfs_drfsbno_t		*tot,
	__uint64_t		*nex,
	blkmap_t		**blkmapp,
	int			whichfork,
	int			check_dups)
{
	xfs_bmdr_block_t	*dib;
	xfs_dfiloff_t		last_key;
	xfs_dfiloff_t		first_key = 0;
	xfs_ino_t		lino;
	xfs_bmbt_ptr_t		*pp;
	xfs_bmbt_key_t		*pkey;
	char			*forkname;
	int			i;
	bmap_cursor_t		cursor;

	dib = (xfs_bmdr_block_t *)XFS_DFORK_PTR_ARCH(dip, whichfork, ARCH_CONVERT);
	lino = XFS_AGINO_TO_INO(mp, agno, ino);
	*tot = 0;
	*nex = 0;

	if (whichfork == XFS_DATA_FORK)
		forkname = "data";
	else
		forkname = "attr";

	if (INT_GET(dib->bb_level, ARCH_CONVERT) == 0) {
		/*
		 * This should never happen since a btree inode
		 * has to have at least one other block in the
		 * bmap in addition to the root block in the
		 * inode's data fork.
		 *
		 * XXX - if we were going to fix up the inode,
		 * we'd try to treat the fork as an interior
		 * node and see if we could get an accurate
		 * level value from one of the blocks pointed
		 * to by the pointers in the fork.  For now
		 * though, we just bail (and blow out the inode).
		 */
		do_warn("bad level 0 in inode %llu bmap btree root block\n",
			XFS_AGINO_TO_INO(mp, agno, ino));
		return(1);
	}
	/*
	 * use bmdr/dfork_dsize since the root block is in the data fork
	 */
	init_bm_cursor(&cursor, INT_GET(dib->bb_level, ARCH_CONVERT) + 1);

	if (XFS_BMDR_SPACE_CALC(INT_GET(dib->bb_numrecs, ARCH_CONVERT)) >
			((whichfork == XFS_DATA_FORK) ?
			XFS_DFORK_DSIZE_ARCH(dip, mp, ARCH_CONVERT) :
			XFS_DFORK_ASIZE_ARCH(dip, mp, ARCH_CONVERT)))  {
		do_warn(
"indicated size of %s btree root (%d bytes) > space in inode %llu %s fork\n",
			forkname, XFS_BMDR_SPACE_CALC(INT_GET(dib->bb_numrecs, ARCH_CONVERT)),
			lino, forkname);
		return(1);
	}

	pp = XFS_BTREE_PTR_ADDR(XFS_DFORK_SIZE_ARCH(dip, mp, whichfork, ARCH_CONVERT),
		xfs_bmdr, dib, 1,
		XFS_BTREE_BLOCK_MAXRECS(XFS_DFORK_SIZE_ARCH(dip, mp, whichfork, ARCH_CONVERT),
		xfs_bmdr, 0));
	pkey = XFS_BTREE_KEY_ADDR(XFS_DFORK_SIZE_ARCH(dip, mp, whichfork, ARCH_CONVERT),
		xfs_bmdr, dib, 1,
		XFS_BTREE_BLOCK_MAXRECS(XFS_DFORK_SIZE_ARCH(dip, mp, whichfork, ARCH_CONVERT),
		xfs_bmdr, 0));

	last_key = NULLDFILOFF;

	for (i = 0; i < INT_GET(dib->bb_numrecs, ARCH_CONVERT); i++)  {
		/*
		 * XXX - if we were going to do more to fix up the inode
		 * btree, we'd do it right here.  For now, if there's a
		 * problem, we'll bail out and presumably clear the inode.
		 */
		if (!verify_dfsbno(mp, INT_GET(pp[i], ARCH_CONVERT)))  {
			do_warn("bad bmap btree ptr 0x%llx in ino %llu\n",
				INT_GET(pp[i], ARCH_CONVERT), lino);
			return(1);
		}

		if (scan_lbtree((xfs_dfsbno_t)INT_GET(pp[i], ARCH_CONVERT), INT_GET(dib->bb_level, ARCH_CONVERT),
				    scanfunc_bmap, type, whichfork,
				    lino, tot, nex, blkmapp, &cursor,
				    1, check_dups))
			return(1);
		/*
		 * fix key (offset) mismatches between the keys in root
		 * block records and the first key of each child block.
		 * fixes cases where entries have been shifted between
		 * blocks but the parent hasn't been updated
		 */
		if (check_dups == 0 &&
				cursor.level[INT_GET(dib->bb_level, ARCH_CONVERT)-1].first_key !=
					INT_GET(pkey[i].br_startoff, ARCH_CONVERT))  {
			if (!no_modify)  {
				do_warn(
"correcting key in bmbt root (was %llu, now %llu) in inode %llu %s fork\n",
					INT_GET(pkey[i].br_startoff, ARCH_CONVERT),
					cursor.level[INT_GET(dib->bb_level, ARCH_CONVERT)-1].first_key,
					XFS_AGINO_TO_INO(mp, agno, ino),
					forkname);
				*dirty = 1;
				INT_SET(pkey[i].br_startoff, ARCH_CONVERT, cursor.level[INT_GET(dib->bb_level, ARCH_CONVERT)-1].first_key);
			} else  {
				do_warn(
"bad key in bmbt root (is %llu, would reset to %llu) in inode %llu %s fork\n",
					INT_GET(pkey[i].br_startoff, ARCH_CONVERT),
					cursor.level[INT_GET(dib->bb_level, ARCH_CONVERT)-1].first_key,
					XFS_AGINO_TO_INO(mp, agno, ino),
					forkname);
			}
		}
		/*
		 * make sure that keys are in ascending order.  blow out
		 * inode if the ordering doesn't hold
		 */
		if (check_dups == 0)  {
			if (last_key != NULLDFILOFF && last_key >=
			    cursor.level[INT_GET(dib->bb_level, ARCH_CONVERT)-1].first_key)  {
				do_warn(
		"out of order bmbt root key %llu in inode %llu %s fork\n",
					first_key,
					XFS_AGINO_TO_INO(mp, agno, ino),
					forkname);
				return(1);
			}
			last_key = cursor.level[INT_GET(dib->bb_level, ARCH_CONVERT)-1].first_key;
		}
	}
	/*
	 * Check that the last child block's forward sibling pointer
	 * is NULL.
	 */
	if (check_dups == 0 &&
		cursor.level[0].right_fsbno != NULLDFSBNO)  {
		do_warn(
	"bad fwd (right) sibling pointer (saw %llu should be NULLDFSBNO)\n",
			cursor.level[0].right_fsbno);
		do_warn(
		"\tin inode %u (%s fork) bmap btree block %llu\n",
			XFS_AGINO_TO_INO(mp, agno, ino), forkname,
			cursor.level[0].fsbno);
		return(1);
	}
	
	return(0);
}

/*
 * return 1 if inode should be cleared, 0 otherwise
 */
/* ARGSUSED */
int
process_exinode(
	xfs_mount_t		*mp,
	xfs_agnumber_t		agno,
	xfs_agino_t		ino,
	xfs_dinode_t		*dip,
	int			type,
	int			*dirty,
	xfs_drfsbno_t		*tot,
	__uint64_t		*nex,
	blkmap_t		**blkmapp,
	int			whichfork,
	int			check_dups)
{
	xfs_ino_t		lino;
	xfs_bmbt_rec_32_t	*rp;
	xfs_dfiloff_t		first_key;
	xfs_dfiloff_t		last_key;

	lino = XFS_AGINO_TO_INO(mp, agno, ino);
	rp = (xfs_bmbt_rec_32_t *)XFS_DFORK_PTR_ARCH(dip, whichfork, ARCH_CONVERT);
	*tot = 0;
	*nex = XFS_DFORK_NEXTENTS_ARCH(dip, whichfork, ARCH_CONVERT);
	/*
	 * XXX - if we were going to fix up the btree record,
	 * we'd do it right here.  For now, if there's a problem,
	 * we'll bail out and presumably clear the inode.
	 */
	if (check_dups == 0)
		return(process_bmbt_reclist(mp, rp, *nex, type, lino,
					tot, blkmapp, &first_key, &last_key,
					whichfork));
	else
		return(scan_bmbt_reclist(mp, rp, *nex, type, lino, tot,
					whichfork));
}

/*
 * return 1 if inode should be cleared, 0 otherwise
 */
/* ARGSUSED */
int
process_lclinode(
	xfs_mount_t		*mp,
	xfs_agnumber_t		agno,
	xfs_agino_t		ino,
	xfs_dinode_t		*dip,
	int			type,
	int			*dirty,
	xfs_drfsbno_t		*tot,
	__uint64_t		*nex,
	blkmap_t		**blkmapp,
	int			whichfork,
	int			check_dups)
{
	xfs_attr_shortform_t	*asf;
	xfs_dinode_core_t	*dic;
	xfs_ino_t		lino;

	*tot = 0;
	*nex = 0;	/* local inodes have 0 extents */

	dic = &dip->di_core;
	lino = XFS_AGINO_TO_INO(mp, agno, ino);
	if (whichfork == XFS_DATA_FORK &&
	    INT_GET(dic->di_size, ARCH_CONVERT) > XFS_DFORK_DSIZE_ARCH(dip, mp, ARCH_CONVERT)) {
		do_warn(
	"local inode %llu data fork is too large (size = %lld, max = %d)\n",
			lino, INT_GET(dic->di_size, ARCH_CONVERT), XFS_DFORK_DSIZE_ARCH(dip, mp, ARCH_CONVERT));
		return(1);
	} else if (whichfork == XFS_ATTR_FORK) {
		asf = (xfs_attr_shortform_t *) XFS_DFORK_APTR_ARCH(dip, ARCH_CONVERT);
		if (INT_GET(asf->hdr.totsize, ARCH_CONVERT) > XFS_DFORK_ASIZE_ARCH(dip, mp, ARCH_CONVERT)) {
			do_warn(
		"local inode %llu attr fork too large (size %d, max = %d)\n",
					lino, INT_GET(asf->hdr.totsize, ARCH_CONVERT),
					XFS_DFORK_ASIZE_ARCH(dip, mp, ARCH_CONVERT));
			return(1);
		}
		if (INT_GET(asf->hdr.totsize, ARCH_CONVERT) < sizeof(xfs_attr_sf_hdr_t)) {
			do_warn(
		"local inode %llu attr too small (size = %d, min size = %d)\n",
					lino, INT_GET(asf->hdr.totsize, ARCH_CONVERT),
					sizeof(xfs_attr_sf_hdr_t));
			return(1);
		}
	}

	return(0);
}

int
process_symlink_extlist(xfs_mount_t *mp, xfs_ino_t lino, xfs_dinode_t *dino)
{
	xfs_dfsbno_t		start;		/* start */
	xfs_dfilblks_t		cnt;		/* count */
	xfs_dfiloff_t		offset;		/* offset */
	xfs_dfiloff_t		expected_offset;
	xfs_bmbt_rec_32_t	*rp;
	int			numrecs;
	int			i;
	int			max_blocks;
	int			whichfork = XFS_DATA_FORK;
	int			flag;

	if (INT_GET(dino->di_core.di_size, ARCH_CONVERT) <= XFS_DFORK_SIZE_ARCH(dino, mp, whichfork, ARCH_CONVERT))  {
		if (dino->di_core.di_format == XFS_DINODE_FMT_LOCAL)  {
			return(0);
		} else  {
			do_warn(
"mismatch between format (%d) and size (%lld) in symlink ino %llu\n",
				dino->di_core.di_format,
				INT_GET(dino->di_core.di_size, ARCH_CONVERT),
				lino);
			return(1);
		}
	} else if (dino->di_core.di_format == XFS_DINODE_FMT_LOCAL)  {
		do_warn(
"mismatch between format (%d) and size (%lld) in symlink inode %llu\n",
				dino->di_core.di_format,
				INT_GET(dino->di_core.di_size, ARCH_CONVERT),
				lino);
		return(1);
	}

	rp = (xfs_bmbt_rec_32_t *)XFS_DFORK_PTR_ARCH(dino, whichfork, ARCH_CONVERT);
	numrecs = XFS_DFORK_NEXTENTS_ARCH(dino, whichfork, ARCH_CONVERT);

	/*
	 * the max # of extents in a symlink inode is equal to the
	 * number of max # of blocks required to store the symlink 
	 */
	if (numrecs > max_symlink_blocks)  {
		do_warn(
		"bad number of extents (%d) in symlink %llu data fork\n",
			numrecs, lino);
		return(1);
	}

	max_blocks = max_symlink_blocks;
	expected_offset = 0;

	for (i = 0; numrecs > 0; i++, numrecs--)  {
		convert_extent(rp, &offset, &start, &cnt, &flag);

		if (offset != expected_offset)  {
			do_warn(
		"bad extent #%d offset (%llu) in symlink %llu data fork\n",
				i, offset, lino);
			return(1);
		}
		if (cnt == 0 || cnt > max_blocks)  {
			do_warn(
		"bad extent #%d count (%llu) in symlink %llu data fork\n",
				i, cnt, lino);
			return(1);
		}

		max_blocks -= cnt;
		expected_offset += cnt;
	}

	return(0);
}

/*
 * takes a name and length and returns 1 if the name contains
 * a \0, returns 0 otherwise
 */
int
null_check(char *name, int length)
{
	int i;

	ASSERT(length < MAXPATHLEN);

	for (i = 0; i < length; i++, name++)  {
		if (*name == '\0')
			return(1);
	}

	return(0);
}

/*
 * like usual, returns 0 if everything's ok and 1 if something's
 * bogus
 */
int
process_symlink(xfs_mount_t *mp, xfs_ino_t lino, xfs_dinode_t *dino,
		blkmap_t *blkmap)
{
	xfs_dfsbno_t		fsbno;
	xfs_dinode_core_t	*dinoc = &dino->di_core;
	xfs_buf_t		*bp = NULL;
	char			*symlink, *cptr, *buf_data;
	int			i, size, amountdone;
	char			data[MAXPATHLEN];

	/*
	 * check size against kernel symlink limits.  we know
	 * size is consistent with inode storage format -- e.g.
	 * the inode is structurally ok so we don't have to check
	 * for that
	 */
	if (INT_GET(dinoc->di_size, ARCH_CONVERT) >= MAXPATHLEN)  {
		do_warn("symlink in inode %llu too long (%lld chars)\n",
			lino, INT_GET(dinoc->di_size, ARCH_CONVERT));
		return(1);
	}

	/*
	 * have to check symlink component by component.
	 * get symlink contents into data area
	 */
	symlink = &data[0];
	if (INT_GET(dinoc->di_size, ARCH_CONVERT)
			<= XFS_DFORK_DSIZE_ARCH(dino, mp, ARCH_CONVERT))  {
		/*
		 * local symlink, just copy the symlink out of the
		 * inode into the data area
		 */
		bcopy((char *)XFS_DFORK_DPTR_ARCH(dino, ARCH_CONVERT),
			symlink, INT_GET(dinoc->di_size, ARCH_CONVERT));
	} else {
		/*
		 * stored in a meta-data file, have to bmap one block
		 * at a time and copy the symlink into the data area
		 */
		i = size = amountdone = 0;
		cptr = symlink;

		while (amountdone < INT_GET(dinoc->di_size, ARCH_CONVERT)) {
			fsbno = blkmap_get(blkmap, i);
			if (fsbno != NULLDFSBNO)
				bp = libxfs_readbuf(mp->m_dev,
						XFS_FSB_TO_DADDR(mp, fsbno),
						XFS_FSB_TO_BB(mp, 1), 0);
			if (!bp || fsbno == NULLDFSBNO) {
				do_warn("cannot read inode %llu, file block %d,"
					" disk block %llu\n", lino, i, fsbno);
				return(1);
			}

			buf_data = (char *)XFS_BUF_PTR(bp);
			size = MIN(INT_GET(dinoc->di_size, ARCH_CONVERT)
				- amountdone, (int)XFS_FSB_TO_BB(mp, 1)*BBSIZE);
			bcopy(buf_data, cptr, size);
			cptr += size;
			amountdone += size;
			i++;
			libxfs_putbuf(bp);
		}
	}
	data[INT_GET(dinoc->di_size, ARCH_CONVERT)] = '\0';

	/*
	 * check for nulls
	 */
	if (null_check(symlink, (int) INT_GET(dinoc->di_size, ARCH_CONVERT)))  {
		do_warn("found illegal null character in symlink inode %llu\n",
			lino);
		return(1);
	}

	/*
	 * check for any component being too long
	 */
	if (INT_GET(dinoc->di_size, ARCH_CONVERT) >= MAXNAMELEN)  {
		cptr = strchr(symlink, '/');

		while (cptr != NULL)  {
			if (cptr - symlink >= MAXNAMELEN)  {
				do_warn(
				"component of symlink in inode %llu too long\n",
					lino);
				return(1);
			}
			symlink = cptr + 1;
			cptr = strchr(symlink, '/');
		}

		if (strlen(symlink) >= MAXNAMELEN)  {
			do_warn("component of symlink in inode %llu too long\n",
				lino);
			return(1);
		}
	}

	return(0);
}

/*
 * called to process the set of misc inode special inode types
 * that have no associated data storage (fifos, pipes, devices, etc.).
 */
/* ARGSUSED */
int
process_misc_ino_types(xfs_mount_t	*mp,
			xfs_dinode_t	*dino,
			xfs_ino_t	lino,
			int		type)
{
	/*
	 * disallow mountpoint inodes until such time as the
	 * kernel actually allows them to be created (will
	 * probably require a superblock version rev, sigh).
	 */
	if (type == XR_INO_MOUNTPOINT)  {
		do_warn("inode %llu has bad inode type (IFMNT)\n", lino);
		return(1);
	}

	/*
	 * must also have a zero size
	 */
	if (INT_GET(dino->di_core.di_size, ARCH_CONVERT) != 0)  {
		switch (type)  {
		case XR_INO_CHRDEV:
			do_warn("size of character device inode %llu != 0 "
				"(%lld bytes)\n", lino,
				INT_GET(dino->di_core.di_size, ARCH_CONVERT));
			break;
		case XR_INO_BLKDEV:
			do_warn("size of block device inode %llu != 0 "
				"(%lld bytes)\n", lino,
				INT_GET(dino->di_core.di_size, ARCH_CONVERT));
			break;
		case XR_INO_SOCK:
			do_warn("size of socket inode %llu != 0 "
				"(%lld bytes)\n", lino,
				INT_GET(dino->di_core.di_size, ARCH_CONVERT));
			break;
		case XR_INO_FIFO:
			do_warn("size of fifo inode %llu != 0 "
				"(%lld bytes)\n", lino,
				INT_GET(dino->di_core.di_size, ARCH_CONVERT));
			break;
		default:
			do_warn("Internal error - process_misc_ino_types, "
				"illegal type %d\n", type);
			abort();
		}

		return(1);
	}

	return(0);
}

int
process_misc_ino_types_blocks(xfs_drfsbno_t totblocks, xfs_ino_t lino, int type)
{
	/*
	 * you can not enforce all misc types have zero data fork blocks
	 * by checking dino->di_core.di_nblocks because atotblocks (attribute
	 * blocks) are part of nblocks. We must check this later when atotblocks
	 * has been calculated or by doing a simple check that anExtents == 0. 
	 * We must also guarantee that totblocks is 0. Thus nblocks checking
	 * will be done later in process_dinode_int for misc types.
	 */

	if (totblocks != 0)  {
		switch (type)  {
		case XR_INO_CHRDEV:
			do_warn(
		"size of character device inode %llu != 0 (%llu blocks)\n",
				lino, totblocks);
			break;
		case XR_INO_BLKDEV:
			do_warn(
		"size of block device inode %llu != 0 (%llu blocks)\n",
				lino, totblocks);
			break;
		case XR_INO_SOCK:
			do_warn(
		"size of socket inode %llu != 0 (%llu blocks)\n",
				lino, totblocks);
			break;
		case XR_INO_FIFO:
			do_warn(
		"size of fifo inode %llu != 0 (%llu blocks)\n",
				lino, totblocks);
			break;
		default:
			return(0);
		}
		return(1);
	}
	return (0);
}

/*
 * returns 0 if the inode is ok, 1 if the inode is corrupt
 * check_dups can be set to 1 *only* when called by the
 * first pass of the duplicate block checking of phase 4.
 * *dirty is set > 0 if the dinode has been altered and
 * needs to be written out.
 *
 * for detailed, info, look at process_dinode() comments.
 */
/* ARGSUSED */
int
process_dinode_int(xfs_mount_t *mp,
		xfs_dinode_t *dino,
		xfs_agnumber_t agno,
		xfs_agino_t ino,
		int was_free,		/* 1 if inode is currently free */
		int *dirty,		/* out == > 0 if inode is now dirty */
		int *cleared,		/* out == 1 if inode was cleared */
		int *used,		/* out == 1 if inode is in use */
		int verify_mode,	/* 1 == verify but don't modify inode */
		int uncertain,		/* 1 == inode is uncertain */
		int ino_discovery,	/* 1 == check dirs for unknown inodes */
		int check_dups,		/* 1 == check if inode claims
					 * duplicate blocks		*/
		int extra_attr_check, /* 1 == do attribute format and value checks */
		int *isa_dir,		/* out == 1 if inode is a directory */
		xfs_ino_t *parent)	/* out -- parent if ino is a dir */
{
	xfs_drfsbno_t		totblocks = 0;
	xfs_drfsbno_t		atotblocks = 0;
	xfs_dinode_core_t	*dinoc;
	char			*rstring;
	int			type;
	int			rtype;
	int			do_rt;
	int			err;
	int			retval = 0;
	__uint64_t		nextents;
	__uint64_t		anextents;
	xfs_ino_t		lino;
	const int		is_free = 0;
	const int		is_used = 1;
	int			repair = 0;
	blkmap_t		*ablkmap = NULL;
	blkmap_t		*dblkmap = NULL;
	static char		okfmts[] = {
		0,				/* free inode */
		1 << XFS_DINODE_FMT_DEV,	/* FIFO */
		1 << XFS_DINODE_FMT_DEV,	/* CHR */
		0,				/* type 3 unused */
		(1 << XFS_DINODE_FMT_LOCAL) |
		(1 << XFS_DINODE_FMT_EXTENTS) |
		(1 << XFS_DINODE_FMT_BTREE),	/* DIR */
		0,				/* type 5 unused */
		1 << XFS_DINODE_FMT_DEV,	/* BLK */
		0,				/* type 7 unused */
		(1 << XFS_DINODE_FMT_EXTENTS) |
		(1 << XFS_DINODE_FMT_BTREE),	/* REG */
		0,				/* type 9 unused */
		(1 << XFS_DINODE_FMT_LOCAL) |
		(1 << XFS_DINODE_FMT_EXTENTS),	/* LNK */
		0,				/* type 11 unused */
		1 << XFS_DINODE_FMT_DEV,	/* SOCK */
		0,				/* type 13 unused */
		1 << XFS_DINODE_FMT_UUID,	/* MNT */
		0				/* type 15 unused */
	};

	retval = 0;
	totblocks = atotblocks = 0;
	*dirty = *isa_dir = *cleared = 0;
	*used = is_used;
	type = rtype = XR_INO_UNKNOWN;
	rstring = NULL;
	do_rt = 0;

	dinoc = &dino->di_core;
	lino = XFS_AGINO_TO_INO(mp, agno, ino);

	/*
	 * if in verify mode, don't modify the inode.
	 *
	 * if correcting, reset stuff that has known values
	 *
	 * if in uncertain mode, be silent on errors since we're
	 * trying to find out if these are inodes as opposed
	 * to assuming that they are.  Just return the appropriate
	 * return code in that case.
	 */

	if (INT_GET(dinoc->di_magic, ARCH_CONVERT) != XFS_DINODE_MAGIC)  {
		retval++;
		if (!verify_mode)  {
			do_warn("bad magic number 0x%x on inode %llu, ", 
				INT_GET(dinoc->di_magic, ARCH_CONVERT), lino);
			if (!no_modify)  {
				do_warn("resetting magic number\n");
				*dirty = 1;
				INT_SET(dinoc->di_magic, ARCH_CONVERT, XFS_DINODE_MAGIC);
			} else  {
				do_warn("would reset magic number\n");
			}
		} else if (!uncertain) {
			do_warn("bad magic number 0x%x on inode %llu\n", 
				INT_GET(dinoc->di_magic, ARCH_CONVERT), lino);
		}
	}

	if (!XFS_DINODE_GOOD_VERSION(dinoc->di_version) ||
	    (!fs_inode_nlink && dinoc->di_version > XFS_DINODE_VERSION_1))  {
		retval++;
		if (!verify_mode)  {
			do_warn("bad version number 0x%x on inode %llu, ", 
				dinoc->di_version, lino);
			if (!no_modify)  {
				do_warn("resetting version number\n");
				*dirty = 1;
				dinoc->di_version = (fs_inode_nlink) ?
					XFS_DINODE_VERSION_2 :
					XFS_DINODE_VERSION_1;
			} else  {
				do_warn("would reset version number\n");
			}
		} else  if (!uncertain) {
			do_warn("bad version number 0x%x on inode %llu\n", 
				dinoc->di_version, lino);
		}
	}

	/*
	 * blow out of here if the inode size is < 0
	 */
	if (INT_GET(dinoc->di_size, ARCH_CONVERT) < 0)  {
		retval++;
		if (!verify_mode)  {
			do_warn("bad (negative) size %lld on inode %llu\n",
				INT_GET(dinoc->di_size, ARCH_CONVERT), lino);
			if (!no_modify)  {
				*dirty += clear_dinode(mp, dino, lino);
				*cleared = 1;
			} else  {
				*dirty = 1;
				*cleared = 1;
			}
			*used = is_free;
		} else if (!uncertain)  {
			do_warn("bad (negative) size %lld on inode %llu\n",
				INT_GET(dinoc->di_size, ARCH_CONVERT), lino);
		}

		return(1);
	}

	/*
	 * was_free value is not meaningful if we're in verify mode
	 */
	if (!verify_mode && INT_GET(dinoc->di_mode, ARCH_CONVERT) == 0 && was_free == 1)  {
		/*
		 * easy case, inode free -- inode and map agree, clear
		 * it just in case to ensure that format, etc. are
		 * set correctly
		 */
		if (!no_modify)  {
			err =  clear_dinode(mp, dino, lino);
			if (err)  {
				*dirty = 1;
				*cleared = 1;
			}
		}
		*used = is_free;
		return(0);
	} else if (!verify_mode && INT_GET(dinoc->di_mode, ARCH_CONVERT) == 0 && was_free == 0)  {
		/*
		 * the inode looks free but the map says it's in use.
		 * clear the inode just to be safe and mark the inode
		 * free.
		 */
		do_warn("imap claims a free inode %llu is in use, ", lino);

		if (!no_modify)  {
			do_warn("correcting imap and clearing inode\n");

			err =  clear_dinode(mp, dino, lino);
			if (err)  {
				retval++;
				*dirty = 1;
				*cleared = 1;
			}
		} else  {
			do_warn("would correct imap and clear inode\n");

			*dirty = 1;
			*cleared = 1;
		}

		*used = is_free;

		return(retval > 0 ? 1 : 0);
	}

	/*
	 * because of the lack of any write ordering guarantee, it's
	 * possible that the core got updated but the forks didn't.
	 * so rather than be ambitious (and probably incorrect),
	 * if there's an inconsistency, we get conservative and 
	 * just pitch the file.  blow off checking formats of
	 * free inodes since technically any format is legal
	 * as we reset the inode when we re-use it.
	 */
	if (INT_GET(dinoc->di_mode, ARCH_CONVERT) != 0 &&
		((((INT_GET(dinoc->di_mode, ARCH_CONVERT) & IFMT) >> 12) > 15) ||
		dinoc->di_format < XFS_DINODE_FMT_DEV ||
		dinoc->di_format > XFS_DINODE_FMT_UUID ||
			(!(okfmts[(INT_GET(dinoc->di_mode, ARCH_CONVERT) & IFMT) >> 12] &
			  (1 << dinoc->di_format))))) {
		/* bad inode format */
		retval++;
		if (!uncertain)
			do_warn("bad inode format in inode %llu\n", lino);
		if (!verify_mode)  {
			if (!no_modify)  {
				*dirty += clear_dinode(mp, dino, lino);
				ASSERT(*dirty > 0);
			}
		}
		*cleared = 1;
		*used = is_free;

		return(retval > 0 ? 1 : 0);
	}

	if (verify_mode)
		return(retval > 0 ? 1 : 0);

	/*
	 * clear the next unlinked field if necessary on a good
	 * inode only during phase 4 -- when checking for inodes
	 * referencing duplicate blocks.  then it's safe because
	 * we've done the inode discovery and have found all the inodes
	 * we're going to find.  check_dups is set to 1 only during
	 * phase 4.  Ugly.
	 */
	if (check_dups && !no_modify)
		*dirty += clear_dinode_unlinked(mp, dino);

	/* set type and map type info */

	switch (INT_GET(dinoc->di_mode, ARCH_CONVERT) & IFMT) {
	case IFDIR:
		type = XR_INO_DIR;
		*isa_dir = 1;
		break;
	case IFREG:
		if (INT_GET(dinoc->di_flags, ARCH_CONVERT) & XFS_DIFLAG_REALTIME)
			type = XR_INO_RTDATA;
		else if (lino == mp->m_sb.sb_rbmino)
			type = XR_INO_RTBITMAP;
		else if (lino == mp->m_sb.sb_rsumino)
			type = XR_INO_RTSUM;
		else
			type = XR_INO_DATA;
		break;
	case IFLNK:
		type = XR_INO_SYMLINK;
		break;
	case IFCHR:
		type = XR_INO_CHRDEV;
		break;
	case IFBLK:
		type = XR_INO_BLKDEV;
		break;
	case IFSOCK:
		type = XR_INO_SOCK;
		break;
	case IFIFO:
		type = XR_INO_FIFO;
		break;
	case IFMNT:
		type = XR_INO_MOUNTPOINT;
		break;
	default:
		type = XR_INO_UNKNOWN;
		do_warn("Unexpected inode type %#o inode %llu\n",
			(int) (INT_GET(dinoc->di_mode, ARCH_CONVERT) & IFMT), lino);
		abort();
		break;
	}

	/*
	 * type checks for root, realtime inodes, and quota inodes
	 */
	if (lino == mp->m_sb.sb_rootino && type != XR_INO_DIR)  {
		do_warn("bad inode type for root inode %llu, ", lino);
		type = XR_INO_DIR;

		if (!no_modify)  {
			do_warn("resetting to directory\n");
			INT_MOD_EXPR(dinoc->di_mode, ARCH_CONVERT, &= ~(INT_GET(dinoc->di_mode, ARCH_CONVERT) & IFMT));
			INT_MOD_EXPR(dinoc->di_mode, ARCH_CONVERT, |= INT_GET(dinoc->di_mode, ARCH_CONVERT) & IFDIR);
		} else  {
			do_warn("would reset to directory\n");
		}
	} else if (lino == mp->m_sb.sb_rsumino)  {
		do_rt = 1;
		rstring = "summary";
		rtype = XR_INO_RTSUM;
	} else if (lino == mp->m_sb.sb_rbmino)  {
		do_rt = 1;
		rstring = "bitmap";
		rtype = XR_INO_RTBITMAP;
	} else if (lino == mp->m_sb.sb_uquotino)  {
		if (type != XR_INO_DATA)  {
			do_warn("user quota inode has bad type 0x%x\n",
				INT_GET(dinoc->di_mode, ARCH_CONVERT) & IFMT);

			if (!no_modify)  {
				*dirty += clear_dinode(mp, dino, lino);
				ASSERT(*dirty > 0);
			}

			*cleared = 1;
			*used = is_free;
			*isa_dir = 0;

			mp->m_sb.sb_uquotino = NULLFSINO;

			return(1);
		}
	} else if (lino == mp->m_sb.sb_gquotino)  {
		if (type != XR_INO_DATA)  {
			do_warn("group quota inode has bad type 0x%x\n",
				INT_GET(dinoc->di_mode, ARCH_CONVERT) & IFMT);

			if (!no_modify)  {
				*dirty += clear_dinode(mp, dino, lino);
				ASSERT(*dirty > 0);
			}

			*cleared = 1;
			*used = is_free;
			*isa_dir = 0;

			mp->m_sb.sb_gquotino = NULLFSINO;

			return(1);
		}
	}

	if (do_rt && type != rtype)  {
		type = XR_INO_DATA;

		do_warn("bad inode type for realtime %s inode %llu, ",
			rstring, lino);

		if (!no_modify)  {
			do_warn("resetting to regular file\n");
			INT_MOD_EXPR(dinoc->di_mode, ARCH_CONVERT, &= ~(INT_GET(dinoc->di_mode, ARCH_CONVERT) & IFMT));
			INT_MOD_EXPR(dinoc->di_mode, ARCH_CONVERT, |= INT_GET(dinoc->di_mode, ARCH_CONVERT) & IFREG);
		} else  {
			do_warn("would reset to regular file\n");
		}
	}

	/*
	 * only realtime inodes should have extsize set
	 */
	if (type != XR_INO_RTDATA && INT_GET(dinoc->di_extsize, ARCH_CONVERT) != 0)  {
		do_warn(
"bad non-zero extent size value %u for non-realtime inode %llu,",
			INT_GET(dinoc->di_extsize, ARCH_CONVERT), lino);

		if (!no_modify)  {
			do_warn("resetting to zero\n");
			INT_ZERO(dinoc->di_extsize, ARCH_CONVERT);
			*dirty = 1;
		} else  {
			do_warn("would reset to zero\n");
		}
	}

	/*
	 * for realtime inodes, check sizes to see that
	 * they are consistent with the # of realtime blocks.
	 * also, verify that they contain only one extent and
	 * are extent format files.  If anything's wrong, clear
	 * the inode -- we'll recreate it in phase 6.
	 */
	if (do_rt && INT_GET(dinoc->di_size, ARCH_CONVERT)
			!= mp->m_sb.sb_rbmblocks * mp->m_sb.sb_blocksize)  {
		do_warn("bad size %llu for realtime %s inode %llu\n",
			INT_GET(dinoc->di_size, ARCH_CONVERT), rstring, lino);

		if (!no_modify)  {
			*dirty += clear_dinode(mp, dino, lino);
			ASSERT(*dirty > 0);
		}

		*cleared = 1;
		*used = is_free;
		*isa_dir = 0;

		return(1);
	}

	if (do_rt && mp->m_sb.sb_rblocks == 0 && INT_GET(dinoc->di_nextents, ARCH_CONVERT) != 0)  {
		do_warn("bad # of extents (%u) for realtime %s inode %llu\n",
			INT_GET(dinoc->di_nextents, ARCH_CONVERT), rstring, lino);

		if (!no_modify)  {
			*dirty += clear_dinode(mp, dino, lino);
			ASSERT(*dirty > 0);
		}

		*cleared = 1;
		*used = is_free;
		*isa_dir = 0;

		return(1);
	}

	/*
	 * Setup nextents and anextents for blkmap_alloc calls.
	 */
	nextents = INT_GET(dinoc->di_nextents, ARCH_CONVERT);
	if (nextents > INT_GET(dinoc->di_nblocks, ARCH_CONVERT) || nextents > XFS_MAX_INCORE_EXTENTS)
		nextents = 1;
	anextents = INT_GET(dinoc->di_anextents, ARCH_CONVERT);
	if (anextents > INT_GET(dinoc->di_nblocks, ARCH_CONVERT) || anextents > XFS_MAX_INCORE_EXTENTS)
		anextents = 1;

	/*
	 * general size/consistency checks:
	 *
	 * if the size <= size of the data fork, directories  must be
	 * local inodes unlike regular files which would be extent inodes.
	 * all the other mentioned types have to have a zero size value.
	 *
	 * if the size and format don't match, get out now rather than
	 * risk trying to process a non-existent extents or btree
	 * type data fork.
	 */
	switch (type)  {
	case XR_INO_DIR:
		if (INT_GET(dinoc->di_size, ARCH_CONVERT) <= XFS_DFORK_DSIZE_ARCH(dino, mp, ARCH_CONVERT)
				&& dinoc->di_format != XFS_DINODE_FMT_LOCAL)  {
			do_warn(
"mismatch between format (%d) and size (%lld) in directory ino %llu\n",
				dinoc->di_format,
				INT_GET(dinoc->di_size, ARCH_CONVERT),
				lino);

			if (!no_modify)  {
				*dirty += clear_dinode(mp,
						dino, lino);
				ASSERT(*dirty > 0);
			}

			*cleared = 1;
			*used = is_free;
			*isa_dir = 0;

			return(1);
		}
		if (dinoc->di_format != XFS_DINODE_FMT_LOCAL)
			dblkmap = blkmap_alloc(nextents);
		break;
	case XR_INO_SYMLINK:
		if (process_symlink_extlist(mp, lino, dino))  {
			do_warn("bad data fork in symlink %llu\n", lino);

			if (!no_modify)  {
				*dirty += clear_dinode(mp,
						dino, lino);
				ASSERT(*dirty > 0);
			}

			*cleared = 1;
			*used = is_free;
			*isa_dir = 0;

			return(1);
		}
		if (dinoc->di_format != XFS_DINODE_FMT_LOCAL)
			dblkmap = blkmap_alloc(nextents);
		break;
	case XR_INO_CHRDEV:	/* fall through to FIFO case ... */
	case XR_INO_BLKDEV:	/* fall through to FIFO case ... */
	case XR_INO_SOCK:	/* fall through to FIFO case ... */
	case XR_INO_MOUNTPOINT:	/* fall through to FIFO case ... */
	case XR_INO_FIFO:
		if (process_misc_ino_types(mp, dino, lino, type))  {
			if (!no_modify)  {
				*dirty += clear_dinode(mp, dino, lino);
				ASSERT(*dirty > 0);
			}

			*cleared = 1;
			*used = is_free;
			*isa_dir = 0;

			return(1);
		}
		break;
	case XR_INO_RTDATA:
		/*
		 * if we have no realtime blocks, any inode claiming
		 * to be a real-time file is bogus
		 */
		if (mp->m_sb.sb_rblocks == 0)  {
			do_warn(
			"found inode %llu claiming to be a real-time file\n",
				lino);

			if (!no_modify)  {
				*dirty += clear_dinode(mp, dino, lino);
				ASSERT(*dirty > 0);
			}

			*cleared = 1;
			*used = is_free;
			*isa_dir = 0;

			return(1);
		}
		break;
	case XR_INO_RTBITMAP:
		if (INT_GET(dinoc->di_size, ARCH_CONVERT) != (__int64_t) mp->m_sb.sb_rbmblocks *
				mp->m_sb.sb_blocksize)  {
			do_warn(
	"realtime bitmap inode %llu has bad size %lld (should be %lld)\n",
				lino, INT_GET(dinoc->di_size, ARCH_CONVERT),
				(__int64_t) mp->m_sb.sb_rbmblocks *
				mp->m_sb.sb_blocksize);

			if (!no_modify)  {
				*dirty += clear_dinode(mp, dino, lino);
				ASSERT(*dirty > 0);
			}

			*cleared = 1;
			*used = is_free;
			*isa_dir = 0;

			return(1);
		}
		dblkmap = blkmap_alloc(nextents);
		break;
	case XR_INO_RTSUM:
		if (INT_GET(dinoc->di_size, ARCH_CONVERT) != mp->m_rsumsize)  {
			do_warn(
	"realtime summary inode %llu has bad size %lld (should be %d)\n",
				lino, INT_GET(dinoc->di_size, ARCH_CONVERT), mp->m_rsumsize);

			if (!no_modify)  {
				*dirty += clear_dinode(mp, dino, lino);
				ASSERT(*dirty > 0);
			}

			*cleared = 1;
			*used = is_free;
			*isa_dir = 0;

			return(1);
		}
		dblkmap = blkmap_alloc(nextents);
		break;
	default:
		break;
	}

	/*
	 * check for illegal values of forkoff
	 */
	err = 0;
	if (dinoc->di_forkoff != 0)  {
		switch (dinoc->di_format)  {
		case XFS_DINODE_FMT_DEV:
			if (dinoc->di_forkoff !=
					(roundup(sizeof(dev_t), 8) >> 3))  {
				do_warn(
		"bad attr fork offset %d in dev inode %llu, should be %d\n",
					(int) dinoc->di_forkoff,
					lino,
					(int) (roundup(sizeof(dev_t), 8) >> 3));
				err = 1;
			}
			break;
		case XFS_DINODE_FMT_UUID:
			if (dinoc->di_forkoff !=
					(roundup(sizeof(uuid_t), 8) >> 3))  {
				do_warn(
		"bad attr fork offset %d in uuid inode %llu, should be %d\n",
					(int) dinoc->di_forkoff,
					lino,
					(int)(roundup(sizeof(uuid_t), 8) >> 3));
				err = 1;
			}
			break;
		case XFS_DINODE_FMT_LOCAL:	/* fall through ... */
		case XFS_DINODE_FMT_EXTENTS:	/* fall through ... */
		case XFS_DINODE_FMT_BTREE:
			if (dinoc->di_forkoff != mp->m_attroffset >> 3)  {
				do_warn(
		"bad attr fork offset %d in inode %llu, should be %d\n",
					(int) dinoc->di_forkoff,
					lino,
					(int) (mp->m_attroffset >> 3));
				err = 1;
			}
			break;
		default:
			do_error("unexpected inode format %d\n",
				(int) dinoc->di_format);
			break;
		}
	}

	if (err)  {
		if (!no_modify)  {
			*dirty += clear_dinode(mp, dino, lino);
			ASSERT(*dirty > 0);
		}

		*cleared = 1;
		*used = is_free;
		*isa_dir = 0;
		blkmap_free(dblkmap);
		return(1);
	}

	/*
	 * check data fork -- if it's bad, clear the inode
	 */
	nextents = 0;
	switch (dinoc->di_format) {
	case XFS_DINODE_FMT_LOCAL:
		err = process_lclinode(mp, agno, ino, dino, type,
			dirty, &totblocks, &nextents, &dblkmap,
			XFS_DATA_FORK, check_dups);
		break;
	case XFS_DINODE_FMT_EXTENTS:
		err = process_exinode(mp, agno, ino, dino, type,
			dirty, &totblocks, &nextents, &dblkmap,
			XFS_DATA_FORK, check_dups);
		break;
	case XFS_DINODE_FMT_BTREE:
		err = process_btinode(mp, agno, ino, dino, type,
			dirty, &totblocks, &nextents, &dblkmap,
			XFS_DATA_FORK, check_dups);
		break;
	case XFS_DINODE_FMT_DEV:	/* fall through */
	case XFS_DINODE_FMT_UUID:
		err = 0;
		break;
	default:
		do_error("unknown format %d, ino %llu (mode = %d)\n",
				dinoc->di_format, lino, INT_GET(dinoc->di_mode, ARCH_CONVERT));
	}

	if (err)  {
		/*
		 * problem in the data fork, clear out the inode
		 * and get out
		 */
		do_warn("bad data fork in inode %llu\n", lino);

		if (!no_modify)  {
			*dirty += clear_dinode(mp, dino, lino);
			ASSERT(*dirty > 0);
		}

		*cleared = 1;
		*used = is_free;
		*isa_dir = 0;
		blkmap_free(dblkmap);

		return(1);
	}

	if (check_dups)  {
		/*
		 * if check_dups was non-zero, we have to
		 * re-process data fork to set bitmap since the
		 * bitmap wasn't set the first time through
		 */
		switch (dinoc->di_format) {
		case XFS_DINODE_FMT_LOCAL:
			err = process_lclinode(mp, agno, ino, dino, type,
				dirty, &totblocks, &nextents, &dblkmap,
				XFS_DATA_FORK, 0);
			break;
		case XFS_DINODE_FMT_EXTENTS:
			err = process_exinode(mp, agno, ino, dino, type,
				dirty, &totblocks, &nextents, &dblkmap,
				XFS_DATA_FORK, 0);
			break;
		case XFS_DINODE_FMT_BTREE:
			err = process_btinode(mp, agno, ino, dino, type,
				dirty, &totblocks, &nextents, &dblkmap,
				XFS_DATA_FORK, 0);
			break;
		case XFS_DINODE_FMT_DEV:	/* fall through */
		case XFS_DINODE_FMT_UUID:
			err = 0;
			break;
		default:
			do_error("unknown format %d, ino %llu (mode = %d)\n",
					dinoc->di_format, lino, INT_GET(dinoc->di_mode, ARCH_CONVERT));
		}

		if (no_modify && err != 0)  {
			*cleared = 1;
			*used = is_free;
			*isa_dir = 0;
			blkmap_free(dblkmap);

			return(1);
		}

		ASSERT(err == 0);
	}

	/*
	 * check attribute fork if necessary.  attributes are
	 * always stored in the regular filesystem.
	 */

	if (!XFS_DFORK_Q_ARCH(dino, ARCH_CONVERT) && dinoc->di_aformat != XFS_DINODE_FMT_EXTENTS) {
		do_warn("bad attribute format %d in inode %llu, ",
			dinoc->di_aformat, lino);
		if (!no_modify) {
			do_warn("resetting value\n");
			dinoc->di_aformat = XFS_DINODE_FMT_EXTENTS;
			*dirty = 1;
		} else
			do_warn("would reset value\n");
		anextents = 0;
	} else if (XFS_DFORK_Q_ARCH(dino, ARCH_CONVERT)) {
		switch (dinoc->di_aformat) {
		case XFS_DINODE_FMT_LOCAL:
			anextents = 0;
			err = process_lclinode(mp, agno, ino, dino,
				type, dirty, &atotblocks, &anextents, &ablkmap,
				XFS_ATTR_FORK, check_dups);
			break;
		case XFS_DINODE_FMT_EXTENTS:
			ablkmap = blkmap_alloc(anextents);
			anextents = 0;
			err = process_exinode(mp, agno, ino, dino,
				type, dirty, &atotblocks, &anextents, &ablkmap,
				XFS_ATTR_FORK, check_dups);
			break;
		case XFS_DINODE_FMT_BTREE:
			ablkmap = blkmap_alloc(anextents);
			anextents = 0;
			err = process_btinode(mp, agno, ino, dino,
				type, dirty, &atotblocks, &anextents, &ablkmap,
				XFS_ATTR_FORK, check_dups);
			break;
		default:
			anextents = 0;
			do_warn("illegal attribute format %d, ino %llu\n",
					dinoc->di_aformat, lino);
			err = 1;
			break;
		}

		if (err)  {
			/*
			 * clear the attribute fork if necessary.  we can't
			 * clear the inode because we've already put the
			 * inode space info into the blockmap.
			 *
			 * XXX - put the inode onto the "move it" list and
			 *	log the the attribute scrubbing
			 */
			do_warn("bad attribute fork in inode %llu", lino);

			if (!no_modify)  {
				if (delete_attr_ok)  {
					do_warn(", clearing attr fork\n");
					*dirty += clear_dinode_attr(mp,
							dino, lino);
				} else  {
					do_warn("\n");
					*dirty += clear_dinode(mp,
							dino, lino);
				}
				ASSERT(*dirty > 0);
			} else  {
				do_warn(", would clear attr fork\n");
			}

			atotblocks = 0;
			anextents = 0;

			if (delete_attr_ok)  {
				if (!no_modify)
					dinoc->di_aformat = XFS_DINODE_FMT_LOCAL;
			} else  {
				*cleared = 1;
				*used = is_free;
				*isa_dir = 0;
				blkmap_free(dblkmap);
				blkmap_free(ablkmap);
			}
			return(1);
			
		} else if (check_dups)  {
			switch (dinoc->di_aformat) {
			case XFS_DINODE_FMT_LOCAL:
				err = process_lclinode(mp, agno, ino, dino,
					type, dirty, &atotblocks, &anextents,
					&ablkmap, XFS_ATTR_FORK, 0);
				break;
			case XFS_DINODE_FMT_EXTENTS:
				err = process_exinode(mp, agno, ino, dino,
					type, dirty, &atotblocks, &anextents,
					&ablkmap, XFS_ATTR_FORK, 0);
				break;
			case XFS_DINODE_FMT_BTREE:
				err = process_btinode(mp, agno, ino, dino,
					type, dirty, &atotblocks, &anextents,
					&ablkmap, XFS_ATTR_FORK, 0);
				break;
			default:
				do_error("illegal attribute fmt %d, ino %llu\n",
						dinoc->di_aformat, lino);
			}

			if (no_modify && err != 0)  {
				*cleared = 1;
				*used = is_free;
				*isa_dir = 0;
				blkmap_free(dblkmap);
				blkmap_free(ablkmap);

				return(1);
			}

			ASSERT(err == 0);
		}

		/*
		 * do attribute semantic-based consistency checks now
		 */

		/* get this only in phase 3, not in both phase 3 and 4 */
		if (extra_attr_check) {
		    if ((err = process_attributes(mp, lino, dino, ablkmap,
				    &repair))) {
			    do_warn("problem with attribute contents in inode %llu\n",lino);
			    if(!repair) {
				    /* clear attributes if not done already */
				    if (!no_modify)  {
					    *dirty += clear_dinode_attr(
							mp, dino, lino);
					    dinoc->di_aformat =
						XFS_DINODE_FMT_LOCAL;
				    } else  {
					    do_warn("would clear attr fork\n");
				    }
				    atotblocks = 0;
				    anextents = 0; 
			    }
			    else {
				    *dirty = 1; /* it's been repaired */
			     }
		    }
		}
		blkmap_free(ablkmap);

	} else
		anextents = 0;

	/* 
	* enforce totblocks is 0 for misc types 
	*/
	if (process_misc_ino_types_blocks(totblocks, lino, type)) {
		if (!no_modify)  {
			*dirty += clear_dinode(mp, dino, lino);
			ASSERT(*dirty > 0);
		}
		*cleared = 1;
		*used = is_free;
		*isa_dir = 0;
		blkmap_free(dblkmap);

		return(1);
	}

	/*
	 * correct space counters if required
	 */
	if (totblocks + atotblocks != INT_GET(dinoc->di_nblocks, ARCH_CONVERT))  {
		if (!no_modify)  {
	do_warn("correcting nblocks for inode %llu, was %llu - counted %llu\n",
				lino, INT_GET(dinoc->di_nblocks, ARCH_CONVERT),
				totblocks + atotblocks);
			*dirty = 1;
			INT_SET(dinoc->di_nblocks, ARCH_CONVERT, totblocks + atotblocks);
		} else  {
		do_warn(
	"bad nblocks %llu for inode %llu, would reset to %llu\n",
				INT_GET(dinoc->di_nblocks, ARCH_CONVERT), lino,
				totblocks + atotblocks);
		}
	}

	if (nextents > MAXEXTNUM)  {
		do_warn("too many data fork extents (%llu) in inode %llu\n",
			nextents, lino);

		if (!no_modify)  {
			*dirty += clear_dinode(mp, dino, lino);
			ASSERT(*dirty > 0);
		}
		*cleared = 1;
		*used = is_free;
		*isa_dir = 0;
		blkmap_free(dblkmap);

		return(1);
	}
	if (nextents != INT_GET(dinoc->di_nextents, ARCH_CONVERT))  {
		if (!no_modify)  {
	do_warn("correcting nextents for inode %llu, was %d - counted %llu\n",
				lino, INT_GET(dinoc->di_nextents, ARCH_CONVERT), nextents);
			*dirty = 1;
			INT_SET(dinoc->di_nextents, ARCH_CONVERT, (xfs_extnum_t) nextents);
		} else  {
			do_warn(
		"bad nextents %d for inode %llu, would reset to %llu\n",
				INT_GET(dinoc->di_nextents, ARCH_CONVERT), lino, nextents);
		}
	}

	if (anextents > MAXAEXTNUM)  {
		do_warn("too many attr fork extents (%llu) in inode %llu\n",
			anextents, lino);

		if (!no_modify)  {
			*dirty += clear_dinode(mp, dino, lino);
			ASSERT(*dirty > 0);
		}
		*cleared = 1;
		*used = is_free;
		*isa_dir = 0;
		blkmap_free(dblkmap);

		return(1);
	}
	if (anextents != INT_GET(dinoc->di_anextents, ARCH_CONVERT))  {
		if (!no_modify)  {
	do_warn("correcting anextents for inode %llu, was %d - counted %llu\n",
				lino, INT_GET(dinoc->di_anextents, ARCH_CONVERT), anextents);
			*dirty = 1;
			INT_SET(dinoc->di_anextents, ARCH_CONVERT, (xfs_aextnum_t) anextents);
		} else  {
			do_warn(
		"bad anextents %d for inode %llu, would reset to %llu\n",
				INT_GET(dinoc->di_anextents, ARCH_CONVERT), lino, anextents);
		}
	}

	/*
	 * do any semantic type-based checking here
	 */
	switch (type)  {
	case XR_INO_DIR:
		if (XFS_SB_VERSION_HASDIRV2(&mp->m_sb))
			err = process_dir2(mp, lino, dino, ino_discovery,
					dirty, "", parent, dblkmap);
		else
			err = process_dir(mp, lino, dino, ino_discovery,
					dirty, "", parent, dblkmap);
		if (err)
			do_warn(
			"problem with directory contents in inode %llu\n",
				lino);
		break;
	case XR_INO_RTBITMAP:
		/* process_rtbitmap XXX */
		err = 0;
		break;
	case XR_INO_RTSUM:
		/* process_rtsummary XXX */
		err = 0;
		break;
	case XR_INO_SYMLINK:
		if ((err = process_symlink(mp, lino, dino, dblkmap)))
			do_warn("problem with symbolic link in inode %llu\n",
				lino);
		break;
	case XR_INO_DATA:	/* fall through to FIFO case ... */
	case XR_INO_RTDATA:	/* fall through to FIFO case ... */
	case XR_INO_CHRDEV:	/* fall through to FIFO case ... */
	case XR_INO_BLKDEV:	/* fall through to FIFO case ... */
	case XR_INO_SOCK:	/* fall through to FIFO case ... */
	case XR_INO_FIFO:
		err = 0;
		break;
	default:
		printf("Unexpected inode type\n");
		abort();
	}

	blkmap_free(dblkmap);

	if (err)  {
		/*
		 * problem in the inode type-specific semantic
		 * checking, clear out the inode and get out
		 */
		if (!no_modify)  {
			*dirty += clear_dinode(mp, dino, lino);
			ASSERT(*dirty > 0);
		}
		*cleared = 1;
		*used = is_free;
		*isa_dir = 0;

		return(1);
	}

	/*
	 * check nlinks feature, if it's a version 1 inode,
	 * just leave nlinks alone.  even if it's set wrong,
	 * it'll be reset when read in.
	 */
	if (dinoc->di_version > XFS_DINODE_VERSION_1 && !fs_inode_nlink)  {
		/*
		 * do we have a fs/inode version mismatch with a valid
		 * version 2 inode here that has to stay version 2 or
		 * lose links?
		 */
		if (INT_GET(dinoc->di_nlink, ARCH_CONVERT) > XFS_MAXLINK_1)  {
			/*
			 * yes.  are nlink inodes allowed?
			 */
			if (fs_inode_nlink_allowed)  {
				/*
				 * yes, update status variable which will
				 * cause sb to be updated later.
				 */
				fs_inode_nlink = 1;
				do_warn(
				"version 2 inode %llu claims > %u links,",
					lino, XFS_MAXLINK_1);
				if (!no_modify)  {
					do_warn(
			"updating superblock version number\n");
				} else  {
					do_warn(
			"would update superblock version number\n");
				}
			} else  {
				/*
				 * no, have to convert back to onlinks
				 * even if we lose some links
				 */
				do_warn(
			"WARNING:  version 2 inode %llu claims > %u links,",
					lino, XFS_MAXLINK_1);
				if (!no_modify)  {
					do_warn(
	"converting back to version 1,\n\tthis may destroy %d links\n",
						INT_GET(dinoc->di_nlink, ARCH_CONVERT)
						- XFS_MAXLINK_1);

					dinoc->di_version =
						XFS_DINODE_VERSION_1;
					INT_SET(dinoc->di_nlink, ARCH_CONVERT, XFS_MAXLINK_1);
					INT_SET(dinoc->di_onlink, ARCH_CONVERT, XFS_MAXLINK_1);

					*dirty = 1;
				} else  {
					do_warn(
	"would convert back to version 1,\n\tthis might destroy %d links\n",
						INT_GET(dinoc->di_nlink, ARCH_CONVERT)
						- XFS_MAXLINK_1);
				}
			}
		} else  {
			/*
			 * do we have a v2 inode that we could convert back
			 * to v1 without losing any links?  if we do and
			 * we have a mismatch between superblock bits and the
			 * version bit, alter the version bit in this case.
			 *
			 * the case where we lost links was handled above.
			 */
			do_warn("found version 2 inode %llu, ", lino);
			if (!no_modify)  {
				do_warn("converting back to version 1\n");

				dinoc->di_version =
					XFS_DINODE_VERSION_1;
				INT_SET(dinoc->di_onlink, ARCH_CONVERT, INT_GET(dinoc->di_nlink, ARCH_CONVERT));

				*dirty = 1;
			} else  {
				do_warn("would convert back to version 1\n");
			}
		}
	}

	/*
	 * ok, if it's still a version 2 inode, it's going
	 * to stay a version 2 inode.  it should have a zero
	 * onlink field, so clear it.
	 */
	if (dinoc->di_version > XFS_DINODE_VERSION_1 &&
			INT_GET(dinoc->di_onlink, ARCH_CONVERT) > 0 && fs_inode_nlink > 0)  {
		if (!no_modify)  {
			do_warn(
"clearing obsolete nlink field in version 2 inode %llu, was %d, now 0\n",
				lino, INT_GET(dinoc->di_onlink, ARCH_CONVERT));
			INT_ZERO(dinoc->di_onlink, ARCH_CONVERT);
			*dirty = 1;
		} else  {
			do_warn(
"would clear obsolete nlink field in version 2 inode %llu, currently %d\n",
				lino, INT_GET(dinoc->di_onlink, ARCH_CONVERT));
			*dirty = 1;
		}
	}

	return(retval > 0 ? 1 : 0);
}

/*
 * returns 1 if inode is used, 0 if free.
 * performs any necessary salvaging actions.
 * note that we leave the generation count alone
 * because nothing we could set it to would be
 * guaranteed to be correct so the best guess for
 * the correct value is just to leave it alone.
 *
 * The trick is detecting empty files.  For those,
 * the core and the forks should all be in the "empty"
 * or zero-length state -- a zero or possibly minimum length
 * (in the case of dirs) extent list -- although inline directories
 * and symlinks might be handled differently.  So it should be
 * possible to sanity check them against each other.
 *
 * If the forks are an empty extent list though, then forget it.
 * The file is toast anyway since we can't recover its storage.
 *
 * Parameters:
 *	Ins:
 *		mp -- mount structure
 *		dino -- pointer to on-disk inode structure
 *		agno/ino -- inode numbers
 *		free -- whether the map thinks the inode is free (1 == free)
 *		ino_discovery -- whether we should examine directory
 *				contents to discover new inodes
 *		check_dups -- whether we should check to see if the
 *				inode references duplicate blocks
 *				if so, we compare the inode's claimed
 *				blocks against the contents of the
 *				duplicate extent list but we don't
 *				set the bitmap.  If not, we set the
 *				bitmap and try and detect multiply
 *				claimed blocks using the bitmap.
 *	Outs:
 *		dirty -- whether we changed the inode (1 == yes)
 *		cleared -- whether we cleared the inode (1 == yes).  In
 *				no modify mode, if we would have cleared it
 *		used -- 1 if the inode is used, 0 if free.  In no modify
 *			mode, whether the inode should be used or free
 *		isa_dir -- 1 if the inode is a directory, 0 if not.  In
 *			no modify mode, if the inode would be a dir or not.
 *
 *	Return value -- 0 if the inode is good, 1 if it is/was corrupt
 */

int
process_dinode(xfs_mount_t *mp,
		xfs_dinode_t *dino,
		xfs_agnumber_t agno,
		xfs_agino_t ino,
		int was_free,
		int *dirty,
		int *cleared,
		int *used,
		int ino_discovery,
		int check_dups,
		int extra_attr_check,
		int *isa_dir,
		xfs_ino_t *parent)
{
	const int verify_mode = 0;
	const int uncertain = 0;

#ifdef XR_INODE_TRACE
	fprintf(stderr, "processing inode %d/%d\n", agno, ino);
#endif
	return(process_dinode_int(mp, dino, agno, ino, was_free, dirty,
				cleared, used, verify_mode, uncertain,
				ino_discovery, check_dups, extra_attr_check,
				isa_dir, parent));
}

/*
 * a more cursory check, check inode core, *DON'T* check forks
 * this basically just verifies whether the inode is an inode
 * and whether or not it has been totally trashed.  returns 0
 * if the inode passes the cursory sanity check, 1 otherwise.
 */
int
verify_dinode(xfs_mount_t *mp,
		xfs_dinode_t *dino,
		xfs_agnumber_t agno,
		xfs_agino_t ino)
{
	xfs_ino_t parent;
	int cleared = 0;
	int used = 0;
	int dirty = 0;
	int isa_dir = 0;
	const int verify_mode = 1;
	const int check_dups = 0;
	const int ino_discovery = 0;
	const int uncertain = 0;

	return(process_dinode_int(mp, dino, agno, ino, 0, &dirty,
				&cleared, &used, verify_mode,
				uncertain, ino_discovery, check_dups,
				0, &isa_dir, &parent));
}

/*
 * like above only for inode on the uncertain list.  it sets
 * the uncertain flag which makes process_dinode_int quieter.
 * returns 0 if the inode passes the cursory sanity check, 1 otherwise.
 */
int
verify_uncertain_dinode(xfs_mount_t *mp,
		xfs_dinode_t *dino,
		xfs_agnumber_t agno,
		xfs_agino_t ino)
{
	xfs_ino_t parent;
	int cleared = 0;
	int used = 0;
	int dirty = 0;
	int isa_dir = 0;
	const int verify_mode = 1;
	const int check_dups = 0;
	const int ino_discovery = 0;
	const int uncertain = 1;

	return(process_dinode_int(mp, dino, agno, ino, 0, &dirty,
				&cleared, &used, verify_mode,
				uncertain, ino_discovery, check_dups,
				0, &isa_dir, &parent));
}
