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
#include "bit.h"
#include "type.h"
#include "faddr.h"
#include "fprint.h"
#include "field.h"
#include "dir.h"
#include "dir2.h"
#include "mount.h"
#include "data.h"

static int	dir2_block_hdr_count(void *obj, int startoff);
static int	dir2_block_leaf_count(void *obj, int startoff);
static int	dir2_block_leaf_offset(void *obj, int startoff, int idx);
static int	dir2_block_tail_count(void *obj, int startoff);
static int	dir2_block_tail_offset(void *obj, int startoff, int idx);
static int	dir2_block_u_count(void *obj, int startoff);
static int	dir2_block_u_offset(void *obj, int startoff, int idx);
static int	dir2_data_union_freetag_count(void *obj, int startoff);
static int	dir2_data_union_inumber_count(void *obj, int startoff);
static int	dir2_data_union_length_count(void *obj, int startoff);
static int	dir2_data_union_name_count(void *obj, int startoff);
static int	dir2_data_union_namelen_count(void *obj, int startoff);
static int	dir2_data_union_tag_count(void *obj, int startoff);
static int	dir2_data_union_tag_offset(void *obj, int startoff, int idx);
static int	dir2_data_hdr_count(void *obj, int startoff);
static int	dir2_data_u_count(void *obj, int startoff);
static int	dir2_data_u_offset(void *obj, int startoff, int idx);
static int	dir2_free_bests_count(void *obj, int startoff);
static int	dir2_free_hdr_count(void *obj, int startoff);
static int	dir2_leaf_bests_count(void *obj, int startoff);
static int	dir2_leaf_bests_offset(void *obj, int startoff, int idx);
static int	dir2_leaf_ents_count(void *obj, int startoff);
static int	dir2_leaf_hdr_count(void *obj, int startoff);
static int	dir2_leaf_tail_count(void *obj, int startoff);
static int	dir2_leaf_tail_offset(void *obj, int startoff, int idx);
static int	dir2_node_btree_count(void *obj, int startoff);
static int	dir2_node_hdr_count(void *obj, int startoff);

const field_t	dir2_hfld[] = {
	{ "", FLDT_DIR2, OI(0), C1, 0, TYP_NONE },
	{ NULL }
};

#define	BOFF(f)	bitize(offsetof(xfs_dir2_block_t, f))
#define	DOFF(f)	bitize(offsetof(xfs_dir2_data_t, f))
#define	FOFF(f)	bitize(offsetof(xfs_dir2_free_t, f))
#define	LOFF(f)	bitize(offsetof(xfs_dir2_leaf_t, f))
#define	NOFF(f)	bitize(offsetof(xfs_da_intnode_t, f))
const field_t	dir2_flds[] = {
	{ "bhdr", FLDT_DIR2_DATA_HDR, OI(BOFF(hdr)), dir2_block_hdr_count,
	  FLD_COUNT, TYP_NONE },
	{ "bu", FLDT_DIR2_DATA_UNION, dir2_block_u_offset, dir2_block_u_count,
	  FLD_ARRAY|FLD_OFFSET|FLD_COUNT, TYP_NONE },
	{ "bleaf", FLDT_DIR2_LEAF_ENTRY, dir2_block_leaf_offset,
	  dir2_block_leaf_count, FLD_ARRAY|FLD_OFFSET|FLD_COUNT, TYP_NONE },
	{ "btail", FLDT_DIR2_BLOCK_TAIL, dir2_block_tail_offset,
	  dir2_block_tail_count, FLD_OFFSET|FLD_COUNT, TYP_NONE },
	{ "dhdr", FLDT_DIR2_DATA_HDR, OI(DOFF(hdr)), dir2_data_hdr_count,
	  FLD_COUNT, TYP_NONE },
	{ "du", FLDT_DIR2_DATA_UNION, dir2_data_u_offset, dir2_data_u_count,
	  FLD_ARRAY|FLD_OFFSET|FLD_COUNT, TYP_NONE },
	{ "lhdr", FLDT_DIR2_LEAF_HDR, OI(LOFF(hdr)), dir2_leaf_hdr_count,
	  FLD_COUNT, TYP_NONE },
	{ "lbests", FLDT_DIR2_DATA_OFF, dir2_leaf_bests_offset,
	  dir2_leaf_bests_count, FLD_ARRAY|FLD_OFFSET|FLD_COUNT, TYP_NONE },
	{ "lents", FLDT_DIR2_LEAF_ENTRY, OI(LOFF(ents)), dir2_leaf_ents_count,
	  FLD_ARRAY|FLD_COUNT, TYP_NONE },
	{ "ltail", FLDT_DIR2_LEAF_TAIL, dir2_leaf_tail_offset,
	  dir2_leaf_tail_count, FLD_OFFSET|FLD_COUNT, TYP_NONE },
	{ "nhdr", FLDT_DIR_NODE_HDR, OI(NOFF(hdr)), dir2_node_hdr_count,
	  FLD_COUNT, TYP_NONE },
	{ "nbtree", FLDT_DIR_NODE_ENTRY, OI(NOFF(btree)), dir2_node_btree_count,
	  FLD_ARRAY|FLD_COUNT, TYP_NONE },
	{ "fhdr", FLDT_DIR2_FREE_HDR, OI(FOFF(hdr)), dir2_free_hdr_count,
	  FLD_COUNT, TYP_NONE },
	{ "fbests", FLDT_DIR2_DATA_OFFNZ, OI(FOFF(bests)),
	  dir2_free_bests_count, FLD_ARRAY|FLD_COUNT, TYP_NONE },
	{ NULL }
};

#define	BTOFF(f)	bitize(offsetof(xfs_dir2_block_tail_t, f))
const field_t	dir2_block_tail_flds[] = {
	{ "count", FLDT_UINT32D, OI(BTOFF(count)), C1, 0, TYP_NONE },
	{ "stale", FLDT_UINT32D, OI(BTOFF(stale)), C1, 0, TYP_NONE },
	{ NULL }
};

#define	DFOFF(f)	bitize(offsetof(xfs_dir2_data_free_t, f))
const field_t	dir2_data_free_flds[] = {
	{ "offset", FLDT_DIR2_DATA_OFF, OI(DFOFF(offset)), C1, 0, TYP_NONE },
	{ "length", FLDT_DIR2_DATA_OFF, OI(DFOFF(length)), C1, 0, TYP_NONE },
	{ NULL }
};

#define	DHOFF(f)	bitize(offsetof(xfs_dir2_data_hdr_t, f))
const field_t	dir2_data_hdr_flds[] = {
	{ "magic", FLDT_UINT32X, OI(DHOFF(magic)), C1, 0, TYP_NONE },
	{ "bestfree", FLDT_DIR2_DATA_FREE, OI(DHOFF(bestfree)),
	  CI(XFS_DIR2_DATA_FD_COUNT), FLD_ARRAY, TYP_NONE },
	{ NULL }
};

#define	DEOFF(f)	bitize(offsetof(xfs_dir2_data_entry_t, f))
#define	DUOFF(f)	bitize(offsetof(xfs_dir2_data_unused_t, f))
const field_t	dir2_data_union_flds[] = {
	{ "freetag", FLDT_UINT16X, OI(DUOFF(freetag)),
	  dir2_data_union_freetag_count, FLD_COUNT, TYP_NONE },
	{ "inumber", FLDT_INO, OI(DEOFF(inumber)),
	  dir2_data_union_inumber_count, FLD_COUNT, TYP_INODE },
	{ "length", FLDT_DIR2_DATA_OFF, OI(DUOFF(length)),
	  dir2_data_union_length_count, FLD_COUNT, TYP_NONE },
	{ "namelen", FLDT_UINT8D, OI(DEOFF(namelen)),
	  dir2_data_union_namelen_count, FLD_COUNT, TYP_NONE },
	{ "name", FLDT_CHARNS, OI(DEOFF(name)), dir2_data_union_name_count,
	  FLD_COUNT, TYP_NONE },
	{ "tag", FLDT_DIR2_DATA_OFF, dir2_data_union_tag_offset,
	  dir2_data_union_tag_count, FLD_OFFSET|FLD_COUNT, TYP_NONE },
	{ NULL }
};

#define	LEOFF(f)	bitize(offsetof(xfs_dir2_leaf_entry_t, f))
const field_t	dir2_leaf_entry_flds[] = {
	{ "hashval", FLDT_UINT32X, OI(LEOFF(hashval)), C1, 0, TYP_NONE },
	{ "address", FLDT_UINT32X, OI(LEOFF(address)), C1, 0, TYP_NONE },
	{ NULL }
};

#define	LHOFF(f)	bitize(offsetof(xfs_dir2_leaf_hdr_t, f))
const field_t	dir2_leaf_hdr_flds[] = {
	{ "info", FLDT_DIR_BLKINFO, OI(LHOFF(info)), C1, 0, TYP_NONE },
	{ "count", FLDT_UINT16D, OI(LHOFF(count)), C1, 0, TYP_NONE },
	{ "stale", FLDT_UINT16D, OI(LHOFF(stale)), C1, 0, TYP_NONE },
	{ NULL }
};

#define	LTOFF(f)	bitize(offsetof(xfs_dir2_leaf_tail_t, f))
const field_t	dir2_leaf_tail_flds[] = {
	{ "bestcount", FLDT_UINT32D, OI(LTOFF(bestcount)), C1, 0, TYP_NONE },
	{ NULL }
};

#define	FHOFF(f)	bitize(offsetof(xfs_dir2_free_hdr_t, f))
const field_t	dir2_free_hdr_flds[] = {
	{ "magic", FLDT_UINT32X, OI(FHOFF(magic)), C1, 0, TYP_NONE },
	{ "firstdb", FLDT_INT32D, OI(FHOFF(firstdb)), C1, 0, TYP_NONE },
	{ "nvalid", FLDT_INT32D, OI(FHOFF(nvalid)), C1, 0, TYP_NONE },
	{ "nused", FLDT_INT32D, OI(FHOFF(nused)), C1, 0, TYP_NONE },
	{ NULL }
};

/*ARGSUSED*/
static int
dir2_block_hdr_count(
	void			*obj,
	int			startoff)
{
	xfs_dir2_block_t	*block;

	ASSERT(startoff == 0);
	block = obj;
	return INT_GET(block->hdr.magic, ARCH_CONVERT) == XFS_DIR2_BLOCK_MAGIC;
}

/*ARGSUSED*/
static int
dir2_block_leaf_count(
	void			*obj,
	int			startoff)
{
	xfs_dir2_block_t	*block;
	xfs_dir2_block_tail_t	*btp;

	ASSERT(startoff == 0);
	block = obj;
	if (INT_GET(block->hdr.magic, ARCH_CONVERT) != XFS_DIR2_BLOCK_MAGIC)
		return 0;
	btp = XFS_DIR2_BLOCK_TAIL_P(mp, block);
	return INT_GET(btp->count, ARCH_CONVERT);
}

/*ARGSUSED*/
static int
dir2_block_leaf_offset(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_dir2_block_t	*block;
	xfs_dir2_block_tail_t	*btp;
	xfs_dir2_leaf_entry_t	*lep;

	ASSERT(startoff == 0);
	block = obj;
	ASSERT(INT_GET(block->hdr.magic, ARCH_CONVERT) == XFS_DIR2_BLOCK_MAGIC);
	btp = XFS_DIR2_BLOCK_TAIL_P(mp, block);
	lep = XFS_DIR2_BLOCK_LEAF_P_ARCH(btp, ARCH_CONVERT) + idx;
	return bitize((int)((char *)lep - (char *)block));
}

/*ARGSUSED*/
static int
dir2_block_tail_count(
	void			*obj,
	int			startoff)
{
	xfs_dir2_block_t	*block;

	ASSERT(startoff == 0);
	block = obj;
	return INT_GET(block->hdr.magic, ARCH_CONVERT) == XFS_DIR2_BLOCK_MAGIC;
}

/*ARGSUSED*/
static int
dir2_block_tail_offset(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_dir2_block_t	*block;
	xfs_dir2_block_tail_t	*btp;

	ASSERT(startoff == 0);
	ASSERT(idx == 0);
	block = obj;
	ASSERT(INT_GET(block->hdr.magic, ARCH_CONVERT) == XFS_DIR2_BLOCK_MAGIC);
	btp = XFS_DIR2_BLOCK_TAIL_P(mp, block);
	return bitize((int)((char *)btp - (char *)block));
}

/*ARGSUSED*/
static int
dir2_block_u_count(
	void			*obj,
	int			startoff)
{
	xfs_dir2_block_t	*block;
	xfs_dir2_block_tail_t	*btp;
	xfs_dir2_data_entry_t	*dep;
	xfs_dir2_data_unused_t	*dup;
	char			*endptr;
	int			i;
	char			*ptr;

	ASSERT(startoff == 0);
	block = obj;
	if (INT_GET(block->hdr.magic, ARCH_CONVERT) != XFS_DIR2_BLOCK_MAGIC)
		return 0;
	btp = XFS_DIR2_BLOCK_TAIL_P(mp, block);
	ptr = (char *)block->u;
	endptr = (char *)XFS_DIR2_BLOCK_LEAF_P_ARCH(btp, ARCH_CONVERT);
	for (i = 0; ptr < endptr; i++) {
		dup = (xfs_dir2_data_unused_t *)ptr;
		if (INT_GET(dup->freetag, ARCH_CONVERT) == XFS_DIR2_DATA_FREE_TAG)
			ptr += INT_GET(dup->length, ARCH_CONVERT);
		else {
			dep = (xfs_dir2_data_entry_t *)ptr;
			ptr += XFS_DIR2_DATA_ENTSIZE(dep->namelen);
		}
	}
	return i;
}

/*ARGSUSED*/
static int
dir2_block_u_offset(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_dir2_block_t	*block;
	xfs_dir2_block_tail_t	*btp;
	xfs_dir2_data_entry_t	*dep;
	xfs_dir2_data_unused_t	*dup;
				/*REFERENCED*/
	char			*endptr;
	int			i;
	char			*ptr;

	ASSERT(startoff == 0);
	block = obj;
	ASSERT(INT_GET(block->hdr.magic, ARCH_CONVERT) == XFS_DIR2_BLOCK_MAGIC);
	btp = XFS_DIR2_BLOCK_TAIL_P(mp, block);
	ptr = (char *)block->u;
	endptr = (char *)XFS_DIR2_BLOCK_LEAF_P_ARCH(btp, ARCH_CONVERT);
	for (i = 0; i < idx; i++) {
		ASSERT(ptr < endptr);
		dup = (xfs_dir2_data_unused_t *)ptr;
		if (INT_GET(dup->freetag, ARCH_CONVERT) == XFS_DIR2_DATA_FREE_TAG)
			ptr += INT_GET(dup->length, ARCH_CONVERT);
		else {
			dep = (xfs_dir2_data_entry_t *)ptr;
			ptr += XFS_DIR2_DATA_ENTSIZE(dep->namelen);
		}
	}
	return bitize((int)(ptr - (char *)block));
}

static int
dir2_data_union_freetag_count(
	void			*obj,
	int			startoff)
{
	xfs_dir2_data_unused_t	*dup;
	char			*end;

	ASSERT(bitoffs(startoff) == 0);
	dup = (xfs_dir2_data_unused_t *)((char *)obj + byteize(startoff));
	end = (char *)&dup->freetag + sizeof(dup->freetag);
	return end <= (char *)obj + mp->m_dirblksize &&
	       INT_GET(dup->freetag, ARCH_CONVERT) == XFS_DIR2_DATA_FREE_TAG;
}

static int
dir2_data_union_inumber_count(
	void			*obj,
	int			startoff)
{
	xfs_dir2_data_entry_t	*dep;
	xfs_dir2_data_unused_t	*dup;
	char			*end;

	ASSERT(bitoffs(startoff) == 0);
	dup = (xfs_dir2_data_unused_t *)((char *)obj + byteize(startoff));
	dep = (xfs_dir2_data_entry_t *)dup;
	end = (char *)&dep->inumber + sizeof(dep->inumber);
	return end <= (char *)obj + mp->m_dirblksize &&
	       INT_GET(dup->freetag, ARCH_CONVERT) != XFS_DIR2_DATA_FREE_TAG;
}

static int
dir2_data_union_length_count(
	void			*obj,
	int			startoff)
{
	xfs_dir2_data_unused_t	*dup;
	char			*end;

	ASSERT(bitoffs(startoff) == 0);
	dup = (xfs_dir2_data_unused_t *)((char *)obj + byteize(startoff));
	end = (char *)&dup->length + sizeof(dup->length);
	return end <= (char *)obj + mp->m_dirblksize &&
	       INT_GET(dup->freetag, ARCH_CONVERT) == XFS_DIR2_DATA_FREE_TAG;
}

static int
dir2_data_union_name_count(
	void			*obj,
	int			startoff)
{
	xfs_dir2_data_entry_t	*dep;
	xfs_dir2_data_unused_t	*dup;
	char			*end;

	ASSERT(bitoffs(startoff) == 0);
	dup = (xfs_dir2_data_unused_t *)((char *)obj + byteize(startoff));
	dep = (xfs_dir2_data_entry_t *)dup;
	end = (char *)&dep->namelen + sizeof(dep->namelen);
	if (end >= (char *)obj + mp->m_dirblksize ||
	    INT_GET(dup->freetag, ARCH_CONVERT) == XFS_DIR2_DATA_FREE_TAG)
		return 0;
	end = (char *)&dep->name[0] + dep->namelen;
	return end <= (char *)obj + mp->m_dirblksize ? dep->namelen : 0;
}

static int
dir2_data_union_namelen_count(
	void			*obj,
	int			startoff)
{
	xfs_dir2_data_entry_t	*dep;
	xfs_dir2_data_unused_t	*dup;
	char			*end;

	ASSERT(bitoffs(startoff) == 0);
	dup = (xfs_dir2_data_unused_t *)((char *)obj + byteize(startoff));
	dep = (xfs_dir2_data_entry_t *)dup;
	end = (char *)&dep->namelen + sizeof(dep->namelen);
	return end <= (char *)obj + mp->m_dirblksize &&
	       INT_GET(dup->freetag, ARCH_CONVERT) != XFS_DIR2_DATA_FREE_TAG;
}

static int
dir2_data_union_tag_count(
	void			*obj,
	int			startoff)
{
	xfs_dir2_data_entry_t	*dep;
	xfs_dir2_data_unused_t	*dup;
	char			*end;
	xfs_dir2_data_off_t	*tagp;

	ASSERT(bitoffs(startoff) == 0);
	dup = (xfs_dir2_data_unused_t *)((char *)obj + byteize(startoff));
	dep = (xfs_dir2_data_entry_t *)dup;
	end = (char *)&dup->freetag + sizeof(dup->freetag);
	if (end > (char *)obj + mp->m_dirblksize)
		return 0;
	if (INT_GET(dup->freetag, ARCH_CONVERT) == XFS_DIR2_DATA_FREE_TAG) {
		end = (char *)&dup->length + sizeof(dup->length);
		if (end > (char *)obj + mp->m_dirblksize)
			return 0;
		tagp = XFS_DIR2_DATA_UNUSED_TAG_P_ARCH(dup, ARCH_CONVERT);
	} else {
		end = (char *)&dep->namelen + sizeof(dep->namelen);
		if (end > (char *)obj + mp->m_dirblksize)
			return 0;
		tagp = XFS_DIR2_DATA_ENTRY_TAG_P(dep);
	}
	end = (char *)tagp + sizeof(*tagp);
	return end <= (char *)obj + mp->m_dirblksize;
}

/*ARGSUSED*/
static int
dir2_data_union_tag_offset(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_dir2_data_entry_t	*dep;
	xfs_dir2_data_unused_t	*dup;

	ASSERT(bitoffs(startoff) == 0);
	ASSERT(idx == 0);
	dup = (xfs_dir2_data_unused_t *)((char *)obj + byteize(startoff));
	if (INT_GET(dup->freetag, ARCH_CONVERT) == XFS_DIR2_DATA_FREE_TAG)
		return bitize((int)((char *)XFS_DIR2_DATA_UNUSED_TAG_P_ARCH(dup, ARCH_CONVERT) -
				    (char *)dup));
	dep = (xfs_dir2_data_entry_t *)dup;
	return bitize((int)((char *)XFS_DIR2_DATA_ENTRY_TAG_P(dep) -
			    (char *)dep));
}

/*ARGSUSED*/
static int
dir2_data_hdr_count(
	void			*obj,
	int			startoff)
{
	xfs_dir2_data_t		*data;

	ASSERT(startoff == 0);
	data = obj;
	return INT_GET(data->hdr.magic, ARCH_CONVERT) == XFS_DIR2_DATA_MAGIC;
}

/*ARGSUSED*/
static int
dir2_data_u_count(
	void			*obj,
	int			startoff)
{
	xfs_dir2_data_t		*data;
	xfs_dir2_data_entry_t	*dep;
	xfs_dir2_data_unused_t	*dup;
	char			*endptr;
	int			i;
	char			*ptr;

	ASSERT(startoff == 0);
	data = obj;
	if (INT_GET(data->hdr.magic, ARCH_CONVERT) != XFS_DIR2_DATA_MAGIC)
		return 0;
	ptr = (char *)data->u;
	endptr = (char *)data + mp->m_dirblksize;
	for (i = 0; ptr < endptr; i++) {
		dup = (xfs_dir2_data_unused_t *)ptr;
		if (INT_GET(dup->freetag, ARCH_CONVERT) == XFS_DIR2_DATA_FREE_TAG)
			ptr += INT_GET(dup->length, ARCH_CONVERT);
		else {
			dep = (xfs_dir2_data_entry_t *)ptr;
			ptr += XFS_DIR2_DATA_ENTSIZE(dep->namelen);
		}
	}
	return i;
}

/*ARGSUSED*/
static int
dir2_data_u_offset(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_dir2_data_t		*data;
	xfs_dir2_data_entry_t	*dep;
	xfs_dir2_data_unused_t	*dup;
				/*REFERENCED*/
	char			*endptr;
	int			i;
	char			*ptr;

	ASSERT(startoff == 0);
	data = obj;
	ASSERT(INT_GET(data->hdr.magic, ARCH_CONVERT) == XFS_DIR2_DATA_MAGIC);
	ptr = (char *)data->u;
	endptr = (char *)data + mp->m_dirblksize;
	for (i = 0; i < idx; i++) {
		ASSERT(ptr < endptr);
		dup = (xfs_dir2_data_unused_t *)ptr;
		if (INT_GET(dup->freetag, ARCH_CONVERT) == XFS_DIR2_DATA_FREE_TAG)
			ptr += INT_GET(dup->length, ARCH_CONVERT);
		else {
			dep = (xfs_dir2_data_entry_t *)ptr;
			ptr += XFS_DIR2_DATA_ENTSIZE(dep->namelen);
		}
	}
	return bitize((int)(ptr - (char *)data));
}

/*ARGSUSED*/
int
dir2_data_union_size(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_dir2_data_entry_t	*dep;
	xfs_dir2_data_unused_t	*dup;

	ASSERT(bitoffs(startoff) == 0);
	ASSERT(idx == 0);
	dup = (xfs_dir2_data_unused_t *)((char *)obj + byteize(startoff));
	if (INT_GET(dup->freetag, ARCH_CONVERT) == XFS_DIR2_DATA_FREE_TAG)
		return bitize(INT_GET(dup->length, ARCH_CONVERT));
	else {
		dep = (xfs_dir2_data_entry_t *)dup;
		return bitize(XFS_DIR2_DATA_ENTSIZE(dep->namelen));
	}
}

/*ARGSUSED*/
static int
dir2_free_bests_count(
	void			*obj,
	int			startoff)
{
	xfs_dir2_free_t		*free;

	ASSERT(startoff == 0);
	free = obj;
	if (INT_GET(free->hdr.magic, ARCH_CONVERT) != XFS_DIR2_FREE_MAGIC)
		return 0;
	return INT_GET(free->hdr.nvalid, ARCH_CONVERT);
}

/*ARGSUSED*/
static int
dir2_free_hdr_count(
	void			*obj,
	int			startoff)
{
	xfs_dir2_free_t		*free;

	ASSERT(startoff == 0);
	free = obj;
	return INT_GET(free->hdr.magic, ARCH_CONVERT) == XFS_DIR2_FREE_MAGIC;
}

/*ARGSUSED*/
static int
dir2_leaf_bests_count(
	void			*obj,
	int			startoff)
{
	xfs_dir2_leaf_t		*leaf;
	xfs_dir2_leaf_tail_t	*ltp;

	ASSERT(startoff == 0);
	leaf = obj;
	if (INT_GET(leaf->hdr.info.magic, ARCH_CONVERT) != XFS_DIR2_LEAF1_MAGIC)
		return 0;
	ltp = XFS_DIR2_LEAF_TAIL_P(mp, leaf);
	return INT_GET(ltp->bestcount, ARCH_CONVERT);
}

/*ARGSUSED*/
static int
dir2_leaf_bests_offset(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_dir2_data_off_t	*lbp;
	xfs_dir2_leaf_t		*leaf;
	xfs_dir2_leaf_tail_t	*ltp;

	ASSERT(startoff == 0);
	leaf = obj;
	ASSERT(INT_GET(leaf->hdr.info.magic, ARCH_CONVERT) == XFS_DIR2_LEAF1_MAGIC);
	ltp = XFS_DIR2_LEAF_TAIL_P(mp, leaf);
	lbp = XFS_DIR2_LEAF_BESTS_P_ARCH(ltp, ARCH_CONVERT) + idx;
	return bitize((int)((char *)lbp - (char *)leaf));
}

/*ARGSUSED*/
static int
dir2_leaf_ents_count(
	void			*obj,
	int			startoff)
{
	xfs_dir2_leaf_t		*leaf;

	ASSERT(startoff == 0);
	leaf = obj;
	if (INT_GET(leaf->hdr.info.magic, ARCH_CONVERT) != XFS_DIR2_LEAF1_MAGIC &&
	    INT_GET(leaf->hdr.info.magic, ARCH_CONVERT) != XFS_DIR2_LEAFN_MAGIC)
		return 0;
	return INT_GET(leaf->hdr.count, ARCH_CONVERT);
}

/*ARGSUSED*/
static int
dir2_leaf_hdr_count(
	void			*obj,
	int			startoff)
{
	xfs_dir2_leaf_t		*leaf;
	
	ASSERT(startoff == 0);
	leaf = obj;
	return INT_GET(leaf->hdr.info.magic, ARCH_CONVERT) == XFS_DIR2_LEAF1_MAGIC ||
	       INT_GET(leaf->hdr.info.magic, ARCH_CONVERT) == XFS_DIR2_LEAFN_MAGIC;
}

/*ARGSUSED*/
static int
dir2_leaf_tail_count(
	void			*obj,
	int			startoff)
{
	xfs_dir2_leaf_t		*leaf;

	ASSERT(startoff == 0);
	leaf = obj;
	return INT_GET(leaf->hdr.info.magic, ARCH_CONVERT) == XFS_DIR2_LEAF1_MAGIC;
}

/*ARGSUSED*/
static int
dir2_leaf_tail_offset(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_dir2_leaf_t		*leaf;
	xfs_dir2_leaf_tail_t	*ltp;

	ASSERT(startoff == 0);
	ASSERT(idx == 0);
	leaf = obj;
	ASSERT(INT_GET(leaf->hdr.info.magic, ARCH_CONVERT) == XFS_DIR2_LEAF1_MAGIC);
	ltp = XFS_DIR2_LEAF_TAIL_P(mp, leaf);
	return bitize((int)((char *)ltp - (char *)leaf));
}

/*ARGSUSED*/
static int
dir2_node_btree_count(
	void			*obj,
	int			startoff)
{
	xfs_da_intnode_t	*node;

	ASSERT(startoff == 0);
	node = obj;
	if (INT_GET(node->hdr.info.magic, ARCH_CONVERT) != XFS_DA_NODE_MAGIC)
		return 0;
	return INT_GET(node->hdr.count, ARCH_CONVERT);
}

/*ARGSUSED*/
static int
dir2_node_hdr_count(
	void			*obj,
	int			startoff)
{
	xfs_da_intnode_t	*node;

	ASSERT(startoff == 0);
	node = obj;
	return INT_GET(node->hdr.info.magic, ARCH_CONVERT) == XFS_DA_NODE_MAGIC;
}

/*ARGSUSED*/
int
dir2_size(
	void	*obj,
	int	startoff,
	int	idx)
{
	return bitize(mp->m_dirblksize);
}
