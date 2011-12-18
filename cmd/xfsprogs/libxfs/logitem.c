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

xfs_zone_t	*xfs_buf_item_zone;
xfs_zone_t	*xfs_ili_zone;		/* inode log item zone */


/*
 * This is called to add the given log item to the transaction's
 * list of log items.  It must find a free log item descriptor
 * or allocate a new one and add the item to that descriptor.
 * The function returns a pointer to item descriptor used to point
 * to the new item.  The log item will now point to its new descriptor
 * with its li_desc field.
 */
xfs_log_item_desc_t *
xfs_trans_add_item(xfs_trans_t *tp, xfs_log_item_t *lip)
{
	xfs_log_item_desc_t	*lidp;
	xfs_log_item_chunk_t	*licp;
	int			i;

	/*
	 * If there are no free descriptors, allocate a new chunk
	 * of them and put it at the front of the chunk list.
	 */
	if (tp->t_items_free == 0) {
		licp = (xfs_log_item_chunk_t*)
		       kmem_alloc(sizeof(xfs_log_item_chunk_t), KM_SLEEP);
		ASSERT(licp != NULL);
		/*
		 * Initialize the chunk, and then
		 * claim the first slot in the newly allocated chunk.
		 */
		XFS_LIC_INIT(licp);
		XFS_LIC_CLAIM(licp, 0);
		licp->lic_unused = 1;
		XFS_LIC_INIT_SLOT(licp, 0);
		lidp = XFS_LIC_SLOT(licp, 0);

		/*
		 * Link in the new chunk and update the free count.
		 */
		licp->lic_next = tp->t_items.lic_next;
		tp->t_items.lic_next = licp;
		tp->t_items_free = XFS_LIC_NUM_SLOTS - 1;

		/*
		 * Initialize the descriptor and the generic portion
		 * of the log item.
		 *
		 * Point the new slot at this item and return it.
		 * Also point the log item at its currently active
		 * descriptor and set the item's mount pointer.
		 */
		lidp->lid_item = lip;
		lidp->lid_flags = 0;
		lidp->lid_size = 0;
		lip->li_desc = lidp;
		lip->li_mountp = tp->t_mountp;
		return (lidp);
	}

	/*
	 * Find the free descriptor. It is somewhere in the chunklist
	 * of descriptors.
	 */
	licp = &tp->t_items;
	while (licp != NULL) {
		if (XFS_LIC_VACANCY(licp)) {
			if (licp->lic_unused <= XFS_LIC_MAX_SLOT) {
				i = licp->lic_unused;
				ASSERT(XFS_LIC_ISFREE(licp, i));
				break;
			}
			for (i = 0; i <= XFS_LIC_MAX_SLOT; i++) {
				if (XFS_LIC_ISFREE(licp, i))
					break;
			}
			ASSERT(i <= XFS_LIC_MAX_SLOT);
			break;
		}
		licp = licp->lic_next;
	}
	ASSERT(licp != NULL);
	/*
	 * If we find a free descriptor, claim it,
	 * initialize it, and return it.
	 */
	XFS_LIC_CLAIM(licp, i);
	if (licp->lic_unused <= i) {
		licp->lic_unused = i + 1;
		XFS_LIC_INIT_SLOT(licp, i);
	}
	lidp = XFS_LIC_SLOT(licp, i);
	tp->t_items_free--;
	lidp->lid_item = lip;
	lidp->lid_flags = 0;
	lidp->lid_size = 0;
	lip->li_desc = lidp;
	lip->li_mountp = tp->t_mountp;
	return (lidp);
}

/*
 * Free the given descriptor.
 * 
 * This requires setting the bit in the chunk's free mask corresponding
 * to the given slot.
 */
void
xfs_trans_free_item(xfs_trans_t	*tp, xfs_log_item_desc_t *lidp)
{
	uint			slot;
	xfs_log_item_chunk_t	*licp;
	xfs_log_item_chunk_t	**licpp;

	slot = XFS_LIC_DESC_TO_SLOT(lidp);
	licp = XFS_LIC_DESC_TO_CHUNK(lidp);
	XFS_LIC_RELSE(licp, slot);
	lidp->lid_item->li_desc = NULL;
	tp->t_items_free++;

	/*
	 * If there are no more used items in the chunk and this is not
	 * the chunk embedded in the transaction structure, then free
	 * the chunk. First pull it from the chunk list and then
	 * free it back to the heap.  We didn't bother with a doubly
	 * linked list here because the lists should be very short
	 * and this is not a performance path.  It's better to save
	 * the memory of the extra pointer.
	 *
	 * Also decrement the transaction structure's count of free items
	 * by the number in a chunk since we are freeing an empty chunk.
	 */
	if (XFS_LIC_ARE_ALL_FREE(licp) && (licp != &(tp->t_items))) {
		licpp = &(tp->t_items.lic_next);
		while (*licpp != licp) {
			ASSERT(*licpp != NULL);
			licpp = &((*licpp)->lic_next);
		}
		*licpp = licp->lic_next;
		kmem_free(licp, sizeof(xfs_log_item_chunk_t));
		tp->t_items_free -= XFS_LIC_NUM_SLOTS;
	}
}

/*
 * This is called to find the descriptor corresponding to the given
 * log item.  It returns a pointer to the descriptor.
 * The log item MUST have a corresponding descriptor in the given
 * transaction.  This routine does not return NULL, it panics.
 *
 * The descriptor pointer is kept in the log item's li_desc field.
 * Just return it.
 */
xfs_log_item_desc_t *
xfs_trans_find_item(xfs_trans_t	*tp, xfs_log_item_t *lip)
{
	ASSERT(lip->li_desc != NULL);

	return (lip->li_desc);
}

/*
 * This is called to unlock all of the items of a transaction and to free
 * all the descriptors of that transaction.
 *
 * It walks the list of descriptors and unlocks each item.  It frees
 * each chunk except that embedded in the transaction as it goes along.
 */
void
xfs_trans_free_items(
	xfs_trans_t	*tp,
	int		flags)
{
	xfs_log_item_chunk_t	*licp;
	xfs_log_item_chunk_t	*next_licp;
	int			abort;

	abort = flags & XFS_TRANS_ABORT;
	licp = &tp->t_items;
	/*
	 * Special case the embedded chunk so we don't free it below.
	 */
	if (!XFS_LIC_ARE_ALL_FREE(licp)) {
		(void) xfs_trans_unlock_chunk(licp, 1, abort, NULLCOMMITLSN);
		XFS_LIC_ALL_FREE(licp);
		licp->lic_unused = 0;
	}
	licp = licp->lic_next;

	/*
	 * Unlock each item in each chunk and free the chunks.
	 */
	while (licp != NULL) {
		ASSERT(!XFS_LIC_ARE_ALL_FREE(licp));
		(void) xfs_trans_unlock_chunk(licp, 1, abort, NULLCOMMITLSN);
		next_licp = licp->lic_next;
		kmem_free(licp, sizeof(xfs_log_item_chunk_t));
		licp = next_licp;
	}

	/*
	 * Reset the transaction structure's free item count.
	 */
	tp->t_items_free = XFS_LIC_NUM_SLOTS;
	tp->t_items.lic_next = NULL;
}

/*
 * Check to see if a buffer matching the given parameters is already
 * a part of the given transaction.  Only check the first, embedded
 * chunk, since we don't want to spend all day scanning large transactions.
 */
STATIC xfs_buf_t *
xfs_trans_buf_item_match(
	xfs_trans_t	*tp,
	buftarg_t	*target,
	xfs_daddr_t	blkno,
	int		len)
{
	xfs_log_item_chunk_t	*licp;
	xfs_log_item_desc_t	*lidp;
	xfs_buf_log_item_t	*blip;
	xfs_buf_t			*bp;
	int			i;

#ifdef LI_DEBUG
	fprintf(stderr, "buf_item_match (fast) log items for xact %p\n", tp);
#endif

	bp = NULL;
	len = BBTOB(len);
	licp = &tp->t_items;
	if (!XFS_LIC_ARE_ALL_FREE(licp)) {
		for (i = 0; i < licp->lic_unused; i++) {
			/*
			 * Skip unoccupied slots.
			 */
			if (XFS_LIC_ISFREE(licp, i)) {
				continue;
			}

			lidp = XFS_LIC_SLOT(licp, i);
			blip = (xfs_buf_log_item_t *)lidp->lid_item;
#ifdef LI_DEBUG
			fprintf(stderr,
				"\tfound log item, xact %p, blip=%p (%d/%d)\n",
				tp, blip, i, licp->lic_unused);
#endif
			if (blip->bli_item.li_type != XFS_LI_BUF) {
				continue;
			}

			bp = blip->bli_buf;
#ifdef LI_DEBUG
			fprintf(stderr,
			"\tfound buf %p log item, xact %p, blip=%p (%d)\n",
				bp, tp, blip, i);
#endif
			if ((XFS_BUF_TARGET(bp) == target->dev) &&
			    (XFS_BUF_ADDR(bp) == blkno) &&
			    (XFS_BUF_COUNT(bp) == len)) {
				/*
				 * We found it.  Break out and
				 * return the pointer to the buffer.
				 */
#ifdef LI_DEBUG
				fprintf(stderr,
					"\tfound REAL buf log item, bp=%p\n",
					bp);
#endif
				break;
			} else {
				bp = NULL;
			}
		}
	}
#ifdef LI_DEBUG
	if (!bp) fprintf(stderr, "\tfast search - got nothing\n");
#endif
	return bp;
}

/*
 * Check to see if a buffer matching the given parameters is already
 * a part of the given transaction.  Check all the chunks, we
 * want to be thorough.
 */
STATIC xfs_buf_t *
xfs_trans_buf_item_match_all(
	xfs_trans_t	*tp,
	buftarg_t	*target,
	xfs_daddr_t	blkno,
	int		len)
{
	xfs_log_item_chunk_t	*licp;
	xfs_log_item_desc_t	*lidp;
	xfs_buf_log_item_t	*blip;
	xfs_buf_t			*bp;
	int			i;

#ifdef LI_DEBUG
	fprintf(stderr, "buf_item_match_all (slow) log items for xact %p\n",
		tp);
#endif

	bp = NULL;
	len = BBTOB(len);
	for (licp = &tp->t_items; licp != NULL; licp = licp->lic_next) {
		if (XFS_LIC_ARE_ALL_FREE(licp)) {
			ASSERT(licp == &tp->t_items);
			ASSERT(licp->lic_next == NULL);
			return NULL;
		}
		for (i = 0; i < licp->lic_unused; i++) {
			/*
			 * Skip unoccupied slots.
			 */
			if (XFS_LIC_ISFREE(licp, i)) {
				continue;
			}

			lidp = XFS_LIC_SLOT(licp, i);
			blip = (xfs_buf_log_item_t *)lidp->lid_item;
#ifdef LI_DEBUG
			fprintf(stderr,
				"\tfound log item, xact %p, blip=%p (%d/%d)\n",
				tp, blip, i, licp->lic_unused);
#endif
			if (blip->bli_item.li_type != XFS_LI_BUF) {
				continue;
			}

			bp = blip->bli_buf;
			ASSERT(bp);
			ASSERT(XFS_BUF_ADDR(bp));
#ifdef LI_DEBUG
			fprintf(stderr,
			"\tfound buf %p log item, xact %p, blip=%p (%d)\n",
				bp, tp, blip, i);
#endif
			if ((XFS_BUF_TARGET(bp) == target->dev) &&
			    (XFS_BUF_ADDR(bp) == blkno) &&
			    (XFS_BUF_COUNT(bp) == len)) {
				/*
				 * We found it.  Break out and
				 * return the pointer to the buffer.
				 */
#ifdef LI_DEBUG
				fprintf(stderr,
					"\tfound REAL buf log item, bp=%p\n",
					bp);
#endif
				return bp;
			}
		}
	}
#ifdef LI_DEBUG
	if (!bp) fprintf(stderr, "slow search - got nothing\n");
#endif
	return NULL;
}

/*
 * Allocate a new buf log item to go with the given buffer.
 * Set the buffer's b_fsprivate field to point to the new
 * buf log item.  If there are other item's attached to the
 * buffer (see xfs_buf_attach_iodone() below), then put the
 * buf log item at the front.
 */
void
xfs_buf_item_init(
	xfs_buf_t	*bp,
	xfs_mount_t	*mp)
{
	xfs_log_item_t		*lip;
	xfs_buf_log_item_t	*bip;

#ifdef LI_DEBUG
	fprintf(stderr, "buf_item_init for buffer %p\n", bp);
#endif

	/*
	 * Check to see if there is already a buf log item for
	 * this buffer.  If there is, it is guaranteed to be
	 * the first.  If we do already have one, there is
	 * nothing to do here so return.
	 */
	if (XFS_BUF_FSPRIVATE3(bp, xfs_mount_t *) != mp)
		XFS_BUF_SET_FSPRIVATE3(bp, mp);
	XFS_BUF_SET_BDSTRAT_FUNC(bp, xfs_bdstrat_cb);
	if (XFS_BUF_FSPRIVATE(bp, void *) != NULL) {
		lip = XFS_BUF_FSPRIVATE(bp, xfs_log_item_t *);
		if (lip->li_type == XFS_LI_BUF) {
#ifdef LI_DEBUG
			fprintf(stderr,
				"reused buf item %p for pre-logged buffer %p\n",
				lip, bp);
#endif
			return;
		}
	}

	bip = (xfs_buf_log_item_t *)kmem_zone_zalloc(xfs_buf_item_zone,
						    KM_SLEEP);
#ifdef LI_DEBUG
	fprintf(stderr, "adding buf item %p for not-logged buffer %p\n",
		bip, bp);
#endif
	bip->bli_item.li_type = XFS_LI_BUF;
	bip->bli_item.li_mountp = mp;
	bip->bli_buf = bp;
	bip->bli_format.blf_type = XFS_LI_BUF;
	bip->bli_format.blf_blkno = (__int64_t)XFS_BUF_ADDR(bp);
	bip->bli_format.blf_len = (ushort)BTOBB(XFS_BUF_COUNT(bp));
	XFS_BUF_SET_FSPRIVATE(bp, bip);
}


/*
 * Mark bytes first through last inclusive as dirty in the buf
 * item's bitmap.
 */
void
xfs_buf_item_log(
	xfs_buf_log_item_t	*bip,
	uint			first,
	uint			last)
{
	/*
	 * Mark the item as having some dirty data for
	 * quick reference in xfs_buf_item_dirty.
	 */
	bip->bli_flags |= XFS_BLI_DIRTY;
}

/*
 * Initialize the inode log item for a newly allocated (in-core) inode.
 */
void
xfs_inode_item_init(
	xfs_inode_t	*ip,
	xfs_mount_t	*mp)
{
	xfs_inode_log_item_t	*iip;

	ASSERT(ip->i_itemp == NULL);
	iip = ip->i_itemp = (xfs_inode_log_item_t *)
			kmem_zone_zalloc(xfs_ili_zone, KM_SLEEP);
#ifdef LI_DEBUG
	fprintf(stderr, "inode_item_init for inode %llu, iip=%p\n",
		ip->i_ino, iip);
#endif

	iip->ili_item.li_type = XFS_LI_INODE;
	iip->ili_item.li_mountp = mp;
	iip->ili_inode = ip;
	iip->ili_format.ilf_type = XFS_LI_INODE;
	iip->ili_format.ilf_ino = ip->i_ino;
	iip->ili_format.ilf_blkno = ip->i_blkno;
	iip->ili_format.ilf_len = ip->i_len;
	iip->ili_format.ilf_boffset = ip->i_boffset;
}
