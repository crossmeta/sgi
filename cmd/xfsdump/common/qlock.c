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

#include <libxfs.h>
#include <jdm.h>

#include "types.h"
#include "qlock.h"
#include "mlog.h"

struct qlock {
	ix_t ql_ord;
		/* ordinal position of this lock
		 */
	pid_t ql_owner;
		/* who owns this lock
		 */
#ifdef HIDDEN
	ulock_t ql_uslockh;
		/* us lock handle
		 */
#endif /* HIDDEN */
};

typedef struct  qlock qlock_t;
	/* internal qlock
	 */

#define QLOCK_SPINS			0x1000
	/* how many times to spin on lock before sleeping for it
	 */

#define QLOCK_THRDCNTMAX			256
	/* arbitrary limit on number of threads supported
	 */

static size_t qlock_thrdcnt;
	/* how many threads have checked in
	 */

typedef size_t ordmap_t;
	/* bitmap of ordinals. used to track what ordinals have
	 * been allocated.
	 */

#define ORDMAX					( 8 * sizeof( ordmap_t ))
	/* ordinals must fit into a wordsize bitmap
	 */

static ordmap_t qlock_ordalloced;
	/* to enforce allocation of only one lock to each ordinal value
	 */

struct thrddesc {
	pid_t td_pid;
	ordmap_t td_ordmap;
};
typedef struct thrddesc thrddesc_t;
#ifdef HIDDEN
static thrddesc_t qlock_thrddesc[ QLOCK_THRDCNTMAX ];
	/* holds the ordmap for each thread
	 */
#endif

#define QLOCK_ORDMAP_SET( ordmap, ord )	( ordmap |= 1U << ord )
	/* sets the ordinal bit in an ordmap
	 */

#define QLOCK_ORDMAP_CLR( ordmap, ord )	( ordmap &= ~( 1U << ord ))
	/* clears the ordinal bit in an ordmap
	 */

#define QLOCK_ORDMAP_GET( ordmap, ord )	( ordmap & ( 1U << ord ))
	/* checks if ordinal set in ordmap
	 */

#define QLOCK_ORDMAP_CHK( ordmap, ord )	( ordmap & (( 1U << ord ) - 1U ))
	/* checks if any bits less than ord are set in the ordmap
	 */

#ifdef HIDDEN
static usptr_t *qlock_usp;
#else
static void *qlock_usp;
#endif /* HIDDEN */

	/* pointer to shared arena from which locks are allocated
	 */

#ifdef HIDDEN
static char *qlock_arenaroot = "xfsrestoreqlockarena";
	/* shared arena file name root
	 */
#endif

/* REFERENCED */
static bool_t qlock_inited = BOOL_FALSE;
	/* to sanity check initialization
	 */

/* forward declarations
 */
#ifdef HIDDEN
static void qlock_ordmap_add( pid_t pid );
static ordmap_t *qlock_ordmapp_get( pid_t pid );
static ix_t qlock_thrdix_get( pid_t pid );
#endif

bool_t
qlock_init( bool_t miniroot )
{
#ifdef HIDDEN
	char arenaname[ 100 ];
	/* REFERENCED */
	intgen_t nwritten;
	intgen_t rval;
#endif

	/* sanity checks
	 */
	ASSERT( ! qlock_inited );

	/* initially no threads checked in
	 */
	qlock_thrdcnt = 0;

	/* initially no ordinals allocated
	 */
	qlock_ordalloced = 0;

	/* if miniroot, fake it
	 */
	if ( miniroot ) {
		qlock_inited = BOOL_TRUE;
		qlock_usp = 0;
		return BOOL_TRUE;
	}
#ifdef HIDDEN

	/* generate the arena name
	 */
	nwritten = sprintf( arenaname,
			    "/tmp/%s.%d",
			    qlock_arenaroot,
			    get_pid() );
	ASSERT( nwritten > 0 );
	ASSERT( ( size_t )nwritten < sizeof( arenaname ));

	/* configure shared arenas to automatically unlink on last close
	 */
	rval = usconfig( CONF_ARENATYPE, ( u_intgen_t )US_SHAREDONLY );
	if ( rval ) {
		mlog( MLOG_NORMAL | MLOG_ERROR | MLOG_NOLOCK,
		      "unable to configure shared arena for auto unlink: %s\n",
		      strerror( errno ));
		return BOOL_FALSE;
	}

	/* allocate a shared arena for the locks
	 */
	qlock_usp = usinit( arenaname );
	if ( ! qlock_usp ) {
		mlog( MLOG_NORMAL | MLOG_ERROR | MLOG_NOLOCK,
		      "unable to allocate shared arena for thread locks: %s\n",
		      strerror( errno ));
		return BOOL_FALSE;
	}

	/* now say we are initialized
	 */
	qlock_inited = BOOL_TRUE;

	/* add the parent thread to the thread list
	 */
	if ( ! qlock_thrdinit( )) {
		qlock_inited = BOOL_FALSE;
		return BOOL_FALSE;
	}
#endif /* HIDDEN */

	return BOOL_TRUE;
}

bool_t
qlock_thrdinit( void )
{
#ifdef HIDDEN
	intgen_t rval;

	/* sanity checks
	 */
	ASSERT( qlock_inited );
	ASSERT( qlock_usp );

	/* add thread to shared arena
	 */
	rval = usadd( qlock_usp );
	if ( rval ) {
		mlog( MLOG_NORMAL | MLOG_ERROR | MLOG_NOLOCK,
		      "unable to add thread to shared arena: %s\n",
		      strerror( errno ));
		return BOOL_FALSE;
	}

	/* add thread to ordmap list
	 */
	qlock_ordmap_add( get_pid() );
#endif /* HIDDEN */

	return BOOL_TRUE;
}

qlockh_t
qlock_alloc( ix_t ord )
{
	qlock_t *qlockp;

	/* sanity checks
	 */
	ASSERT( qlock_inited );

	/* verify the ordinal is not already taken, and mark as taken
	 */
	ASSERT( ! QLOCK_ORDMAP_GET( qlock_ordalloced, ord ));
	QLOCK_ORDMAP_SET( qlock_ordalloced, ord );

	/* allocate lock memory
	 */
	qlockp = ( qlock_t * )calloc( 1, sizeof( qlock_t ));
	ASSERT( qlockp );

#ifdef HIDDEN
	/* allocate a us lock: bypass if miniroot
	 */
	if ( qlock_usp ) {
		qlockp->ql_uslockh = usnewlock( qlock_usp );
		ASSERT( qlockp->ql_uslockh );
	}
#endif /* HIDDEN */

	/* assign the ordinal position
	 */
	qlockp->ql_ord = ord;

	return ( qlockh_t )qlockp;
}

void
qlock_lock( qlockh_t qlockh )
{
#ifdef HIDDEN
	qlock_t *qlockp = ( qlock_t * )qlockh;
	pid_t pid;
	ix_t thrdix;
	ordmap_t *ordmapp;
	/* REFERENCED */
	bool_t lockacquired;
#endif
	
	/* sanity checks
	 */
	ASSERT( qlock_inited );

	/* bypass if miniroot
	 */
	if ( ! qlock_usp ) {
		return;
	}

#ifdef HIDDEN

	/* get the caller's pid and thread index
	 */
	pid = get_pid();

	thrdix = qlock_thrdix_get( pid );

	/* get the ordmap for this thread
	 */
	ordmapp = qlock_ordmapp_get( pid );

	/* assert that this lock not already held
	 */
	if ( QLOCK_ORDMAP_GET( *ordmapp, qlockp->ql_ord )) {
		mlog( MLOG_NORMAL | MLOG_WARNING | MLOG_NOLOCK,
		      "lock already held: thrd %d pid %d ord %d map %x\n",
		      thrdix,
		      pid,
		      qlockp->ql_ord,
		      *ordmapp );
	}
	ASSERT( ! QLOCK_ORDMAP_GET( *ordmapp, qlockp->ql_ord ));

	/* assert that no locks with a lesser ordinal are held by this thread
	 */
	if ( QLOCK_ORDMAP_CHK( *ordmapp, qlockp->ql_ord )) {
		mlog( MLOG_NORMAL | MLOG_WARNING | MLOG_NOLOCK,
		      "lock ordinal violation: thrd %d pid %d ord %d map %x\n",
		      thrdix,
		      pid,
		      qlockp->ql_ord,
		      *ordmapp );
	}
	ASSERT( ! QLOCK_ORDMAP_CHK( *ordmapp, qlockp->ql_ord ));

	/* acquire the us lock
	 */
	lockacquired = uswsetlock( qlockp->ql_uslockh, QLOCK_SPINS );
	ASSERT( lockacquired );

	/* verify lock is not already held
	 */
	ASSERT( ! qlockp->ql_owner );

	/* add ordinal to this threads ordmap
	 */
	QLOCK_ORDMAP_SET( *ordmapp, qlockp->ql_ord );

	/* indicate the lock's owner
	 */
	qlockp->ql_owner = pid;
#endif /* HIDDEN */
}

void
qlock_unlock( qlockh_t qlockh )
{
#ifdef HIDDEN
	qlock_t *qlockp = ( qlock_t * )qlockh;
	pid_t pid;
	ordmap_t *ordmapp;
	/* REFERENCED */
	intgen_t rval;
#endif
	
	/* sanity checks
	 */
	ASSERT( qlock_inited );

	/* bypass if miniroot
	 */
	if ( ! qlock_usp ) {
		return;
	}

#ifdef HIDDEN
	/* get the caller's pid
	 */
	pid = get_pid();

	/* get the ordmap for this thread
	 */
	ordmapp = qlock_ordmapp_get( pid );

	/* verify lock is held by this thread
	 */
	ASSERT( QLOCK_ORDMAP_GET( *ordmapp, qlockp->ql_ord ));
	ASSERT( qlockp->ql_owner == pid );

	/* clear lock owner
	 */
	qlockp->ql_owner = 0;

	/* clear lock's ord from thread's ord map
	 */
	QLOCK_ORDMAP_CLR( *ordmapp, qlockp->ql_ord );
	
	/* release the us lock
	 */
	rval = usunsetlock( qlockp->ql_uslockh );
	ASSERT( ! rval );
#endif /* HIDDEN */
}

qsemh_t
qsem_alloc( ix_t cnt )
{
#ifdef HIDDEN
	usema_t *usemap;

	/* sanity checks
	 */
	ASSERT( qlock_inited );
	ASSERT( qlock_usp );

	/* allocate a us semaphore
	 */
	usemap = usnewsema( qlock_usp, ( intgen_t )cnt );
	ASSERT( usemap );

	return ( qsemh_t )usemap;
#else
	return 0;
#endif /* HIDDEN */
}

void
qsem_free( qsemh_t qsemh )
{
#ifdef HIDDEN
	usema_t *usemap = ( usema_t * )qsemh;

	/* sanity checks
	 */
	ASSERT( qlock_inited );
	ASSERT( qlock_usp );

	/* free the us semaphore
	 */
	usfreesema( usemap, qlock_usp );
#endif /* HIDDEN */
}

void
qsemP( qsemh_t qsemh )
{
#ifdef HIDDEN
	usema_t *usemap = ( usema_t * )qsemh;
	intgen_t rval;
	
	/* sanity checks
	 */
	ASSERT( qlock_inited );
	ASSERT( qlock_usp );

	/* "P" the semaphore
	 */
	rval = uspsema( usemap );
	if ( rval != 1 ) {
		mlog( MLOG_NORMAL | MLOG_ERROR | MLOG_NOLOCK,
		      "unable to \"P\" semaphore: "
		      "rval == %d, errno == %d (%s)\n",
		      rval,
		      errno,
		      strerror( errno ));
	}
	ASSERT( rval == 1 );
#endif /* HIDDEN */
}

void
qsemV( qsemh_t qsemh )
{
#ifdef HIDDEN
	usema_t *usemap = ( usema_t * )qsemh;
	intgen_t rval;
	
	/* sanity checks
	 */
	ASSERT( qlock_inited );
	ASSERT( qlock_usp );

	/* "V" the semaphore
	 */
	rval = usvsema( usemap );
	if ( rval != 0 ) {
		mlog( MLOG_NORMAL | MLOG_ERROR | MLOG_NOLOCK,
		      "unable to \"V\" semaphore: "
		      "rval == %d, errno == %d (%s)\n",
		      rval,
		      errno,
		      strerror( errno ));
	}
	ASSERT( rval == 0 );
#endif /* HIDDEN */
}

bool_t
qsemPwouldblock( qsemh_t qsemh )
{
#ifdef HIDDEN
	usema_t *usemap = ( usema_t * )qsemh;
	intgen_t rval;
	
	/* sanity checks
	 */
	ASSERT( qlock_inited );
	ASSERT( qlock_usp );

	/* check the semaphore
	 */
	rval = ustestsema( usemap );

	/* if equal to zero, no tokens left. if less than zero, other thread(s)
	 * currently waiting.
	 */
	if ( rval <= 0 ) {
		return BOOL_TRUE;
	} else {
		return BOOL_FALSE;
	}
#else
return BOOL_FALSE;
#endif /* HIDDEN */
}

size_t
qsemPavail( qsemh_t qsemh )
{
#ifdef HIDDEN
	usema_t *usemap = ( usema_t * )qsemh;
	intgen_t rval;
	
	/* sanity checks
	 */
	ASSERT( qlock_inited );
	ASSERT( qlock_usp );

	/* check the semaphore
	 */
	rval = ustestsema( usemap );

	/* if greater or equal to zero, no one is blocked and that is the number
	 * of resources available. if less than zero, absolute value is the
	 * number of blocked threads.
	 */
	if ( rval < 0 ) {
		return 0;
	} else {
		return ( size_t )rval;
	}
#else
return 0;
#endif /* HIDDEN */
}

size_t
qsemPblocked( qsemh_t qsemh )
{
#ifdef HIDDEN
	usema_t *usemap = ( usema_t * )qsemh;
	intgen_t rval;
	
	/* sanity checks
	 */
	ASSERT( qlock_inited );
	ASSERT( qlock_usp );

	/* check the semaphore
	 */
	rval = ustestsema( usemap );

	/* if greater or equal to zero, no one is blocked. if less than zero,
	 * absolute value is the number of blocked threads.
	 */
	if ( rval < 0 ) {
		return ( size_t )( 0 - rval );
	} else {
		return 0;
	}
#else
return 0;
#endif /* HIDDEN */
}

qbarrierh_t
qbarrier_alloc( void )
{
#ifdef HIDDEN
	barrier_t *barrierp;

	/* sanity checks
	 */
	ASSERT( qlock_inited );
	ASSERT( qlock_usp );

	/* allocate a us barrier
	 */
	barrierp = new_barrier( qlock_usp );
	ASSERT( barrierp );

	return ( qbarrierh_t )barrierp;
#else
return 0;
#endif /* HIDDEN */
}

void
qbarrier( qbarrierh_t qbarrierh, size_t thrdcnt )
{
#ifdef HIDDEN
	barrier_t *barrierp = ( barrier_t * )qbarrierh;

	/* sanity checks
	 */
	ASSERT( qlock_inited );
	ASSERT( qlock_usp );

	barrier( barrierp, thrdcnt );
#endif /* HIDDEN */
}

/* internal ordinal map abstraction
 */
#ifdef HIDDEN
static void
qlock_ordmap_add( pid_t pid )
{
	ASSERT( qlock_thrdcnt < QLOCK_THRDCNTMAX );
	qlock_thrddesc[ qlock_thrdcnt ].td_pid = pid;
	qlock_thrddesc[ qlock_thrdcnt ].td_ordmap = 0;
	qlock_thrdcnt++;
}

static thrddesc_t *
qlock_thrddesc_get( pid_t pid )
{
	thrddesc_t *p;
	thrddesc_t *endp;

	for ( p = &qlock_thrddesc[ 0 ],
	      endp = &qlock_thrddesc[ qlock_thrdcnt ]
	      ;
	      p < endp
	      ;
	      p++ ) {
		if ( p->td_pid == pid ) {
			return p;
		}
	}

	ASSERT( 0 );
	return 0;
}

static ordmap_t *
qlock_ordmapp_get( pid_t pid )
{
	thrddesc_t *p;
	p = qlock_thrddesc_get( pid );
	return &p->td_ordmap;
}

static ix_t
qlock_thrdix_get( pid_t pid )
{
	thrddesc_t *p;
	p = qlock_thrddesc_get( pid );
	ASSERT( p >= &qlock_thrddesc[ 0 ] );
	return ( ix_t )( p - &qlock_thrddesc[ 0 ] );
}
#endif
