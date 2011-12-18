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
#ifndef QLOCK_H
#define QLOCK_H

/* qlock - quick locks abstraction
 *
 * threads may allocate quick locks using qlock_alloc, and free them with
 * qlock_free. the abstraction is initialized with qlock_init. the underlying
 * mechanism is the IRIX us lock primitive. in order to use this, a temporary
 * shared arena is created in /tmp. this will be automatically unlinked
 * when the last thread exits.
 *
 * deadlock detection is accomplished by giving an ordinal number to each
 * lock allocated, and record all locks held by each thread. locks may not
 * be acquired out of order. that is, subsequently acquired locks must have
 * a lower ordinal than all locks currently held. for convenience, the ordinals
 * of all locks to be allocated will be defined in this file.
 *
 * ADDITION: added counting semaphores. simpler to do here since same
 * shared arena can be used.
 */

#define QLOCK_ORD_CRIT	0
	/* ordinal for global critical region lock
	 */
#define QLOCK_ORD_WIN	1
	/* ordinal for win abstraction critical regions
	 */
#define QLOCK_ORD_PI	2
	/* ordinal for persistent inventory abstraction critical regions
	 */
#define QLOCK_ORD_MLOG	3
	/* ordinal for mlog lock
	 */

typedef void *qlockh_t;
#define QLOCKH_NULL	0
	/* opaque handle
	 */

extern bool_t qlock_init( bool_t miniroot );
	/* called by main to initialize abstraction. returns FALSE if
	 * utility should abort.
	 */

extern bool_t qlock_thrdinit( void );
	/* called by each thread to prepare it for participation
	 */

extern qlockh_t qlock_alloc( ix_t ord );
	/* allocates a qlock with the specified ordinal. returns
	 * NULL if lock can't be allocated.
	 */
extern void qlock_lock( qlockh_t qlockh );
	/* acquires the specified lock.
	 */
extern void qlock_unlock( qlockh_t qlockh );
	/* releases the specified lock.
	 */

typedef void *qsemh_t;
#define QSEMH_NULL	0
	/* opaque handle
	 */

extern qsemh_t qsem_alloc( size_t cnt );
	/* allocates a counting semaphore initialized to the specified
	 * count. returns a qsem handle
	 */
extern void qsem_free( qsemh_t qsemh );
	/* frees the counting semaphore
	 */
extern void qsemP( qsemh_t qsemh );
	/* "P" (decrement) op
	 */
extern void qsemV( qsemh_t qsemh );
	/* "V" (increment) op
	 */
extern bool_t qsemPwouldblock( qsemh_t qsemh );
	/* returns true if a qsemP op would block
	 */
extern size_t qsemPavail( qsemh_t qsemh );
	/* number of resources available
	 */
extern size_t qsemPblocked( qsemh_t qsemh );
	/* number of threads currently blocked on this semaphore
	 */

typedef void *qbarrierh_t;
#define QBARRIERH_NULL	0
	/* opaque handle
	 */
extern qbarrierh_t qbarrier_alloc( void );
	/* allocates a rendezvous barrier
	 */
extern void qbarrier( qbarrierh_t barrierh, size_t thrdcnt );
	/* causes thrdcnt threads to rendezvous
	 */

#endif /* QLOCK_H */
