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

#include <linux/time.h>
#include <linux/sched.h>
#include <asm/system.h>
#include <linux/interrupt.h>
#include <asm/current.h>
#include <linux/module.h> /* for EXPORT_SYMBOL */

#include <xfs_support/sv.h>

/* Synchronisation variables ---------------------------------------- */

void _sv_init( sv_t *sv)
{
	init_waitqueue_head( &sv->waiters );
	spin_lock_init(&sv->lock);
}

void _sv_wait( sv_t *sv, spinlock_t *lock, int spl, int intr, struct timespec *timeout)
{
	DECLARE_WAITQUEUE( wait, current );

	spin_lock(&sv->lock);	/* No need to do interrupts since	
						they better be disabled */
	
	/* Don't restore interrupts until we are done with both locks */
	spin_unlock( lock );
	add_wait_queue_exclusive( &sv->waiters, &wait );
#if 0
	if (intr) {
		set_current_state(TASK_INTERRUPTIBLE | TASK_EXCLUSIVE);
	} else {
		set_current_state(TASK_UNINTERRUPTIBLE | TASK_EXCLUSIVE);
	}
#endif
	if (intr) {
		set_current_state(TASK_INTERRUPTIBLE );
	} else {
		set_current_state(TASK_UNINTERRUPTIBLE );
	}
	spin_unlock_irqrestore( & sv->lock, (long)spl );
        
        if (timeout) {
	        schedule_timeout(timespec_to_jiffies(timeout));
        } else {
        	schedule();
        }

	set_current_state(TASK_RUNNING);
	remove_wait_queue( &sv->waiters, &wait );
}

void
_sv_broadcast(sv_t *sv)
{
	unsigned long flags;

	spin_lock_irqsave(&sv->lock, flags);

	wake_up_all(&sv->waiters );
	spin_unlock_irqrestore(&sv->lock, flags);
}

/*
 * Set runnable, at most, one thread waiting on sync variable.
 * Returns # of threads set runnable (0 or 1).
 */
void
_sv_signal(sv_t *svp)
{
	unsigned long flags;

	spin_lock_irqsave(&svp->lock, flags);
	wake_up(&svp->waiters);
	spin_unlock_irqrestore(&svp->lock, flags);
}

EXPORT_SYMBOL(_sv_init);
EXPORT_SYMBOL(_sv_wait);
EXPORT_SYMBOL(_sv_broadcast);
EXPORT_SYMBOL(_sv_signal);

