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

#include <xfs.h>

/*
 * xfs_attr_leaf.c
 *
 * Routines to implement leaf blocks of attributes as Btrees of hashed names.
 */

/*========================================================================
 * Routines used for growing the Btree.
 *========================================================================*/

/*
 * Create the initial contents of a leaf attribute list
 * or a leaf in a node attribute list.
 */
int
xfs_attr_leaf_create(xfs_da_args_t *args, xfs_dablk_t blkno, xfs_dabuf_t **bpp)
{
	xfs_attr_leafblock_t *leaf;
	xfs_attr_leaf_hdr_t *hdr;
	xfs_inode_t *dp;
	xfs_dabuf_t *bp;
	int error;

	dp = args->dp;
	ASSERT(dp != NULL);
	error = xfs_da_get_buf(args->trans, args->dp, blkno, -1, &bp,
					    XFS_ATTR_FORK);
	if (error)
		return(error);
	ASSERT(bp != NULL);
	leaf = bp->data;
	bzero((char *)leaf, XFS_LBSIZE(dp->i_mount));
	hdr = &leaf->hdr;
	INT_SET(hdr->info.magic, ARCH_CONVERT, XFS_ATTR_LEAF_MAGIC);
	INT_SET(hdr->firstused, ARCH_CONVERT, XFS_LBSIZE(dp->i_mount));
	if (INT_GET(hdr->firstused, ARCH_CONVERT) == 0) {
		INT_SET(hdr->firstused, ARCH_CONVERT,
			XFS_LBSIZE(dp->i_mount) - XFS_ATTR_LEAF_NAME_ALIGN);
	}

	INT_SET(hdr->freemap[0].base, ARCH_CONVERT,
						sizeof(xfs_attr_leaf_hdr_t));
	INT_SET(hdr->freemap[0].size, ARCH_CONVERT,
					  INT_GET(hdr->firstused, ARCH_CONVERT)
					- INT_GET(hdr->freemap[0].base,
								ARCH_CONVERT));

	xfs_da_log_buf(args->trans, bp, 0, XFS_LBSIZE(dp->i_mount) - 1);

	*bpp = bp;
	return(0);
}

/*
 * Split the leaf node, rebalance, then add the new entry.
 */
int
xfs_attr_leaf_split(xfs_da_state_t *state, xfs_da_state_blk_t *oldblk,
				   xfs_da_state_blk_t *newblk)
{
	xfs_dablk_t blkno;
	int error;

	/*
	 * Allocate space for a new leaf node.
	 */
	ASSERT(oldblk->magic == XFS_ATTR_LEAF_MAGIC);
	error = xfs_da_grow_inode(state->args, &blkno);
	if (error)
		return(error);
	error = xfs_attr_leaf_create(state->args, blkno, &newblk->bp);
	if (error)
		return(error);
	newblk->blkno = blkno;
	newblk->magic = XFS_ATTR_LEAF_MAGIC;

	/*
	 * Rebalance the entries across the two leaves.
	 * NOTE: rebalance() currently depends on the 2nd block being empty.
	 */
	xfs_attr_leaf_rebalance(state, oldblk, newblk);
	error = xfs_da_blk_link(state, oldblk, newblk);
	if (error)
		return(error);

	/*
	 * Save info on "old" attribute for "atomic rename" ops, leaf_add()
	 * modifies the index/blkno/rmtblk/rmtblkcnt fields to show the
	 * "new" attrs info.  Will need the "old" info to remove it later.
	 *
	 * Insert the "new" entry in the correct block.
	 */
	if (state->inleaf)
		error = xfs_attr_leaf_add(oldblk->bp, state->args);
	else
		error = xfs_attr_leaf_add(newblk->bp, state->args);

	/*
	 * Update last hashval in each block since we added the name.
	 */
	oldblk->hashval = xfs_attr_leaf_lasthash(oldblk->bp, NULL);
	newblk->hashval = xfs_attr_leaf_lasthash(newblk->bp, NULL);
	return(error);
}

/*
 * Add a name to the leaf attribute list structure.
 */
int
xfs_attr_leaf_add(xfs_dabuf_t *bp, xfs_da_args_t *args)
{
	xfs_attr_leafblock_t *leaf;
	xfs_attr_leaf_hdr_t *hdr;
	xfs_attr_leaf_map_t *map;
	int tablesize, entsize, sum, tmp, i;

	leaf = bp->data;
	ASSERT(INT_GET(leaf->hdr.info.magic, ARCH_CONVERT)
						== XFS_ATTR_LEAF_MAGIC);
	ASSERT((args->index >= 0)
		&& (args->index <= INT_GET(leaf->hdr.count, ARCH_CONVERT)));
	hdr = &leaf->hdr;
	entsize = xfs_attr_leaf_newentsize(args,
			   args->trans->t_mountp->m_sb.sb_blocksize, NULL);

	/*
	 * Search through freemap for first-fit on new name length.
	 * (may need to figure in size of entry struct too)
	 */
	tablesize = (INT_GET(hdr->count, ARCH_CONVERT) + 1)
					* sizeof(xfs_attr_leaf_entry_t)
					+ sizeof(xfs_attr_leaf_hdr_t);
	map = &hdr->freemap[XFS_ATTR_LEAF_MAPSIZE-1];
	for (sum = 0, i = XFS_ATTR_LEAF_MAPSIZE-1; i >= 0; map--, i--) {
		if (tablesize > INT_GET(hdr->firstused, ARCH_CONVERT)) {
			sum += INT_GET(map->size, ARCH_CONVERT);
			continue;
		}
		if (INT_GET(map->size, ARCH_CONVERT) == 0)
			continue;	/* no space in this map */
		tmp = entsize;
		if (INT_GET(map->base, ARCH_CONVERT)
				< INT_GET(hdr->firstused, ARCH_CONVERT))
			tmp += sizeof(xfs_attr_leaf_entry_t);
		if (INT_GET(map->size, ARCH_CONVERT) >= tmp) {
			tmp = xfs_attr_leaf_add_work(bp, args, i);
			return(tmp);
		}
		sum += INT_GET(map->size, ARCH_CONVERT);
	}

	/*
	 * If there are no holes in the address space of the block,
	 * and we don't have enough freespace, then compaction will do us
	 * no good and we should just give up.
	 */
	if (!hdr->holes && (sum < entsize))
		return(XFS_ERROR(ENOSPC));

	/*
	 * Compact the entries to coalesce free space.
	 * This may change the hdr->count via dropping INCOMPLETE entries.
	 */
	xfs_attr_leaf_compact(args->trans, bp);

	/*
	 * After compaction, the block is guaranteed to have only one
	 * free region, in freemap[0].  If it is not big enough, give up.
	 */
	if (INT_GET(hdr->freemap[0].size, ARCH_CONVERT)
				< (entsize + sizeof(xfs_attr_leaf_entry_t)))
		return(XFS_ERROR(ENOSPC));

	return(xfs_attr_leaf_add_work(bp, args, 0));
}

/*
 * Add a name to a leaf attribute list structure.
 */
STATIC int
xfs_attr_leaf_add_work(xfs_dabuf_t *bp, xfs_da_args_t *args, int mapindex)
{
	xfs_attr_leafblock_t *leaf;
	xfs_attr_leaf_hdr_t *hdr;
	xfs_attr_leaf_entry_t *entry;
	xfs_attr_leaf_name_local_t *name_loc;
	xfs_attr_leaf_name_remote_t *name_rmt;
	xfs_attr_leaf_map_t *map;
	xfs_mount_t *mp;
	int tmp, i;

	leaf = bp->data;
	ASSERT(INT_GET(leaf->hdr.info.magic, ARCH_CONVERT)
						== XFS_ATTR_LEAF_MAGIC);
	hdr = &leaf->hdr;
	ASSERT((mapindex >= 0) && (mapindex < XFS_ATTR_LEAF_MAPSIZE));
	ASSERT((args->index >= 0)
		&& (args->index <= INT_GET(hdr->count, ARCH_CONVERT)));

	/*
	 * Force open some space in the entry array and fill it in.
	 */
	entry = &leaf->entries[args->index];
	if (args->index < INT_GET(hdr->count, ARCH_CONVERT)) {
		tmp  = INT_GET(hdr->count, ARCH_CONVERT) - args->index;
		tmp *= sizeof(xfs_attr_leaf_entry_t);
		ovbcopy((char *)entry, (char *)(entry+1), tmp);
		xfs_da_log_buf(args->trans, bp,
		    XFS_DA_LOGRANGE(leaf, entry, tmp + sizeof(*entry)));
	}
	INT_MOD(hdr->count, ARCH_CONVERT, 1);

	/*
	 * Allocate space for the new string (at the end of the run).
	 */
	map = &hdr->freemap[mapindex];
	mp = args->trans->t_mountp;
	ASSERT(INT_GET(map->base, ARCH_CONVERT) < XFS_LBSIZE(mp));
	ASSERT((INT_GET(map->base, ARCH_CONVERT) & 0x3) == 0);
	ASSERT(INT_GET(map->size, ARCH_CONVERT)
				>= xfs_attr_leaf_newentsize(args,
					     mp->m_sb.sb_blocksize, NULL));
	ASSERT(INT_GET(map->size, ARCH_CONVERT) < XFS_LBSIZE(mp));
	ASSERT((INT_GET(map->size, ARCH_CONVERT) & 0x3) == 0);
	INT_MOD(map->size, ARCH_CONVERT,
		-xfs_attr_leaf_newentsize(args, mp->m_sb.sb_blocksize, &tmp));
	INT_SET(entry->nameidx, ARCH_CONVERT,
					INT_GET(map->base, ARCH_CONVERT)
				      + INT_GET(map->size, ARCH_CONVERT));
	INT_SET(entry->hashval, ARCH_CONVERT, args->hashval);
	entry->flags = tmp ? XFS_ATTR_LOCAL : 0;
	entry->flags |= (args->flags & ATTR_ROOT) ? XFS_ATTR_ROOT : 0;
	if (args->rename) {
		entry->flags |= XFS_ATTR_INCOMPLETE;
		if ((args->blkno2 == args->blkno) &&
		    (args->index2 <= args->index)) {
			args->index2++;
		}
	}
	xfs_da_log_buf(args->trans, bp,
			  XFS_DA_LOGRANGE(leaf, entry, sizeof(*entry)));
	ASSERT((args->index == 0) || (INT_GET(entry->hashval, ARCH_CONVERT)
						>= INT_GET((entry-1)->hashval,
							    ARCH_CONVERT)));
	ASSERT((args->index == INT_GET(hdr->count, ARCH_CONVERT)-1) ||
	       (INT_GET(entry->hashval, ARCH_CONVERT)
			    <= (INT_GET((entry+1)->hashval, ARCH_CONVERT))));

	/*
	 * Copy the attribute name and value into the new space.
	 *
	 * For "remote" attribute values, simply note that we need to 
	 * allocate space for the "remote" value.  We can't actually
	 * allocate the extents in this transaction, and we can't decide
	 * which blocks they should be as we might allocate more blocks
	 * as part of this transaction (a split operation for example).
	 */
	if (entry->flags & XFS_ATTR_LOCAL) {
		name_loc = XFS_ATTR_LEAF_NAME_LOCAL(leaf, args->index);
		name_loc->namelen = args->namelen;
		INT_SET(name_loc->valuelen, ARCH_CONVERT, args->valuelen);
		bcopy(args->name, (char *)name_loc->nameval, args->namelen);
		bcopy(args->value, (char *)&name_loc->nameval[args->namelen],
				   INT_GET(name_loc->valuelen, ARCH_CONVERT));
	} else {
		name_rmt = XFS_ATTR_LEAF_NAME_REMOTE(leaf, args->index);
		name_rmt->namelen = args->namelen;
		bcopy(args->name, (char *)name_rmt->name, args->namelen);
		entry->flags |= XFS_ATTR_INCOMPLETE;
		/* just in case */
		INT_SET(name_rmt->valuelen, ARCH_CONVERT, 0);
		INT_SET(name_rmt->valueblk, ARCH_CONVERT, 0);
		args->rmtblkno = 1;
		args->rmtblkcnt = XFS_B_TO_FSB(mp, args->valuelen);
	}
	xfs_da_log_buf(args->trans, bp,
	     XFS_DA_LOGRANGE(leaf, XFS_ATTR_LEAF_NAME(leaf, args->index),
				   xfs_attr_leaf_entsize(leaf, args->index)));

	/*
	 * Update the control info for this leaf node
	 */
	if (INT_GET(entry->nameidx, ARCH_CONVERT)
				< INT_GET(hdr->firstused, ARCH_CONVERT)) {
		INT_SET(hdr->firstused, ARCH_CONVERT,
					INT_GET(entry->nameidx, ARCH_CONVERT));
	}
	ASSERT(INT_GET(hdr->firstused, ARCH_CONVERT)
				>= ((INT_GET(hdr->count, ARCH_CONVERT)
					* sizeof(*entry))+sizeof(*hdr)));
	tmp = (INT_GET(hdr->count, ARCH_CONVERT)-1)
					* sizeof(xfs_attr_leaf_entry_t)
					+ sizeof(xfs_attr_leaf_hdr_t);
	map = &hdr->freemap[0];
	for (i = 0; i < XFS_ATTR_LEAF_MAPSIZE; map++, i++) {
		if (INT_GET(map->base, ARCH_CONVERT) == tmp) {
			INT_MOD(map->base, ARCH_CONVERT,
					sizeof(xfs_attr_leaf_entry_t));
			INT_MOD(map->size, ARCH_CONVERT,
					-sizeof(xfs_attr_leaf_entry_t));
		}
	}
	INT_MOD(hdr->usedbytes, ARCH_CONVERT,
				xfs_attr_leaf_entsize(leaf, args->index));
	xfs_da_log_buf(args->trans, bp,
		XFS_DA_LOGRANGE(leaf, hdr, sizeof(*hdr)));
	return(0);
}

/*
 * Garbage collect a leaf attribute list block by copying it to a new buffer.
 */
STATIC void
xfs_attr_leaf_compact(xfs_trans_t *trans, xfs_dabuf_t *bp)
{
	xfs_attr_leafblock_t *leaf_s, *leaf_d;
	xfs_attr_leaf_hdr_t *hdr_s, *hdr_d;
	xfs_mount_t *mp;
	char *tmpbuffer;

	mp = trans->t_mountp;
	tmpbuffer = kmem_alloc(XFS_LBSIZE(mp), KM_SLEEP);
	ASSERT(tmpbuffer != NULL);
	bcopy(bp->data, tmpbuffer, XFS_LBSIZE(mp));
	bzero(bp->data, XFS_LBSIZE(mp));

	/*
	 * Copy basic information
	 */
	leaf_s = (xfs_attr_leafblock_t *)tmpbuffer;
	leaf_d = bp->data;
	hdr_s = &leaf_s->hdr;
	hdr_d = &leaf_d->hdr;
	hdr_d->info = hdr_s->info;	/* struct copy */
	INT_SET(hdr_d->firstused, ARCH_CONVERT, XFS_LBSIZE(mp));
	/* handle truncation gracefully */
	if (INT_GET(hdr_d->firstused, ARCH_CONVERT) == 0) {
		INT_SET(hdr_d->firstused, ARCH_CONVERT,
				XFS_LBSIZE(mp) - XFS_ATTR_LEAF_NAME_ALIGN);
	}
	INT_SET(hdr_d->usedbytes, ARCH_CONVERT, 0);
	INT_SET(hdr_d->count, ARCH_CONVERT, 0);
	hdr_d->holes = 0;
	INT_SET(hdr_d->freemap[0].base, ARCH_CONVERT,
					sizeof(xfs_attr_leaf_hdr_t));
	INT_SET(hdr_d->freemap[0].size, ARCH_CONVERT,
				INT_GET(hdr_d->firstused, ARCH_CONVERT)
			      - INT_GET(hdr_d->freemap[0].base, ARCH_CONVERT));

	/*
	 * Copy all entry's in the same (sorted) order,
	 * but allocate name/value pairs packed and in sequence.
	 */
	xfs_attr_leaf_moveents(leaf_s, 0, leaf_d, 0,
				(int)INT_GET(hdr_s->count, ARCH_CONVERT), mp);

	xfs_da_log_buf(trans, bp, 0, XFS_LBSIZE(mp) - 1);

	kmem_free(tmpbuffer, XFS_LBSIZE(mp));
}

/*
 * Redistribute the attribute list entries between two leaf nodes,
 * taking into account the size of the new entry.
 *
 * NOTE: if new block is empty, then it will get the upper half of the
 * old block.  At present, all (one) callers pass in an empty second block.
 *
 * This code adjusts the args->index/blkno and args->index2/blkno2 fields
 * to match what it is doing in splitting the attribute leaf block.  Those
 * values are used in "atomic rename" operations on attributes.  Note that
 * the "new" and "old" values can end up in different blocks.
 */
STATIC void
xfs_attr_leaf_rebalance(xfs_da_state_t *state, xfs_da_state_blk_t *blk1,
				       xfs_da_state_blk_t *blk2)
{
	xfs_da_args_t *args;
	xfs_da_state_blk_t *tmp_blk;
	xfs_attr_leafblock_t *leaf1, *leaf2;
	xfs_attr_leaf_hdr_t *hdr1, *hdr2;
	int count, totallen, max, space, swap;

	/*
	 * Set up environment.
	 */
	ASSERT(blk1->magic == XFS_ATTR_LEAF_MAGIC);
	ASSERT(blk2->magic == XFS_ATTR_LEAF_MAGIC);
	leaf1 = blk1->bp->data;
	leaf2 = blk2->bp->data;
	ASSERT(INT_GET(leaf1->hdr.info.magic, ARCH_CONVERT)
						== XFS_ATTR_LEAF_MAGIC);
	ASSERT(INT_GET(leaf2->hdr.info.magic, ARCH_CONVERT)
						== XFS_ATTR_LEAF_MAGIC);
	args = state->args;

	/*
	 * Check ordering of blocks, reverse if it makes things simpler.
	 *
	 * NOTE: Given that all (current) callers pass in an empty
	 * second block, this code should never set "swap".
	 */
	swap = 0;
	if (xfs_attr_leaf_order(blk1->bp, blk2->bp)) {
		tmp_blk = blk1;
		blk1 = blk2;
		blk2 = tmp_blk;
		leaf1 = blk1->bp->data;
		leaf2 = blk2->bp->data;
		swap = 1;
	}
	hdr1 = &leaf1->hdr;
	hdr2 = &leaf2->hdr;

	/*
	 * Examine entries until we reduce the absolute difference in
	 * byte usage between the two blocks to a minimum.  Then get
	 * the direction to copy and the number of elements to move.
	 *
	 * "inleaf" is true if the new entry should be inserted into blk1.
	 * If "swap" is also true, then reverse the sense of "inleaf".
	 */
	state->inleaf = xfs_attr_leaf_figure_balance(state, blk1, blk2,
							    &count, &totallen);
	if (swap)
		state->inleaf = !state->inleaf;

	/*
	 * Move any entries required from leaf to leaf:
	 */
	if (count < INT_GET(hdr1->count, ARCH_CONVERT)) {
		/*
		 * Figure the total bytes to be added to the destination leaf.
		 */
		/* number entries being moved */
		count = INT_GET(hdr1->count, ARCH_CONVERT) - count;
		space  = INT_GET(hdr1->usedbytes, ARCH_CONVERT) - totallen;
		space += count * sizeof(xfs_attr_leaf_entry_t);

		/*
		 * leaf2 is the destination, compact it if it looks tight.
		 */
		max  = INT_GET(hdr2->firstused, ARCH_CONVERT)
						- sizeof(xfs_attr_leaf_hdr_t);
		max -= INT_GET(hdr2->count, ARCH_CONVERT)
					* sizeof(xfs_attr_leaf_entry_t);
		if (space > max) {
			xfs_attr_leaf_compact(args->trans, blk2->bp);
		}

		/*
		 * Move high entries from leaf1 to low end of leaf2.
		 */
		xfs_attr_leaf_moveents(leaf1,
				INT_GET(hdr1->count, ARCH_CONVERT)-count,
				leaf2, 0, count, state->mp);

		xfs_da_log_buf(args->trans, blk1->bp, 0, state->blocksize-1);
		xfs_da_log_buf(args->trans, blk2->bp, 0, state->blocksize-1);
	} else if (count > INT_GET(hdr1->count, ARCH_CONVERT)) {
		/*
		 * I assert that since all callers pass in an empty
		 * second buffer, this code should never execute.
		 */

		/*
		 * Figure the total bytes to be added to the destination leaf.
		 */
		/* number entries being moved */
		count -= INT_GET(hdr1->count, ARCH_CONVERT);
		space  = totallen - INT_GET(hdr1->usedbytes, ARCH_CONVERT);
		space += count * sizeof(xfs_attr_leaf_entry_t);

		/*
		 * leaf1 is the destination, compact it if it looks tight.
		 */
		max  = INT_GET(hdr1->firstused, ARCH_CONVERT)
						- sizeof(xfs_attr_leaf_hdr_t);
		max -= INT_GET(hdr1->count, ARCH_CONVERT)
					* sizeof(xfs_attr_leaf_entry_t);
		if (space > max) {
			xfs_attr_leaf_compact(args->trans, blk1->bp);
		}

		/*
		 * Move low entries from leaf2 to high end of leaf1.
		 */
		xfs_attr_leaf_moveents(leaf2, 0, leaf1,
				(int)INT_GET(hdr1->count, ARCH_CONVERT), count,
				state->mp);

		xfs_da_log_buf(args->trans, blk1->bp, 0, state->blocksize-1);
		xfs_da_log_buf(args->trans, blk2->bp, 0, state->blocksize-1);
	}

	/*
	 * Copy out last hashval in each block for B-tree code.
	 */
	blk1->hashval =
	    INT_GET(leaf1->entries[INT_GET(leaf1->hdr.count,
				    ARCH_CONVERT)-1].hashval, ARCH_CONVERT);
	blk2->hashval =
	    INT_GET(leaf2->entries[INT_GET(leaf2->hdr.count,
				    ARCH_CONVERT)-1].hashval, ARCH_CONVERT);

	/*
	 * Adjust the expected index for insertion.
	 * NOTE: this code depends on the (current) situation that the
	 * second block was originally empty.
	 *
	 * If the insertion point moved to the 2nd block, we must adjust
	 * the index.  We must also track the entry just following the
	 * new entry for use in an "atomic rename" operation, that entry
	 * is always the "old" entry and the "new" entry is what we are
	 * inserting.  The index/blkno fields refer to the "old" entry,
	 * while the index2/blkno2 fields refer to the "new" entry.
	 */
	if (blk1->index > INT_GET(leaf1->hdr.count, ARCH_CONVERT)) {
		ASSERT(state->inleaf == 0);
		blk2->index = blk1->index
				- INT_GET(leaf1->hdr.count, ARCH_CONVERT);
		args->index = args->index2 = blk2->index;
		args->blkno = args->blkno2 = blk2->blkno;
	} else if (blk1->index == INT_GET(leaf1->hdr.count, ARCH_CONVERT)) {
		if (state->inleaf) {
			args->index = blk1->index;
			args->blkno = blk1->blkno;
			args->index2 = 0;
			args->blkno2 = blk2->blkno;
		} else {
			blk2->index = blk1->index
				    - INT_GET(leaf1->hdr.count, ARCH_CONVERT);
			args->index = args->index2 = blk2->index;
			args->blkno = args->blkno2 = blk2->blkno;
		}
	} else {
		ASSERT(state->inleaf == 1);
		args->index = args->index2 = blk1->index;
		args->blkno = args->blkno2 = blk1->blkno;
	}
}

/*
 * Examine entries until we reduce the absolute difference in
 * byte usage between the two blocks to a minimum.
 * GROT: Is this really necessary?  With other than a 512 byte blocksize,
 * GROT: there will always be enough room in either block for a new entry.
 * GROT: Do a double-split for this case?
 */
STATIC int
xfs_attr_leaf_figure_balance(xfs_da_state_t *state,
				    xfs_da_state_blk_t *blk1,
				    xfs_da_state_blk_t *blk2,
				    int *countarg, int *usedbytesarg)
{
	xfs_attr_leafblock_t *leaf1, *leaf2;
	xfs_attr_leaf_hdr_t *hdr1, *hdr2;
	xfs_attr_leaf_entry_t *entry;
	int count, max, index, totallen, half;
	int lastdelta, foundit, tmp;

	/*
	 * Set up environment.
	 */
	leaf1 = blk1->bp->data;
	leaf2 = blk2->bp->data;
	hdr1 = &leaf1->hdr;
	hdr2 = &leaf2->hdr;
	foundit = 0;
	totallen = 0;

	/*
	 * Examine entries until we reduce the absolute difference in
	 * byte usage between the two blocks to a minimum.
	 */
	max = INT_GET(hdr1->count, ARCH_CONVERT)
			+ INT_GET(hdr2->count, ARCH_CONVERT);
	half  = (max+1) * sizeof(*entry);
	half += INT_GET(hdr1->usedbytes, ARCH_CONVERT)
				+ INT_GET(hdr2->usedbytes, ARCH_CONVERT)
				+ xfs_attr_leaf_newentsize(state->args,
						     state->blocksize, NULL);
	half /= 2;
	lastdelta = state->blocksize;
	entry = &leaf1->entries[0];
	for (count = index = 0; count < max; entry++, index++, count++) {

#define XFS_ATTR_ABS(A)	(((A) < 0) ? -(A) : (A))
		/*
		 * The new entry is in the first block, account for it.
		 */
		if (count == blk1->index) {
			tmp = totallen + sizeof(*entry) +
				xfs_attr_leaf_newentsize(state->args,
							 state->blocksize,
							 NULL);
			if (XFS_ATTR_ABS(half - tmp) > lastdelta)
				break;
			lastdelta = XFS_ATTR_ABS(half - tmp);
			totallen = tmp;
			foundit = 1;
		}

		/*
		 * Wrap around into the second block if necessary.
		 */
		if (count == INT_GET(hdr1->count, ARCH_CONVERT)) {
			leaf1 = leaf2;
			entry = &leaf1->entries[0];
			index = 0;
		}

		/*
		 * Figure out if next leaf entry would be too much.
		 */
		tmp = totallen + sizeof(*entry) + xfs_attr_leaf_entsize(leaf1,
									index);
		if (XFS_ATTR_ABS(half - tmp) > lastdelta)
			break;
		lastdelta = XFS_ATTR_ABS(half - tmp);
		totallen = tmp;
#undef XFS_ATTR_ABS
	}

	/*
	 * Calculate the number of usedbytes that will end up in lower block.
	 * If new entry not in lower block, fix up the count.
	 */
	totallen -= count * sizeof(*entry);
	if (foundit) {
		totallen -= sizeof(*entry) + 
				xfs_attr_leaf_newentsize(state->args,
							 state->blocksize,
							 NULL);
	}

	*countarg = count;
	*usedbytesarg = totallen;
	return(foundit);
}

/*========================================================================
 * Routines used for shrinking the Btree.
 *========================================================================*/

/*
 * Check a leaf block and its neighbors to see if the block should be
 * collapsed into one or the other neighbor.  Always keep the block
 * with the smaller block number.
 * If the current block is over 50% full, don't try to join it, return 0.
 * If the block is empty, fill in the state structure and return 2.
 * If it can be collapsed, fill in the state structure and return 1.
 * If nothing can be done, return 0.
 *
 * GROT: allow for INCOMPLETE entries in calculation.
 */
int
xfs_attr_leaf_toosmall(xfs_da_state_t *state, int *action)
{
	xfs_attr_leafblock_t *leaf;
	xfs_da_state_blk_t *blk;
	xfs_da_blkinfo_t *info;
	int count, bytes, forward, error, retval, i;
	xfs_dablk_t blkno;
	xfs_dabuf_t *bp;

	/*
	 * Check for the degenerate case of the block being over 50% full.
	 * If so, it's not worth even looking to see if we might be able
	 * to coalesce with a sibling.
	 */
	blk = &state->path.blk[ state->path.active-1 ];
	info = blk->bp->data;
	ASSERT(INT_GET(info->magic, ARCH_CONVERT) == XFS_ATTR_LEAF_MAGIC);
	leaf = (xfs_attr_leafblock_t *)info;
	count = INT_GET(leaf->hdr.count, ARCH_CONVERT);
	bytes = sizeof(xfs_attr_leaf_hdr_t) +
		count * sizeof(xfs_attr_leaf_entry_t) +
		INT_GET(leaf->hdr.usedbytes, ARCH_CONVERT);
	if (bytes > (state->blocksize >> 1)) {
		*action = 0;	/* blk over 50%, dont try to join */
		return(0);
	}

	/*
	 * Check for the degenerate case of the block being empty.
	 * If the block is empty, we'll simply delete it, no need to
	 * coalesce it with a sibling block.  We choose (aribtrarily)
	 * to merge with the forward block unless it is NULL.
	 */
	if (count == 0) {
		/*
		 * Make altpath point to the block we want to keep and
		 * path point to the block we want to drop (this one).
		 */
		forward = (INT_GET(info->forw, ARCH_CONVERT) != 0);
		bcopy(&state->path, &state->altpath, sizeof(state->path));
		error = xfs_da_path_shift(state, &state->altpath, forward,
						 0, &retval);
		if (error)
			return(error);
		if (retval) {
			*action = 0;
		} else {
			*action = 2;
		}
		return(0);
	}

	/*
	 * Examine each sibling block to see if we can coalesce with
	 * at least 25% free space to spare.  We need to figure out
	 * whether to merge with the forward or the backward block.
	 * We prefer coalescing with the lower numbered sibling so as
	 * to shrink an attribute list over time.
	 */
	/* start with smaller blk num */
	forward = (INT_GET(info->forw, ARCH_CONVERT)
					< INT_GET(info->back, ARCH_CONVERT));
	for (i = 0; i < 2; forward = !forward, i++) {
		if (forward)
			blkno = INT_GET(info->forw, ARCH_CONVERT);
		else
			blkno = INT_GET(info->back, ARCH_CONVERT);
		if (blkno == 0)
			continue;
		error = xfs_da_read_buf(state->args->trans, state->args->dp,
					blkno, -1, &bp, XFS_ATTR_FORK);
		if (error)
			return(error);
		ASSERT(bp != NULL);

		leaf = (xfs_attr_leafblock_t *)info;
		count  = INT_GET(leaf->hdr.count, ARCH_CONVERT);
		bytes  = state->blocksize - (state->blocksize>>2);
		bytes -= INT_GET(leaf->hdr.usedbytes, ARCH_CONVERT);
		leaf = bp->data;
		ASSERT(INT_GET(leaf->hdr.info.magic, ARCH_CONVERT)
						== XFS_ATTR_LEAF_MAGIC);
		count += INT_GET(leaf->hdr.count, ARCH_CONVERT);
		bytes -= INT_GET(leaf->hdr.usedbytes, ARCH_CONVERT);
		bytes -= count * sizeof(xfs_attr_leaf_entry_t);
		bytes -= sizeof(xfs_attr_leaf_hdr_t);
		xfs_da_brelse(state->args->trans, bp);
		if (bytes >= 0)
			break;	/* fits with at least 25% to spare */
	}
	if (i >= 2) {
		*action = 0;
		return(0);
	}

	/*
	 * Make altpath point to the block we want to keep (the lower
	 * numbered block) and path point to the block we want to drop.
	 */
	bcopy(&state->path, &state->altpath, sizeof(state->path));
	if (blkno < blk->blkno) {
		error = xfs_da_path_shift(state, &state->altpath, forward,
						 0, &retval);
	} else {
		error = xfs_da_path_shift(state, &state->path, forward,
						 0, &retval);
	}
	if (error)
		return(error);
	if (retval) {
		*action = 0;
	} else {
		*action = 1;
	}
	return(0);
}

/*
 * Move all the attribute list entries from drop_leaf into save_leaf.
 */
void
xfs_attr_leaf_unbalance(xfs_da_state_t *state, xfs_da_state_blk_t *drop_blk,
				       xfs_da_state_blk_t *save_blk)
{
	xfs_attr_leafblock_t *drop_leaf, *save_leaf, *tmp_leaf;
	xfs_attr_leaf_hdr_t *drop_hdr, *save_hdr, *tmp_hdr;
	xfs_mount_t *mp;
	char *tmpbuffer;

	/*
	 * Set up environment.
	 */
	mp = state->mp;
	ASSERT(drop_blk->magic == XFS_ATTR_LEAF_MAGIC);
	ASSERT(save_blk->magic == XFS_ATTR_LEAF_MAGIC);
	drop_leaf = drop_blk->bp->data;
	save_leaf = save_blk->bp->data;
	ASSERT(INT_GET(drop_leaf->hdr.info.magic, ARCH_CONVERT)
						== XFS_ATTR_LEAF_MAGIC);
	ASSERT(INT_GET(save_leaf->hdr.info.magic, ARCH_CONVERT)
						== XFS_ATTR_LEAF_MAGIC);
	drop_hdr = &drop_leaf->hdr;
	save_hdr = &save_leaf->hdr;

	/*
	 * Save last hashval from dying block for later Btree fixup.
	 */
	drop_blk->hashval =
		INT_GET(drop_leaf->entries[INT_GET(drop_leaf->hdr.count,
						ARCH_CONVERT)-1].hashval,
								ARCH_CONVERT);

	/*
	 * Check if we need a temp buffer, or can we do it in place.
	 * Note that we don't check "leaf" for holes because we will
	 * always be dropping it, toosmall() decided that for us already.
	 */
	if (save_hdr->holes == 0) {
		/*
		 * dest leaf has no holes, so we add there.  May need
		 * to make some room in the entry array.
		 */
		if (xfs_attr_leaf_order(save_blk->bp, drop_blk->bp)) {
			xfs_attr_leaf_moveents(drop_leaf, 0, save_leaf, 0,
			     (int)INT_GET(drop_hdr->count, ARCH_CONVERT), mp);
		} else {
			xfs_attr_leaf_moveents(drop_leaf, 0, save_leaf,
				  INT_GET(save_hdr->count, ARCH_CONVERT),
				  (int)INT_GET(drop_hdr->count, ARCH_CONVERT),
				  mp);
		}
	} else {
		/*
		 * Destination has holes, so we make a temporary copy
		 * of the leaf and add them both to that.
		 */
		tmpbuffer = kmem_alloc(state->blocksize, KM_SLEEP);
		ASSERT(tmpbuffer != NULL);
		bzero(tmpbuffer, state->blocksize);
		tmp_leaf = (xfs_attr_leafblock_t *)tmpbuffer;
		tmp_hdr = &tmp_leaf->hdr;
		tmp_hdr->info = save_hdr->info;	/* struct copy */
		INT_SET(tmp_hdr->count, ARCH_CONVERT, 0);
		INT_SET(tmp_hdr->firstused, ARCH_CONVERT, state->blocksize);
		if (INT_GET(tmp_hdr->firstused, ARCH_CONVERT) == 0) {
			INT_SET(tmp_hdr->firstused, ARCH_CONVERT,
				state->blocksize - XFS_ATTR_LEAF_NAME_ALIGN);
		}
		INT_SET(tmp_hdr->usedbytes, ARCH_CONVERT, 0);
		if (xfs_attr_leaf_order(save_blk->bp, drop_blk->bp)) {
			xfs_attr_leaf_moveents(drop_leaf, 0, tmp_leaf, 0,
				(int)INT_GET(drop_hdr->count, ARCH_CONVERT),
				mp);
			xfs_attr_leaf_moveents(save_leaf, 0, tmp_leaf,
				  INT_GET(tmp_leaf->hdr.count, ARCH_CONVERT),
				 (int)INT_GET(save_hdr->count, ARCH_CONVERT),
				 mp);
		} else {
			xfs_attr_leaf_moveents(save_leaf, 0, tmp_leaf, 0,
				(int)INT_GET(save_hdr->count, ARCH_CONVERT),
				mp);
			xfs_attr_leaf_moveents(drop_leaf, 0, tmp_leaf,
				INT_GET(tmp_leaf->hdr.count, ARCH_CONVERT),
				(int)INT_GET(drop_hdr->count, ARCH_CONVERT),
				mp);
		}
		bcopy((char *)tmp_leaf, (char *)save_leaf, state->blocksize);
		kmem_free(tmpbuffer, state->blocksize);
	}

	xfs_da_log_buf(state->args->trans, save_blk->bp, 0,
					   state->blocksize - 1);

	/*
	 * Copy out last hashval in each block for B-tree code.
	 */
	save_blk->hashval =
		INT_GET(save_leaf->entries[INT_GET(save_leaf->hdr.count,
						ARCH_CONVERT)-1].hashval,
								ARCH_CONVERT);
}


/*========================================================================
 * Utility routines.
 *========================================================================*/

/*
 * Move the indicated entries from one leaf to another.
 * NOTE: this routine modifies both source and destination leaves.
 */
/*ARGSUSED*/
STATIC void
xfs_attr_leaf_moveents(xfs_attr_leafblock_t *leaf_s, int start_s,
			xfs_attr_leafblock_t *leaf_d, int start_d,
			int count, xfs_mount_t *mp)
{
	xfs_attr_leaf_hdr_t *hdr_s, *hdr_d;
	xfs_attr_leaf_entry_t *entry_s, *entry_d;
	int desti, tmp, i;

	/*
	 * Check for nothing to do.
	 */
	if (count == 0)
		return;

	/*
	 * Set up environment.
	 */
	ASSERT(INT_GET(leaf_s->hdr.info.magic, ARCH_CONVERT)
						== XFS_ATTR_LEAF_MAGIC);
	ASSERT(INT_GET(leaf_d->hdr.info.magic, ARCH_CONVERT)
						== XFS_ATTR_LEAF_MAGIC);
	hdr_s = &leaf_s->hdr;
	hdr_d = &leaf_d->hdr;
	ASSERT((INT_GET(hdr_s->count, ARCH_CONVERT) > 0)
				&& (INT_GET(hdr_s->count, ARCH_CONVERT)
						< (XFS_LBSIZE(mp)/8)));
	ASSERT(INT_GET(hdr_s->firstused, ARCH_CONVERT) >= 
		((INT_GET(hdr_s->count, ARCH_CONVERT)
					* sizeof(*entry_s))+sizeof(*hdr_s)));
	ASSERT(INT_GET(hdr_d->count, ARCH_CONVERT) < (XFS_LBSIZE(mp)/8));
	ASSERT(INT_GET(hdr_d->firstused, ARCH_CONVERT) >= 
		((INT_GET(hdr_d->count, ARCH_CONVERT)
					* sizeof(*entry_d))+sizeof(*hdr_d)));

	ASSERT(start_s < INT_GET(hdr_s->count, ARCH_CONVERT));
	ASSERT(start_d <= INT_GET(hdr_d->count, ARCH_CONVERT));
	ASSERT(count <= INT_GET(hdr_s->count, ARCH_CONVERT));

	/*
	 * Move the entries in the destination leaf up to make a hole?
	 */
	if (start_d < INT_GET(hdr_d->count, ARCH_CONVERT)) {
		tmp  = INT_GET(hdr_d->count, ARCH_CONVERT) - start_d;
		tmp *= sizeof(xfs_attr_leaf_entry_t);
		entry_s = &leaf_d->entries[start_d];
		entry_d = &leaf_d->entries[start_d + count];
		ovbcopy((char *)entry_s, (char *)entry_d, tmp);
	}

	/*
	 * Copy all entry's in the same (sorted) order,
	 * but allocate attribute info packed and in sequence.
	 */
	entry_s = &leaf_s->entries[start_s];
	entry_d = &leaf_d->entries[start_d];
	desti = start_d;
	for (i = 0; i < count; entry_s++, entry_d++, desti++, i++) {
		ASSERT(INT_GET(entry_s->nameidx, ARCH_CONVERT)
				>= INT_GET(hdr_s->firstused, ARCH_CONVERT));
		tmp = xfs_attr_leaf_entsize(leaf_s, start_s + i);
#ifdef GROT
		/*
		 * Code to drop INCOMPLETE entries.  Difficult to use as we
		 * may also need to change the insertion index.  Code turned
		 * off for 6.2, should be revisited later.
		 */
		if (entry_s->flags & XFS_ATTR_INCOMPLETE) { /* skip partials? */
			bzero(XFS_ATTR_LEAF_NAME(leaf_s, start_s + i), tmp);
			INT_MOD(hdr_s->usedbytes, ARCH_CONVERT, -tmp);
			INT_MOD(hdr_s->count, ARCH_CONVERT, -1);
			entry_d--;	/* to compensate for ++ in loop hdr */
			desti--;
			if ((start_s + i) < offset)
				result++;	/* insertion index adjustment */
		} else {
#endif /* GROT */
			INT_MOD(hdr_d->firstused, ARCH_CONVERT, -tmp);
			INT_SET(entry_d->hashval, ARCH_CONVERT,
				    INT_GET(entry_s->hashval, ARCH_CONVERT));
			INT_SET(entry_d->nameidx, ARCH_CONVERT,
						INT_GET(hdr_d->firstused,
								ARCH_CONVERT));
			entry_d->flags = entry_s->flags;
			ASSERT(INT_GET(entry_d->nameidx, ARCH_CONVERT) + tmp
							<= XFS_LBSIZE(mp));
			ovbcopy(XFS_ATTR_LEAF_NAME(leaf_s, start_s + i),
			      XFS_ATTR_LEAF_NAME(leaf_d, desti), tmp);
			ASSERT(INT_GET(entry_s->nameidx, ARCH_CONVERT) + tmp
							<= XFS_LBSIZE(mp));
			bzero(XFS_ATTR_LEAF_NAME(leaf_s, start_s + i), tmp);
			INT_MOD(hdr_s->usedbytes, ARCH_CONVERT, -tmp);
			INT_MOD(hdr_d->usedbytes, ARCH_CONVERT, tmp);
			INT_MOD(hdr_s->count, ARCH_CONVERT, -1);
			INT_MOD(hdr_d->count, ARCH_CONVERT, 1);
			tmp = INT_GET(hdr_d->count, ARCH_CONVERT)
						* sizeof(xfs_attr_leaf_entry_t)
						+ sizeof(xfs_attr_leaf_hdr_t);
			ASSERT(INT_GET(hdr_d->firstused, ARCH_CONVERT) >= tmp);
#ifdef GROT
		}
#endif /* GROT */
	}

	/*
	 * Zero out the entries we just copied.
	 */
	if (start_s == INT_GET(hdr_s->count, ARCH_CONVERT)) {
		tmp = count * sizeof(xfs_attr_leaf_entry_t);
		entry_s = &leaf_s->entries[start_s];
		ASSERT(((char *)entry_s + tmp) <=
		       ((char *)leaf_s + XFS_LBSIZE(mp)));
		bzero((char *)entry_s, tmp);
	} else {
		/*
		 * Move the remaining entries down to fill the hole,
		 * then zero the entries at the top.
		 */
		tmp  = INT_GET(hdr_s->count, ARCH_CONVERT) - count;
		tmp *= sizeof(xfs_attr_leaf_entry_t);
		entry_s = &leaf_s->entries[start_s + count];
		entry_d = &leaf_s->entries[start_s];
		ovbcopy((char *)entry_s, (char *)entry_d, tmp);

		tmp = count * sizeof(xfs_attr_leaf_entry_t);
		entry_s = &leaf_s->entries[INT_GET(hdr_s->count,
							ARCH_CONVERT)];
		ASSERT(((char *)entry_s + tmp) <=
		       ((char *)leaf_s + XFS_LBSIZE(mp)));
		bzero((char *)entry_s, tmp);
	}

	/*
	 * Fill in the freemap information
	 */
	INT_SET(hdr_d->freemap[0].base, ARCH_CONVERT,
					sizeof(xfs_attr_leaf_hdr_t));
	INT_MOD(hdr_d->freemap[0].base, ARCH_CONVERT,
				INT_GET(hdr_d->count, ARCH_CONVERT)
					* sizeof(xfs_attr_leaf_entry_t));
	INT_SET(hdr_d->freemap[0].size, ARCH_CONVERT,
				INT_GET(hdr_d->firstused, ARCH_CONVERT)
			      - INT_GET(hdr_d->freemap[0].base, ARCH_CONVERT));
	INT_SET(hdr_d->freemap[1].base, ARCH_CONVERT, 0);
	INT_SET(hdr_d->freemap[2].base, ARCH_CONVERT, 0);
	INT_SET(hdr_d->freemap[1].size, ARCH_CONVERT, 0);
	INT_SET(hdr_d->freemap[2].size, ARCH_CONVERT, 0);
	hdr_s->holes = 1;	/* leaf may not be compact */
}

/*
 * Compare two leaf blocks "order".
 * Return 0 unless leaf2 should go before leaf1.
 */
int
xfs_attr_leaf_order(xfs_dabuf_t *leaf1_bp, xfs_dabuf_t *leaf2_bp)
{
	xfs_attr_leafblock_t *leaf1, *leaf2;

	leaf1 = leaf1_bp->data;
	leaf2 = leaf2_bp->data;
	ASSERT((INT_GET(leaf1->hdr.info.magic, ARCH_CONVERT)
						== XFS_ATTR_LEAF_MAGIC) &&
	       (INT_GET(leaf2->hdr.info.magic, ARCH_CONVERT)
						== XFS_ATTR_LEAF_MAGIC));
	if (   (INT_GET(leaf1->hdr.count, ARCH_CONVERT) > 0)
	    && (INT_GET(leaf2->hdr.count, ARCH_CONVERT) > 0)
	    && (   (INT_GET(leaf2->entries[ 0 ].hashval, ARCH_CONVERT) <
		      INT_GET(leaf1->entries[ 0 ].hashval, ARCH_CONVERT))
	        || (INT_GET(leaf2->entries[INT_GET(leaf2->hdr.count,
				ARCH_CONVERT)-1].hashval, ARCH_CONVERT) <
		      INT_GET(leaf1->entries[INT_GET(leaf1->hdr.count,
				ARCH_CONVERT)-1].hashval, ARCH_CONVERT))) ) {
		return(1);
	}
	return(0);
}

/*
 * Pick up the last hashvalue from a leaf block.
 */
xfs_dahash_t
xfs_attr_leaf_lasthash(xfs_dabuf_t *bp, int *count)
{
	xfs_attr_leafblock_t *leaf;

	leaf = bp->data;
	ASSERT(INT_GET(leaf->hdr.info.magic, ARCH_CONVERT)
						== XFS_ATTR_LEAF_MAGIC);
	if (count)
		*count = INT_GET(leaf->hdr.count, ARCH_CONVERT);
	if (INT_GET(leaf->hdr.count, ARCH_CONVERT) == 0)
		return(0);
	return(INT_GET(leaf->entries[INT_GET(leaf->hdr.count,
				ARCH_CONVERT)-1].hashval, ARCH_CONVERT));
}

/*
 * Calculate the number of bytes used to store the indicated attribute
 * (whether local or remote only calculate bytes in this block).
 */
int
xfs_attr_leaf_entsize(xfs_attr_leafblock_t *leaf, int index)
{
	xfs_attr_leaf_name_local_t *name_loc;
	xfs_attr_leaf_name_remote_t *name_rmt;
	int size;

	ASSERT(INT_GET(leaf->hdr.info.magic, ARCH_CONVERT)
						== XFS_ATTR_LEAF_MAGIC);
	if (leaf->entries[index].flags & XFS_ATTR_LOCAL) {
		name_loc = XFS_ATTR_LEAF_NAME_LOCAL(leaf, index);
		size = XFS_ATTR_LEAF_ENTSIZE_LOCAL(name_loc->namelen,
						   INT_GET(name_loc->valuelen,
								ARCH_CONVERT));
	} else {
		name_rmt = XFS_ATTR_LEAF_NAME_REMOTE(leaf, index);
		size = XFS_ATTR_LEAF_ENTSIZE_REMOTE(name_rmt->namelen);
	}
	return(size);
}

/*
 * Calculate the number of bytes that would be required to store the new
 * attribute (whether local or remote only calculate bytes in this block).
 * This routine decides as a side effect whether the attribute will be
 * a "local" or a "remote" attribute.
 */
int
xfs_attr_leaf_newentsize(xfs_da_args_t *args, int blocksize, int *local)
{
	int size;

	size = XFS_ATTR_LEAF_ENTSIZE_LOCAL(args->namelen, args->valuelen);
	if (size < XFS_ATTR_LEAF_ENTSIZE_LOCAL_MAX(blocksize)) { 
		if (local) {
			*local = 1;
		}
	} else {
		size = XFS_ATTR_LEAF_ENTSIZE_REMOTE(args->namelen);
		if (local) {
			*local = 0;
		}
	}
	return(size);
}
