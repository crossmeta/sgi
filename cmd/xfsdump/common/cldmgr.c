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

#include <sys/wait.h>
#include <sys/ipc.h>
#include <sys/sem.h>
#include <sys/prctl.h>
#include <signal.h>
#include <errno.h>

#include "stkchk.h"
#include "types.h"
#include "lock.h"
#include "qlock.h"
#include "stream.h"
#include "mlog.h"
#include "cldmgr.h"
#include "sproc.h"

extern size_t pgsz;

#define CLD_MAX	( STREAM_SIMMAX * 2 )
struct cld {
	bool_t c_busy;
	pid_t c_pid;
	ix_t c_streamix;
	int ( * c_entry )( void *arg1 );
	void * c_arg1;
};

typedef struct cld cld_t;

static cld_t cld[ CLD_MAX ];
static bool_t cldmgr_stopflag;

static cld_t *cldmgr_getcld( void );
static cld_t * cldmgr_findbypid( pid_t );
static int cldmgr_entry( void * );
/* REFERENCED */
static pid_t cldmgr_parentpid;

bool_t
cldmgr_init( void )
{
	/* REFERENCED */
	intgen_t rval;

	( void )memset( ( void * )cld, 0, sizeof( cld ));
	cldmgr_stopflag = BOOL_FALSE;
	cldmgr_parentpid = getpid( );

	rval = atexit( cldmgr_killall );
	ASSERT( ! rval );

	return BOOL_TRUE;
}

bool_t
cldmgr_create( int ( * entry )( void *arg1 ),
	       u_intgen_t inh,
	       ix_t streamix,
	       char *descstr,
	       void *arg1 )
{
	cld_t *cldp;
	pid_t cldpid;

	ASSERT( getpid( ) == cldmgr_parentpid );

	cldp = cldmgr_getcld( );
	if ( ! cldp ) {
		mlog( MLOG_NORMAL | MLOG_ERROR | MLOG_PROC,
		      "cannot create %s thread for stream %u: "
		      "too many child threads (max allowed is %d)\n",
		      descstr,
		      streamix,
		      CLD_MAX );
		return BOOL_FALSE;
	}
	
	cldp->c_streamix = streamix;
	cldp->c_entry = entry;
	cldp->c_arg1 = arg1;
	cldpid = ( pid_t )sproc( cldmgr_entry, inh, ( void * )cldp );
	if ( cldpid < 0 ) {
		mlog( MLOG_NORMAL | MLOG_ERROR | MLOG_PROC,
		      "sproc failed creating %s thread for stream %u: %s\n",
		      descstr,
		      streamix,
		      strerror( errno ));
	} else {
		mlog( MLOG_NITTY | MLOG_PROC,
		      "%s thread created for stream %u: pid %d\n",
		      descstr,
		      streamix,
		      cldpid );
	}

	return cldpid < 0 ? BOOL_FALSE : BOOL_TRUE;
}

void
cldmgr_stop( void )
{
	/* must NOT mlog here!
	 * locked up by main loop dialog
	 */
	cldmgr_stopflag = BOOL_TRUE;
}

/* cldmgr_killall()
 *
 */
void
cldmgr_killall( void )
{
	cld_t *p = cld;
	cld_t *ep = cld + sizeof( cld ) / sizeof( cld[ 0 ] );

	signal( SIGCLD, SIG_IGN );
	for ( ; p < ep ; p++ ) {
		if ( p->c_busy ) {
			mlog( MLOG_NITTY | MLOG_PROC,
			      "sending SIGKILL to pid %d\n",
			      p->c_pid );
			kill( p->c_pid, SIGKILL );
			cldmgr_died( p->c_pid );
		}
	}
}

void
cldmgr_died( pid_t pid )
{
	cld_t *cldp = cldmgr_findbypid( pid );

	if ( ! cldp ) {
		return;
	}
	cldp->c_busy = BOOL_FALSE;
	if ( ( intgen_t )( cldp->c_streamix ) >= 0 ) {
		stream_unregister( pid );
	}
}

bool_t
cldmgr_stop_requested( void )
{
	return cldmgr_stopflag;
}

size_t
cldmgr_remainingcnt( void )
{
	cld_t *p = cld;
	cld_t *ep = cld + sizeof( cld ) / sizeof( cld[ 0 ] );
	size_t cnt;

	cnt = 0;
	lock( );
	for ( ; p < ep ; p++ ) {
		if ( p->c_busy ) {
			cnt++;
		}
	}
	unlock( );

	return cnt;
}

bool_t
cldmgr_otherstreamsremain( ix_t streamix )
{
	cld_t *p = cld;
	cld_t *ep = cld + sizeof( cld ) / sizeof( cld[ 0 ] );

	lock( );
	for ( ; p < ep ; p++ ) {
		if ( p->c_busy && p->c_streamix != streamix ) {
			unlock( );
			return BOOL_TRUE;
		}
	}
	unlock( );

	return BOOL_FALSE;
}

static cld_t *
cldmgr_getcld( void )
{
	cld_t *p = cld;
	cld_t *ep = cld + sizeof( cld ) / sizeof( cld[ 0 ] );

	lock();
	for ( ; p < ep ; p++ ) {
		if ( ! p->c_busy ) {
			p->c_busy = BOOL_TRUE;
			break;
		}
	}
	unlock();

	return ( p < ep ) ? p : 0;
}

static cld_t *
cldmgr_findbypid( pid_t pid )
{
	cld_t *p = cld;
	cld_t *ep = cld + sizeof( cld ) / sizeof( cld[ 0 ] );

	for ( ; p < ep ; p++ ) {
		if ( p->c_busy && p->c_pid == pid ) {
			break;
		}
	}

	return ( p < ep ) ? p : 0;
}

static int
cldmgr_entry( void *arg1 )
{
	cld_t *cldp = ( cld_t * )arg1;
	pid_t pid = getpid( );
	/* REFERENCED */
	bool_t ok;

	signal( SIGHUP, SIG_IGN );
	signal( SIGINT, SIG_IGN );
	signal( SIGQUIT, SIG_IGN );
	signal( SIGCLD, SIG_DFL );
	alarm( 0 );
	cldp->c_pid = pid;
	ok = qlock_thrdinit( );
	ASSERT( ok );
	if ( ( intgen_t )( cldp->c_streamix ) >= 0 ) {
		stream_register( pid, ( intgen_t )cldp->c_streamix );
	}
	mlog( MLOG_DEBUG | MLOG_PROC,
	      "child %d created for stream %d\n",
	      pid,
	      cldp->c_streamix );
	stkchk_register( );
	return ( * cldp->c_entry )( cldp->c_arg1 );
}
