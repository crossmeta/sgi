/*
 * Copyright (c) 2001 Silicon Graphics, Inc.  All Rights Reserved.
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


#ifndef __XFS_ACL_H__
#define __XFS_ACL_H__

struct xfs_inode;
struct vattr;
struct vnode;

extern int  xfs_acl_inherit(struct vnode *, struct vnode *, struct vattr *);
extern int  xfs_acl_iaccess(struct xfs_inode *, mode_t, cred_t *);
extern int  xfs_acl_get(struct vnode *, struct acl *, struct acl *);
extern int  xfs_acl_set(struct vnode *, struct acl *, struct acl *);

#ifdef CONFIG_FS_POSIX_ACL
#define _ACL_INHERIT(p,c,v)	(xfs_acl_inherit(p,c,v))
#define _ACL_XFS_IACCESS(a,b,c)	(xfs_acl_iaccess(a,b,c))
#else
#define _ACL_INHERIT(p,c,v)	(0)
#define _ACL_XFS_IACCESS(a,b,c)	(-1)
#endif

#endif /* __XFS_ACL_H__ */
