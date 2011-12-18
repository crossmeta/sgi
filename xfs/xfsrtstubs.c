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
 
#include <xfs.h>


int	
xfs_rtallocate_extent(
	struct xfs_trans	*tp,	
	xfs_rtblock_t		bno,	
	xfs_extlen_t		minlen,	
	xfs_extlen_t		maxlen,	
	xfs_extlen_t		*len,	
	xfs_alloctype_t		type,	
	int			wasdel,	
	xfs_extlen_t		prod,	
	xfs_rtblock_t		*rtblock)
{
    return nopkg(); 
}

int
xfs_rtfree_extent(
	struct xfs_trans	*tp,
	xfs_rtblock_t		bno,
	xfs_extlen_t		len)
{ 
    return nopkg(); 
}

int
xfs_rtpick_extent(
	struct xfs_mount	*mp,
	struct xfs_trans	*tp,
	xfs_extlen_t		len,
	xfs_rtblock_t		*pick)
{ 
    return nopkg(); 
}

int
xfs_rtmount_init(xfs_mount_t *mp)
{
	if (mp->m_sb.sb_rblocks == 0)
		return 0;
        cmn_err(CE_WARN, "XFS: RT not enabled (CONFIG_XFS_RT)\n");
	return nopkg();
}

int
xfs_rtmount_inodes(xfs_mount_t *mp)
{
	if (mp->m_sb.sb_rblocks == 0)
		return 0;
	return nopkg();
}

int
xfs_growfs_rt(
	xfs_mount_t *mp,
	xfs_growfs_rt_t	*in)
{
	return nopkg();
}

