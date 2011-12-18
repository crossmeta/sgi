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


#include <xfs_support/types.h>
#include <sys/kern_svcs.h>
#include <xfs_support/spin.h>
#include <xfs_support/mrlock.h>

spinlock_t Atomic_spin = SPIN_LOCK_UNLOCKED;

/*
 * Macros to lock/unlock the mrlock_t.
 */
#define MRLOCK_INT(m, s)	(s) = LOCK(&(m)->SpinLock)
#define MRUNLOCK_INT(m, s)	UNLOCK(&(m)->SpinLock, s)
#define MRLOCK(m)
#define MRUNLOCK(m)


/* ARGSUSED */
void __inline__
mrfree(mrlock_t *mrp)
{

	RWSLEEP_DEINIT(mrp);
}

/* ARGSUSED */
void __inline__
mrlock(mrlock_t *mrp, int type, int flags)
{
	if (type == MR_ACCESS)
		mraccess(mrp);
	else
		mrupdate(mrp);
}

/* ARGSUSED */
void __inline__
mraccessf(mrlock_t *mrp, int flags)
{

	RWSLEEP_RDLOCK(mrp);
}

void __inline__
mrupdatef(mrlock_t *mrp, int flags)
{

	RWSLEEP_WRLOCK(mrp);
}

int __inline__
mrtryaccess(mrlock_t *mrp)
{

	return (RWSLEEP_TRYRDLOCK(mrp));
}

/*
 * This is really a kludge. Use it with care
 */
int
mrtrypromote(mrlock_t *mrp)
{
	long	s;
	POWNER_ENTRY op1, op2;

	MRLOCK_INT(mrp, s);

	op2 = &mrp->OwnerThreads[1];
	if (mrp->ActiveCount == 1 && 
	    op2->OwnerThread == PsGetCurrentThread()) { 
		mrp->Flag |= ResourceOwnedExclusive;
		op1 = &mrp->OwnerThreads[0];
		op1->OwnerThread = op2->OwnerThread;
		op1->OwnerCount = op2->OwnerCount;
		op2->OwnerThread = NULL;
		op2->OwnerCount = 0;
		MRUNLOCK_INT(mrp, s);
		return (1);
	}

	MRUNLOCK_INT(mrp, s);
	return (0);
}

int __inline__
mrtryupdate(mrlock_t *mrp)
{

	return (RWSLEEP_TRYWRLOCK(mrp));
}


void __inline__
mraccunlock(mrlock_t *mrp)
{

	RWSLEEP_UNLOCK(mrp);
}

void __inline__
mrunlock(mrlock_t *mrp)
{

	RWSLEEP_UNLOCK(mrp);
}

int
ismrlocked(mrlock_t *mrp, int type)

{		/* No need to lock since info can change */
	if (type == MR_ACCESS)
		return (RWSLEEP_RDOWNED(mrp)); /* Read lock */
	else if (type == MR_UPDATE)
		return (RWSLEEP_WROWNED(mrp)); /* Write lock */
	else if (type == (MR_UPDATE | MR_ACCESS))
		return (mrp->ActiveCount);	/* Any type of lock held */
	else /* Any waiters */
		return (mrp->NumberOfSharedWaiters | 
			mrp->NumberOfExclusiveWaiters);
}

/*
 * Demote from update to access. We better be the only thread with the
 * lock in update mode so it should be easy to set to 1.
 * Wake-up any readers waiting.
 */

void __inline__
mrdemote(mrlock_t *mrp)
{

	RWSLEEP_W2RLOCK(mrp);
}

int __inline__
mrislocked_access(mrlock_t *mrp)
{

	return (RWSLEEP_RDOWNED(mrp));
}

int __inline__
mrislocked_update(mrlock_t *mrp)
{

	return (RWSLEEP_WROWNED(mrp));
}

