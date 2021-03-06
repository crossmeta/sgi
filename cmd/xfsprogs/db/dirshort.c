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
#include "dirshort.h"

static int	dir_sf_entry_name_count(void *obj, int startoff);
static int	dir_shortform_list_count(void *obj, int startoff);
static int	dir_shortform_list_offset(void *obj, int startoff, int idx);

#define	OFF(f)	bitize(offsetof(xfs_dir_shortform_t, f))
const field_t	dir_shortform_flds[] = {
	{ "hdr", FLDT_DIR_SF_HDR, OI(OFF(hdr)), C1, 0, TYP_NONE },
	{ "list", FLDT_DIR_SF_ENTRY, dir_shortform_list_offset,
	  dir_shortform_list_count, FLD_ARRAY|FLD_COUNT|FLD_OFFSET, TYP_NONE },
	{ NULL }
};

#define	HOFF(f)	bitize(offsetof(xfs_dir_sf_hdr_t, f))
const field_t	dir_sf_hdr_flds[] = {
	{ "parent", FLDT_DIR_INO, OI(HOFF(parent)), C1, 0, TYP_INODE },
	{ "count", FLDT_UINT8D, OI(HOFF(count)), C1, 0, TYP_NONE },
	{ NULL }
};

#define	EOFF(f)	bitize(offsetof(xfs_dir_sf_entry_t, f))
const field_t	dir_sf_entry_flds[] = {
	{ "inumber", FLDT_DIR_INO, OI(EOFF(inumber)), C1, 0, TYP_INODE },
	{ "namelen", FLDT_UINT8D, OI(EOFF(namelen)), C1, 0, TYP_NONE },
	{ "name", FLDT_CHARNS, OI(EOFF(name)), dir_sf_entry_name_count,
	  FLD_COUNT, TYP_NONE },
	{ NULL }
};

static int
dir_sf_entry_name_count(
	void			*obj,
	int			startoff)
{
	xfs_dir_sf_entry_t	*e;
	
	ASSERT(bitoffs(startoff) == 0);
	e = (xfs_dir_sf_entry_t *)((char *)obj + byteize(startoff));
	return e->namelen;
}

int
dir_sf_entry_size(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_dir_sf_entry_t	*e;
	int			i;
	xfs_dir_shortform_t	*sf;

	ASSERT(bitoffs(startoff) == 0);
	sf = (xfs_dir_shortform_t *)((char *)obj + byteize(startoff));
	e = &sf->list[0];
	for (i = 0; i < idx; i++)
		e = XFS_DIR_SF_NEXTENTRY(e);
	return bitize((int)XFS_DIR_SF_ENTSIZE_BYENTRY(e));
}

static int
dir_shortform_list_count(
	void			*obj,
	int			startoff)
{
	xfs_dir_shortform_t	*sf;

	ASSERT(bitoffs(startoff) == 0);
	sf = (xfs_dir_shortform_t *)((char *)obj + byteize(startoff));
	return sf->hdr.count;
}

static int
dir_shortform_list_offset(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_dir_sf_entry_t	*e;
	int			i;
	xfs_dir_shortform_t	*sf;

	ASSERT(bitoffs(startoff) == 0);
	sf = (xfs_dir_shortform_t *)((char *)obj + byteize(startoff));
	e = &sf->list[0];
	for (i = 0; i < idx; i++)
		e = XFS_DIR_SF_NEXTENTRY(e);
	return bitize((int)((char *)e - (char *)sf));
}

int
dirshort_size(
	void			*obj,
	int			startoff,
	int			idx)
{
	xfs_dir_sf_entry_t	*e;
	int			i;
	xfs_dir_shortform_t	*sf;

	ASSERT(bitoffs(startoff) == 0);
	ASSERT(idx == 0);
	sf = (xfs_dir_shortform_t *)((char *)obj + byteize(startoff));
	e = &sf->list[0];
	for (i = 0; i < sf->hdr.count; i++)
		e = XFS_DIR_SF_NEXTENTRY(e);
	return bitize((int)((char *)e - (char *)sf));
}
