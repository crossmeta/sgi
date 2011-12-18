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
#include "globals.h"
#include "agheader.h"
#include "protos.h"
#include "err_protos.h"

int
verify_set_agf(xfs_mount_t *mp, xfs_agf_t *agf, xfs_agnumber_t i)
{
	xfs_drfsbno_t agblocks;
	int retval = 0;

	/* check common fields */

	if (INT_GET(agf->agf_magicnum, ARCH_CONVERT) != XFS_AGF_MAGIC)  {
		retval = XR_AG_AGF;
		do_warn("bad magic # 0x%x for agf %d\n", INT_GET(agf->agf_magicnum, ARCH_CONVERT), i);

		if (!no_modify)
			INT_SET(agf->agf_magicnum, ARCH_CONVERT, XFS_AGF_MAGIC);
	}

	if (!XFS_AGF_GOOD_VERSION(INT_GET(agf->agf_versionnum, ARCH_CONVERT)))  {
		retval = XR_AG_AGF;
		do_warn("bad version # %d for agf %d\n",
			INT_GET(agf->agf_versionnum, ARCH_CONVERT), i);

		if (!no_modify)
			INT_SET(agf->agf_versionnum, ARCH_CONVERT, XFS_AGF_VERSION);
	}

	if (INT_GET(agf->agf_seqno, ARCH_CONVERT) != i)  {
		retval = XR_AG_AGF;
		do_warn("bad sequence # %d for agf %d\n", INT_GET(agf->agf_seqno, ARCH_CONVERT), i);

		if (!no_modify)
			INT_SET(agf->agf_seqno, ARCH_CONVERT, i);
	}

	if (INT_GET(agf->agf_length, ARCH_CONVERT) != mp->m_sb.sb_agblocks)  {
		if (i != mp->m_sb.sb_agcount - 1)  {
			retval = XR_AG_AGF;
			do_warn("bad length %d for agf %d, should be %d\n",
				INT_GET(agf->agf_length, ARCH_CONVERT), i, mp->m_sb.sb_agblocks);
			if (!no_modify)
				INT_SET(agf->agf_length, ARCH_CONVERT, mp->m_sb.sb_agblocks);
		} else  {
			agblocks = mp->m_sb.sb_dblocks -
				(xfs_drfsbno_t) mp->m_sb.sb_agblocks * i;

			if (INT_GET(agf->agf_length, ARCH_CONVERT) != agblocks)  {
				retval = XR_AG_AGF;
				do_warn(
			"bad length %d for agf %d, should be %llu\n",
					INT_GET(agf->agf_length, ARCH_CONVERT), i, agblocks);
				if (!no_modify)
					INT_SET(agf->agf_length, ARCH_CONVERT, (xfs_agblock_t) agblocks);
			}
		}
	}

	/*
	 * check first/last AGF fields.  if need be, lose the free
	 * space in the AGFL, we'll reclaim it later.
	 */
	if (INT_GET(agf->agf_flfirst, ARCH_CONVERT) >= XFS_AGFL_SIZE)  {
		do_warn("flfirst %d in agf %d too large (max = %d)\n",
			INT_GET(agf->agf_flfirst, ARCH_CONVERT), i, XFS_AGFL_SIZE);
		if (!no_modify)
			INT_ZERO(agf->agf_flfirst, ARCH_CONVERT);
	}

	if (INT_GET(agf->agf_fllast, ARCH_CONVERT) >= XFS_AGFL_SIZE)  {
		do_warn("fllast %d in agf %d too large (max = %d)\n",
			INT_GET(agf->agf_fllast, ARCH_CONVERT), i, XFS_AGFL_SIZE);
		if (!no_modify)
			INT_ZERO(agf->agf_fllast, ARCH_CONVERT);
	}

	/* don't check freespace btrees -- will be checked by caller */

	return(retval);
}

int
verify_set_agi(xfs_mount_t *mp, xfs_agi_t *agi, xfs_agnumber_t i)
{
	xfs_drfsbno_t agblocks;
	int retval = 0;

	/* check common fields */

	if (INT_GET(agi->agi_magicnum, ARCH_CONVERT) != XFS_AGI_MAGIC)  {
		retval = XR_AG_AGI;
		do_warn("bad magic # 0x%x for agi %d\n", INT_GET(agi->agi_magicnum, ARCH_CONVERT), i);

		if (!no_modify)
			INT_SET(agi->agi_magicnum, ARCH_CONVERT, XFS_AGI_MAGIC);
	}

	if (!XFS_AGI_GOOD_VERSION(INT_GET(agi->agi_versionnum, ARCH_CONVERT)))  {
		retval = XR_AG_AGI;
		do_warn("bad version # %d for agi %d\n",
			INT_GET(agi->agi_versionnum, ARCH_CONVERT), i);

		if (!no_modify)
			INT_SET(agi->agi_versionnum, ARCH_CONVERT, XFS_AGI_VERSION);
	}

	if (INT_GET(agi->agi_seqno, ARCH_CONVERT) != i)  {
		retval = XR_AG_AGI;
		do_warn("bad sequence # %d for agi %d\n", INT_GET(agi->agi_seqno, ARCH_CONVERT), i);

		if (!no_modify)
			INT_SET(agi->agi_seqno, ARCH_CONVERT, i);
	}

	if (INT_GET(agi->agi_length, ARCH_CONVERT) != mp->m_sb.sb_agblocks)  {
		if (i != mp->m_sb.sb_agcount - 1)  {
			retval = XR_AG_AGI;
			do_warn("bad length # %d for agi %d, should be %d\n",
				INT_GET(agi->agi_length, ARCH_CONVERT), i, mp->m_sb.sb_agblocks);
			if (!no_modify)
				INT_SET(agi->agi_length, ARCH_CONVERT, mp->m_sb.sb_agblocks);
		} else  {
			agblocks = mp->m_sb.sb_dblocks -
				(xfs_drfsbno_t) mp->m_sb.sb_agblocks * i;

			if (INT_GET(agi->agi_length, ARCH_CONVERT) != agblocks)  {
				retval = XR_AG_AGI;
				do_warn(
			"bad length # %d for agi %d, should be %llu\n",
					INT_GET(agi->agi_length, ARCH_CONVERT), i, agblocks);
				if (!no_modify)
					INT_SET(agi->agi_length, ARCH_CONVERT, (xfs_agblock_t) agblocks);
			}
		}
	}

	/* don't check inode btree -- will be checked by caller */

	return(retval);
}

/*
 * superblock comparison - compare arbitrary superblock with
 *			filesystem mount-point superblock
 *
 * the verified fields include id and geometry.

 * the inprogress fields, version numbers, and counters
 * are allowed to differ as well as all fields after the
 * counters to cope with the pre-6.5 mkfs non-bzeroed
 * secondary superblock sectors.
 */

int
compare_sb(xfs_mount_t *mp, xfs_sb_t *sb)
{
	fs_geometry_t fs_geo, sb_geo;

	get_sb_geometry(&fs_geo, &mp->m_sb);
	get_sb_geometry(&sb_geo, sb);

	if (memcmp(&fs_geo, &sb_geo,
		   (char *) &fs_geo.sb_shared_vn - (char *) &fs_geo))
		return(XR_SB_GEO_MISMATCH);

	return(XR_OK);
}

/*
 * possible fields that may have been set at mkfs time,
 * sb_inoalignmt, sb_unit, sb_width.  We know that
 * the quota inode fields in the secondaries should be zero.
 * Likewise, the sb_flags and sb_shared_vn should also be
 * zero and the shared version bit should be cleared for
 * current mkfs's.
 *
 * And everything else in the buffer beyond sb_width should
 * be zeroed.
 */
int
secondary_sb_wack(xfs_mount_t *mp, xfs_buf_t *sbuf, xfs_sb_t *sb,
	xfs_agnumber_t i)
{
	int do_bzero;
	int size;
	int *ip;
	int rval;

	rval = do_bzero = 0;

	/*
	 * mkfs's that stamped a feature bit besides the ones in the mask
	 * (e.g. were pre-6.5 beta) could leave garbage in the secondary
	 * superblock sectors.  Anything stamping the shared fs bit or better
	 * into the secondaries is ok and should generate clean secondary
	 * superblock sectors.  so only run the bzero check on the
	 * potentially garbaged secondaries.
	 */
	if (pre_65_beta ||
	    (sb->sb_versionnum & XR_GOOD_SECSB_VNMASK) == 0 ||
	    sb->sb_versionnum < XFS_SB_VERSION_4)  {
		/*
		 * check for garbage beyond the last field set by the
		 * pre-6.5 mkfs's.  Don't blindly use sizeof(sb).
		 * Use field addresses instead so this code will still
		 * work against older filesystems when the superblock
		 * gets rev'ed again with new fields appended.
		 */
		size = (__psint_t)&sb->sb_width + sizeof(sb->sb_width)
			- (__psint_t)sb;
		for (ip = (int *)((__psint_t)sb + size);
		     ip < (int *)((__psint_t)sb + mp->m_sb.sb_sectsize);
		     ip++)  {
			if (*ip)  {
				do_bzero = 1;
				break;
			}
		}

		if (do_bzero)  {
			rval |= XR_AG_SB_SEC;
			if (!no_modify)  {
				do_warn(
		"zeroing unused portion of secondary superblock %d sector\n",
					i);
				bzero((void *)((__psint_t)sb + size),
					mp->m_sb.sb_sectsize - size);
			} else
				do_warn(
		"would zero unused portion of secondary superblock %d sector\n",
					i);
		}
	}

	/*
	 * now look for the fields we can manipulate directly.
	 * if we did a bzero and that bzero could have included
	 * the field in question, just silently reset it.  otherwise,
	 * complain.
	 *
	 * for now, just zero the flags field since only
	 * the readonly flag is used
	 */
	if (sb->sb_flags)  {
		if (!no_modify)
			sb->sb_flags = 0;
		if (sb->sb_versionnum & XR_PART_SECSB_VNMASK || !do_bzero)  {
			rval |= XR_AG_SB;
			do_warn("bad flags field in superblock %d\n", i);
		} else
			rval |= XR_AG_SB_SEC;
	}

	/*
	 * quota inodes and flags in secondary superblocks
	 * are never set by mkfs.  However, they could be set
	 * in a secondary if a fs with quotas was growfs'ed since
	 * growfs copies the new primary into the secondaries.
	 */
	if (sb->sb_inprogress == 1 && sb->sb_uquotino)  {
		if (!no_modify)
			sb->sb_uquotino = 0;
		if (sb->sb_versionnum & XR_PART_SECSB_VNMASK || !do_bzero)  {
			rval |= XR_AG_SB;
			do_warn(
			"non-null user quota inode field in superblock %d\n",
				i);
		} else
			rval |= XR_AG_SB_SEC;
	}

	if (sb->sb_inprogress == 1 && sb->sb_gquotino)  {
		if (!no_modify)
			sb->sb_gquotino = 0;
		if (sb->sb_versionnum & XR_PART_SECSB_VNMASK || !do_bzero)  {
			rval |= XR_AG_SB;
			do_warn(
			"non-null group quota inode field in superblock %d\n",
				i);
		} else
			rval |= XR_AG_SB_SEC;
	}

	if (sb->sb_inprogress == 1 && sb->sb_qflags)  {
		if (!no_modify)
			sb->sb_qflags = 0;
		if (sb->sb_versionnum & XR_PART_SECSB_VNMASK || !do_bzero)  {
			rval |= XR_AG_SB;
			do_warn("non-null quota flags in superblock %d\n", i);
		} else
			rval |= XR_AG_SB_SEC;
	}

	/*
	 * if the secondaries agree on a stripe unit/width or inode
	 * alignment, those fields ought to be valid since they are
	 * written at mkfs time (and the corresponding sb version bits
	 * are set).
	 */
	if (!XFS_SB_VERSION_HASSHARED(sb) && sb->sb_shared_vn != 0)  {
		if (!no_modify)
			sb->sb_shared_vn = 0;
		if (sb->sb_versionnum & XR_PART_SECSB_VNMASK || !do_bzero)  {
			rval |= XR_AG_SB;
			do_warn("bad shared version number in superblock %d\n",
				i);
		} else
			rval |= XR_AG_SB_SEC;
	}

	if (!XFS_SB_VERSION_HASALIGN(sb) && sb->sb_inoalignmt != 0)  {
		if (!no_modify)
			sb->sb_inoalignmt = 0;
		if (sb->sb_versionnum & XR_PART_SECSB_VNMASK || !do_bzero)  {
			rval |= XR_AG_SB;
			do_warn("bad inode alignment field in superblock %d\n",
				i);
		} else
			rval |= XR_AG_SB_SEC;
	}

	if (!XFS_SB_VERSION_HASDALIGN(sb) &&
	    (sb->sb_unit != 0 || sb->sb_width != 0))  {
		if (!no_modify)
			sb->sb_unit = sb->sb_width = 0;
		if (sb->sb_versionnum & XR_GOOD_SECSB_VNMASK || !do_bzero)  {
			rval |= XR_AG_SB;
			do_warn(
			"bad stripe unit/width fields in superblock %d\n",
				i);
		} else
			rval |= XR_AG_SB_SEC;
	}

	return(rval);
}

/*
 * verify and reset the ag header if required.
 *
 * lower 4 bits of rval are set depending on what got modified.
 * (see agheader.h for more details)
 *
 * NOTE -- this routine does not tell the user that it has
 * altered things.  Rather, it is up to the caller to do so
 * using the bits encoded into the return value.
 */

int
verify_set_agheader(xfs_mount_t *mp, xfs_buf_t *sbuf, xfs_sb_t *sb,
	xfs_agf_t *agf, xfs_agi_t *agi, xfs_agnumber_t i)
{
	int rval = 0;
	int status = XR_OK;
	int status_sb = XR_OK;

	status = verify_sb(sb, (i == 0));

	if (status != XR_OK)  {
		do_warn("bad on-disk superblock %d - %s\n",
			i, err_string(status));
	}

	status_sb = compare_sb(mp, sb);

	if (status_sb != XR_OK)  {
		do_warn("primary and secondary superblock %d conflict - %s\n",
			i, err_string(status_sb));
	}

	if (status != XR_OK || status_sb != XR_OK)  {
		if (!no_modify)  {
			*sb = mp->m_sb;

			/*
			 * clear the more transient fields
			 */
			sb->sb_inprogress = 1;

			sb->sb_icount = 0;
			sb->sb_ifree = 0;
			sb->sb_fdblocks = 0;
			sb->sb_frextents = 0;

			sb->sb_qflags = 0;
		}

		rval |= XR_AG_SB;
	}

	rval |= secondary_sb_wack(mp, sbuf, sb, i);

	rval |= verify_set_agf(mp, agf, i);
	rval |= verify_set_agi(mp, agi, i);

	return(rval);
}
