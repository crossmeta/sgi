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

/*
 * Block mapping code taken from xfs_db.
 */

/*
 * Block map entry.
 */
typedef struct blkent {
	xfs_dfiloff_t	startoff;
	xfs_dfilblks_t	nblks;
	xfs_dfsbno_t	blks[1];
} blkent_t;
#define	BLKENT_SIZE(n)	\
	(offsetof(blkent_t, blks) + (sizeof(xfs_dfsbno_t) * (n)))

/*
 * Block map.
 */
typedef	struct blkmap {
	int		naents;
	int		nents;
	blkent_t	*ents[1];
} blkmap_t;
#define	BLKMAP_SIZE(n)	\
	(offsetof(blkmap_t, ents) + (sizeof(blkent_t *) * (n)))

/*
 * Extent descriptor.
 */
typedef struct bmap_ext {
	xfs_dfiloff_t	startoff;
	xfs_dfsbno_t	startblock;
	xfs_dfilblks_t	blockcount;
	int		flag;
} bmap_ext_t;

void		blkent_append(blkent_t **entp, xfs_dfsbno_t b,
			      xfs_dfilblks_t c);
blkent_t	*blkent_new(xfs_dfiloff_t o, xfs_dfsbno_t b, xfs_dfilblks_t c);
void		blkent_prepend(blkent_t **entp, xfs_dfsbno_t b,
			       xfs_dfilblks_t c);
blkmap_t	*blkmap_alloc(xfs_extnum_t);
void		blkmap_free(blkmap_t *blkmap);
xfs_dfsbno_t	blkmap_get(blkmap_t *blkmap, xfs_dfiloff_t o);
int		blkmap_getn(blkmap_t *blkmap, xfs_dfiloff_t o,
			    xfs_dfilblks_t nb, bmap_ext_t **bmpp);
void		blkmap_grow(blkmap_t **blkmapp, blkent_t **entp,
			    blkent_t *newent);
xfs_dfiloff_t	blkmap_last_off(blkmap_t *blkmap);
xfs_dfiloff_t	blkmap_next_off(blkmap_t *blkmap, xfs_dfiloff_t o, int *t);
void		blkmap_set_blk(blkmap_t **blkmapp, xfs_dfiloff_t o,
			       xfs_dfsbno_t b);
void		blkmap_set_ext(blkmap_t **blkmapp, xfs_dfiloff_t o,
			       xfs_dfsbno_t b, xfs_dfilblks_t c);
void		blkmap_shrink(blkmap_t *blkmap, blkent_t **entp);
