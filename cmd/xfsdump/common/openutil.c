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

#include <sys/stat.h>
#include <limits.h>
#include <errno.h>
#include <fcntl.h>

#include "types.h"
#include "mlog.h"

char *
open_pathalloc( char *dirname, char *basename, pid_t pid )
{
	size_t dirlen;
	size_t pidlen;
	char *namebuf;

	if ( strcmp( dirname, "/" )) {
		dirlen = strlen( dirname );
	} else {
		dirlen = 0;
		dirname = "";
	}

	if ( pid ) {
		pidlen = 1 + 6;
	} else {
		pidlen = 0;
	}
	namebuf = ( char * )calloc( 1,
				    dirlen
				    +
				    1
				    +
				    strlen( basename )
				    +
				    pidlen
				    +
				    1 );
	ASSERT( namebuf );

	if ( pid ) {
		( void )sprintf( namebuf, "%s/%s.%d", dirname, basename, pid );
	} else {
		( void )sprintf( namebuf, "%s/%s", dirname, basename );
	}

	return namebuf;
}

intgen_t
open_trwp( char *pathname )
{
	intgen_t fd;

	fd = open( pathname, O_CREAT | O_TRUNC | O_RDWR, S_IRUSR | S_IWUSR );
	if ( fd < 0 ) {
		mlog( MLOG_NORMAL,
		      "could not create %s: %s\n",
		      pathname,
		      strerror( errno ));
	}

	return fd;
}

intgen_t
open_erwp( char *pathname )
{
	intgen_t fd;

	fd = open( pathname, O_EXCL | O_CREAT | O_RDWR, S_IRUSR | S_IWUSR );
	if ( fd < 0 ) {
		mlog( MLOG_DEBUG,
		      "could not create %s: %s\n",
		      pathname,
		      strerror( errno ));
	}

	return fd;
}

intgen_t
open_rwp( char *pathname )
{
	intgen_t fd;

	fd = open( pathname, O_RDWR );

	return fd;
}

intgen_t
mkdir_tp( char *pathname )
{
	intgen_t rval;

	rval = mkdir( pathname, S_IRWXU );

	return rval;
}

intgen_t
open_trwdb( char *dirname, char *basename, pid_t pid )
{
	char *pathname;
	intgen_t fd;

	pathname = open_pathalloc( dirname, basename, pid );
	fd = open_trwp( pathname );
	free( ( void * )pathname );

	return fd;
}

intgen_t
open_erwdb( char *dirname, char *basename, pid_t pid )
{
	char *pathname;
	intgen_t fd;

	pathname = open_pathalloc( dirname, basename, pid );
	fd = open_erwp( pathname );
	free( ( void * )pathname );

	return fd;
}

intgen_t
open_rwdb( char *dirname, char *basename, pid_t pid )
{
	char *pathname;
	intgen_t fd;

	pathname = open_pathalloc( dirname, basename, pid );
	fd = open_rwp( pathname );
	free( ( void * )pathname );

	return fd;
}
