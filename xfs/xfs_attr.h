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
#ifndef __XFS_ATTR_H__
#define	__XFS_ATTR_H__

/*
 * xfs_attr.h
 *
 * Large attribute lists are structured around Btrees where all the data
 * elements are in the leaf nodes.  Attribute names are hashed into an int,
 * then that int is used as the index into the Btree.  Since the hashval
 * of an attribute name may not be unique, we may have duplicate keys.
 * The internal links in the Btree are logical block offsets into the file.
 *
 * Small attribute lists use a different format and are packed as tightly
 * as possible so as to fit into the literal area of the inode.
 */

#ifdef XFS_ALL_TRACE
#define	XFS_ATTR_TRACE
#endif

#if !defined(DEBUG)
#undef XFS_ATTR_TRACE
#endif

/*========================================================================
 * Function prototypes for the kernel.
 *========================================================================*/

struct cred;
struct vnode;
struct xfs_inode;
struct attrlist_cursor_kern;
struct xfs_ext_attr;
struct xfs_da_args;

/*
 * Overall external interface routines.
 */
int xfs_attr_get(bhv_desc_t *, char *, char *, int *, int, struct cred *);
int xfs_attr_set(bhv_desc_t *, char *, char *, int, int, struct cred *);
int xfs_attr_remove(bhv_desc_t *, char *, int, struct cred *);
int xfs_attr_list(bhv_desc_t *, char *, int, int,
			 struct attrlist_cursor_kern *, struct cred *);
int xfs_attr_inactive(struct xfs_inode *dp);

int xfs_attr_node_get(struct xfs_da_args *);
int xfs_attr_leaf_get(struct xfs_da_args *);
int xfs_attr_shortform_getvalue(struct xfs_da_args *);
int xfs_attr_fetch(struct xfs_inode *, char *, char *, int);

#endif	/* __XFS_ATTR_H__ */
