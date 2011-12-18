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
#include <handle.h>
#include <jdm.h>

/* internal fshandle - typecast to a void for external use */
#define FSHANDLE_SZ		8
typedef struct fshandle {
	char fsh_space[FSHANDLE_SZ];
} fshandle_t;

/* private file handle - for use by open_by_handle */
#define FILEHANDLE_SZ		24
#define FILEHANDLE_SZ_FOLLOWING	14
#define FILEHANDLE_SZ_PAD	2
typedef struct filehandle {
	fshandle_t fh_fshandle;		/* handle of fs containing this inode */
	int16_t fh_sz_following;	/* bytes in handle after this member */
	char fh_pad[FILEHANDLE_SZ_PAD];	/* padding, must be zeroed */
	__uint32_t fh_gen;		/* generation count */
	xfs_ino_t fh_ino;		/* 64 bit ino */
} filehandle_t;


static void
jdm_fill_filehandle( filehandle_t *handlep,
		     fshandle_t *fshandlep,
		     xfs_bstat_t *statp )
{
	handlep->fh_fshandle = *fshandlep;
	handlep->fh_sz_following = FILEHANDLE_SZ_FOLLOWING;
	bzero(handlep->fh_pad, FILEHANDLE_SZ_PAD);
	handlep->fh_gen = statp->bs_gen;
	handlep->fh_ino = statp->bs_ino;
}

jdm_fshandle_t *
jdm_getfshandle( char *mntpnt )
{
	fshandle_t *fshandlep;
	size_t fshandlesz;
        char resolved[MAXPATHLEN];

	/* sanity checks */
	ASSERT( sizeof( fshandle_t ) == FSHANDLE_SZ );
	ASSERT( sizeof( filehandle_t ) == FILEHANDLE_SZ );
	ASSERT( sizeof( filehandle_t )
		-
		offsetofmember( filehandle_t, fh_pad )
		==
		FILEHANDLE_SZ_FOLLOWING );
	ASSERT( sizeofmember( filehandle_t, fh_pad ) == FILEHANDLE_SZ_PAD );
	ASSERT( FILEHANDLE_SZ_PAD == sizeof( int16_t ));

	fshandlep = 0; /* for lint */
	fshandlesz = sizeof( *fshandlep );
        
        if (!realpath( mntpnt, resolved ))
                return NULL;
        
	if (path_to_fshandle( resolved, ( void ** )&fshandlep, &fshandlesz ))
		return NULL;
        
	assert( fshandlesz == sizeof( *fshandlep ));
        
	return ( jdm_fshandle_t * )fshandlep;
}


/* externally visible functions */

void
jdm_new_filehandle( jdm_filehandle_t **handlep,
		    size_t *hlen,
		    jdm_fshandle_t *fshandlep,
		    xfs_bstat_t *statp)
{
	/* allocate and fill filehandle */
	*hlen = sizeof(filehandle_t);
	*handlep = (filehandle_t *) malloc(*hlen);

	if (*handlep)
		jdm_fill_filehandle(*handlep, (fshandle_t *) fshandlep, statp);
}

/* ARGSUSED */
void
jdm_delete_filehandle( jdm_filehandle_t *handlep, size_t hlen )
{
	free(handlep);
}

intgen_t
jdm_open( jdm_fshandle_t *fshp, xfs_bstat_t *statp, intgen_t oflags )
{
	register fshandle_t *fshandlep = ( fshandle_t * )fshp;
	filehandle_t filehandle;
	intgen_t fd;

	jdm_fill_filehandle( &filehandle, fshandlep, statp );
	fd = open_by_handle( ( void * )&filehandle,
			     sizeof( filehandle ),
			     oflags );
	return fd;
}

intgen_t
jdm_readlink( jdm_fshandle_t *fshp,
	      xfs_bstat_t *statp,
	      char *bufp, size_t bufsz )
{
	register fshandle_t *fshandlep = ( fshandle_t * )fshp;
	filehandle_t filehandle;
	intgen_t rval;

	jdm_fill_filehandle( &filehandle, fshandlep, statp );
	rval = readlink_by_handle( ( void * )&filehandle,
				   sizeof( filehandle ),
				   ( void * )bufp,
				   bufsz );
	return rval;
}
