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
#ifndef __XFS_SUPPORT_SEMA_H__
#define __XFS_SUPPORT_SEMA_H__


/*
 * sema_t structure just maps to KSEMAPHORE in windows NT
 */

#define sema_t		KSEMAPHORE

#define init_sema(sp, val, c, d)  	sema_init(sp, val)
#define initsema(sp, val)	  	sema_init(sp, val)
#define initnsema(sp, val, name)  	sema_init(sp, val)
#define	sema_init(sp, val)		\
	KeInitializeSemaphore(sp, val, MAXLONG)
#define psema(sp, b)			\
	((void)KeWaitForSingleObject(sp, Executive, KernelMode, FALSE, NULL))
#define vsema(sp)			\
	((void)KeReleaseSemaphore(sp, 0, 1, FALSE))
#define valusema(sp)			KeReadStateSemaphore(sp)
#define freesema(sema)

/*
 * Map cpsema (try to get the sema) to down_trylock. We need to switch
 * the return values since cpsema returns 1 (acquired) 0 (failed) and
 * down_trylock returns the reverse 0 (acquired) 1 (failed).
 */

static __inline__ int
cpsema(sema_t *sp)
{
	NTSTATUS stat;

	/*
	 * The correct way to do this would be to acquire
	 * thread dispatcher lock, check SignalState and then do
	 * WaitForSingleObject() call.
	 * Kludgy way & the only way here is to pass timeout of 0 value
	 */
	if (KeReadStateSemaphore(sp)) {
		LARGE_INTEGER tim = { 0, 0};

		stat = KeWaitForSingleObject(sp, Executive, KernelMode,
						 FALSE, &tim);
		if (stat == STATUS_TIMEOUT)
			return (0);
		return (1);	/* we got it ... */
	}
	return (0);
}

/*
 * Didn't do cvsema(sp). Not sure how to map this to up/down/...
 * It does a vsema if the values is < 0 other wise nothing.
 */

#endif /* __XFS_SUPPORT_SEMA_H__ */
