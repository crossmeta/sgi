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

#ifndef __XFS_CXFS_H__
#define __XFS_CXFS_H__

/*
 * xfs_cxfs.h -- Interface cxfs presents to non-cell xfs code
 *
 * This header specifies the interface that cxfs V1 code presents to the 
 * non-cellular parts of xfs.  When the specfied routines are not present,
 * stubs will be provided.
 */

struct xfs_inode;
struct xfs_mount;
struct xfs_args;
struct mounta;
struct vfs;
struct vfsops;
struct vnode;
struct xfs_buf;

/*
 * Array mount routines.  Stubs provided for the non-CELL case.
 */
extern void cxfs_arrinit(void); /* Initialization for array mount logic. */
extern int cxfs_mount(	        /* For any specia mount handling. */
		struct xfs_mount    *mp,
                struct xfs_args     *ap,
		dev_t		    dev,
		int	            *client);
extern void cxfs_unmount(       /* For any special unmount handling. */
		struct xfs_mount    *mp);

/*
 * Other cxfs routines.  Stubs provided in non-CELL case.
 */
extern void cxfs_inode_quiesce(             /* Quiesce new inode for vfs */
		struct xfs_inode    *ip);   /* relocation. */
extern int cxfs_inode_qset(                 /* Set quiesce flag on inode. */
		struct xfs_inode    *ip);  
extern int cxfs_remount_server(             /* Modify mount parameters.  This */
                struct xfs_mount    *mp,    /* may result in vfs relocation. */
                struct mounta       *uap,   /* There are separate implementa- */
                struct xfs_args     *ap);   /* tions for arrays and ssi as */
                                            /* well as a stub for non-CELL. */

extern struct xfs_mount *get_cxfs_mountp(struct vfs *);

extern void cxfs_strat_complete_buf(struct xfs_buf *);

extern __uint64_t cfs_start_defrag(
		struct vnode		*vp);
extern void	cfs_end_defrag(
		struct vnode    	*vp,
		__uint64_t		handle);
		
#endif /* __XFS_CXFS_H__ */
