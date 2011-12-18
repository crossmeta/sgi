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
#include "data.h"
#include "type.h"
#include "faddr.h"
#include "fprint.h"
#include "field.h"
#include "inobt.h"
#include "print.h"
#include "bit.h"
#include "mount.h"

static int	inobt_key_count(void *obj, int startoff);
static int	inobt_key_offset(void *obj, int startoff, int idx);
static int	inobt_ptr_count(void *obj, int startoff);
static int	inobt_ptr_offset(void *obj, int startoff, int idx);
static int	inobt_rec_count(void *obj, int startoff);
static int	inobt_rec_offset(void *obj, int startoff, int idx);

const field_t	inobt_hfld[] = {
	{ "", FLDT_INOBT, OI(0), C1, 0, TYP_NONE },
	{ NULL }
};

#define	OFF(f)	bitize(offsetof(xfs_inobt_block_t, bb_ ## f))
const field_t	inobt_flds[] = {
	{ "magic", FLDT_UINT32X, OI(OFF(magic)), C1, 0, TYP_NONE },
	{ "level", FLDT_UINT16D, OI(OFF(level)), C1, 0, TYP_NONE },
	{ "numrecs", FLDT_UINT16D, OI(OFF(numrecs)), C1, 0, TYP_NONE },
	{ "leftsib", FLDT_AGBLOCK, OI(OFF(leftsib)), C1, 0, TYP_INOBT },
	{ "rightsib", FLDT_AGBLOCK, OI(OFF(rightsib)), C1, 0, TYP_INOBT },
	{ "recs", FLDT_INOBTREC, inobt_rec_offset, inobt_rec_count,
	  FLD_ARRAY|FLD_ABASE1|FLD_COUNT|FLD_OFFSET, TYP_NONE },
	{ "keys", FLDT_INOBTKEY, inobt_key_offset, inobt_key_count,
	  FLD_ARRAY|FLD_ABASE1|FLD_COUNT|FLD_OFFSET, TYP_NONE },
	{ "ptrs", FLDT_INOBTPTR, inobt_ptr_offset, inobt_ptr_count,
	  FLD_ARRAY|FLD_ABASE1|FLD_COUNT|FLD_OFFSET, TYP_INOBT },
	{ NULL }
};

#define	KOFF(f)	bitize(offsetof(xfs_inobt_key_t, ir_ ## f))
const field_t	inobt_key_flds[] = {
	{ "startino", FLDT_AGINO, OI(KOFF(startino)), C1, 0, TYP_INODE },
	{ NULL }
};

#define	ROFF(f)	bitize(offsetof(xfs_inobt_rec_t, ir_ ## f))
const field_t	inobt_rec_flds[] = {
	{ "startino", FLDT_AGINO, OI(ROFF(startino)), C1, 0, TYP_INODE },
	{ "freecount", FLDT_INT32D, OI(ROFF(freecount)), C1, 0, TYP_NONE },
	{ "free", FLDT_INOFREE, OI(ROFF(free)), C1, 0, TYP_NONE },
	{ NULL }
};

/*ARGSUSED*/
static int
inobt_key_count(
	void			*obj,
	int			startoff)
{
	xfs_inobt_block_t	*block;

	ASSERT(startoff == 0);
	block = obj;
	if (INT_GET(block->bb_level, ARCH_CONVERT) == 0)
		return 0;
	return INT_GET(block->bb_numrecs, ARCH_CONVERT);
}

/*ARGSUSED*/
static int
inobt_key_offset(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_inobt_block_t	*block;
	xfs_inobt_key_t		*kp;

	ASSERT(startoff == 0);
	block = obj;
	ASSERT(INT_GET(block->bb_level, ARCH_CONVERT) > 0);
	kp = XFS_BTREE_KEY_ADDR(mp->m_sb.sb_blocksize, xfs_inobt, block, idx,
		XFS_BTREE_BLOCK_MAXRECS(mp->m_sb.sb_blocksize, xfs_inobt, 0));
	return bitize((int)((char *)kp - (char *)block));
}

/*ARGSUSED*/
static int
inobt_ptr_count(
	void			*obj,
	int			startoff)
{
	xfs_inobt_block_t	*block;

	ASSERT(startoff == 0);
	block = obj;
	if (INT_GET(block->bb_level, ARCH_CONVERT) == 0)
		return 0;
	return INT_GET(block->bb_numrecs, ARCH_CONVERT);
}

/*ARGSUSED*/
static int
inobt_ptr_offset(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_inobt_block_t	*block;
	xfs_inobt_ptr_t		*pp;

	ASSERT(startoff == 0);
	block = obj;
	ASSERT(INT_GET(block->bb_level, ARCH_CONVERT) > 0);
	pp = XFS_BTREE_PTR_ADDR(mp->m_sb.sb_blocksize, xfs_inobt, block, idx,
		XFS_BTREE_BLOCK_MAXRECS(mp->m_sb.sb_blocksize, xfs_inobt, 0));
	return bitize((int)((char *)pp - (char *)block));
}

/*ARGSUSED*/
static int
inobt_rec_count(
	void			*obj,
	int			startoff)
{
	xfs_inobt_block_t	*block;

	ASSERT(startoff == 0);
	block = obj;
	if (INT_GET(block->bb_level, ARCH_CONVERT) > 0)
		return 0;
	return INT_GET(block->bb_numrecs, ARCH_CONVERT);
}

/*ARGSUSED*/
static int
inobt_rec_offset(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_inobt_block_t	*block;
	xfs_inobt_rec_t		*rp;

	ASSERT(startoff == 0);
	block = obj;
	ASSERT(INT_GET(block->bb_level, ARCH_CONVERT) == 0);
	rp = XFS_BTREE_REC_ADDR(mp->m_sb.sb_blocksize, xfs_inobt, block, idx,
		XFS_BTREE_BLOCK_MAXRECS(mp->m_sb.sb_blocksize, xfs_inobt, 1));
	return bitize((int)((char *)rp - (char *)block));
}

/*ARGSUSED*/
int
inobt_size(
	void	*obj,
	int	startoff,
	int	idx)
{
	return bitize(mp->m_sb.sb_blocksize);
}
