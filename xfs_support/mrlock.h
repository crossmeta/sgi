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
 * CROSSMETA Windows porting changes.
 * Copyright (c) 2001 Supramani Sammandam.  suprasam _at_ crossmeta.org
 */
#ifndef __XFS_SUPPORT_MRLOCK_H__
#define __XFS_SUPPORT_MRLOCK_H__


/*
 * Implement mrlocks on Windows NT that work for XFS.
 *
 * These are sleep locks and not spinlocks. If one wants read/write spinlocks,
 * use read_lock, write_lock, ... see spinlock.h.
 */

#define mrlock_t	rwsleep_t

#define MR_ACCESS	1
#define MR_UPDATE	2

#define MRLOCK_BARRIER		0x1
#define MRLOCK_ALLOW_EQUAL_PRI	0x8

/*
 * mraccessf/mrupdatef take flags to be passed in while sleeping;
 * only PLTWAIT is currently supported.
 */

extern void __inline__	mraccessf(mrlock_t *, int);
extern void __inline__	mrupdatef(mrlock_t *, int);
extern void __inline__	mrlock(mrlock_t *, int, int);
extern void __inline__	mrunlock(mrlock_t *);
extern void __inline__	mraccunlock(mrlock_t *);
extern int __inline__	mrtryupdate(mrlock_t *);
extern int __inline__	mrtryaccess(mrlock_t *);
extern int 		mrtrypromote(mrlock_t *);
extern void __inline__	mrdemote(mrlock_t *);

extern int 		ismrlocked(mrlock_t *, int);
extern void __inline__	mrlock_init(mrlock_t *, int type, char *name,
					long sequence);
extern void __inline__	mrfree(mrlock_t *);

#define mrinit(mrp, nm)	mrlock_init(mrp, MRLOCK_BARRIER, nm, -1)
#define mraccess(mrp)	mraccessf(mrp, 0)	/* grab for READ/ACCESS */
#define mrupdate(mrp)	mrupdatef(mrp, 0)	/* grab for WRITE/UPDATE */

/*
 * We don't seem to need lock_type (only one supported), name, or
 * sequence. But, XFS will pass it so let's leave them here for now.
 */
#define mrlock_init(mrp, lock_type, name, sequence) \
	RWSLEEP_INIT(mrp);

#endif /* __XFS_SUPPORT_MRLOCK_H__ */
