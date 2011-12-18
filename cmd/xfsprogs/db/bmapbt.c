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
#include "bmapbt.h"
#include "print.h"
#include "bit.h"
#include "mount.h"

static int	bmapbta_key_count(void *obj, int startoff);
static int	bmapbta_key_offset(void *obj, int startoff, int idx);
static int	bmapbta_ptr_count(void *obj, int startoff);
static int	bmapbta_ptr_offset(void *obj, int startoff, int idx);
static int	bmapbta_rec_count(void *obj, int startoff);
static int	bmapbta_rec_offset(void *obj, int startoff, int idx);
static int	bmapbtd_key_count(void *obj, int startoff);
static int	bmapbtd_key_offset(void *obj, int startoff, int idx);
static int	bmapbtd_ptr_count(void *obj, int startoff);
static int	bmapbtd_ptr_offset(void *obj, int startoff, int idx);
static int	bmapbtd_rec_count(void *obj, int startoff);
static int	bmapbtd_rec_offset(void *obj, int startoff, int idx);

const field_t	bmapbta_hfld[] = {
	{ "", FLDT_BMAPBTA, OI(0), C1, 0, TYP_NONE },
	{ NULL }
};
const field_t	bmapbtd_hfld[] = {
	{ "", FLDT_BMAPBTD, OI(0), C1, 0, TYP_NONE },
	{ NULL }
};

#define	OFF(f)	bitize(offsetof(xfs_bmbt_block_t, bb_ ## f))
const field_t	bmapbta_flds[] = {
	{ "magic", FLDT_UINT32X, OI(OFF(magic)), C1, 0, TYP_NONE },
	{ "level", FLDT_UINT16D, OI(OFF(level)), C1, 0, TYP_NONE },
	{ "numrecs", FLDT_UINT16D, OI(OFF(numrecs)), C1, 0, TYP_NONE },
	{ "leftsib", FLDT_DFSBNO, OI(OFF(leftsib)), C1, 0, TYP_BMAPBTA },
	{ "rightsib", FLDT_DFSBNO, OI(OFF(rightsib)), C1, 0, TYP_BMAPBTA },
	{ "recs", FLDT_BMAPBTAREC, bmapbta_rec_offset, bmapbta_rec_count,
	  FLD_ARRAY|FLD_ABASE1|FLD_COUNT|FLD_OFFSET, TYP_NONE },
	{ "keys", FLDT_BMAPBTAKEY, bmapbta_key_offset, bmapbta_key_count,
	  FLD_ARRAY|FLD_ABASE1|FLD_COUNT|FLD_OFFSET, TYP_NONE },
	{ "ptrs", FLDT_BMAPBTAPTR, bmapbta_ptr_offset, bmapbta_ptr_count,
	  FLD_ARRAY|FLD_ABASE1|FLD_COUNT|FLD_OFFSET, TYP_BMAPBTA },
	{ NULL }
};
const field_t	bmapbtd_flds[] = {
	{ "magic", FLDT_UINT32X, OI(OFF(magic)), C1, 0, TYP_NONE },
	{ "level", FLDT_UINT16D, OI(OFF(level)), C1, 0, TYP_NONE },
	{ "numrecs", FLDT_UINT16D, OI(OFF(numrecs)), C1, 0, TYP_NONE },
	{ "leftsib", FLDT_DFSBNO, OI(OFF(leftsib)), C1, 0, TYP_BMAPBTD },
	{ "rightsib", FLDT_DFSBNO, OI(OFF(rightsib)), C1, 0, TYP_BMAPBTD },
	{ "recs", FLDT_BMAPBTDREC, bmapbtd_rec_offset, bmapbtd_rec_count,
	  FLD_ARRAY|FLD_ABASE1|FLD_COUNT|FLD_OFFSET, TYP_NONE },
	{ "keys", FLDT_BMAPBTDKEY, bmapbtd_key_offset, bmapbtd_key_count,
	  FLD_ARRAY|FLD_ABASE1|FLD_COUNT|FLD_OFFSET, TYP_NONE },
	{ "ptrs", FLDT_BMAPBTDPTR, bmapbtd_ptr_offset, bmapbtd_ptr_count,
	  FLD_ARRAY|FLD_ABASE1|FLD_COUNT|FLD_OFFSET, TYP_BMAPBTD },
	{ NULL }
};

#define	KOFF(f)	bitize(offsetof(xfs_bmbt_key_t, br_ ## f))
const field_t	bmapbta_key_flds[] = {
	{ "startoff", FLDT_DFILOFFA, OI(KOFF(startoff)), C1, 0, TYP_ATTR },
	{ NULL }
};
const field_t	bmapbtd_key_flds[] = {
	{ "startoff", FLDT_DFILOFFD, OI(KOFF(startoff)), C1, 0, TYP_INODATA },
	{ NULL }
};

const field_t	bmapbta_rec_flds[] = {
	{ "startoff", FLDT_CFILEOFFA, OI(BMBT_STARTOFF_BITOFF), C1, 0,
	  TYP_ATTR },
	{ "startblock", FLDT_CFSBLOCK, OI(BMBT_STARTBLOCK_BITOFF), C1, 0,
	  TYP_ATTR },
	{ "blockcount", FLDT_CEXTLEN, OI(BMBT_BLOCKCOUNT_BITOFF), C1, 0,
	  TYP_NONE },
	{ "extentflag", FLDT_CEXTFLG, OI(BMBT_EXNTFLAG_BITOFF), C1, 0,
	  TYP_NONE },
	{ NULL }
};
const field_t	bmapbtd_rec_flds[] = {
	{ "startoff", FLDT_CFILEOFFD, OI(BMBT_STARTOFF_BITOFF), C1, 0,
	  TYP_INODATA },
	{ "startblock", FLDT_CFSBLOCK, OI(BMBT_STARTBLOCK_BITOFF), C1, 0,
	  TYP_INODATA },
	{ "blockcount", FLDT_CEXTLEN, OI(BMBT_BLOCKCOUNT_BITOFF), C1, 0,
	  TYP_NONE },
	{ "extentflag", FLDT_CEXTFLG, OI(BMBT_EXNTFLAG_BITOFF), C1, 0,
	  TYP_NONE },
	{ NULL }
};

static int
bmapbta_key_count(
	void			*obj,
	int			startoff)
{
	xfs_bmbt_block_t	*block;

	ASSERT(startoff == 0);
	block = obj;
	if (INT_GET(block->bb_level, ARCH_CONVERT) == 0)
		return 0;
	return INT_GET(block->bb_numrecs, ARCH_CONVERT);
}

static int
bmapbta_key_offset(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_bmbt_block_t	*block;
	xfs_bmbt_key_t		*kp;

	ASSERT(startoff == 0);
	block = obj;
	ASSERT(INT_GET(block->bb_level, ARCH_CONVERT) > 0);
	kp = XFS_BTREE_KEY_ADDR(mp->m_sb.sb_blocksize, xfs_bmbt, block, idx,
		XFS_BTREE_BLOCK_MAXRECS(mp->m_sb.sb_blocksize, xfs_bmbt, 0));
	return bitize((int)((char *)kp - (char *)block));
}

static int
bmapbta_ptr_count(
	void			*obj,
	int			startoff)
{
	xfs_bmbt_block_t	*block;

	ASSERT(startoff == 0);
	block = obj;
	if (INT_GET(block->bb_level, ARCH_CONVERT) == 0)
		return 0;
	return INT_GET(block->bb_numrecs, ARCH_CONVERT);
}

static int
bmapbta_ptr_offset(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_bmbt_block_t	*block;
	xfs_bmbt_ptr_t		*pp;

	ASSERT(startoff == 0);
	block = obj;
	ASSERT(INT_GET(block->bb_level, ARCH_CONVERT) > 0);
	pp = XFS_BTREE_PTR_ADDR(mp->m_sb.sb_blocksize, xfs_bmbt, block, idx,
		XFS_BTREE_BLOCK_MAXRECS(mp->m_sb.sb_blocksize, xfs_bmbt, 0));
	return bitize((int)((char *)pp - (char *)block));
}

static int
bmapbta_rec_count(
	void			*obj,
	int			startoff)
{
	xfs_bmbt_block_t	*block;

	ASSERT(startoff == 0);
	block = obj;
	if (INT_GET(block->bb_level, ARCH_CONVERT) > 0)
		return 0;
	return INT_GET(block->bb_numrecs, ARCH_CONVERT);
}

static int
bmapbta_rec_offset(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_bmbt_block_t	*block;
	xfs_bmbt_rec_t		*rp;

	ASSERT(startoff == 0);
	block = obj;
	ASSERT(INT_GET(block->bb_level, ARCH_CONVERT) == 0);
	rp = XFS_BTREE_REC_ADDR(mp->m_sb.sb_blocksize, xfs_bmbt, block, idx,
		XFS_BTREE_BLOCK_MAXRECS(mp->m_sb.sb_blocksize, xfs_bmbt, 1));
	return bitize((int)((char *)rp - (char *)block));
}

int
bmapbta_size(
	void	*obj,
	int	startoff,
	int	idx)
{
	return bitize(mp->m_sb.sb_blocksize);
}

static int
bmapbtd_key_count(
	void			*obj,
	int			startoff)
{
	xfs_bmbt_block_t	*block;

	ASSERT(startoff == 0);
	block = obj;
	if (INT_GET(block->bb_level, ARCH_CONVERT) == 0)
		return 0;
	return INT_GET(block->bb_numrecs, ARCH_CONVERT);
}

static int
bmapbtd_key_offset(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_bmbt_block_t	*block;
	xfs_bmbt_key_t		*kp;

	ASSERT(startoff == 0);
	block = obj;
	ASSERT(INT_GET(block->bb_level, ARCH_CONVERT) > 0);
	kp = XFS_BTREE_KEY_ADDR(mp->m_sb.sb_blocksize, xfs_bmbt, block, idx,
		XFS_BTREE_BLOCK_MAXRECS(mp->m_sb.sb_blocksize, xfs_bmbt, 0));
	return bitize((int)((char *)kp - (char *)block));
}

static int
bmapbtd_ptr_count(
	void			*obj,
	int			startoff)
{
	xfs_bmbt_block_t	*block;

	ASSERT(startoff == 0);
	block = obj;
	if (INT_GET(block->bb_level, ARCH_CONVERT) == 0)
		return 0;
	return INT_GET(block->bb_numrecs, ARCH_CONVERT);
}

static int
bmapbtd_ptr_offset(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_bmbt_block_t	*block;
	xfs_bmbt_ptr_t		*pp;

	ASSERT(startoff == 0);
	block = obj;
	ASSERT(INT_GET(block->bb_level, ARCH_CONVERT) > 0);
	pp = XFS_BTREE_PTR_ADDR(mp->m_sb.sb_blocksize, xfs_bmbt, block, idx,
		XFS_BTREE_BLOCK_MAXRECS(mp->m_sb.sb_blocksize, xfs_bmbt, 0));
	return bitize((int)((char *)pp - (char *)block));
}

static int
bmapbtd_rec_count(
	void			*obj,
	int			startoff)
{
	xfs_bmbt_block_t	*block;

	ASSERT(startoff == 0);
	block = obj;
	if (INT_GET(block->bb_level, ARCH_CONVERT) > 0)
		return 0;
	return INT_GET(block->bb_numrecs, ARCH_CONVERT);
}

static int
bmapbtd_rec_offset(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_bmbt_block_t	*block;
	xfs_bmbt_rec_t		*rp;

	ASSERT(startoff == 0);
	block = obj;
	ASSERT(INT_GET(block->bb_level, ARCH_CONVERT) == 0);
	rp = XFS_BTREE_REC_ADDR(mp->m_sb.sb_blocksize, xfs_bmbt, block, idx,
		XFS_BTREE_BLOCK_MAXRECS(mp->m_sb.sb_blocksize, xfs_bmbt, 1));
	return bitize((int)((char *)rp - (char *)block));
}

int
bmapbtd_size(
	void	*obj,
	int	startoff,
	int	idx)
{
	return bitize(mp->m_sb.sb_blocksize);
}
