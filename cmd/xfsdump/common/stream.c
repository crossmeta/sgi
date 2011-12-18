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
#include "stream.h"
#include "lock.h"

#define PROCMAX	( STREAM_SIMMAX * 2 + 1 )

struct spm {
	bool_t s_busy;
	pid_t s_pid;
	intgen_t s_ix;
};

typedef struct spm spm_t;

static spm_t spm[ STREAM_SIMMAX * 3 ];

void
stream_init( void )
{
#ifdef HIDDEN
	/* REFERENCED */
	intgen_t rval;

	rval = ( intgen_t )usconfig( CONF_INITUSERS, PROCMAX );
	ASSERT( rval >= 0 );
#endif /* HIDDEN */

	( void )memset( ( void * )spm, 0, sizeof( spm ));

}

void
stream_register( pid_t pid, intgen_t streamix )
{
	spm_t *p = spm;
	spm_t *ep = spm + sizeof( spm ) / sizeof( spm[ 0 ] );

	ASSERT( streamix < STREAM_SIMMAX );

	lock( );
	for ( ; p < ep ; p++ ) {
		if ( ! p->s_busy ) {
			p->s_busy = BOOL_TRUE;
			break;
		}
	}
	unlock();

	ASSERT( p < ep );
	if ( p >= ep ) return;

	p->s_pid = pid;
	p->s_ix = streamix;
}

void
stream_unregister( pid_t pid )
{
	spm_t *p = spm;
	spm_t *ep = spm + sizeof( spm ) / sizeof( spm[ 0 ] );

	lock( );
	for ( ; p < ep ; p++ ) {
		if ( p->s_pid == pid ) {
			p->s_pid = 0;
			p->s_ix = -1;
			p->s_busy = BOOL_FALSE;
			break;
		}
	}
	unlock();

	ASSERT( p < ep );
}

intgen_t
stream_getix( pid_t pid )
{
	spm_t *p = spm;
	spm_t *ep = spm + sizeof( spm ) / sizeof( spm[ 0 ] );

	for ( ; p < ep ; p++ ) {
		if ( p->s_pid == pid ) {
			break;
		}
	}

	return ( p >= ep ) ? -1 : p->s_ix;
}

size_t
stream_cnt( void )
{
	spm_t *p = spm;
	spm_t *ep = spm + sizeof( spm ) / sizeof( spm[ 0 ] );
	size_t ixmap = 0;
	size_t ixcnt;
	size_t bitix;

	ASSERT( sizeof( ixmap ) * NBBY >= STREAM_SIMMAX );
	
	lock( );
	for ( ; p < ep ; p++ ) {
		if ( p->s_busy ) {
			ixmap |= ( size_t )1 << p->s_ix;
		}
	}
	unlock();

	ixcnt = 0;
	for ( bitix = 0 ; bitix < STREAM_SIMMAX ; bitix++ ) {
		if ( ixmap & ( ( size_t )1 << bitix )) {
			ixcnt++;
		}
	}

	return ixcnt;
}
