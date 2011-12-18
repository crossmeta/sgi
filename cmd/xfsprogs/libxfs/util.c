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
#include <time.h>

/*
 * Wrapper around call to libxfs_ialloc. Takes care of committing and
 * allocating a new transaction as needed.
 *
 * Originally there were two copies of this code - one in mkfs, the
 * other in repair - now there is just the one.
 */
int
libxfs_inode_alloc(
	xfs_trans_t     **tp,
	xfs_inode_t     *pip,
	mode_t		mode,
	ushort		nlink,
	dev_t		rdev,
	cred_t		*cr,
	xfs_inode_t	**ipp)
{
	boolean_t	call_again;
	int		i;
	xfs_buf_t	*ialloc_context;
	xfs_inode_t	*ip;
	xfs_trans_t	*ntp;
	int		error;

	call_again = B_FALSE;
	ialloc_context = (xfs_buf_t *)0;
	error = libxfs_ialloc(*tp, pip, mode, nlink, rdev, cr, (xfs_prid_t) 0,
			   1, &ialloc_context, &call_again, &ip);
	if (error) {
		return error;
	}
	if (call_again) {
		xfs_trans_bhold(*tp, ialloc_context);
		ntp = xfs_trans_dup(*tp);
		xfs_trans_commit(*tp, 0, NULL);
		*tp = ntp;
		if ((i = xfs_trans_reserve(*tp, 0, 0, 0, 0, 0))) {
			fprintf(stderr, "%s: cannot reserve space: %s\n",
				progname, strerror(errno));
			exit(1);
		}
		xfs_trans_bjoin(*tp, ialloc_context);
		error = libxfs_ialloc(*tp, pip, mode, nlink, rdev, cr,
				   (xfs_prid_t) 0, 1, &ialloc_context,
				   &call_again, &ip);
		if (error) {
			return error;
		}
	}
	*ipp = ip;
	ASSERT(ip);
	return error;
}

/*
 * Change the requested timestamp in the given inode.
 * 
 * This was once shared with the kernel, but has diverged to the point
 * where its no longer worth the hassle of maintaining common code.
 */
void
libxfs_ichgtime(xfs_inode_t *ip, int flags)
{
	struct timespec	tv;
	struct timeval	stv;

	gettimeofday(&stv, (struct timezone *)0);
	tv.tv_sec = stv.tv_sec;
	tv.tv_nsec = stv.tv_usec * 1000;
	if (flags & XFS_ICHGTIME_MOD) {
		ip->i_d.di_mtime.t_sec = (__int32_t)tv.tv_sec;
		ip->i_d.di_mtime.t_nsec = (__int32_t)tv.tv_nsec;
	}
	if (flags & XFS_ICHGTIME_ACC) {
		ip->i_d.di_atime.t_sec = (__int32_t)tv.tv_sec;
		ip->i_d.di_atime.t_nsec = (__int32_t)tv.tv_nsec;
	}
	if (flags & XFS_ICHGTIME_CHG) {
		ip->i_d.di_ctime.t_sec = (__int32_t)tv.tv_sec;
		ip->i_d.di_ctime.t_nsec = (__int32_t)tv.tv_nsec;
	}
}

/*
 * Allocate an inode on disk and return a copy of it's in-core version.
 * Set mode, nlink, and rdev appropriately within the inode.
 * The uid and gid for the inode are set according to the contents of
 * the given cred structure.
 *
 * This was once shared with the kernel, but has diverged to the point
 * where its no longer worth the hassle of maintaining common code.
 */
int
libxfs_ialloc(
	xfs_trans_t	*tp,
	xfs_inode_t	*pip,
	mode_t		mode,
	nlink_t		nlink,
	dev_t		rdev,
	cred_t		*cr,
	xfs_prid_t	prid,
	int		okalloc,
	xfs_buf_t	**ialloc_context,
	boolean_t	*call_again,
	xfs_inode_t	**ipp)
{
	xfs_ino_t	ino;
	xfs_inode_t	*ip;
	uint		flags;
	int		error;

	/*
	 * Call the space management code to pick
	 * the on-disk inode to be allocated.
	 */
	error = xfs_dialloc(tp, pip ? pip->i_ino : 0, mode, okalloc,
			    ialloc_context, call_again, &ino);
	if (error != 0)
		return error;
	if (*call_again || ino == NULLFSINO) {
		*ipp = NULL;
		return 0;
	}
	ASSERT(*ialloc_context == NULL);

	error = xfs_trans_iget(tp->t_mountp, tp, ino, 0, &ip);
	if (error != 0)
		return error;
	ASSERT(ip != NULL);

	ip->i_d.di_mode = (__uint16_t)mode;
	ip->i_d.di_onlink = 0;
	ip->i_d.di_nlink = nlink;
	ASSERT(ip->i_d.di_nlink == nlink);
	ip->i_d.di_uid = cr->cr_uid;
	ip->i_d.di_gid = cr->cr_gid;
	ip->i_d.di_projid = prid;
	bzero(&(ip->i_d.di_pad[0]), sizeof(ip->i_d.di_pad));

	/*
	 * If the superblock version is up to where we support new format
	 * inodes and this is currently an old format inode, then change
	 * the inode version number now.  This way we only do the conversion
	 * here rather than here and in the flush/logging code.
	 */
	if (XFS_SB_VERSION_HASNLINK(&tp->t_mountp->m_sb) &&
	    ip->i_d.di_version == XFS_DINODE_VERSION_1) {
		ip->i_d.di_version = XFS_DINODE_VERSION_2;
		/* old link count, projid field, pad field already zeroed */
        }

	ip->i_d.di_size = 0;
	ip->i_d.di_nextents = 0;
	ASSERT(ip->i_d.di_nblocks == 0);
	xfs_ichgtime(ip, XFS_ICHGTIME_CHG|XFS_ICHGTIME_ACC|XFS_ICHGTIME_MOD);
	/*
	 * di_gen will have been taken care of in xfs_iread.
	 */
	ip->i_d.di_extsize = 0;
	ip->i_d.di_dmevmask = 0;
	ip->i_d.di_dmstate = 0;
	ip->i_d.di_flags = 0;
	flags = XFS_ILOG_CORE;
	switch (mode & IFMT) {
	case IFIFO:
	case IFCHR:
	case IFBLK:
	case IFSOCK:
		ip->i_d.di_format = XFS_DINODE_FMT_DEV;
		ip->i_df.if_u2.if_rdev = makedev(major(rdev), minor(rdev));			ip->i_df.if_flags = 0;
		flags |= XFS_ILOG_DEV;
		break;
	case IFREG:
	case IFDIR:
	case IFLNK:
		ip->i_d.di_format = XFS_DINODE_FMT_EXTENTS;
		ip->i_df.if_flags = XFS_IFEXTENTS;
		ip->i_df.if_bytes = ip->i_df.if_real_bytes = 0;
		ip->i_df.if_u1.if_extents = NULL;
		break;
	default:
		ASSERT(0);
	}
	/* Attribute fork settings for new inode. */
	ip->i_d.di_aformat = XFS_DINODE_FMT_EXTENTS;
	ip->i_d.di_anextents = 0;

	/*
	 * Log the new values stuffed into the inode.
	 */
	xfs_trans_log_inode(tp, ip, flags);
	*ipp = ip;
	return 0;
}

void
libxfs_iprint(xfs_inode_t *ip)
{
	xfs_dinode_core_t	*dip;
	xfs_bmbt_rec_t	*ep;
	xfs_extnum_t	i;
	xfs_extnum_t	nextents;

	printf("Inode %p\n", ip);
	printf("    i_dev %x\n", (uint)ip->i_dev);
	printf("    i_ino %Lx\n", ip->i_ino);

	if (ip->i_df.if_flags & XFS_IFEXTENTS)
		printf("EXTENTS ");
	printf("\n");
	printf("    i_df.if_bytes %d\n", ip->i_df.if_bytes);
	printf("    i_df.if_u1.if_extents/if_data %p\n", ip->i_df.if_u1.if_extents);
	if (ip->i_df.if_flags & XFS_IFEXTENTS) {
		nextents = ip->i_df.if_bytes / (uint)sizeof(*ep);
		for (ep = ip->i_df.if_u1.if_extents, i = 0; i < nextents; i++, ep++) {
			xfs_bmbt_irec_t	rec;

			xfs_bmbt_get_all(ep, &rec);
			printf("\t%d: startoff %Lu, startblock 0x%Lx,"
			" blockcount %Lu, state %d\n",
				i, (xfs_dfiloff_t)rec.br_startoff,
				(xfs_dfsbno_t)rec.br_startblock,
				(xfs_dfilblks_t)rec.br_blockcount,
				(int)rec.br_state);
		}
	}
	printf("    i_df.if_broot %p\n", ip->i_df.if_broot);
	printf("    i_df.if_broot_bytes %x\n", ip->i_df.if_broot_bytes);

	dip = &(ip->i_d);
	printf("\nOn disk portion\n");
	printf("    di_magic %x\n", dip->di_magic);
	printf("    di_mode %o\n", dip->di_mode);
	printf("    di_version %x\n", (uint)dip->di_version);
	switch (ip->i_d.di_format) {
	case XFS_DINODE_FMT_LOCAL:
		printf("    Inline inode\n");
		break;
	case XFS_DINODE_FMT_EXTENTS:
		printf("    Extents inode\n");
		break;
	case XFS_DINODE_FMT_BTREE:
		printf("    B-tree inode\n");
		break;
	default:
		printf("    Other inode\n");
		break;
	}
	printf("   di_nlink %x\n", dip->di_nlink);
	printf("   di_uid %d\n", dip->di_uid);
	printf("   di_gid %d\n", dip->di_gid);
	printf("   di_nextents %d\n", dip->di_nextents);
	printf("   di_size %Ld\n", dip->di_size);
	printf("   di_gen %x\n", dip->di_gen);
	printf("   di_extsize %d\n", dip->di_extsize);
	printf("   di_flags %x\n", dip->di_flags);
	printf("   di_nblocks %Ld\n", dip->di_nblocks);
}

/*
 * Writes a modified inode's changes out to the inode's on disk home.
 * Originally based on xfs_iflush_int() from xfs_inode.c in the kernel.
 */
int
libxfs_iflush_int(xfs_inode_t *ip, xfs_buf_t *bp)
{
	xfs_inode_log_item_t	*iip;
	xfs_dinode_t		*dip;
	xfs_mount_t		*mp;

	ASSERT(XFS_BUF_FSPRIVATE(bp, void *) != NULL);
	ASSERT(ip->i_d.di_format != XFS_DINODE_FMT_BTREE ||
		ip->i_d.di_nextents > ip->i_df.if_ext_max);

	iip = ip->i_itemp;
	mp = ip->i_mount;

	/* set *dip = inode's place in the buffer */
	dip = (xfs_dinode_t *)xfs_buf_offset(bp, ip->i_boffset);

#ifdef DEBUG
	ASSERT(ip->i_d.di_magic == XFS_DINODE_MAGIC);
	if ((ip->i_d.di_mode & IFMT) == IFREG) {
		ASSERT( (ip->i_d.di_format == XFS_DINODE_FMT_EXTENTS) ||
			(ip->i_d.di_format == XFS_DINODE_FMT_BTREE) );
	}
	else if ((ip->i_d.di_mode & IFMT) == IFDIR) {
		ASSERT( (ip->i_d.di_format == XFS_DINODE_FMT_EXTENTS) ||
			(ip->i_d.di_format == XFS_DINODE_FMT_BTREE)   ||
			(ip->i_d.di_format == XFS_DINODE_FMT_LOCAL) );
	}
	ASSERT(ip->i_d.di_nextents+ip->i_d.di_anextents <= ip->i_d.di_nblocks);
	ASSERT(ip->i_d.di_forkoff <= mp->m_sb.sb_inodesize);
#endif

	/*
	 * Copy the dirty parts of the inode into the on-disk
	 * inode.  We always copy out the core of the inode,
	 * because if the inode is dirty at all the core must
	 * be.
	 */
	xfs_xlate_dinode_core((xfs_caddr_t)&(dip->di_core), &(ip->i_d), -1,
				ARCH_CONVERT);
	/*
	 * If this is really an old format inode and the superblock version
	 * has not been updated to support only new format inodes, then
	 * convert back to the old inode format.  If the superblock version
	 * has been updated, then make the conversion permanent.
	 */
	ASSERT(ip->i_d.di_version == XFS_DINODE_VERSION_1 ||
		XFS_SB_VERSION_HASNLINK(&mp->m_sb));
	if (ip->i_d.di_version == XFS_DINODE_VERSION_1) {
		if (!XFS_SB_VERSION_HASNLINK(&mp->m_sb)) {
			/*
			 * Convert it back.
			 */
			ASSERT(ip->i_d.di_nlink <= XFS_MAXLINK_1);
			INT_SET(dip->di_core.di_onlink, ARCH_CONVERT,
				ip->i_d.di_nlink);
		} else {
			/*
			 * The superblock version has already been bumped,
			 * so just make the conversion to the new inode
			 * format permanent.
			 */
			ip->i_d.di_version = XFS_DINODE_VERSION_2;
			INT_SET(dip->di_core.di_version, ARCH_CONVERT,
				XFS_DINODE_VERSION_2);
			ip->i_d.di_onlink = 0;
			INT_ZERO(dip->di_core.di_onlink, ARCH_CONVERT);
			bzero(&(ip->i_d.di_pad[0]), sizeof(ip->i_d.di_pad));
			bzero(&(dip->di_core.di_pad[0]),
				sizeof(dip->di_core.di_pad));
			ASSERT(ip->i_d.di_projid == 0);
		}
	}

	if (xfs_iflush_fork(ip, dip, iip, XFS_DATA_FORK, bp) == EFSCORRUPTED)
		return EFSCORRUPTED;
	if (XFS_IFORK_Q(ip)) {
		/* The only error from xfs_iflush_fork is on the data fork. */
		xfs_iflush_fork(ip, dip, iip, XFS_ATTR_FORK, bp);
	}

	return 0;
}

/*
 * Given a block number in a fork, return the next valid block number
 * (not a hole).
 * If this is the last block number then NULLFILEOFF is returned.
 *
 * This was originally in the kernel, but only used in xfs_repair.
 */
int
libxfs_bmap_next_offset(
	xfs_trans_t	*tp,			/* transaction pointer */
	xfs_inode_t	*ip,			/* incore inode */
	xfs_fileoff_t	*bnop,			/* current block */
	int		whichfork)		/* data or attr fork */
{
	xfs_fileoff_t	bno;			/* current block */
	int		eof;			/* hit end of file */
	int		error;			/* error return value */
	xfs_bmbt_irec_t	got;			/* current extent value */
	xfs_ifork_t	*ifp;			/* inode fork pointer */
	xfs_extnum_t	lastx;			/* last extent used */
	xfs_bmbt_irec_t	prev;			/* previous extent value */

	if (XFS_IFORK_FORMAT(ip, whichfork) != XFS_DINODE_FMT_BTREE &&
	    XFS_IFORK_FORMAT(ip, whichfork) != XFS_DINODE_FMT_EXTENTS &&
	    XFS_IFORK_FORMAT(ip, whichfork) != XFS_DINODE_FMT_LOCAL)
	       return XFS_ERROR(EIO);
	if (XFS_IFORK_FORMAT(ip, whichfork) == XFS_DINODE_FMT_LOCAL) {
		*bnop = NULLFILEOFF;
		return 0;
	}
	ifp = XFS_IFORK_PTR(ip, whichfork);
	if (!(ifp->if_flags & XFS_IFEXTENTS) &&
	    (error = xfs_iread_extents(tp, ip, whichfork)))
		return error;
	bno = *bnop + 1;
	xfs_bmap_search_extents(ip, bno, whichfork, &eof, &lastx, &got, &prev);
	if (eof)
		*bnop = NULLFILEOFF;
	else
		*bnop = got.br_startoff < bno ? bno : got.br_startoff;
	return 0;
}

/*
 * Like xfs_dir_removename, but only for removing entries with
 * (name, hashvalue) pairs that may not be consistent (hashvalue
 * may not be correctly set for the name).
 * 
 * This was originally in the kernel, but only used in xfs_repair.
 */
int
xfs_dir_bogus_removename(xfs_trans_t *trans, xfs_inode_t *dp, char *name,
		xfs_fsblock_t *firstblock, xfs_bmap_free_t *flist,
		xfs_extlen_t total, xfs_dahash_t hashval, int namelen)
{
	xfs_da_args_t args;
	int count, totallen, newsize, retval;

	ASSERT((dp->i_d.di_mode & IFMT) == IFDIR);
	if (namelen >= MAXNAMELEN) {
		return EINVAL;
	}

	/*
	 * Fill in the arg structure for this request.
	 */
	args.name = name;
	args.namelen = namelen;
	args.hashval = hashval;
	args.inumber = 0;
	args.dp = dp;
	args.firstblock = firstblock;
	args.flist = flist;
	args.total = total;
	args.whichfork = XFS_DATA_FORK;
	args.trans = trans;
	args.justcheck = args.addname = 0;
	args.oknoent = 1;

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

/*
 * Like xfs_dir_removename, but only for removing entries with
 * (name, hashvalue) pairs that may not be consistent (hashvalue
 * may not be correctly set for the name).
 * 
 * This was originally in the kernel, but only used in xfs_repair.
 */
int
xfs_dir2_bogus_removename(
	xfs_trans_t	*tp,		/* transaction pointer */
	xfs_inode_t	*dp,		/* incore directory inode */
	char		*name,		/* name of entry to remove */
	xfs_fsblock_t	*first,		/* bmap's firstblock */
	xfs_bmap_free_t	*flist,		/* bmap's freeblock list */
	xfs_extlen_t	total,		/* bmap's total block count */
	xfs_dahash_t	hash,		/* name's real hash value */
	int		namelen)	/* entry's name length */
{
	xfs_da_args_t	args;		/* operation arguments */
	int		rval;		/* return value */
	int		v;		/* type-checking value */

	ASSERT((dp->i_d.di_mode & IFMT) == IFDIR);
	if (namelen >= MAXNAMELEN)
		return EINVAL;

	/*
	 * Fill in the arg structure for this request.
	 */
	args.name = name;
	args.namelen = namelen;
	args.hashval = hash;
	args.inumber = 0;
	args.dp = dp;
	args.firstblock = first;
	args.flist = flist;
	args.total = total;
	args.whichfork = XFS_DATA_FORK;
	args.trans = tp;
	args.justcheck = args.addname = 0;
	args.oknoent = 1;

	/*
	 * Decide on what work routines to call based on the inode size.
	 */
	if (dp->i_d.di_format == XFS_DINODE_FMT_LOCAL)
		rval = xfs_dir2_sf_removename(&args);
	else if (rval = xfs_dir2_isblock(tp, dp, &v))
		return rval;
	else if (v)
		rval = xfs_dir2_block_removename(&args);
	else if (rval = xfs_dir2_isleaf(tp, dp, &v))
		return rval;
	else if (v)
		rval = xfs_dir2_leaf_removename(&args);
	else
		rval = xfs_dir2_node_removename(&args);
	return rval;
}

/*
 * Utility routine common used to apply a delta to a field in the
 * in-core superblock.
 * Switch on the field indicated and apply the delta to that field.
 * Fields are not allowed to dip below zero, so if the delta would
 * do this do not apply it and return EINVAL.
 *
 * Originally derived from xfs_mod_incore_sb().
 */
int
libxfs_mod_incore_sb(xfs_mount_t *mp, xfs_sb_field_t field, int delta, int rsvd)
{
	int64_t	lcounter;	/* long counter for 64 bit fields */

	switch (field) {
	case XFS_SBS_FDBLOCKS:
		lcounter = (int64_t)mp->m_sb.sb_fdblocks;
		lcounter += delta;
		if (lcounter < 0)
			return (XFS_ERROR(ENOSPC));
		mp->m_sb.sb_fdblocks = lcounter;
		break;
	default:
		ASSERT(0);
	}
	return 0;
}

int
libxfs_bmap_finish(
	xfs_trans_t	**tp,
	xfs_bmap_free_t	*flist,
	xfs_fsblock_t	firstblock,
	int		*committed)
{
	xfs_bmap_free_item_t	*free;	/* free extent list item */
	xfs_bmap_free_item_t	*next;	/* next item on free list */
	int			error;
	xfs_trans_t		*ntp;

	if (flist->xbf_count == 0) {
		*committed = 0;
		return 0;
	}

	for (free = flist->xbf_first; free != NULL; free = next) {
		next = free->xbfi_next;
		if (error = xfs_free_extent(*tp, free->xbfi_startblock,
				free->xbfi_blockcount))
			return error;
		xfs_bmap_del_free(flist, NULL, free);
	}
	return 0;
}

/*
 * This routine allocates disk space for the given file.
 * Originally derived from xfs_alloc_file_space().
 */
int
libxfs_alloc_file_space(
	xfs_inode_t	*ip,
	xfs_off_t	offset,
	xfs_off_t	len,
	int		alloc_type,
	int		attr_flags)
{
	xfs_mount_t	*mp;
	xfs_off_t	count;
	xfs_filblks_t	datablocks;
	xfs_filblks_t	allocated_fsb;
	xfs_filblks_t	allocatesize_fsb;
	xfs_fsblock_t	firstfsb;
	xfs_bmap_free_t	free_list;
	xfs_bmbt_irec_t	*imapp;
	xfs_bmbt_irec_t	imaps[1];
	int		reccount;
	uint		resblks;
	xfs_fileoff_t	startoffset_fsb;
	xfs_trans_t	*tp;
	int		xfs_bmapi_flags;
	int		committed;
	int		error;

	if (len <= 0)
		return EINVAL;

	count = len;
	error = 0;
	imapp = &imaps[0];
	reccount = 1;
	xfs_bmapi_flags = XFS_BMAPI_WRITE | (alloc_type ? XFS_BMAPI_PREALLOC : 0);
	mp = ip->i_mount;
	startoffset_fsb = XFS_B_TO_FSBT(mp, offset);
	allocatesize_fsb = XFS_B_TO_FSB(mp, count);

	/* allocate file space until done or until there is an error */
	while (allocatesize_fsb && !error) {
		datablocks = allocatesize_fsb;

		tp = xfs_trans_alloc(mp, XFS_TRANS_DIOSTRAT);
		resblks = (uint)XFS_DIOSTRAT_SPACE_RES(mp, datablocks);
		error = xfs_trans_reserve(tp, resblks, 0, 0, 0, 0);
		if (error)
			break;
		xfs_trans_ijoin(tp, ip, 0);
		xfs_trans_ihold(tp, ip);

		XFS_BMAP_INIT(&free_list, &firstfsb);
		error = xfs_bmapi(tp, ip, startoffset_fsb, allocatesize_fsb,
				xfs_bmapi_flags, &firstfsb, 0, imapp,
				&reccount, &free_list);
		if (error)
			break;

		/* complete the transaction */
		error = xfs_bmap_finish(&tp, &free_list, firstfsb, &committed);
		if (error)
			break;

		error = xfs_trans_commit(tp, 0, NULL);
		if (error)
			break;

		allocated_fsb = imapp->br_blockcount;
		if (reccount == 0)
			return ENOSPC;

		startoffset_fsb += allocated_fsb;
		allocatesize_fsb -= allocated_fsb;
	}
	return error;
}

unsigned int
libxfs_log2_roundup(unsigned int i)
{
	unsigned int	rval;

	for (rval = 0; rval < NBBY * sizeof(i); rval++) {
		if ((1 << rval) >= i)
			break;
	}
	return rval;
}

/*
 * Get a buffer for the dir/attr block, fill in the contents.
 * Don't check magic number, the caller will (it's xfs_repair).
 * 
 * Originally from xfs_da_btree.c in the kernel, but only used
 * in userspace so it now resides here.
 */
int
libxfs_da_read_bufr(
	xfs_trans_t	*trans,
	xfs_inode_t	*dp,
	xfs_dablk_t	bno,
	xfs_daddr_t		mappedbno,
	xfs_dabuf_t	**bpp,
	int		whichfork)
{
	return libxfs_da_do_buf(trans, dp, bno, &mappedbno, bpp, whichfork, 2,
		(inst_t *)__return_address);
}

/*
 * Hold dabuf at transaction commit.
 * 
 * Originally from xfs_da_btree.c in the kernel, but only used
 * in userspace so it now resides here.
 */
void
libxfs_da_bhold(xfs_trans_t *tp, xfs_dabuf_t *dabuf)
{
	int	i;

	for (i = 0; i < dabuf->nbuf; i++)
		xfs_trans_bhold(tp, dabuf->bps[i]);
}

/*
 * Join dabuf to transaction.
 * 
 * Originally from xfs_da_btree.c in the kernel, but only used
 * in userspace so it now resides here.
 */
void
libxfs_da_bjoin(xfs_trans_t *tp, xfs_dabuf_t *dabuf)
{
	int	i;

	for (i = 0; i < dabuf->nbuf; i++)
		xfs_trans_bjoin(tp, dabuf->bps[i]);
}
