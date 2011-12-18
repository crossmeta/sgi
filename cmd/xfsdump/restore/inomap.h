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

/* map state values
 */
#define MAP_INO_UNUSED	0       /* ino not in use by fs */
#define MAP_DIR_NOCHNG	1       /* dir, ino in use by fs, but not dumped */
#define MAP_NDR_NOCHNG	2       /* non-dir, ino in use by fs, but not dumped */
#define MAP_DIR_CHANGE	3       /* dir, changed since last dump */
#define MAP_NDR_CHANGE	4       /* non-dir, changed since last dump */
#define MAP_DIR_SUPPRT	5       /* dir, unchanged but needed for hierarchy */
#define MAP_NDR_NOREST	6       /* was MAP_NDR_CHANGE, but not to be restored */
#define MAP_RESERVED2	7       /* this state currently not used */


/* map context and operators
 */

/* the inomap is implemented as a linked list of chunks. each chunk contains
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

#define INOPERSEG	( sizeofmember( seg_t, lobits ) * NBBY )

#define HNKSZ		( 4 * PGSZ )
#define SEGPERHNK	( ( HNKSZ / sizeof( seg_t )) - 1 )

struct hnk {
	seg_t seg[ SEGPERHNK ];
	xfs_ino_t maxino;
	struct hnk *nextp;
	char pad[sizeof( seg_t ) - sizeof( xfs_ino_t ) - sizeof( struct hnk * )];
};

typedef struct hnk hnk_t;


extern bool_t inomap_sync_pers( char *hkdir );
extern rv_t inomap_restore_pers( drive_t *drivep,
				 content_inode_hdr_t *scrhdrp,
				 char *hkdir );
extern void inomap_del_pers( char *hkdir );
extern void inomap_sanitize( void );
extern bool_t inomap_rst_needed( xfs_ino_t begino, xfs_ino_t endino );
extern void inomap_rst_add( xfs_ino_t ino );
extern void inomap_rst_del( xfs_ino_t ino );
extern rv_t inomap_discard( drive_t *drivep, content_inode_hdr_t *scrhdrp );
extern void inomap_cbiter( intgen_t mapstatemask,
			   bool_t ( * cbfunc )( void *ctxp, xfs_ino_t ino ),
			   void *ctxp );

#endif /* INOMAP_H */
