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
 *
 * CROSSMETA Windows porting changes. http://www.crossmeta.org
 * Copyright (c) 2001 Supramani Sammandam.  suprasam _at_ crossmeta.org
 */
#ifndef __XFS_SUPER_H__
#define __XFS_SUPER_H__

void
linvfs_inode_attr_in(
	struct inode	*inode);

void
linvfs_release_inode(
	struct inode	*inode);

struct inode *
linvfs_make_inode(
	kdev_t		dev,
	struct super_block *sb);

int
fs_dounmount(
        bhv_desc_t      *bdp,
        int             flags,
        vnode_t         *rootvp,
        cred_t          *cr);

int
spectodevs(
	struct super_block *sb,
        struct xfs_args	*args,
        dev_t  		*ddevp,
        dev_t		*logdevp,
        dev_t		*rtdevp);

#endif	/* __XFS_SUPER_H__ */
