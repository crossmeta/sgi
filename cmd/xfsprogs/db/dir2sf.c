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
#include "type.h"
#include "faddr.h"
#include "fprint.h"
#include "field.h"
#include "bit.h"
#include "dir2sf.h"

static int	dir2_inou_i4_count(void *obj, int startoff);
static int	dir2_inou_i8_count(void *obj, int startoff);
static int	dir2_sf_entry_inumber_offset(void *obj, int startoff, int idx);
static int	dir2_sf_entry_name_count(void *obj, int startoff);
static int	dir2_sf_list_count(void *obj, int startoff);
static int	dir2_sf_list_offset(void *obj, int startoff, int idx);

#define	OFF(f)	bitize(offsetof(xfs_dir2_sf_t, f))
const field_t	dir2sf_flds[] = {
	{ "hdr", FLDT_DIR2_SF_HDR, OI(OFF(hdr)), C1, 0, TYP_NONE },
	{ "list", FLDT_DIR2_SF_ENTRY, dir2_sf_list_offset, dir2_sf_list_count,
	  FLD_ARRAY|FLD_COUNT|FLD_OFFSET, TYP_NONE },
	{ NULL }
};

#define UOFF(f)	bitize(offsetof(xfs_dir2_inou_t, f))
const field_t	dir2_inou_flds[] = {
	{ "i8", FLDT_DIR2_INO8, OI(UOFF(i8)), dir2_inou_i8_count, FLD_COUNT,
	  TYP_INODE },
	{ "i4", FLDT_DIR2_INO4, OI(UOFF(i4)), dir2_inou_i4_count, FLD_COUNT,
	  TYP_INODE },
	{ NULL }
};

#define	HOFF(f)	bitize(offsetof(xfs_dir2_sf_hdr_t, f))
const field_t	dir2_sf_hdr_flds[] = {
	{ "count", FLDT_UINT8D, OI(HOFF(count)), C1, 0, TYP_NONE },
	{ "i8count", FLDT_UINT8D, OI(HOFF(i8count)), C1, 0, TYP_NONE },
	{ "parent", FLDT_DIR2_INOU, OI(HOFF(parent)), C1, 0, TYP_NONE },
	{ NULL }
};

#define	EOFF(f)	bitize(offsetof(xfs_dir2_sf_entry_t, f))
const field_t	dir2_sf_entry_flds[] = {
	{ "namelen", FLDT_UINT8D, OI(EOFF(namelen)), C1, 0, TYP_NONE },
	{ "offset", FLDT_DIR2_SF_OFF, OI(EOFF(offset)), C1, 0, TYP_NONE },
	{ "name", FLDT_CHARNS, OI(EOFF(name)), dir2_sf_entry_name_count,
	  FLD_COUNT, TYP_NONE },
	{ "inumber", FLDT_DIR2_INOU, dir2_sf_entry_inumber_offset, C1,
	  FLD_OFFSET, TYP_NONE },
	{ NULL }
};

/*ARGSUSED*/
static int
dir2_inou_i4_count(
	void		*obj,
	int		startoff)
{
	xfs_dir2_sf_t	*sf;

	ASSERT(bitoffs(startoff) == 0);
	sf = &((xfs_dinode_t *)obj)->di_u.di_dir2sf;
	return sf->hdr.i8count == 0;
}

/*ARGSUSED*/
static int
dir2_inou_i8_count(
	void		*obj,
	int		startoff)
{
	xfs_dir2_sf_t	*sf;

	ASSERT(bitoffs(startoff) == 0);
	sf = &((xfs_dinode_t *)obj)->di_u.di_dir2sf;
	return sf->hdr.i8count != 0;
}

/*ARGSUSED*/
int
dir2_inou_size(
	void		*obj,
	int		startoff,
	int		idx)
{
	xfs_dir2_sf_t	*sf;

	ASSERT(bitoffs(startoff) == 0);
	ASSERT(idx == 0);
	sf = &((xfs_dinode_t *)obj)->di_u.di_dir2sf;
	return bitize(sf->hdr.i8count ?
		      (uint)sizeof(xfs_dir2_ino8_t) :
		      (uint)sizeof(xfs_dir2_ino4_t));
}

static int
dir2_sf_entry_name_count(
	void			*obj,
	int			startoff)
{
	xfs_dir2_sf_entry_t	*e;

	ASSERT(bitoffs(startoff) == 0);
	e = (xfs_dir2_sf_entry_t *)((char *)obj + byteize(startoff));
	return e->namelen;
}

/*ARGSUSED*/
static int
dir2_sf_entry_inumber_offset(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_dir2_sf_entry_t	*e;

	ASSERT(bitoffs(startoff) == 0);
	ASSERT(idx == 0);
	e = (xfs_dir2_sf_entry_t *)((char *)obj + byteize(startoff));
	return bitize((int)((char *)XFS_DIR2_SF_INUMBERP(e) - (char *)e));
}

int
dir2_sf_entry_size(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_dir2_sf_entry_t	*e;
	int			i;
	xfs_dir2_sf_t		*sf;

	ASSERT(bitoffs(startoff) == 0);
	sf = (xfs_dir2_sf_t *)((char *)obj + byteize(startoff));
	e = XFS_DIR2_SF_FIRSTENTRY(sf);
	for (i = 0; i < idx; i++)
		e = XFS_DIR2_SF_NEXTENTRY(sf, e);
	return bitize((int)XFS_DIR2_SF_ENTSIZE_BYENTRY(sf, e));
}

/*ARGSUSED*/
int
dir2_sf_hdr_size(
	void		*obj,
	int		startoff,
	int		idx)
{
	xfs_dir2_sf_t	*sf;

	ASSERT(bitoffs(startoff) == 0);
	ASSERT(idx == 0);
	sf = (xfs_dir2_sf_t *)((char *)obj + byteize(startoff));
	return bitize(XFS_DIR2_SF_HDR_SIZE(sf->hdr.i8count));
}

static int
dir2_sf_list_count(
	void			*obj,
	int			startoff)
{
	xfs_dir2_sf_t		*sf;

	ASSERT(bitoffs(startoff) == 0);
	sf = (xfs_dir2_sf_t *)((char *)obj + byteize(startoff));
	return sf->hdr.count;
}

static int
dir2_sf_list_offset(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_dir2_sf_entry_t	*e;
	int			i;
	xfs_dir2_sf_t		*sf;

	ASSERT(bitoffs(startoff) == 0);
	sf = (xfs_dir2_sf_t *)((char *)obj + byteize(startoff));
	e = XFS_DIR2_SF_FIRSTENTRY(sf);
	for (i = 0; i < idx; i++)
		e = XFS_DIR2_SF_NEXTENTRY(sf, e);
	return bitize((int)((char *)e - (char *)sf));
}

/*ARGSUSED*/
int
dir2sf_size(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_dir2_sf_entry_t	*e;
	int			i;
	xfs_dir2_sf_t		*sf;

	ASSERT(bitoffs(startoff) == 0);
	ASSERT(idx == 0);
	sf = (xfs_dir2_sf_t *)((char *)obj + byteize(startoff));
	e = XFS_DIR2_SF_FIRSTENTRY(sf);
	for (i = 0; i < sf->hdr.count; i++)
		e = XFS_DIR2_SF_NEXTENTRY(sf, e);
	return bitize((int)((char *)e - (char *)sf));
}
