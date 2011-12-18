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
#ifndef __XFS_ITABLE_H__
#define	__XFS_ITABLE_H__

struct xfs_mount;
struct xfs_trans;

/*
 * Prototypes for visible xfs_itable.c routines.
 */

/*
 * Convert file descriptor of a file in the filesystem to
 * a mount structure pointer.
 */
int					/* error status */
xfs_fd_to_mp(
	int			fd,	/* file descriptor */
	int			wperm,	/* need write perm on device fd */
	struct xfs_mount	**mpp,	/* output: mount structure pointer */
	int			rperm);	/* need root perm on file fd */

/* 
 * xfs_bulkstat() is used to fill in xfs_bstat structures as well as dm_stat
 * structures (by the dmi library). This is a pointer to a formatter function
 * that will iget the inode and fill in the appropriate structure.
 * see xfs_bulkstat_one() and dm_bulkstat_one() in dmi_xfs.c
 */
typedef int (*bulkstat_one_pf)(struct xfs_mount	*mp, 
			       struct xfs_trans	*tp,
			       xfs_ino_t   	ino,
			       void	     	*buffer,
			       xfs_daddr_t	bno,
			       void		*dip,
			       int		*stat);
/*
 * Values for stat return value.
 */
#define	BULKSTAT_RV_NOTHING	0
#define	BULKSTAT_RV_DIDONE	1
#define	BULKSTAT_RV_GIVEUP	2

/*
 * Values for bulkstat flag argument.
 */
#define	BULKSTAT_FG_IGET	0x1	/* Go through the buffer cache */
#define	BULKSTAT_FG_QUICK	0x2	/* No iget, walk the dinode cluster */
#define BULKSTAT_FG_VFSLOCKED	0x4	/* Already have vfs lock */

/*
 * Return stat information in bulk (by-inode) for the filesystem.
 */
int					/* error status */
xfs_bulkstat(
	struct xfs_mount	*mp,	/* mount point for filesystem */
	struct xfs_trans	*tp,	/* transaction pointer */
	xfs_ino_t	*lastino,	/* last inode returned */
	int		*count,		/* size of buffer/count returned */
	bulkstat_one_pf formatter,	/* func that'd fill a single buf */
	size_t		statstruct_size,/* sizeof struct that we're filling */
	xfs_caddr_t	ubuffer,	/* buffer with inode stats */
	int		flags,		/* flag to control access method */
	int		*done);		/* 1 if there're more stats to get */

int
xfs_bulkstat_single(
	struct xfs_mount	*mp,
	xfs_ino_t		*lastinop,
	xfs_caddr_t		buffer,
	int			*done);

int
xfs_bulkstat_one(
	struct xfs_mount	*mp,
	struct xfs_trans	*tp,
	xfs_ino_t		ino,
	void			*buffer,
	xfs_daddr_t		bno,
	void			*dibuff,
	int			*stat);

#endif	/* __XFS_ITABLE_H__ */
