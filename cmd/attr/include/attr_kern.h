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
#ifndef __ATTR_KERN_H__
#define	__ATTR_KERN_H__

/*
 * The (experimental) Linux generic attribute syscall - attrctl(2)
 */

typedef union attr_obj {
	char	*path;
	int	fd;
	pid_t	pid;
} attr_obj_t;

/* 
 * attr_obj_t type identifiers
 */
#define ATTR_TYPE_FD		1	/* file descriptor */
#define ATTR_TYPE_PATH		2	/* path - follow symlinks */
#define ATTR_TYPE_LPATH		3	/* path - don't follow symlinks */
#define ATTR_TYPE_PID		4	/* process id */

/*
 * Kernel-internal version of the attrlist cursor.
 */
typedef struct attrlist_cursor_kern {
	__u32		hashval;	/* hash value of next entry to add */
	__u32		blkno;		/* block containing entry (suggestion)*/
	__u32		offset;		/* offset in list of equal-hashvals */
	__u16		pad1;		/* padding to match user-level */
	__u8		pad2;		/* padding to match user-level */
	__u8		initted;	/* T/F: cursor has been initialized */
} attrlist_cursor_kern_t;

#endif	/* __ATTR_KERN_H__ */
