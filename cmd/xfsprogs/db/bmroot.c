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
#include "bmroot.h"
#include "io.h"
#include "print.h"
#include "bit.h"
#include "mount.h"

static int	bmroota_key_count(void *obj, int startoff);
static int	bmroota_key_offset(void *obj, int startoff, int idx);
static int	bmroota_ptr_count(void *obj, int startoff);
static int	bmroota_ptr_offset(void *obj, int startoff, int idx);
static int	bmrootd_key_count(void *obj, int startoff);
static int	bmrootd_key_offset(void *obj, int startoff, int idx);
static int	bmrootd_ptr_count(void *obj, int startoff);
static int	bmrootd_ptr_offset(void *obj, int startoff, int idx);

#define	OFF(f)	bitize(offsetof(xfs_bmdr_block_t, bb_ ## f))
const field_t	bmroota_flds[] = {
	{ "level", FLDT_UINT16D, OI(OFF(level)), C1, 0, TYP_NONE },
	{ "numrecs", FLDT_UINT16D, OI(OFF(numrecs)), C1, 0, TYP_NONE },
	{ "keys", FLDT_BMROOTAKEY, bmroota_key_offset, bmroota_key_count,
	  FLD_ARRAY|FLD_ABASE1|FLD_COUNT|FLD_OFFSET, TYP_NONE },
	{ "ptrs", FLDT_BMROOTAPTR, bmroota_ptr_offset, bmroota_ptr_count,
	  FLD_ARRAY|FLD_ABASE1|FLD_COUNT|FLD_OFFSET, TYP_BMAPBTA },
	{ NULL }
};
const field_t	bmrootd_flds[] = {
	{ "level", FLDT_UINT16D, OI(OFF(level)), C1, 0, TYP_NONE },
	{ "numrecs", FLDT_UINT16D, OI(OFF(numrecs)), C1, 0, TYP_NONE },
	{ "keys", FLDT_BMROOTDKEY, bmrootd_key_offset, bmrootd_key_count,
	  FLD_ARRAY|FLD_ABASE1|FLD_COUNT|FLD_OFFSET, TYP_NONE },
	{ "ptrs", FLDT_BMROOTDPTR, bmrootd_ptr_offset, bmrootd_ptr_count,
	  FLD_ARRAY|FLD_ABASE1|FLD_COUNT|FLD_OFFSET, TYP_BMAPBTD },
	{ NULL }
};

#define	KOFF(f)	bitize(offsetof(xfs_bmdr_key_t, br_ ## f))
const field_t	bmroota_key_flds[] = {
	{ "startoff", FLDT_DFILOFFA, OI(KOFF(startoff)), C1, 0, TYP_NONE },
	{ NULL }
};
const field_t	bmrootd_key_flds[] = {
	{ "startoff", FLDT_DFILOFFD, OI(KOFF(startoff)), C1, 0, TYP_NONE },
	{ NULL }
};

static int
bmroota_key_count(
	void			*obj,
	int			startoff)
{
	xfs_bmdr_block_t	*block;
#ifdef DEBUG
	xfs_dinode_t		*dip = obj;
#endif

	ASSERT(bitoffs(startoff) == 0);
	ASSERT(obj == iocur_top->data);
	block = (xfs_bmdr_block_t *)((char *)obj + byteize(startoff));
	ASSERT(XFS_DFORK_Q(dip) && (char *)block == XFS_DFORK_APTR(dip));
	ASSERT(INT_GET(block->bb_level, ARCH_CONVERT) > 0);
	return INT_GET(block->bb_numrecs, ARCH_CONVERT);
}

static int
bmroota_key_offset(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_bmdr_block_t	*block;
	/* REFERENCED */
	xfs_dinode_t		*dip;
	xfs_bmdr_key_t		*kp;

	ASSERT(bitoffs(startoff) == 0);
	ASSERT(obj == iocur_top->data);
	dip = obj;
	block = (xfs_bmdr_block_t *)((char *)obj + byteize(startoff));
	ASSERT(XFS_DFORK_Q(dip) && (char *)block == XFS_DFORK_APTR(dip));
	ASSERT(INT_GET(block->bb_level, ARCH_CONVERT) > 0);
	kp = XFS_BTREE_KEY_ADDR(iocur_top->len, xfs_bmdr, block, idx,
		XFS_BTREE_BLOCK_MAXRECS(XFS_DFORK_ASIZE(dip, mp), xfs_bmdr, 0));
	return bitize((int)((char *)kp - (char *)block));
}

static int
bmroota_ptr_count(
	void			*obj,
	int			startoff)
{
	xfs_bmdr_block_t	*block;
#ifdef DEBUG
	xfs_dinode_t		*dip = obj;
#endif

	ASSERT(bitoffs(startoff) == 0);
	ASSERT(obj == iocur_top->data);
	block = (xfs_bmdr_block_t *)((char *)obj + byteize(startoff));
	ASSERT(XFS_DFORK_Q(dip) && (char *)block == XFS_DFORK_APTR(dip));
	ASSERT(INT_GET(block->bb_level, ARCH_CONVERT) > 0);
	return INT_GET(block->bb_numrecs, ARCH_CONVERT);
}

static int
bmroota_ptr_offset(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_bmdr_block_t	*block;
	xfs_dinode_t		*dip;
	xfs_bmdr_ptr_t		*pp;

	ASSERT(bitoffs(startoff) == 0);
	ASSERT(obj == iocur_top->data);
	dip = obj;
	block = (xfs_bmdr_block_t *)((char *)obj + byteize(startoff));
	ASSERT(XFS_DFORK_Q(dip) && (char *)block == XFS_DFORK_APTR(dip));
	ASSERT(INT_GET(block->bb_level, ARCH_CONVERT) > 0);
	pp = XFS_BTREE_PTR_ADDR(iocur_top->len, xfs_bmdr, block, idx,
		XFS_BTREE_BLOCK_MAXRECS(XFS_DFORK_ASIZE(dip, mp), xfs_bmdr, 0));
	return bitize((int)((char *)pp - (char *)block));
}

int
bmroota_size(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_dinode_t		*dip;
#ifdef DEBUG
	xfs_bmdr_block_t	*block;
#endif

	ASSERT(bitoffs(startoff) == 0);
	ASSERT(obj == iocur_top->data);
	ASSERT(idx == 0);
	dip = obj;
#ifdef DEBUG
	block = (xfs_bmdr_block_t *)((char *)obj + byteize(startoff));
	ASSERT(XFS_DFORK_Q(dip) && (char *)block == XFS_DFORK_APTR(dip));
#endif
	return bitize((int)XFS_DFORK_ASIZE(dip, mp));
}

static int
bmrootd_key_count(
	void			*obj,
	int			startoff)
{
	xfs_bmdr_block_t	*block;
#ifdef DEBUG
	xfs_dinode_t		*dip = obj;
#endif

	ASSERT(bitoffs(startoff) == 0);
	ASSERT(obj == iocur_top->data);
	block = (xfs_bmdr_block_t *)((char *)obj + byteize(startoff));
	ASSERT((char *)block == XFS_DFORK_DPTR(dip));
	ASSERT(INT_GET(block->bb_level, ARCH_CONVERT) > 0);
	return INT_GET(block->bb_numrecs, ARCH_CONVERT);
}

static int
bmrootd_key_offset(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_bmdr_block_t	*block;
	xfs_bmdr_key_t		*kp;
	xfs_dinode_t		*dip;

	ASSERT(bitoffs(startoff) == 0);
	ASSERT(obj == iocur_top->data);
	dip = obj;
	block = (xfs_bmdr_block_t *)((char *)obj + byteize(startoff));
	ASSERT(INT_GET(block->bb_level, ARCH_CONVERT) > 0);
	kp = XFS_BTREE_KEY_ADDR(iocur_top->len, xfs_bmdr, block, idx,
		XFS_BTREE_BLOCK_MAXRECS(XFS_DFORK_DSIZE(dip, mp), xfs_bmdr, 0));
	return bitize((int)((char *)kp - (char *)block));
}

static int
bmrootd_ptr_count(
	void			*obj,
	int			startoff)
{
	xfs_bmdr_block_t	*block;
#ifdef DEBUG
	xfs_dinode_t		*dip = obj;
#endif

	ASSERT(bitoffs(startoff) == 0);
	ASSERT(obj == iocur_top->data);
	block = (xfs_bmdr_block_t *)((char *)obj + byteize(startoff));
	ASSERT((char *)block == XFS_DFORK_DPTR(dip));
	ASSERT(INT_GET(block->bb_level, ARCH_CONVERT) > 0);
	return INT_GET(block->bb_numrecs, ARCH_CONVERT);
}

static int
bmrootd_ptr_offset(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_bmdr_block_t	*block;
	xfs_bmdr_ptr_t		*pp;
	xfs_dinode_t		*dip;

	ASSERT(bitoffs(startoff) == 0);
	ASSERT(obj == iocur_top->data);
	dip = obj;
	block = (xfs_bmdr_block_t *)((char *)obj + byteize(startoff));
	ASSERT(INT_GET(block->bb_level, ARCH_CONVERT) > 0);
	pp = XFS_BTREE_PTR_ADDR(iocur_top->len, xfs_bmdr, block, idx,
		XFS_BTREE_BLOCK_MAXRECS(XFS_DFORK_DSIZE(dip, mp), xfs_bmdr, 0));
	return bitize((int)((char *)pp - (char *)block));
}

int
bmrootd_size(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_dinode_t		*dip;

	ASSERT(bitoffs(startoff) == 0);
	ASSERT(obj == iocur_top->data);
	ASSERT(idx == 0);
	dip = obj;
	return bitize((int)XFS_DFORK_DSIZE(dip, mp));
}
