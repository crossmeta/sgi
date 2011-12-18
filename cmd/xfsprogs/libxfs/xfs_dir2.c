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
 * XFS v2 directory implmentation.
 * Top-level and utility routines.
 */

#include <xfs.h>


/*
 * Initialize directory-related fields in the mount structure.
 */
void
xfs_dir2_mount(
	xfs_mount_t	*mp)		/* filesystem mount point */
{
	mp->m_dirversion = 2;
	ASSERT((1 << (mp->m_sb.sb_blocklog + mp->m_sb.sb_dirblklog)) <=
	       XFS_MAX_BLOCKSIZE);
	mp->m_dirblksize = 1 << (mp->m_sb.sb_blocklog + mp->m_sb.sb_dirblklog);
	mp->m_dirblkfsbs = 1 << mp->m_sb.sb_dirblklog;
	mp->m_dirdatablk = XFS_DIR2_DB_TO_DA(mp, XFS_DIR2_DATA_FIRSTDB(mp));
	mp->m_dirleafblk = XFS_DIR2_DB_TO_DA(mp, XFS_DIR2_LEAF_FIRSTDB(mp));
	mp->m_dirfreeblk = XFS_DIR2_DB_TO_DA(mp, XFS_DIR2_FREE_FIRSTDB(mp));
	mp->m_da_node_ents =
		(mp->m_dirblksize - (uint)sizeof(xfs_da_node_hdr_t)) /
		(uint)sizeof(xfs_da_node_entry_t);
	mp->m_dir_magicpct = (mp->m_dirblksize * 37) / 100;
}

/*
 * Initialize a directory with its "." and ".." entries.
 */
int				/* error */
xfs_dir2_init(
	xfs_trans_t	*tp,		/* transaction pointer */
	xfs_inode_t	*dp,		/* incore directory inode */
	xfs_inode_t	*pdp)		/* incore parent directory inode */
{
	xfs_da_args_t	args;		/* operation arguments */
	int		error;		/* error return value */

	bzero((char *)&args, sizeof(args));
	args.dp = dp;
	args.trans = tp;
	ASSERT((dp->i_d.di_mode & IFMT) == IFDIR);
	if (error = xfs_dir_ino_validate(tp->t_mountp, pdp->i_ino)) {
#pragma mips_frequency_hint NEVER
		return error;
	}
	return xfs_dir2_sf_create(&args, pdp->i_ino);
}

/*
  Enter a name in a directory.
 */
STATIC int					/* error */
xfs_dir2_createname(
	xfs_trans_t		*tp,		/* transaction pointer */
	xfs_inode_t		*dp,		/* incore directory inode */
	char			*name,		/* new entry name */
	int			namelen,	/* new entry name length */
	xfs_ino_t		inum,		/* new entry inode number */
	xfs_fsblock_t		*first,		/* bmap's firstblock */
	xfs_bmap_free_t		*flist,		/* bmap's freeblock list */
	xfs_extlen_t		total)		/* bmap's total block count */
{
	xfs_da_args_t		args;		/* operation arguments */
	int			rval;		/* return value */
	int			v;		/* type-checking value */

	ASSERT((dp->i_d.di_mode & IFMT) == IFDIR);
	if (rval = xfs_dir_ino_validate(tp->t_mountp, inum)) {
#pragma mips_frequency_hint NEVER
		return rval;
	}
	XFS_STATS_INC(xs_dir_create);
	/*
	 * Fill in the arg structure for this request.
	 */
	args.name = name;
	args.namelen = namelen;
	args.hashval = xfs_da_hashname(name, namelen);
	args.inumber = inum;
	args.dp = dp;
	args.firstblock = first;
	args.flist = flist;
	args.total = total;
	args.whichfork = XFS_DATA_FORK;
	args.trans = tp;
	args.justcheck = 0;
	args.addname = args.oknoent = 1;
	/*
	 * Decide on what work routines to call based on the inode size.
	 */
	if (dp->i_d.di_format == XFS_DINODE_FMT_LOCAL)
		rval = xfs_dir2_sf_addname(&args);
	else if (rval = xfs_dir2_isblock(tp, dp, &v)) {
#pragma mips_frequency_hint NEVER
		return rval;
	} else if (v)
		rval = xfs_dir2_block_addname(&args);
	else if (rval = xfs_dir2_isleaf(tp, dp, &v)) {
#pragma mips_frequency_hint NEVER
		return rval;
	} else if (v)
		rval = xfs_dir2_leaf_addname(&args);
	else
		rval = xfs_dir2_node_addname(&args);
	return rval;
}

/*
 * Lookup a name in a directory, give back the inode number.
 */
STATIC int				/* error */
xfs_dir2_lookup(
	xfs_trans_t	*tp,		/* transaction pointer */
	xfs_inode_t	*dp,		/* incore directory inode */
	char		*name,		/* lookup name */
	int		namelen,	/* lookup name length */
	xfs_ino_t	*inum)		/* out: inode number */
{
	xfs_da_args_t	args;		/* operation arguments */
	int		rval;		/* return value */
	int		v;		/* type-checking value */

	ASSERT((dp->i_d.di_mode & IFMT) == IFDIR);
	if (namelen >= MAXNAMELEN) {
#pragma mips_frequency_hint NEVER
		return XFS_ERROR(EINVAL);
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
	args.trans = tp;
	args.justcheck = args.addname = 0;
	args.oknoent = 1;
	/*
	 * Decide on what work routines to call based on the inode size.
	 */
	if (dp->i_d.di_format == XFS_DINODE_FMT_LOCAL)
		rval = xfs_dir2_sf_lookup(&args);
	else if (rval = xfs_dir2_isblock(tp, dp, &v)) {
#pragma mips_frequency_hint NEVER
		return rval;
	} else if (v)
		rval = xfs_dir2_block_lookup(&args);
	else if (rval = xfs_dir2_isleaf(tp, dp, &v)) {
#pragma mips_frequency_hint NEVER
		return rval;
	} else if (v)
		rval = xfs_dir2_leaf_lookup(&args);
	else
		rval = xfs_dir2_node_lookup(&args);
	if (rval == EEXIST)
		rval = 0;
	if (rval == 0)
		*inum = args.inumber;
	return rval;
}

/*
 * Remove an entry from a directory.
 */
STATIC int				/* error */
xfs_dir2_removename(
	xfs_trans_t	*tp,		/* transaction pointer */
	xfs_inode_t	*dp,		/* incore directory inode */
	char		*name,		/* name of entry to remove */
	int		namelen,	/* name length of entry to remove */
	xfs_ino_t	ino,		/* inode number of entry to remove */
	xfs_fsblock_t	*first,		/* bmap's firstblock */
	xfs_bmap_free_t	*flist,		/* bmap's freeblock list */
	xfs_extlen_t	total)		/* bmap's total block count */
{
	xfs_da_args_t	args;		/* operation arguments */
	int		rval;		/* return value */
	int		v;		/* type-checking value */

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
	args.firstblock = first;
	args.flist = flist;
	args.total = total;
	args.whichfork = XFS_DATA_FORK;
	args.trans = tp;
	args.justcheck = args.addname = args.oknoent = 0;
	/*
	 * Decide on what work routines to call based on the inode size.
	 */
	if (dp->i_d.di_format == XFS_DINODE_FMT_LOCAL)
		rval = xfs_dir2_sf_removename(&args);
	else if (rval = xfs_dir2_isblock(tp, dp, &v)) {
#pragma mips_frequency_hint NEVER
		return rval;
	} else if (v)
		rval = xfs_dir2_block_removename(&args);
	else if (rval = xfs_dir2_isleaf(tp, dp, &v)) {
#pragma mips_frequency_hint NEVER
		return rval;
	} else if (v)
		rval = xfs_dir2_leaf_removename(&args);
	else
		rval = xfs_dir2_node_removename(&args);
	return rval;
}

/*
 * Replace the inode number of a directory entry.
 */
STATIC int				/* error */
xfs_dir2_replace(
	xfs_trans_t	*tp,		/* transaction pointer */
	xfs_inode_t	*dp,		/* incore directory inode */
	char		*name,		/* name of entry to replace */
	int		namelen,	/* name length of entry to replace */
	xfs_ino_t	inum,		/* new inode number */
	xfs_fsblock_t	*first,		/* bmap's firstblock */
	xfs_bmap_free_t	*flist,		/* bmap's freeblock list */
	xfs_extlen_t	total)		/* bmap's total block count */
{
	xfs_da_args_t	args;		/* operation arguments */
	int		rval;		/* return value */
	int		v;		/* type-checking value */

	ASSERT((dp->i_d.di_mode & IFMT) == IFDIR);
	if (namelen >= MAXNAMELEN) {
#pragma mips_frequency_hint NEVER
		return XFS_ERROR(EINVAL);
	}
	if (rval = xfs_dir_ino_validate(tp->t_mountp, inum)) {
#pragma mips_frequency_hint NEVER
		return rval;
	}
	/*
	 * Fill in the arg structure for this request.
	 */
	args.name = name;
	args.namelen = namelen;
	args.hashval = xfs_da_hashname(name, namelen);
	args.inumber = inum;
	args.dp = dp;
	args.firstblock = first;
	args.flist = flist;
	args.total = total;
	args.whichfork = XFS_DATA_FORK;
	args.trans = tp;
	args.justcheck = args.addname = args.oknoent = 0;
	/*
	 * Decide on what work routines to call based on the inode size.
	 */
	if (dp->i_d.di_format == XFS_DINODE_FMT_LOCAL)
		rval = xfs_dir2_sf_replace(&args);
	else if (rval = xfs_dir2_isblock(tp, dp, &v)) {
#pragma mips_frequency_hint NEVER
		return rval;
	} else if (v)
		rval = xfs_dir2_block_replace(&args);
	else if (rval = xfs_dir2_isleaf(tp, dp, &v)) {
#pragma mips_frequency_hint NEVER
		return rval;
	} else if (v)
		rval = xfs_dir2_leaf_replace(&args);
	else
		rval = xfs_dir2_node_replace(&args);
	return rval;
}

/*
 * Utility routines.
 */

/*
 * Add a block to the directory.
 * This routine is for data and free blocks, not leaf/node blocks
 * which are handled by xfs_da_grow_inode.
 */
int					/* error */
xfs_dir2_grow_inode(
	xfs_da_args_t	*args,		/* operation arguments */
	int		space,		/* v2 dir's space XFS_DIR2_xxx_SPACE */
	xfs_dir2_db_t	*dbp)		/* out: block number added */
{
	xfs_fileoff_t	bno;		/* directory offset of new block */
	int		count;		/* count of filesystem blocks */
	xfs_inode_t	*dp;		/* incore directory inode */
	int		error;		/* error return value */
	int		got;		/* blocks actually mapped */
	int		i;		/* temp mapping index */
	xfs_bmbt_irec_t	map;		/* single structure for bmap */
	int		mapi;		/* mapping index */
	xfs_bmbt_irec_t	*mapp;		/* bmap mapping structure(s) */
	xfs_mount_t	*mp;		/* filesystem mount point */
	int		nmap;		/* number of bmap entries */
	xfs_trans_t	*tp;		/* transaction pointer */

	xfs_dir2_trace_args_s("grow_inode", args, space);
	dp = args->dp;
	tp = args->trans;
	mp = dp->i_mount;
	/*
	 * Set lowest possible block in the space requested.
	 */
	bno = XFS_B_TO_FSBT(mp, space * XFS_DIR2_SPACE_SIZE);
	count = mp->m_dirblkfsbs;
	/*
	 * Find the first hole for our block.
	 */
	if (error = xfs_bmap_first_unused(tp, dp, count, &bno, XFS_DATA_FORK)) {
#pragma mips_frequency_hint NEVER
		return error;
	}
	nmap = 1;
	ASSERT(args->firstblock != NULL);
	/*
	 * Try mapping the new block contiguously (one extent).
	 */
	if (error = xfs_bmapi(tp, dp, bno, count,
			XFS_BMAPI_WRITE|XFS_BMAPI_METADATA|XFS_BMAPI_CONTIG,
			args->firstblock, args->total, &map, &nmap,
			args->flist)) {
#pragma mips_frequency_hint NEVER
		return error;
	}
	ASSERT(nmap <= 1);
	/*
	 * Got it in 1.
	 */
	if (nmap == 1) {
		mapp = &map;
		mapi = 1;
	}
	/*
	 * Didn't work and this is a multiple-fsb directory block.
	 * Try again with contiguous flag turned on.
	 */
	else if (nmap == 0 && count > 1) {
#pragma mips_frequency_hint NEVER
		xfs_fileoff_t	b;	/* current file offset */

		/*
		 * Space for maximum number of mappings.
		 */
		mapp = kmem_alloc(sizeof(*mapp) * count, KM_SLEEP);
		/*
		 * Iterate until we get to the end of our block.
		 */
		for (b = bno, mapi = 0; b < bno + count; ) {
			int	c;	/* current fsb count */

			/*
			 * Can't map more than MAX_NMAP at once.
			 */
			nmap = MIN(XFS_BMAP_MAX_NMAP, count);
			c = (int)(bno + count - b);
			if (error = xfs_bmapi(tp, dp, b, c,
					XFS_BMAPI_WRITE|XFS_BMAPI_METADATA,
					args->firstblock, args->total,
					&mapp[mapi], &nmap, args->flist)) {
				kmem_free(mapp, sizeof(*mapp) * count);
				return error;
			}
			if (nmap < 1)
				break;
			/*
			 * Add this bunch into our table, go to the next offset.
			 */
			mapi += nmap;
			b = mapp[mapi - 1].br_startoff +
			    mapp[mapi - 1].br_blockcount;
		}
	}
	/*
	 * Didn't work.
	 */
	else {
#pragma mips_frequency_hint NEVER
		mapi = 0;
		mapp = NULL;
	}
	/*
	 * See how many fsb's we got.
	 */
	for (i = 0, got = 0; i < mapi; i++)
		got += mapp[i].br_blockcount;
	/*
	 * Didn't get enough fsb's, or the first/last block's are wrong.
	 */
	if (got != count || mapp[0].br_startoff != bno ||
	    mapp[mapi - 1].br_startoff + mapp[mapi - 1].br_blockcount !=
	    bno + count) {
#pragma mips_frequency_hint NEVER
		if (mapp != &map)
			kmem_free(mapp, sizeof(*mapp) * count);
		return XFS_ERROR(ENOSPC);
	}
	/*
	 * Done with the temporary mapping table.
	 */
	if (mapp != &map)
		kmem_free(mapp, sizeof(*mapp) * count);
	*dbp = XFS_DIR2_DA_TO_DB(mp, (xfs_dablk_t)bno);
	/*
	 * Update file's size if this is the data space and it grew.
	 */
	if (space == XFS_DIR2_DATA_SPACE) {
		xfs_fsize_t	size;		/* directory file (data) size */

		size = XFS_FSB_TO_B(mp, bno + count);
		if (size > dp->i_d.di_size) {
			dp->i_d.di_size = size;
			xfs_trans_log_inode(tp, dp, XFS_ILOG_CORE);
		}
	}
	return 0;
}

/*
 * See if the directory is a single-block form directory.
 */
int					/* error */
xfs_dir2_isblock(
	xfs_trans_t	*tp,		/* transaction pointer */
	xfs_inode_t	*dp,		/* incore directory inode */
	int		*vp)		/* out: 1 is block, 0 is not block */
{
	xfs_fileoff_t	last;		/* last file offset */
	xfs_mount_t	*mp;		/* filesystem mount point */
	int		rval;		/* return value */

	mp = dp->i_mount;
	if (rval = xfs_bmap_last_offset(tp, dp, &last, XFS_DATA_FORK)) {
#pragma mips_frequency_hint NEVER
		return rval;
	}
	rval = XFS_FSB_TO_B(mp, last) == mp->m_dirblksize;
	ASSERT(rval == 0 || dp->i_d.di_size == mp->m_dirblksize);
	*vp = rval;
	return 0;
}

/*
 * See if the directory is a single-leaf form directory.
 */
int					/* error */
xfs_dir2_isleaf(
	xfs_trans_t	*tp,		/* transaction pointer */
	xfs_inode_t	*dp,		/* incore directory inode */
	int		*vp)		/* out: 1 is leaf, 0 is not leaf */
{
	xfs_fileoff_t	last;		/* last file offset */
	xfs_mount_t	*mp;		/* filesystem mount point */
	int		rval;		/* return value */

	mp = dp->i_mount;
	if (rval = xfs_bmap_last_offset(tp, dp, &last, XFS_DATA_FORK)) {
#pragma mips_frequency_hint NEVER
		return rval;
	}
	*vp = last == mp->m_dirleafblk + (1 << mp->m_sb.sb_dirblklog);
	return 0;
}

/*
 * Remove the given block from the directory.
 * This routine is used for data and free blocks, leaf/node are done
 * by xfs_da_shrink_inode.
 */
int
xfs_dir2_shrink_inode(
	xfs_da_args_t	*args,		/* operation arguments */
	xfs_dir2_db_t	db,		/* directory block number */
	xfs_dabuf_t	*bp)		/* block's buffer */
{
	xfs_fileoff_t	bno;		/* directory file offset */
	xfs_dablk_t	da;		/* directory file offset */
	int		done;		/* bunmap is finished */
	xfs_inode_t	*dp;		/* incore directory inode */
	int		error;		/* error return value */
	xfs_mount_t	*mp;		/* filesystem mount point */
	xfs_trans_t	*tp;		/* transaction pointer */

	xfs_dir2_trace_args_db("shrink_inode", args, db, bp);
	dp = args->dp;
	mp = dp->i_mount;
	tp = args->trans;
	da = XFS_DIR2_DB_TO_DA(mp, db);
	/*
	 * Unmap the fsblock(s).
	 */
	if (error = xfs_bunmapi(tp, dp, da, mp->m_dirblkfsbs,
			XFS_BMAPI_METADATA, 0, args->firstblock, args->flist,
			&done)) {
#pragma mips_frequency_hint NEVER
		/*
		 * ENOSPC actually can happen if we're in a removename with
		 * no space reservation, and the resulting block removal
		 * would cause a bmap btree split or conversion from extents
		 * to btree.  This can only happen for un-fragmented
		 * directory blocks, since you need to be punching out
		 * the middle of an extent.
		 * In this case we need to leave the block in the file,
		 * and not binval it.
		 * So the block has to be in a consistent empty state
		 * and appropriately logged.
		 * We don't free up the buffer, the caller can tell it 
		 * hasn't happened since it got an error back.
		 */
		return error;
	}
	ASSERT(done);
	/*
	 * Invalidate the buffer from the transaction.
	 */
	xfs_da_binval(tp, bp);
	/*
	 * If it's not a data block, we're done.
	 */
	if (db >= XFS_DIR2_LEAF_FIRSTDB(mp)) 
		return 0;
	/*
	 * If the block isn't the last one in the directory, we're done.
	 */
	if (dp->i_d.di_size > XFS_DIR2_DB_OFF_TO_BYTE(mp, db + 1, 0))
		return 0;
	bno = da;
	if (error = xfs_bmap_last_before(tp, dp, &bno, XFS_DATA_FORK)) {
#pragma mips_frequency_hint NEVER
		/*
		 * This can't really happen unless there's kernel corruption.
		 */
		return error;
	}
	if (db == mp->m_dirdatablk)
		ASSERT(bno == 0);
	else
		ASSERT(bno > 0);
	/*
	 * Set the size to the new last block.
	 */
	dp->i_d.di_size = XFS_FSB_TO_B(mp, bno);
	xfs_trans_log_inode(tp, dp, XFS_ILOG_CORE);
	return 0;
}
