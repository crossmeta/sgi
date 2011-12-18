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

#include <sys/types.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <stdio.h>
#include <stdlib.h>
#include <unistd.h>
#include <errno.h>
#include <string.h>

#include "types.h"
#include "mlog.h"
#include "namreg.h"
#include "openutil.h"
#include "cleanup.h"

/* structure definitions used locally ****************************************/

/* template for the name of the tmp file containing the names
 */
#define NAMETEMPLATE	"namreg"

/* context for a namreg - allocated by and pointer to returned by namreg_init()
 */
struct namreg_context {
	int nc_fd;		/* file descriptor of tmp file */
	bool_t nc_not_at_end;
	off64_t nc_nextoff;
	char *nc_pathname;
};

typedef struct namreg_context namreg_context_t;


/* declarations of externally defined global symbols *************************/


/* forward declarations of locally defined static functions ******************/

static void namreg_abort_cleanup( void *, void * );


/* definition of locally defined global variables ****************************/


/* definition of locally defined static variables *****************************/


/* definition of locally defined global functions ****************************/

namreg_t *
namreg_init( bool_t cumulative,
	     bool_t delta,
	     char *housekeepingdir )
{
	namreg_context_t *ncp;

	/* allocate and initialize context
	 */
	ncp = ( namreg_context_t * )calloc( 1, sizeof( namreg_context_t ));
	ASSERT( ncp );

	/* generate a string containing the pathname of the namreg file
	 */
	ncp->nc_pathname = open_pathalloc( housekeepingdir, NAMETEMPLATE, 0 );

	/* if not a cumulative restore, be sure the name registry gets removed
	 * on exit.
	 */
	if ( ! cumulative ) {
		( void )cleanup_register( namreg_abort_cleanup,
					  ( void * )ncp,
					  0 );
	}

	/* if this is a delta during a cumulative restore, the namreg file must
	 * already exist. if not, create/truncate.
	 */
	if ( cumulative && delta ) {
		ncp->nc_fd = open_rwp( ncp->nc_pathname );
		if ( ncp->nc_fd < 0 ) {
			mlog( MLOG_NORMAL,
			      "could not open %s: %s\n",
			      ncp->nc_pathname,
			      strerror( errno ));
			return 0;
		}
	} else {
		ncp->nc_fd = open_trwp( ncp->nc_pathname );
		if ( ncp->nc_fd < 0 ) {
			return 0;
		}
	}

	return ( namreg_t * )ncp;
}

namreg_ix_t
namreg_add( namreg_t *namregp, char *name, size_t namelen )
{
	register namreg_context_t *ncp = ( namreg_context_t *)namregp;
	off64_t oldoff;
	intgen_t nwritten;
	unsigned char c;
	
	/* make sure file pointer is positioned to write at end of file
	 */
	if ( ncp->nc_not_at_end ) {
		off64_t new_off;
		new_off = lseek64( ncp->nc_fd, ( off64_t )0, SEEK_END );
		if ( new_off == ( off64_t )-1 ) {
			mlog( MLOG_NORMAL,
			      "lseek of namreg failed: %s\n",
			      strerror( errno ));
			ASSERT( 0 );
			return NAMREG_IX_NULL;
		}
		ncp->nc_nextoff = new_off;
		ncp->nc_not_at_end = BOOL_FALSE;
	}

	/* save the current offset
	 */
	oldoff = ncp->nc_nextoff;

	/* write a one byte unsigned string length
	 */
	ASSERT( namelen < 256 );
	c = ( unsigned char )( namelen & 0xff );
	nwritten = write( ncp->nc_fd, ( void * )&c, 1 );
	if ( nwritten != 1 ) {
		mlog( MLOG_NORMAL,
		      "write of namreg failed: %s\n",
		      strerror( errno ));
		ASSERT( 0 );
		return NAMREG_IX_NULL;
	}

	/* write the name string
	 */
	nwritten = write( ncp->nc_fd, ( void * )name, namelen );
	if ( ( size_t )nwritten != namelen ) {
		mlog( MLOG_NORMAL,
		      "write of namreg failed: %s\n",
		      strerror( errno ));
		ASSERT( 0 );
		return NAMREG_IX_NULL;
	}

	ncp->nc_nextoff += ( off64_t )( 1 + namelen );
	return ( namreg_ix_t )oldoff;
}

/* ARGSUSED */
void
namreg_del( namreg_t *namregp, namreg_ix_t namreg_ix )
{
	/* currently not implemented - grows, never shrinks
	 */
}

intgen_t
namreg_get( namreg_t *namregp,
	    namreg_ix_t namreg_ix,
	    char *bufp,
	    size_t bufsz )
{
	register namreg_context_t *ncp = ( namreg_context_t *)namregp;
	off64_t new_off;
	intgen_t nread;
	size_t len;
	unsigned char c;

	/* convert the ix into the offset
	 */
	new_off = ( off64_t )namreg_ix;

	/* seek to the name
	 */
	ASSERT( namreg_ix != NAMREG_IX_NULL );
	new_off = lseek64( ncp->nc_fd, new_off, SEEK_SET );
	if ( new_off == ( off64_t )-1 ) {
		mlog( MLOG_NORMAL,
		      "lseek of namreg failed: %s\n",
		      strerror( errno ));
		return -3;
	}

	/* read the name length
	 */
	c = 0; /* unnecessary, but keeps lint happy */
	nread = read( ncp->nc_fd, ( void * )&c, 1 );
	if ( nread != 1 ) {
		mlog( MLOG_NORMAL,
		      "read of namreg failed: %s (nread = %d)\n",
		      strerror( errno ),
		      nread );
		return -3;
	}
	
	/* deal with a short caller-supplied buffer
	 */
	len = ( size_t )c;
	if ( bufsz < len + 1 ) {
		return -1;
	}

	/* read the name
	 */
	nread = read( ncp->nc_fd, ( void * )bufp, len );
	if ( ( size_t )nread != len ) {
		mlog( MLOG_NORMAL,
		      "read of namreg failed: %s\n",
		      strerror( errno ));
		return -3;
	}

	/* null-terminate the string if room
	 */
	bufp[ len ] = 0;

	ncp->nc_not_at_end = BOOL_TRUE;
	
	return ( intgen_t )len;
}


/* definition of locally defined static functions ****************************/

static void
namreg_abort_cleanup( void *arg1, void *arg2 )
{
	namreg_context_t *ncp = ( namreg_context_t * )arg1;
	( void )unlink( ncp->nc_pathname );
}
