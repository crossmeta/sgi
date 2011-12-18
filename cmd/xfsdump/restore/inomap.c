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
#include <errno.h>
#include <time.h>
#include <fcntl.h>
#include <dirent.h>
#include <time.h>

#include "types.h"
#include "util.h"
#include "openutil.h"
#include "mlog.h"
#include "global.h"
#include "drive.h"
#include "media.h"
#include "content.h"
#include "content_inode.h"
#include "inomap.h"
#include "mmap.h"
#include "arch_xlate.h"

/* structure definitions used locally ****************************************/

/* restores the inomap into a file
 */
#define PERS_NAME	"inomap"

/* reserve the first page for persistent state
 */
struct pers {
	size64_t hnkcnt;
		/* number of hunks in map
		 */
	size64_t segcnt;
		/* number of segments
		 */
	xfs_ino_t last_ino_added;
};

typedef struct pers pers_t;

#define PERSSZ	perssz



/* declarations of externally defined global symbols *************************/

extern size_t pgsz;


/* forward declarations of locally defined static functions ******************/

/* inomap primitives
 */
static intgen_t map_getset( xfs_ino_t, intgen_t, bool_t );
static intgen_t map_set( xfs_ino_t ino, intgen_t );


/* definition of locally defined global variables ****************************/


/* definition of locally defined static variables *****************************/

static intgen_t pers_fd = -1;
	/* file descriptor for persistent inomap backing store
	 */

/* context for inomap construction - initialized by inomap_restore_pers
 */
static u_int64_t hnkcnt;
static u_int64_t segcnt;
static hnk_t *roothnkp = 0;
static hnk_t *tailhnkp;
static seg_t *lastsegp;
static xfs_ino_t last_ino_added;


/* definition of locally defined global functions ****************************/

rv_t
inomap_restore_pers( drive_t *drivep,
		     content_inode_hdr_t *scrhdrp,
		     char *hkdir )
{
	drive_ops_t *dop = drivep->d_opsp;
	char *perspath;
	pers_t *persp;
	hnk_t *pershnkp;
	hnk_t *tmphnkp;
	intgen_t fd;
	/* REFERENCED */
	intgen_t nread;
	intgen_t rval;
	/* REFERENCED */
	intgen_t rval1;
	int i;
	bool_t ok;

	/* sanity checks
	 */
	ASSERT( INOPERSEG == ( sizeof( (( seg_t * )0 )->lobits ) * NBBY ));
	ASSERT( sizeof( hnk_t ) == HNKSZ );
	ASSERT( HNKSZ >= pgsz );
	ASSERT( ! ( HNKSZ % pgsz ));
	ASSERT( sizeof( pers_t ) <= PERSSZ );

	/* get inomap info from media hdr
	 */
	hnkcnt = scrhdrp->cih_inomap_hnkcnt;
	segcnt = scrhdrp->cih_inomap_segcnt;
	last_ino_added = scrhdrp->cih_inomap_lastino;

	/* truncate and open the backing store
	 */
	perspath = open_pathalloc( hkdir, PERS_NAME, 0 );
	( void )unlink( perspath );
	fd = open( perspath,
		   O_RDWR | O_CREAT,
		   S_IRUSR | S_IWUSR );
	if ( fd < 0 ) {
		mlog( MLOG_NORMAL | MLOG_ERROR,
		      "could not open %s: %s\n",
		      perspath,
		      strerror( errno ));
		return RV_ERROR;
	}

	/* mmap the persistent hdr and space for the map
	 */
	ASSERT( sizeof( hnk_t ) * ( size_t )hnkcnt >= pgsz );
	ASSERT( ! ( sizeof( hnk_t ) * ( size_t )hnkcnt % pgsz ));
	persp = ( pers_t * ) mmap_autogrow(
				     PERSSZ
				     +
				     sizeof( hnk_t ) * ( size_t )hnkcnt,
				     fd,
				     ( off64_t )0 );
	if ( persp == ( pers_t * )-1 ) {
		mlog( MLOG_NORMAL | MLOG_ERROR,
		      "unable to map %s: %s\n",
		      perspath,
		      strerror( errno ));
		return RV_ERROR;
	}

	/* load the pers hdr
	 */
	persp->hnkcnt = hnkcnt;
	persp->segcnt = segcnt;
	persp->last_ino_added = last_ino_added;

	tmphnkp = ( hnk_t * )calloc( ( size_t )hnkcnt, sizeof( hnk_t ));
	ASSERT( tmphnkp );

	/* read the map in from media
	 */
	nread = read_buf( ( char * )tmphnkp,
			  sizeof( hnk_t ) * ( size_t )hnkcnt,
			  ( void * )drivep,
			  ( rfp_t )dop->do_read,
			  ( rrbfp_t )dop->do_return_read_buf,
			  &rval );

	pershnkp = (hnk_t *)((char *)persp + PERSSZ);
	for(i = 0; i < hnkcnt; i++) {
		xlate_hnk(&tmphnkp[i], &pershnkp[i], 1);
	}
	free(tmphnkp);

	mlog(MLOG_NITTY, "inomap_restore_pers: pre-munmap\n");

	/* close up
	 */
	rval1 = munmap( ( void * )persp,
		        PERSSZ
		        +
		        sizeof( hnk_t ) * ( size_t )hnkcnt );
	ASSERT( ! rval1 );
	( void )close( fd );
	free( ( void * )perspath );

	mlog(MLOG_NITTY, "inomap_restore_pers: post-munmap\n");

	/* check the return code from read
	 */
	switch( rval ) {
	case 0:
		ASSERT( ( size_t )nread == sizeof( hnk_t ) * ( size_t )hnkcnt );
		ok = inomap_sync_pers( hkdir );
		if ( ! ok ) {
			return RV_ERROR;
		}
		return RV_OK;
	case DRIVE_ERROR_EOD:
	case DRIVE_ERROR_EOF:
	case DRIVE_ERROR_EOM:
	case DRIVE_ERROR_MEDIA:
	case DRIVE_ERROR_CORRUPTION:
		return RV_CORRUPT;
	case DRIVE_ERROR_DEVICE:
		return RV_DRIVE;
	case DRIVE_ERROR_CORE:
	default:
		return RV_CORE;
	}
}

/* peels inomap from media
 */
rv_t
inomap_discard( drive_t *drivep, content_inode_hdr_t *scrhdrp )
{
	drive_ops_t *dop = drivep->d_opsp;
	u_int64_t tmphnkcnt;
	/* REFERENCED */
	intgen_t nread;
	intgen_t rval;

	/* get inomap info from media hdr
	 */
	tmphnkcnt = scrhdrp->cih_inomap_hnkcnt;

	/* read the map in from media
	 */
	nread = read_buf( 0,
			  sizeof( hnk_t ) * ( size_t )tmphnkcnt,
			  ( void * )drivep,
			  ( rfp_t )dop->do_read,
			  ( rrbfp_t )dop->do_return_read_buf,
			  &rval );
	/* check the return code from read
	 */
	switch( rval ) {
	case 0:
		ASSERT( ( size_t )nread == sizeof( hnk_t ) * ( size_t )hnkcnt );
		return RV_OK;
	case DRIVE_ERROR_EOD:
	case DRIVE_ERROR_EOF:
	case DRIVE_ERROR_EOM:
	case DRIVE_ERROR_MEDIA:
	case DRIVE_ERROR_CORRUPTION:
		return RV_CORRUPT;
	case DRIVE_ERROR_DEVICE:
		return RV_DRIVE;
	case DRIVE_ERROR_CORE:
	default:
		return RV_CORE;
	}
}

bool_t
inomap_sync_pers( char *hkdir )
{
	char *perspath;
	pers_t *persp;
	hnk_t *hnkp;

	/* sanity checks
	 */
	ASSERT( sizeof( hnk_t ) == HNKSZ );
	ASSERT( HNKSZ >= pgsz );
	ASSERT( ! ( HNKSZ % pgsz ));

	/* only needed once per session
	 */
	if ( pers_fd >= 0 ) {
		return BOOL_TRUE;
	}

	/* open the backing store. if not present, ok, hasn't been created yet
	 */
	perspath = open_pathalloc( hkdir, PERS_NAME, 0 );
	pers_fd = open( perspath, O_RDWR );
	if ( pers_fd < 0 ) {
		return BOOL_TRUE;
	}

	/* mmap the persistent hdr
	 */
	persp = ( pers_t * ) mmap_autogrow(
				     PERSSZ,
				     pers_fd,
				     ( off64_t )0 );
	if ( persp == ( pers_t * )-1 ) {
		mlog( MLOG_NORMAL | MLOG_ERROR,
		      "unable to map %s hdr: %s\n",
		      perspath,
		      strerror( errno ));
		return BOOL_FALSE;
	}

	/* read the pers hdr
	 */
	hnkcnt = persp->hnkcnt;
	segcnt = persp->segcnt;
	last_ino_added = persp->last_ino_added;

	/* mmap the pers inomap
	 */
	ASSERT( hnkcnt * sizeof( hnk_t ) <= ( size64_t )INT32MAX );
	roothnkp = ( hnk_t * ) mmap_autogrow(
				       sizeof( hnk_t ) * ( size_t )hnkcnt,
				       pers_fd,
				       ( off64_t )PERSSZ );
	if ( roothnkp == ( hnk_t * )-1 ) {
		mlog( MLOG_NORMAL | MLOG_ERROR,
		      "unable to map %s: %s\n",
		      perspath,
		      strerror( errno ));
		return BOOL_FALSE;
	}

	/* correct the next pointers
	 */
	for ( hnkp = roothnkp
	      ;
	      hnkp < roothnkp + ( intgen_t )hnkcnt - 1
	      ;
	      hnkp++ ) {
		hnkp->nextp = hnkp + 1;
	}
	hnkp->nextp = 0;

	/* calculate the tail pointers
	 */
	tailhnkp = hnkp;
	ASSERT( hnkcnt > 0 );
	lastsegp = &tailhnkp->seg[ ( intgen_t )( segcnt
						 -
						 SEGPERHNK * ( hnkcnt - 1 )
						 -
						 1 ) ];
	
	/* now all inomap operators will work
	 */
	return BOOL_TRUE;
}

/* de-allocate the persistent inomap
 */
void
inomap_del_pers( char *hkdir )
{
	char *perspath = open_pathalloc( hkdir, PERS_NAME, 0 );
	( void )unlink( perspath );
	free( ( void * )perspath );
}

/* mark all included non-dirs as MAP_NDR_NOREST
 */
void
inomap_sanitize( void )
{
	hnk_t *hnkp;
	seg_t *segp;

	/* step through all hunks, segs, and inos
	 */
	for ( hnkp = roothnkp
	      ;
	      hnkp != 0
	      ;
	      hnkp = hnkp->nextp ) {
		for ( segp = hnkp->seg
		      ;
		      segp < hnkp->seg + SEGPERHNK
		      ;
		      segp++ ) {
			xfs_ino_t ino;
			if ( hnkp == tailhnkp && segp > lastsegp ) {
				return;
			}
			for ( ino = segp->base
			      ;
			      ino < segp->base + INOPERSEG
			      ;
			      ino++ ) {
				intgen_t state;
				if ( ino > last_ino_added ) {
					return;
				}
				SEG_GET_BITS( segp, ino, state );
				if ( state == MAP_NDR_CHANGE ) {
					state = MAP_NDR_NOREST;
					SEG_SET_BITS( segp, ino, state );
				}
			}
		}
	}
}

/* called to mark a non-dir ino as TO be restored
 */
void
inomap_rst_add( xfs_ino_t ino )
{
		ASSERT( pers_fd >= 0 );
		( void )map_set( ino, MAP_NDR_CHANGE );
}

/* called to mark a non-dir ino as NOT to be restored
 */
void
inomap_rst_del( xfs_ino_t ino )
{
		ASSERT( pers_fd >= 0 );
		( void )map_set( ino, MAP_NDR_NOREST );
}

/* called to ask if any inos in the given range need to be restored.
 * range is inclusive
 */
bool_t
inomap_rst_needed( xfs_ino_t firstino, xfs_ino_t lastino )
{
	hnk_t *hnkp;
	seg_t *segp;

	/* if inomap not restored/resynced, just say yes
	 */
	if ( ! roothnkp ) {
		return BOOL_TRUE;
	}

	/* may be completely out of range
	 */
	if ( firstino > last_ino_added ) {
		return BOOL_FALSE;
	}

	/* find the hunk/seg containing first ino or any ino beyond
	 */
	for ( hnkp = roothnkp ; hnkp != 0 ; hnkp = hnkp->nextp ) {
		if ( firstino > hnkp->maxino ) {
			continue;
		}
		for ( segp = hnkp->seg; segp < hnkp->seg + SEGPERHNK ; segp++ ){
			if ( hnkp == tailhnkp && segp > lastsegp ) {
				return BOOL_FALSE;
			}
			if ( firstino < segp->base + INOPERSEG ) {
				goto begin;
			}
		}
	}
	return BOOL_FALSE;

begin:
	/* search until at least one ino is needed or until beyond last ino
	 */
	for ( ; ; ) {
		xfs_ino_t ino;

		if ( segp->base > lastino ) {
			return BOOL_FALSE;
		}
		for ( ino = segp->base ; ino < segp->base + INOPERSEG ; ino++ ){
			intgen_t state;
			if ( ino < firstino ) {
				continue;
			}
			if ( ino > lastino ) {
				return BOOL_FALSE;
			}
			SEG_GET_BITS( segp, ino, state );
			if ( state == MAP_NDR_CHANGE ) {
				return BOOL_TRUE;
			}
		}
		segp++;
		if ( hnkp == tailhnkp && segp > lastsegp ) {
			return BOOL_FALSE;
		}
		if ( segp >= hnkp->seg + SEGPERHNK ) {
			hnkp = hnkp->nextp;
			if ( ! hnkp ) {
				return BOOL_FALSE;
			}
			segp = hnkp->seg;
		}
	}
	/* NOTREACHED */
}

/* calls the callback for all inos with an inomap state included
 * in the state mask. stops iteration when inos exhaused or cb
 * returns FALSE.
 */
void
inomap_cbiter( intgen_t statemask,
	       bool_t ( * cbfunc )( void *ctxp, xfs_ino_t ino ),
	       void *ctxp )
{
	hnk_t *hnkp;
	seg_t *segp;

	/* step through all hunks, segs, and inos
	 */
	for ( hnkp = roothnkp
	      ;
	      hnkp != 0
	      ;
	      hnkp = hnkp->nextp ) {
		for ( segp = hnkp->seg
		      ;
		      segp < hnkp->seg + SEGPERHNK
		      ;
		      segp++ ) {
			xfs_ino_t ino;
			if ( hnkp == tailhnkp && segp > lastsegp ) {
				return;
			}
			for ( ino = segp->base
			      ;
			      ino < segp->base + INOPERSEG
			      ;
			      ino++ ) {
				intgen_t state;
				if ( ino > last_ino_added ) {
					return;
				}
				SEG_GET_BITS( segp, ino, state );
				if ( statemask & ( 1 << state )) {
					bool_t ok;
					ok = ( cbfunc )( ctxp, ino );
					if ( ! ok ) {
						return;
					}
				}
			}
		}
	}
}

/* definition of locally defined static functions ****************************/

/* map_getset - locates and gets the state of the specified ino,
 * and optionally sets the state to a new value.
 */
static intgen_t
map_getset( xfs_ino_t ino, intgen_t newstate, bool_t setflag )
{
	hnk_t *hnkp;
	seg_t *segp;

	if ( ino > last_ino_added ) {
		return MAP_INO_UNUSED;
	}
	for ( hnkp = roothnkp ; hnkp != 0 ; hnkp = hnkp->nextp ) {
		if ( ino > hnkp->maxino ) {
			continue;
		}
		for ( segp = hnkp->seg; segp < hnkp->seg + SEGPERHNK ; segp++ ){
			if ( hnkp == tailhnkp && segp > lastsegp ) {
				return MAP_INO_UNUSED;
			}
			if ( ino < segp->base ) {
				return MAP_INO_UNUSED;
			}
			if ( ino < segp->base + INOPERSEG ) {
				intgen_t state;
				SEG_GET_BITS( segp, ino, state );
				if ( setflag ) {
					SEG_SET_BITS( segp, ino, newstate );
				}
				return state;
			}
		}
		return MAP_INO_UNUSED;
	}
	return MAP_INO_UNUSED;
}

static intgen_t
map_set( xfs_ino_t ino, intgen_t state )
{
	intgen_t oldstate;

 	oldstate = map_getset( ino, state, BOOL_TRUE );
	return oldstate;
}
