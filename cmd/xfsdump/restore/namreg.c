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

#include <sys/mman.h>
#include <sys/stat.h>
#include <sys/ioctl.h>
#include <fcntl.h>
#include <errno.h>

#include "types.h"
#include "lock.h"
#include "mlog.h"
#include "namreg.h"
#include "openutil.h"
#include "mmap.h"

/* structure definitions used locally ****************************************/

#define max( a, b )	( ( ( a ) > ( b ) ) ? ( a ) : ( b ) )

#define NAMREG_AVGLEN	10

/* persistent context for a namreg - placed in first page
 * of the namreg file by namreg_init if not a sync
 */
struct namreg_pers {
	off64_t np_appendoff;
};

typedef struct namreg_pers namreg_pers_t;

#define NAMREG_PERS_SZ	pgsz

/* transient context for a namreg - allocated by namreg_init()
 */
struct namreg_tran {
	char *nt_pathname;
	int nt_fd;
	bool_t nt_at_endpr;
};

typedef struct namreg_tran namreg_tran_t;


#ifdef NAMREGCHK

/* macros for manipulating namreg handles when handle consistency
 * checking is enabled.
 */
#define CHKBITCNT		2
#define	CHKBITSHIFT		( NBBY * sizeof( nrh_t ) - CHKBITCNT )
#define	CHKBITLOMASK		( ( 1 << CHKBITCNT ) - 1 )
#define	CHKBITMASK		( CHKBITLOMASK << CHKBITSHIFT )
#define CHKHDLCNT		CHKBITSHIFT
#define CHKHDLMASK		( ( 1 << CHKHDLCNT ) - 1 )
#define CHKGETBIT( h )		( ( h >> CHKBITSHIFT ) & CHKBITLOMASK )
#define CHKGETHDL( h )		( h & CHKHDLMASK )
#define CHKMKHDL( c, h )	( ( ( c << CHKBITSHIFT ) & CHKBITMASK )	\
				  |					\
				  ( h & CHKHDLMASK ))
#define HDLMAX			( ( off64_t )CHKHDLMASK )

#else /* NAMREGCHK */

#define HDLMAX			( ( ( off64_t )1			\
				    <<					\
				    ( ( off64_t )NBBY			\
				      *					\
				      ( off64_t )sizeof( nrh_t )))	\
				  -					\
				  ( off64_t )2 ) /* 2 to avoid NRH_NULL */

#endif /* NAMREGCHK */


/* declarations of externally defined global symbols *************************/

extern size_t pgsz;

/* forward declarations of locally defined static functions ******************/


/* definition of locally defined global variables ****************************/


/* definition of locally defined static variables *****************************/

static char *namregfile = "namreg";
static namreg_tran_t *ntp = 0;
static namreg_pers_t *npp = 0;


/* definition of locally defined global functions ****************************/

bool_t
namreg_init( char *hkdir, bool_t resume, u_int64_t inocnt )
{
#ifdef SESSCPLT
	if ( ntp ) {
		return BOOL_TRUE;
	}
#endif /* SESSCPLT */

	/* sanity checks
	 */
	ASSERT( ! ntp );
	ASSERT( ! npp );

	ASSERT( sizeof( namreg_pers_t ) <= NAMREG_PERS_SZ );

	/* allocate and initialize context
	 */
	ntp = ( namreg_tran_t * )calloc( 1, sizeof( namreg_tran_t ));
	ASSERT( ntp );

	/* generate a string containing the pathname of the namreg file
	 */
	ntp->nt_pathname = open_pathalloc( hkdir, namregfile, 0 );

	/* open the namreg file
	 */
	if ( resume ) {
		/* open existing file
		 */
		ntp->nt_fd = open( ntp->nt_pathname, O_RDWR );
		if ( ntp->nt_fd < 0 ) {
			mlog( MLOG_NORMAL | MLOG_ERROR,
			      "could not find name registry file %s: "
			      "%s\n",
			      ntp->nt_pathname,
			      strerror( errno ));
			return BOOL_FALSE;
		}
	} else {
		/* create the namreg file, first unlinking any older version
		 * laying around
		 */
		( void )unlink( ntp->nt_pathname );
		ntp->nt_fd = open( ntp->nt_pathname,
				   O_RDWR | O_CREAT | O_EXCL,
				   S_IRUSR | S_IWUSR );
		if ( ntp->nt_fd < 0 ) {
			mlog( MLOG_NORMAL | MLOG_ERROR,
			      "could not create name registry file %s: "
			      "%s\n",
			      ntp->nt_pathname,
			      strerror( errno ));
			return BOOL_FALSE;
		}

		/* reserve space for the backing store. try to use F_RESVSP64.
		 * if doesn't work, try F_RESVSP64. the former is faster, since
		 * it does not zero the space.
		 */
		{
		bool_t successpr;
		intgen_t ioctlcmd;
		intgen_t loglevel;
		size_t trycnt;

#ifndef F_RESVSP64
#define F_RESVSP64 0
#endif /* F_RESVSP64 */

		for ( trycnt = 0,
		      successpr = BOOL_FALSE,
		      ioctlcmd = XFS_IOC_RESVSP64,
		      loglevel = MLOG_VERBOSE
		      ;
		      ! successpr && trycnt < 2
		      ;
		      trycnt++,
		      ioctlcmd = XFS_IOC_ALLOCSP64,
		      loglevel = max( MLOG_NORMAL, loglevel - 1 )) {
			off64_t initsz;
			struct flock64 flock64;
			intgen_t rval;

			if ( ! ioctlcmd ) {
				continue;
			}

			initsz = ( off64_t )NAMREG_PERS_SZ
				 +
				 ( ( off64_t )inocnt * NAMREG_AVGLEN );
			flock64.l_whence = 0;
			flock64.l_start = 0;
			flock64.l_len = initsz;
			rval = ioctl( ntp->nt_fd, ioctlcmd, &flock64 );
			if ( rval ) {
				mlog( loglevel | MLOG_NOTE,
				      "attempt to reserve %lld bytes for %s "
				      "using %s "
				      "failed: %s (%d)\n",
				      initsz,
				      ntp->nt_pathname,
				      ioctlcmd == F_RESVSP64
				      ?
				      "F_RESVSP64"
				      :
				      "F_ALLOCSP64",
				      strerror( errno ),
				      errno );
			} else {
				successpr = BOOL_TRUE;
			}
		}
		}
	}

	/* mmap the persistent descriptor
	 */
	ASSERT( ! ( NAMREG_PERS_SZ % pgsz ));
	npp = ( namreg_pers_t * ) mmap_autogrow(
				        NAMREG_PERS_SZ,
				        ntp->nt_fd,
				        ( off_t )0 );
	if ( npp == ( namreg_pers_t * )-1 ) {
		mlog( MLOG_NORMAL | MLOG_ERROR,
		      "unable to map %s: %s\n",
		      ntp->nt_pathname,
		      strerror( errno ));
		return BOOL_FALSE;
	}

	/* initialize persistent state
	 */
	if ( ! resume ) {
		npp->np_appendoff = ( off64_t )NAMREG_PERS_SZ;
	}

	/* initialize transient state
	 */
	ntp->nt_at_endpr = BOOL_FALSE;

	return BOOL_TRUE;
}

nrh_t
namreg_add( char *name, size_t namelen )
{
	off64_t oldoff;
	intgen_t nwritten;
	unsigned char c;
	nrh_t nrh;
	
	/* sanity checks
	 */
	ASSERT( ntp );
	ASSERT( npp );

	/* make sure file pointer is positioned to append
	 */
	if ( ! ntp->nt_at_endpr ) {
		off64_t newoff;
		newoff = lseek64( ntp->nt_fd, npp->np_appendoff, SEEK_SET );
		if ( newoff == ( off64_t )-1 ) {
			mlog( MLOG_NORMAL,
			      "lseek of namreg failed: %s\n",
			      strerror( errno ));
			ASSERT( 0 );
			return NRH_NULL;
		}
		ASSERT( npp->np_appendoff == newoff );
		ntp->nt_at_endpr = BOOL_TRUE;
	}

	/* save the current offset
	 */
	oldoff = npp->np_appendoff;

	/* write a one byte unsigned string length
	 */
	ASSERT( namelen < 256 );
	c = ( unsigned char )( namelen & 0xff );
	nwritten = write( ntp->nt_fd, ( void * )&c, 1 );
	if ( nwritten != 1 ) {
		mlog( MLOG_NORMAL,
		      "write of namreg failed: %s\n",
		      strerror( errno ));
		ASSERT( 0 );
		return NRH_NULL;
	}

	/* write the name string
	 */
	nwritten = write( ntp->nt_fd, ( void * )name, namelen );
	if ( ( size_t )nwritten != namelen ) {
		mlog( MLOG_NORMAL,
		      "write of namreg failed: %s\n",
		      strerror( errno ));
		ASSERT( 0 );
		return NRH_NULL;
	}

	npp->np_appendoff += ( off64_t )( 1 + namelen );
	ASSERT( oldoff <= HDLMAX );

#ifdef NAMREGCHK

	/* encode the lsb of the len plus the first character into the handle.
	 */
	nrh = CHKMKHDL( ( nrh_t )namelen + ( nrh_t )*name, ( nrh_t )oldoff );

#else /* NAMREGCHK */

	nrh = ( nrh_t )oldoff;

#endif /* NAMREGCHK */

	return nrh;
}

/* ARGSUSED */
void
namreg_del( nrh_t nrh )
{
	/* currently not implemented - grows, never shrinks
	 */
}

intgen_t
namreg_get( nrh_t nrh,
	    char *bufp,
	    size_t bufsz )
{
	off64_t newoff;
	intgen_t nread;
	size_t len;
	unsigned char c;
#ifdef NAMREGCHK
	nrh_t chkbit;
#endif /* NAMREGCHK */

	/* sanity checks
	 */
	ASSERT( ntp );
	ASSERT( npp );

	/* make sure we aren't being given a NULL handle
	 */
	ASSERT( nrh != NRH_NULL );

	/* convert the handle into the offset
	 */
#ifdef NAMREGCHK

	newoff = ( off64_t )( size64_t )CHKGETHDL( nrh );
	chkbit = CHKGETBIT( nrh );

#else /* NAMREGCHK */

	newoff = ( off64_t )( size64_t )nrh;

#endif /* NAMREGCHK */

	/* do sanity check on offset
	 */
	ASSERT( newoff <= HDLMAX );
	ASSERT( newoff < npp->np_appendoff );
	ASSERT( newoff >= ( off64_t )NAMREG_PERS_SZ );

	lock( );

	/* seek to the name
	 */
	newoff = lseek64( ntp->nt_fd, newoff, SEEK_SET );
	if ( newoff == ( off64_t )-1 ) {
		unlock( );
		mlog( MLOG_NORMAL,
		      "lseek of namreg failed: %s\n",
		      strerror( errno ));
		return -3;
	}

	/* read the name length
	 */
	c = 0; /* unnecessary, but keeps lint happy */
	nread = read( ntp->nt_fd, ( void * )&c, 1 );
	if ( nread != 1 ) {
		unlock( );
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
		unlock( );
		return -1;
	}

	/* read the name
	 */
	nread = read( ntp->nt_fd, ( void * )bufp, len );
	if ( ( size_t )nread != len ) {
		unlock( );
		mlog( MLOG_NORMAL,
		      "read of namreg failed: %s\n",
		      strerror( errno ));
		return -3;
	}

#ifdef NAMREGCHK

	/* validate the checkbit
	 */
	ASSERT( chkbit
		==
		( ( ( nrh_t )len + ( nrh_t )bufp[ 0 ] ) & CHKBITLOMASK ));

#endif /* NAMREGCHK */

	/* null-terminate the string if room
	 */
	bufp[ len ] = 0;

	ntp->nt_at_endpr = BOOL_FALSE;
	
	unlock( );

	return ( intgen_t )len;
}


/* definition of locally defined static functions ****************************/
