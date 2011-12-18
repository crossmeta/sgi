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
#ifndef INOMAP_H
#define INOMAP_H

/* inomap.[hc] - inode map abstraction
 *
 * an inode map describes the inode numbers (inos) in a file system dump.
 * the map identifies which inos are in-use by the fs, which of those are
 * directories, and which are dumped.
 *
 * the map is represented as a list of map segments. a map segment is
 * a 64-bit starting ino and two 64-bit bitmaps. the bitmaps describe
 * the 64 inos beginning with the starting ino. two bits are available
 * for each ino.
 */

/* inomap_build - this function allocates and constructs an in-memory
 * representation of the bitmap. it prunes from the map inos of files not
 * changed since the last dump, inos not identified by the subtree list,
 * and directories not needed to represent a hierarchy containing
 * changed inodes. it handles hard links; a file linked to multiple
 * directory entries will not be pruned if at least one of those
 * directories has an ancestor in the subtree list.
 *
 * it returns by reference an array of startpoints in the non-directory
 * portion of the dump, as well as the count of dir and nondir inos
 * makred as present and to be dumped. A startpoint identifies a non-dir ino,
 * and a non-hole accumulated size position within that file. only very large
 * files will contain a startpoint; in all other cases the startpoints will
 * fall at file boundaries. returns BOOL_FALSE if error encountered (should
 * abort the dump; else returns BOOL_TRUE.
 */
extern bool_t inomap_build( jdm_fshandle_t *fshandlep,
			    intgen_t fsfd,
			    xfs_bstat_t *rootstatp,
			    bool_t last,
	      		    time_t lasttime,
			    bool_t resume,
	      		    time_t resumetime,
			    size_t resumerangecnt,
			    drange_t *resumerangep,
			    char *subtreebuf[],
			    ix_t subtreecnt,
			    startpt_t startptp[],
	      		    size_t startptcnt,
			    ix_t *statphasep,
			    ix_t *statpassp,
			    size64_t statcnt,
			    size64_t *statdonep );

#ifdef SIZEEST
extern u_int64_t inomap_getsz( void );
#endif /* SIZEEST */

/* inomap_skip - tell inomap about inodes to skip in the dump
 */
extern void inomap_skip( xfs_ino_t ino );


/* inomap_writehdr - updates the write header with inomap-private info
 * to be communicated to the restore side
 */
extern void inomap_writehdr( content_inode_hdr_t *scwhdrp );


/* inomap_dump - dumps the map to media - content-abstraction-knowledgable
 *
 * returns error from media write op
 */
extern rv_t inomap_dump( drive_t *drivep );


/* map state values
 */
#define MAP_INO_UNUSED	0       /* ino not in use by fs */
#define MAP_DIR_NOCHNG	1       /* dir, ino in use by fs, but not dumped */
#define MAP_NDR_NOCHNG	2       /* non-dir, ino in use by fs, but not dumped */
#define MAP_DIR_CHANGE	3       /* dir, changed since last dump */
#define MAP_NDR_CHANGE	4       /* non-dir, changed since last dump */
#define MAP_DIR_SUPPRT	5       /* dir, unchanged but needed for hierarchy */
#define MAP_RESERVED1	6       /* this state currently not used */
#define MAP_RESERVED2	7       /* this state currently not used */


/* map context and operators
 */

/* the inomap is implimented as a linked list of chunks. each chunk contains
 * an array of map segments. a map segment contains a start ino and a
 * bitmap of 64 3-bit state values (see MAP_... in inomap.h). the SEG_macros
 * index and manipulate the 3-bit state values.
 */
struct seg {
	xfs_ino_t base;
	u_int64_t lobits;
	u_int64_t mebits;
	u_int64_t hibits;
};

typedef struct seg seg_t;

#define SEG_SET_BASE( segp, ino )					\
	{								\
		segp->base = ino;					\
	}

#ifdef OLDCODE
#define SEG_ADD_BITS( segp, ino, state )				\
	{								\
		register xfs_ino_t relino;				\
		relino = ino - segp->base;				\
		segp->lobits |= ( u_int64_t )( ( state >> 0 ) & 1 ) << relino; \
		segp->mebits |= ( u_int64_t )( ( state >> 1 ) & 1 ) << relino; \
		segp->hibits |= ( u_int64_t )( ( state >> 2 ) & 1 ) << relino; \
	}

#define SEG_SET_BITS( segp, ino, state )				\
	{								\
		register xfs_ino_t relino;				\
		register u_int64_t clrmask;				\
		relino = ino - segp->base;				\
		clrmask = ~( ( u_int64_t )1 << relino );		\
		segp->lobits &= clrmask;				\
		segp->mebits &= clrmask;				\
		segp->hibits &= clrmask;				\
		segp->lobits |= ( u_int64_t )( ( state >> 0 ) & 1 ) << relino; \
		segp->mebits |= ( u_int64_t )( ( state >> 1 ) & 1 ) << relino; \
		segp->hibits |= ( u_int64_t )( ( state >> 2 ) & 1 ) << relino; \
	}

#define SEG_GET_BITS( segp, ino, state )				\
	{								\
		register xfs_ino_t relino;				\
		relino = ino - segp->base;				\
		state = 0;						\
		state |= ( intgen_t )((( segp->lobits >> relino ) & 1 ) * 1 );\
		state |= ( intgen_t )((( segp->mebits >> relino ) & 1 ) * 2 );\
		state |= ( intgen_t )((( segp->hibits >> relino ) & 1 ) * 4 );\
	}
#else /* OLDCODE */
/*
 * The converse on MACROBITS are the functions defined in inomap.c
 */
#ifdef MACROBITS
#define SEG_ADD_BITS( segp, ino, state )				\
	{								\
		register xfs_ino_t relino;				\
		register u_int64_t mask;				\
		relino = ino - segp->base;				\
		mask = ( u_int64_t )1 << relino;			\
		switch( state ) {					\
		case 0:							\
			break;						\
		case 1:							\
			segp->lobits |= mask;				\
			break;						\
		case 2:							\
			segp->mebits |= mask;				\
			break;						\
		case 3:							\
			segp->lobits |= mask;				\
			segp->mebits |= mask;				\
			break;						\
		case 4:							\
			segp->hibits |= mask;				\
			break;						\
		case 5:							\
			segp->lobits |= mask;				\
			segp->hibits |= mask;				\
			break;						\
		case 6:							\
			segp->mebits |= mask;				\
			segp->hibits |= mask;				\
			break;						\
		case 7:							\
			segp->lobits |= mask;				\
			segp->mebits |= mask;				\
			segp->hibits |= mask;				\
			break;						\
		}							\
	}

#define SEG_SET_BITS( segp, ino, state )				\
	{								\
		register xfs_ino_t relino;				\
		register u_int64_t mask;				\
		register u_int64_t clrmask;				\
		relino = ino - segp->base;				\
		mask = ( u_int64_t )1 << relino;			\
		clrmask = ~mask;					\
		switch( state ) {					\
		case 0:							\
			segp->lobits &= clrmask;			\
			segp->mebits &= clrmask;			\
			segp->hibits &= clrmask;			\
			break;						\
		case 1:							\
			segp->lobits |= mask;				\
			segp->mebits &= clrmask;			\
			segp->hibits &= clrmask;			\
			break;						\
		case 2:							\
			segp->lobits &= clrmask;			\
			segp->mebits |= mask;				\
			segp->hibits &= clrmask;			\
			break;						\
		case 3:							\
			segp->lobits |= mask;				\
			segp->mebits |= mask;				\
			segp->hibits &= clrmask;			\
			break;						\
		case 4:							\
			segp->lobits &= clrmask;			\
			segp->mebits &= clrmask;			\
			segp->hibits |= mask;				\
			break;						\
		case 5:							\
			segp->lobits |= mask;				\
			segp->mebits &= clrmask;			\
			segp->hibits |= mask;				\
			break;						\
		case 6:							\
			segp->lobits &= clrmask;			\
			segp->mebits |= mask;				\
			segp->hibits |= mask;				\
			break;						\
		case 7:							\
			segp->lobits |= mask;				\
			segp->mebits |= mask;				\
			segp->hibits |= mask;				\
			break;						\
		}							\
	}

#define SEG_GET_BITS( segp, ino, state )				\
	{								\
		register xfs_ino_t relino;				\
		register u_int64_t mask;				\
		relino = ino - segp->base;				\
		mask = ( u_int64_t )1 << relino;			\
		if ( segp->lobits & mask ) {				\
			state = 1;					\
		} else {						\
			state = 0;					\
		}							\
		if ( segp->mebits & mask ) {				\
			state |= 2;					\
		}							\
		if ( segp->hibits & mask ) {				\
			state |= 4;					\
		}							\
	}
#endif /* MACROBITS */
#endif /* OLDCODE */

#define INOPERSEG	( sizeof( (( seg_t * )0 )->lobits ) * NBBY )

#define HNKSZ		( 4 * PGSZ )
#define SEGPERHNK	( ( HNKSZ / sizeof( seg_t )) - 1 )

struct hnk {
	seg_t seg[ SEGPERHNK ];
	xfs_ino_t maxino;
	struct hnk *nextp;
	char pad[sizeof( seg_t ) - sizeof( xfs_ino_t ) - sizeof( struct hnk * )];
};

typedef struct hnk hnk_t;


/* inomap_state - returns the map state of the given ino.
 * highly optimized for monotonically increasing arguments to
 * successive calls. requires a pointer to a context block, obtained from
 * inomap_state_getcontext(), and released by inomap_state_freecontext().
 */
extern void *inomap_state_getcontext( void );
extern intgen_t inomap_state( void *contextp, xfs_ino_t ino );
extern void inomap_state_freecontext( void *contextp );
void inomap_state_postaccum( void *p );


#ifdef NOTUSED
/* inomap_iter_cb - will call the supplied function for each ino in
 * the map matching a state in the state mask. if the callback returns
 * FALSE, the iteration will be aborted and inomap_iter_cb() will
 * return FALSE. the state mask is constructed by OR'ing bits in bit
 * positions corresponding to the state values.
 */
extern bool_t inomap_iter_cb( void *contextp,
			      intgen_t statemask,
			      bool_t ( *funcp )( void *contextp,
					         xfs_ino_t ino,
					         intgen_t state ));
#endif /* NOTUSED */

#endif /* INOMAP_H */
