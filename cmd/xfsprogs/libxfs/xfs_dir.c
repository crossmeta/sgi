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
 * xfs_dir.c
 *
 * Provide the external interfaces to manage directories.
 */


xfs_dahash_t	xfs_dir_hash_dot, xfs_dir_hash_dotdot;

/*
 * One-time startup routine called from xfs_init().
 */
void
xfs_dir_startup(void)
{
	xfs_dir_hash_dot = xfs_da_hashname(".", 1);
	xfs_dir_hash_dotdot = xfs_da_hashname("..", 2);
}

/*
 * Initialize directory-related fields in the mount structure.
 */
STATIC void
xfs_dir_mount(xfs_mount_t *mp)
{
	uint shortcount, leafcount, count;

	mp->m_dirversion = 1;
	shortcount = (mp->m_attroffset - (uint)sizeof(xfs_dir_sf_hdr_t)) /
		     (uint)sizeof(xfs_dir_sf_entry_t);
	leafcount = (XFS_LBSIZE(mp) - (uint)sizeof(xfs_dir_leaf_hdr_t)) /
		    ((uint)sizeof(xfs_dir_leaf_entry_t) +
		     (uint)sizeof(xfs_dir_leaf_name_t));
	count = shortcount > leafcount ? shortcount : leafcount;
	mp->m_dircook_elog = xfs_da_log2_roundup(count + 1);
	ASSERT(mp->m_dircook_elog <= mp->m_sb.sb_blocklog);
	mp->m_da_node_ents =
		(XFS_LBSIZE(mp) - (uint)sizeof(xfs_da_node_hdr_t)) /
		(uint)sizeof(xfs_da_node_entry_t);
	mp->m_dir_magicpct = (XFS_LBSIZE(mp) * 37) / 100;
	mp->m_dirblksize = mp->m_sb.sb_blocksize;
	mp->m_dirblkfsbs = 1;
}

/*
 * Initialize a directory with its "." and ".." entries.
 */
STATIC int
xfs_dir_init(xfs_trans_t *trans, xfs_inode_t *dir, xfs_inode_t *parent_dir)
{
	xfs_da_args_t args;
	int error;

	bzero((char *)&args, sizeof(args));
	args.dp = dir;
	args.trans = trans;

	ASSERT((dir->i_d.di_mode & IFMT) == IFDIR);
	if (error = xfs_dir_ino_validate(trans->t_mountp, parent_dir->i_ino))
		return error;

	return(xfs_dir_shortform_create(&args, parent_dir->i_ino));
}

/*
 * Generic handler routine to add a name to a directory.
 * Transitions directory from shortform to Btree as necessary.
 */
STATIC int						/* error */
xfs_dir_createname(xfs_trans_t *trans, xfs_inode_t *dp, char *name,
		   int namelen, xfs_ino_t inum, xfs_fsblock_t *firstblock,
		   xfs_bmap_free_t *flist, xfs_extlen_t total)
{
	xfs_da_args_t args;
	int retval, newsize, done;

	ASSERT((dp->i_d.di_mode & IFMT) == IFDIR);

	if (retval = xfs_dir_ino_validate(trans->t_mountp, inum))
		return (retval);

	XFS_STATS_INC(xs_dir_create);
	/*
	 * Fill in the arg structure for this request.
	 */
	args.name = name;
	args.namelen = namelen;
	args.hashval = xfs_da_hashname(name, namelen);
	args.inumber = inum;
	args.dp = dp;
	args.firstblock = firstblock;
	args.flist = flist;
	args.total = total;
	args.whichfork = XFS_DATA_FORK;
	args.trans = trans;
	args.justcheck = 0;
	args.addname = args.oknoent = 1;

	/*
	 * Decide on what work routines to call based on the inode size.
	 */
	done = 0;
	if (dp->i_d.di_format == XFS_DINODE_FMT_LOCAL) {
		newsize = XFS_DIR_SF_ENTSIZE_BYNAME(args.namelen);
		if ((dp->i_d.di_size + newsize) <= XFS_IFORK_DSIZE(dp)) {
			retval = xfs_dir_shortform_addname(&args);
			done = 1;
		} else {
			if (total == 0)
				return XFS_ERROR(ENOSPC);
			retval = xfs_dir_shortform_to_leaf(&args);
			done = retval != 0;
		}
	}
	if (!done && xfs_bmap_one_block(dp, XFS_DATA_FORK)) {
		retval = xfs_dir_leaf_addname(&args);
		done = retval != ENOSPC;
		if (!done) {
			if (total == 0)
				return XFS_ERROR(ENOSPC);
			retval = xfs_dir_leaf_to_node(&args);
			done = retval != 0;
		}
	}
	if (!done) {
		retval = xfs_dir_node_addname(&args);
	}
	return(retval);
}

/*
 * Generic handler routine to remove a name from a directory.
 * Transitions directory from Btree to shortform as necessary.
 */
STATIC int							/* error */
xfs_dir_removename(xfs_trans_t *trans, xfs_inode_t *dp, char *name,
		   int namelen, xfs_ino_t ino, xfs_fsblock_t *firstblock,
		   xfs_bmap_free_t *flist, xfs_extlen_t total)
{
	xfs_da_args_t args;
	int count, totallen, newsize, retval;

	ASSERT((dp->i_d.di_mode & IFMT) == IFDIR);
	XFS_STATS_INC(xs_dir_remove);
	/*
	 * Fill in the arg structure for this request.
	 */
	args.name = name;
	args.namelen = namelen;
	args.hashval = xfs_da_hashname(name, namelen);
	args.inumber = ino;
	args.dp = dp;
	args.firstblock = firstblock;
	args.flist = flist;
	args.total = total;
	args.whichfork = XFS_DATA_FORK;
	args.trans = trans;
	args.justcheck = args.addname = args.oknoent = 0;

	/*
	 * Decide on what work routines to call based on the inode size.
	 */
	if (dp->i_d.di_format == XFS_DINODE_FMT_LOCAL) {
		retval = xfs_dir_shortform_removename(&args);
	} else if (xfs_bmap_one_block(dp, XFS_DATA_FORK)) {
		retval = xfs_dir_leaf_removename(&args, &count, &totallen);
		if (retval == 0) {
			newsize = XFS_DIR_SF_ALLFIT(count, totallen);
			if (newsize <= XFS_IFORK_DSIZE(dp)) {
				retval = xfs_dir_leaf_to_shortform(&args);
			}
		}
	} else {
		retval = xfs_dir_node_removename(&args);
	}
	return(retval);
}

STATIC int							/* error */
xfs_dir_lookup(xfs_trans_t *trans, xfs_inode_t *dp, char *name, int namelen,
				   xfs_ino_t *inum)
{
	xfs_da_args_t args;
	int retval;

	ASSERT((dp->i_d.di_mode & IFMT) == IFDIR);
	if (namelen >= MAXNAMELEN) {
		return(XFS_ERROR(EINVAL));
	}

	XFS_STATS_INC(xs_dir_lookup);
	/*
	 * Fill in the arg structure for this request.
	 */
	args.name = name;
	args.namelen = namelen;
	args.hashval = xfs_da_hashname(name, namelen);
	args.inumber = 0;
	args.dp = dp;
	args.firstblock = NULL;
	args.flist = NULL;
	args.total = 0;
	args.whichfork = XFS_DATA_FORK;
	args.trans = trans;
	args.justcheck = args.addname = 0;
	args.oknoent = 1;

	/*
	 * Decide on what work routines to call based on the inode size.
	 */
	if (dp->i_d.di_format == XFS_DINODE_FMT_LOCAL) {
		retval = xfs_dir_shortform_lookup(&args);
	} else if (xfs_bmap_one_block(dp, XFS_DATA_FORK)) {
		retval = xfs_dir_leaf_lookup(&args);
	} else {
		retval = xfs_dir_node_lookup(&args);
	}
	if (retval == EEXIST)
		retval = 0;
	*inum = args.inumber;
	return(retval);
}

STATIC int							/* error */
xfs_dir_replace(xfs_trans_t *trans, xfs_inode_t *dp, char *name, int namelen,
				    xfs_ino_t inum, xfs_fsblock_t *firstblock,
				    xfs_bmap_free_t *flist, xfs_extlen_t total)
{
	xfs_da_args_t args;
	int retval;

	ASSERT((dp->i_d.di_mode & IFMT) == IFDIR);
	if (namelen >= MAXNAMELEN) {
		return(XFS_ERROR(EINVAL));
	}

	if (retval = xfs_dir_ino_validate(trans->t_mountp, inum))
		return retval;

	/*
	 * Fill in the arg structure for this request.
	 */
	args.name = name;
	args.namelen = namelen;
	args.hashval = xfs_da_hashname(name, namelen);
	args.inumber = inum;
	args.dp = dp;
	args.firstblock = firstblock;
	args.flist = flist;
	args.total = total;
	args.whichfork = XFS_DATA_FORK;
	args.trans = trans;
	args.justcheck = args.addname = args.oknoent = 0;

	/*
	 * Decide on what work routines to call based on the inode size.
	 */
	if (dp->i_d.di_format == XFS_DINODE_FMT_LOCAL) {
		retval = xfs_dir_shortform_replace(&args);
	} else if (xfs_bmap_one_block(dp, XFS_DATA_FORK)) {
		retval = xfs_dir_leaf_replace(&args);
	} else {
		retval = xfs_dir_node_replace(&args);
	}

	return(retval);
}


/*========================================================================
 * External routines when dirsize == XFS_LBSIZE(dp->i_mount).
 *========================================================================*/

/*
 * Add a name to the leaf directory structure
 * This is the external routine.
 */
int
xfs_dir_leaf_addname(xfs_da_args_t *args)
{
	int index, retval;
	xfs_dabuf_t *bp;

	retval = xfs_da_read_buf(args->trans, args->dp, 0, -1, &bp,
					      XFS_DATA_FORK);
	if (retval)
		return(retval);
	ASSERT(bp != NULL);

	retval = xfs_dir_leaf_lookup_int(bp, args, &index);
	if (retval == ENOENT)
		retval = xfs_dir_leaf_add(bp, args, index);
	xfs_da_buf_done(bp);
	return(retval);
}

/*
 * Remove a name from the leaf directory structure
 * This is the external routine.
 */
STATIC int
xfs_dir_leaf_removename(xfs_da_args_t *args, int *count, int *totallen)
{
	xfs_dir_leafblock_t *leaf;
	int index, retval;
	xfs_dabuf_t *bp;

	retval = xfs_da_read_buf(args->trans, args->dp, 0, -1, &bp,
					      XFS_DATA_FORK);
	if (retval)
		return(retval);
	ASSERT(bp != NULL);
	leaf = bp->data;
	ASSERT(INT_GET(leaf->hdr.info.magic, ARCH_CONVERT) == XFS_DIR_LEAF_MAGIC);
	retval = xfs_dir_leaf_lookup_int(bp, args, &index);
	if (retval == EEXIST) {
		(void)xfs_dir_leaf_remove(args->trans, bp, index);
		*count = INT_GET(leaf->hdr.count, ARCH_CONVERT);
		*totallen = INT_GET(leaf->hdr.namebytes, ARCH_CONVERT);
		retval = 0;
	}
	xfs_da_buf_done(bp);
	return(retval);
}

/*
 * Look up a name in a leaf directory structure.
 * This is the external routine.
 */
STATIC int
xfs_dir_leaf_lookup(xfs_da_args_t *args)
{
	int index, retval;
	xfs_dabuf_t *bp;

	retval = xfs_da_read_buf(args->trans, args->dp, 0, -1, &bp,
					      XFS_DATA_FORK);
	if (retval)
		return(retval);
	ASSERT(bp != NULL);
	retval = xfs_dir_leaf_lookup_int(bp, args, &index);
	xfs_da_brelse(args->trans, bp);
	return(retval);
}

/*
 * Look up a name in a leaf directory structure, replace the inode number.
 * This is the external routine.
 */
STATIC int
xfs_dir_leaf_replace(xfs_da_args_t *args)
{
	int index, retval;
	xfs_dabuf_t *bp;
	xfs_ino_t inum;
	xfs_dir_leafblock_t *leaf;
	xfs_dir_leaf_entry_t *entry;
	xfs_dir_leaf_name_t *namest;

	inum = args->inumber;
	retval = xfs_da_read_buf(args->trans, args->dp, 0, -1, &bp,
					      XFS_DATA_FORK);
	if (retval)
		return(retval);
	ASSERT(bp != NULL);
	retval = xfs_dir_leaf_lookup_int(bp, args, &index);
	if (retval == EEXIST) {
		leaf = bp->data;
		entry = &leaf->entries[index];
		namest = XFS_DIR_LEAF_NAMESTRUCT(leaf, INT_GET(entry->nameidx, ARCH_CONVERT));
                /* XXX - replace assert? */
		XFS_DIR_SF_PUT_DIRINO_ARCH(&inum, &namest->inumber, ARCH_CONVERT);
		xfs_da_log_buf(args->trans, bp, 
		    XFS_DA_LOGRANGE(leaf, namest, sizeof(namest->inumber)));
		xfs_da_buf_done(bp);
		retval = 0;
	} else
		xfs_da_brelse(args->trans, bp);
	return(retval);
}


/*========================================================================
 * External routines when dirsize > XFS_LBSIZE(mp).
 *========================================================================*/

/*
 * Add a name to a Btree-format directory.
 *
 * This will involve walking down the Btree, and may involve splitting
 * leaf nodes and even splitting intermediate nodes up to and including
 * the root node (a special case of an intermediate node).
 */
STATIC int
xfs_dir_node_addname(xfs_da_args_t *args)
{
	xfs_da_state_t *state;
	xfs_da_state_blk_t *blk;
	int retval, error;

	/*
	 * Fill in bucket of arguments/results/context to carry around.
	 */
	state = xfs_da_state_alloc();
	state->args = args;
	state->mp = args->dp->i_mount;
	state->blocksize = state->mp->m_sb.sb_blocksize;

	/*
	 * Search to see if name already exists, and get back a pointer
	 * to where it should go.
	 */
	error = xfs_da_node_lookup_int(state, &retval);
	if (error)
		retval = error;
	if (retval != ENOENT)
		goto error;
	blk = &state->path.blk[ state->path.active-1 ];
	ASSERT(blk->magic == XFS_DIR_LEAF_MAGIC);
	retval = xfs_dir_leaf_add(blk->bp, args, blk->index);
	if (retval == 0) {
		/*
		 * Addition succeeded, update Btree hashvals.
		 */
		if (!args->justcheck)
			xfs_da_fixhashpath(state, &state->path);
	} else {
		/*
		 * Addition failed, split as many Btree elements as required.
		 */
		if (args->total == 0) {
			ASSERT(retval == ENOSPC);
			goto error;
		}
		retval = xfs_da_split(state);
	}
error:
	xfs_da_state_free(state);

	return(retval);
}

/*
 * Remove a name from a B-tree directory.
 *
 * This will involve walking down the Btree, and may involve joining
 * leaf nodes and even joining intermediate nodes up to and including
 * the root node (a special case of an intermediate node).
 */
STATIC int
xfs_dir_node_removename(xfs_da_args_t *args)
{
	xfs_da_state_t *state;
	xfs_da_state_blk_t *blk;
	int retval, error;

	state = xfs_da_state_alloc();
	state->args = args;
	state->mp = args->dp->i_mount;
	state->blocksize = state->mp->m_sb.sb_blocksize;

	/*
	 * Search to see if name exists, and get back a pointer to it.
	 */
	error = xfs_da_node_lookup_int(state, &retval);
	if (error)
		retval = error;
	if (retval != EEXIST) {
		xfs_da_state_free(state);
		return(retval);
	}

	/*
	 * Remove the name and update the hashvals in the tree.
	 */
	blk = &state->path.blk[ state->path.active-1 ];
	ASSERT(blk->magic == XFS_DIR_LEAF_MAGIC);
	retval = xfs_dir_leaf_remove(args->trans, blk->bp, blk->index);
	xfs_da_fixhashpath(state, &state->path);

	/*
	 * Check to see if the tree needs to be collapsed.
	 */
	error = 0;
	if (retval) {
		error = xfs_da_join(state);
	}

	xfs_da_state_free(state);
	if (error)
		return(error);
	return(0);
}

/*
 * Look up a filename in a int directory.
 * Use an internal routine to actually do all the work.
 */
STATIC int
xfs_dir_node_lookup(xfs_da_args_t *args)
{
	xfs_da_state_t *state;
	int retval, error, i;

	state = xfs_da_state_alloc();
	state->args = args;
	state->mp = args->dp->i_mount;
	state->blocksize = state->mp->m_sb.sb_blocksize;

	/*
	 * Search to see if name exists,
	 * and get back a pointer to it.
	 */
	error = xfs_da_node_lookup_int(state, &retval);
	if (error) {
		retval = error;
	}

	/* 
	 * If not in a transaction, we have to release all the buffers.
	 */
	for (i = 0; i < state->path.active; i++) {
		xfs_da_brelse(args->trans, state->path.blk[i].bp);
		state->path.blk[i].bp = NULL;
	}

	xfs_da_state_free(state);
	return(retval);
}

/*
 * Look up a filename in an int directory, replace the inode number.
 * Use an internal routine to actually do the lookup.
 */
STATIC int
xfs_dir_node_replace(xfs_da_args_t *args)
{
	xfs_da_state_t *state;
	xfs_da_state_blk_t *blk;
	xfs_dir_leafblock_t *leaf;
	xfs_dir_leaf_entry_t *entry;
	xfs_dir_leaf_name_t *namest;
	xfs_ino_t inum;
	int retval, error, i;
	xfs_dabuf_t *bp;

	state = xfs_da_state_alloc();
	state->args = args;
	state->mp = args->dp->i_mount;
	state->blocksize = state->mp->m_sb.sb_blocksize;
	inum = args->inumber;

	/*
	 * Search to see if name exists,
	 * and get back a pointer to it.
	 */
	error = xfs_da_node_lookup_int(state, &retval);
	if (error) {
		retval = error;
	}

	if (retval == EEXIST) {
		blk = &state->path.blk[state->path.active - 1];
		ASSERT(blk->magic == XFS_DIR_LEAF_MAGIC);
		bp = blk->bp;
		leaf = bp->data;
		entry = &leaf->entries[blk->index];
		namest = XFS_DIR_LEAF_NAMESTRUCT(leaf, INT_GET(entry->nameidx, ARCH_CONVERT));
                /* XXX - replace assert ? */
		XFS_DIR_SF_PUT_DIRINO_ARCH(&inum, &namest->inumber, ARCH_CONVERT);
		xfs_da_log_buf(args->trans, bp,
		    XFS_DA_LOGRANGE(leaf, namest, sizeof(namest->inumber)));
		xfs_da_buf_done(bp);
		blk->bp = NULL;
		retval = 0;
	} else {
		i = state->path.active - 1;
		xfs_da_brelse(args->trans, state->path.blk[i].bp);
		state->path.blk[i].bp = NULL;
	}
	for (i = 0; i < state->path.active - 1; i++) {
		xfs_da_brelse(args->trans, state->path.blk[i].bp);
		state->path.blk[i].bp = NULL;
	}

	xfs_da_state_free(state);
	return(retval);
}
