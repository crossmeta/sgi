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

xfs_zone_t *xfs_ifork_zone;
xfs_zone_t *xfs_inode_zone;

#ifdef DEBUG
void
xfs_inobp_check(
	xfs_mount_t	*mp,
	xfs_buf_t	*bp)
{
	int		i;
	int		j;
	xfs_dinode_t	*dip;

	j = mp->m_inode_cluster_size >> mp->m_sb.sb_inodelog;

	for (i = 0; i < j; i++) {
		dip = (xfs_dinode_t *)xfs_buf_offset(bp,
					i * mp->m_sb.sb_inodesize);
		if (INT_ISZERO(dip->di_next_unlinked, ARCH_CONVERT))  {
			xfs_fs_cmn_err(CE_ALERT, mp,
				"Detected a bogus zero next_unlinked field in incore inode buffer 0x%p.  About to pop an ASSERT.",
				bp);
			ASSERT(!INT_ISZERO(dip->di_next_unlinked, ARCH_CONVERT));
		}
	}
}
#endif


/*
 * This routine is called to map an inode to the buffer containing
 * the on-disk version of the inode.  It returns a pointer to the
 * buffer containing the on-disk inode in the bpp parameter, and in
 * the dip parameter it returns a pointer to the on-disk inode within
 * that buffer.
 *
 * If a non-zero error is returned, then the contents of bpp and
 * dipp are undefined.
 *
 * If the inode is new and has not yet been initialized, use xfs_imap()
 * to determine the size and location of the buffer to read from disk.
 * If the inode has already been mapped to its buffer and read in once,
 * then use the mapping information stored in the inode rather than
 * calling xfs_imap().  This allows us to avoid the overhead of looking
 * at the inode btree for small block file systems (see xfs_dilocate()).
 * We can tell whether the inode has been mapped in before by comparing
 * its disk block address to 0.  Only uninitialized inodes will have
 * 0 for the disk block address.
 */
int
xfs_itobp(
	xfs_mount_t	*mp,
	xfs_trans_t	*tp,
	xfs_inode_t	*ip,	
	xfs_dinode_t	**dipp,
	xfs_buf_t	**bpp,
	xfs_daddr_t	bno)
{
	xfs_buf_t	*bp;
	int		error;
	xfs_imap_t	imap;
#ifdef __KERNEL__
	int		i;
	int		ni;
#endif

	if (ip->i_blkno == (xfs_daddr_t)0) {
		/*
		 * Call the space management code to find the location of the
		 * inode on disk.
		 */
		imap.im_blkno = bno;
		error = xfs_imap(mp, tp, ip->i_ino, &imap, XFS_IMAP_LOOKUP);
		if (error != 0) {
			return error;
		}

		/*
		 * If the inode number maps to a block outside the bounds
		 * of the file system then return NULL rather than calling
		 * read_buf and panicing when we get an error from the
		 * driver.
		 */
		if ((imap.im_blkno + imap.im_len) >
		    XFS_FSB_TO_BB(mp, mp->m_sb.sb_dblocks)) {
			return XFS_ERROR(EINVAL);
		}

		/*
		 * Fill in the fields in the inode that will be used to
		 * map the inode to its buffer from now on.
		 */
		ip->i_blkno = imap.im_blkno;
		ip->i_len = imap.im_len;
		ip->i_boffset = imap.im_boffset;
	} else {
		/*
		 * We've already mapped the inode once, so just use the
		 * mapping that we saved the first time.
		 */
		imap.im_blkno = ip->i_blkno;
		imap.im_len = ip->i_len;
		imap.im_boffset = ip->i_boffset;
	}
	ASSERT(bno == 0 || bno == imap.im_blkno);

	/*
	 * Read in the buffer.  If tp is NULL, xfs_trans_read_buf() will
	 * default to just a read_buf() call.
	 */
	error = xfs_trans_read_buf(mp, tp, mp->m_ddev_targp, imap.im_blkno,
				   (int)imap.im_len, XFS_BUF_LOCK, &bp);

	if (error) {
		return error;
	}
#ifdef __KERNEL__
	/*
	 * Validate the magic number and version of every inode in the buffer
	 * (if DEBUG kernel) or the first inode in the buffer, otherwise.
	 */
#ifdef DEBUG
	ni = BBTOB(imap.im_len) >> mp->m_sb.sb_inodelog;
#else
	ni = 1;
#endif
	for (i = 0; i < ni; i++) {
		int		di_ok;
		xfs_dinode_t	*dip;

		dip = (xfs_dinode_t *)xfs_buf_offset(bp,
					(i << mp->m_sb.sb_inodelog));
		di_ok = INT_GET(dip->di_core.di_magic, ARCH_CONVERT) == XFS_DINODE_MAGIC &&
			    XFS_DINODE_GOOD_VERSION(INT_GET(dip->di_core.di_version, ARCH_CONVERT));
		if (XFS_TEST_ERROR(!di_ok, mp, XFS_ERRTAG_ITOBP_INOTOBP,
				 XFS_RANDOM_ITOBP_INOTOBP)) {
#ifdef DEBUG
			prdev("bad inode magic/vsn daddr 0x%Lx #%d (magic=%x)", 
				mp->m_dev, imap.im_blkno, i,
				INT_GET(dip->di_core.di_magic, ARCH_CONVERT));
#endif
			xfs_trans_brelse(tp, bp);
			return XFS_ERROR(EFSCORRUPTED);
		}
	}
#endif	/* __KERNEL__ */

	xfs_inobp_check(mp, bp);

	/*
	 * Mark the buffer as an inode buffer now that it looks good
	 */
	XFS_BUF_SET_VTYPE(bp, B_FS_INO);

	/*
	 * Set *dipp to point to the on-disk inode in the buffer.
	 */
	*dipp = (xfs_dinode_t *)xfs_buf_offset(bp, imap.im_boffset);
	*bpp = bp;
	return 0;
}

/*
 * Move inode type and inode format specific information from the
 * on-disk inode to the in-core inode.  For fifos, devs, and sockets
 * this means set if_rdev to the proper value.  For files, directories,
 * and symlinks this means to bring in the in-line data or extent
 * pointers.  For a file in B-tree format, only the root is immediately
 * brought in-core.  The rest will be in-lined in if_extents when it
 * is first referenced (see xfs_iread_extents()).
 */
STATIC int
xfs_iformat(
	xfs_inode_t		*ip,
	xfs_dinode_t		*dip)
{
	xfs_attr_shortform_t	*atp;
	int			size;
	int			error;
        xfs_fsize_t             di_size;
	ip->i_df.if_ext_max =
		XFS_IFORK_DSIZE(ip) / (uint)sizeof(xfs_bmbt_rec_t);
	error = 0;

	if (INT_GET(dip->di_core.di_nextents, ARCH_CONVERT) + 
                INT_GET(dip->di_core.di_anextents, ARCH_CONVERT) >
	    INT_GET(dip->di_core.di_nblocks, ARCH_CONVERT)) {
		xfs_fs_cmn_err(CE_WARN, ip->i_mount,
			"corrupt dinode %Lu, extent total = %d, nblocks = %Ld.  Unmount and run xfs_repair.",
			ip->i_ino,
			(int)(INT_GET(dip->di_core.di_nextents, ARCH_CONVERT) + INT_GET(dip->di_core.di_anextents, ARCH_CONVERT)),
			INT_GET(dip->di_core.di_nblocks, ARCH_CONVERT));
		return XFS_ERROR(EFSCORRUPTED);
	}

	if (INT_GET(dip->di_core.di_forkoff, ARCH_CONVERT) > ip->i_mount->m_sb.sb_inodesize) {
		xfs_fs_cmn_err(CE_WARN, ip->i_mount,
			"corrupt dinode %Lu, forkoff = 0x%x.  Unmount and run xfs_repair.",
			ip->i_ino, (int)(INT_GET(dip->di_core.di_forkoff, ARCH_CONVERT)));
		return XFS_ERROR(EFSCORRUPTED);
	}

	switch (ip->i_d.di_mode & IFMT) {
	case IFIFO:
	case IFCHR:
	case IFBLK:
	case IFSOCK:
		if (INT_GET(dip->di_core.di_format, ARCH_CONVERT) != XFS_DINODE_FMT_DEV)
			return XFS_ERROR(EFSCORRUPTED);
		ip->i_d.di_size = 0;
		ip->i_df.if_u2.if_rdev = INT_GET(dip->di_u.di_dev, ARCH_CONVERT);
		break;

	case IFREG:
	case IFLNK:
	case IFDIR:
		switch (INT_GET(dip->di_core.di_format, ARCH_CONVERT)) {
		case XFS_DINODE_FMT_LOCAL:
			/*
			 * no local regular files yet
			 */
			if ((INT_GET(dip->di_core.di_mode, ARCH_CONVERT) & IFMT) == IFREG) {
				xfs_fs_cmn_err(CE_WARN, ip->i_mount,
					"corrupt inode (local format for regular file) %Lu.  Unmount and run xfs_repair.",
					ip->i_ino);
				return XFS_ERROR(EFSCORRUPTED);
			}
                        
                        di_size=INT_GET(dip->di_core.di_size, ARCH_CONVERT);
			if (di_size >
			    XFS_DFORK_DSIZE_ARCH(dip, ip->i_mount, ARCH_CONVERT)) {
				xfs_fs_cmn_err(CE_WARN, ip->i_mount,
					"corrupt inode %Lu (bad size %Ld for local inode).  Unmount and run xfs_repair.",
					ip->i_ino, di_size);
				return XFS_ERROR(EFSCORRUPTED);
			}

			size = (int)di_size;
			error = xfs_iformat_local(ip, dip, XFS_DATA_FORK, size);
			break;
		case XFS_DINODE_FMT_EXTENTS:
			error = xfs_iformat_extents(ip, dip, XFS_DATA_FORK);
			break;
		case XFS_DINODE_FMT_BTREE:
			error = xfs_iformat_btree(ip, dip, XFS_DATA_FORK);
			break;
		default:
			return XFS_ERROR(EFSCORRUPTED);
		}
		break;

	default:
		return XFS_ERROR(EFSCORRUPTED);
	}
	if (error) {
		return error;
        }
	if (!XFS_DFORK_Q_ARCH(dip, ARCH_CONVERT))
		return 0;
	ASSERT(ip->i_afp == NULL);
	ip->i_afp = kmem_zone_zalloc(xfs_ifork_zone, KM_SLEEP);
	ip->i_afp->if_ext_max =
		XFS_IFORK_ASIZE(ip) / (uint)sizeof(xfs_bmbt_rec_t);
	switch (INT_GET(dip->di_core.di_aformat, ARCH_CONVERT)) {
	case XFS_DINODE_FMT_LOCAL:
		atp = (xfs_attr_shortform_t *)XFS_DFORK_APTR_ARCH(dip, ARCH_CONVERT);
		size = (int)INT_GET(atp->hdr.totsize, ARCH_CONVERT);
		error = xfs_iformat_local(ip, dip, XFS_ATTR_FORK, size);
		break;
	case XFS_DINODE_FMT_EXTENTS:
		error = xfs_iformat_extents(ip, dip, XFS_ATTR_FORK);
		break;
	case XFS_DINODE_FMT_BTREE:
		error = xfs_iformat_btree(ip, dip, XFS_ATTR_FORK);
		break;
	default:
		error = XFS_ERROR(EFSCORRUPTED);
		break;
	}
	if (error) {
		kmem_zone_free(xfs_ifork_zone, ip->i_afp);
		ip->i_afp = NULL;
		xfs_idestroy_fork(ip, XFS_DATA_FORK);
	}
	return error;
}

/*
 * The file is in-lined in the on-disk inode.
 * If it fits into if_inline_data, then copy
 * it there, otherwise allocate a buffer for it
 * and copy the data there.  Either way, set
 * if_data to point at the data.
 * If we allocate a buffer for the data, make
 * sure that its size is a multiple of 4 and
 * record the real size in i_real_bytes.
 */
STATIC int
xfs_iformat_local(
	xfs_inode_t	*ip,
	xfs_dinode_t	*dip,
	int		whichfork,
	int		size)
{
	xfs_ifork_t	*ifp;
	int		real_size;

	/*
	 * If the size is unreasonable, then something
	 * is wrong and we just bail out rather than crash in
	 * kmem_alloc() or bcopy() below.
	 */
	if (size > XFS_DFORK_SIZE_ARCH(dip, ip->i_mount, whichfork, ARCH_CONVERT)) {
		xfs_fs_cmn_err(CE_WARN, ip->i_mount,
			"corrupt inode %Lu (bad size %d for local fork, size = %d).  Unmount and run xfs_repair.",
			ip->i_ino, size,
			XFS_DFORK_SIZE_ARCH(dip, ip->i_mount, whichfork, ARCH_CONVERT));
		return XFS_ERROR(EFSCORRUPTED);
	}
	ifp = XFS_IFORK_PTR(ip, whichfork);
	real_size = 0;
	if (size == 0)
		ifp->if_u1.if_data = NULL;
	else if (size <= sizeof(ifp->if_u2.if_inline_data))
		ifp->if_u1.if_data = ifp->if_u2.if_inline_data;
	else {
		real_size = roundup(size, 4);
		ifp->if_u1.if_data = kmem_alloc(real_size, KM_SLEEP);
	}
	ifp->if_bytes = size;
	ifp->if_real_bytes = real_size;
	if (size)
		bcopy(XFS_DFORK_PTR_ARCH(dip, whichfork, ARCH_CONVERT), ifp->if_u1.if_data, size);
	ifp->if_flags &= ~XFS_IFEXTENTS;
	ifp->if_flags |= XFS_IFINLINE;
	return 0;
}

/*
 * The file consists of a set of extents all
 * of which fit into the on-disk inode.
 * If there are few enough extents to fit into
 * the if_inline_ext, then copy them there.
 * Otherwise allocate a buffer for them and copy
 * them into it.  Either way, set if_extents
 * to point at the extents.
 */
STATIC int
xfs_iformat_extents(
	xfs_inode_t	*ip,
	xfs_dinode_t	*dip,
	int		whichfork)
{
	xfs_ifork_t	*ifp;
	int		nex;
	int		real_size;
	int		size;

	ifp = XFS_IFORK_PTR(ip, whichfork);
	nex = XFS_DFORK_NEXTENTS_ARCH(dip, whichfork, ARCH_CONVERT);
	size = nex * (uint)sizeof(xfs_bmbt_rec_t);

	/*
	 * If the number of extents is unreasonable, then something
	 * is wrong and we just bail out rather than crash in
	 * kmem_alloc() or bcopy() below.
	 */
	if (size < 0 || size > XFS_DFORK_SIZE_ARCH(dip, ip->i_mount, whichfork, ARCH_CONVERT)) {
		xfs_fs_cmn_err(CE_WARN, ip->i_mount,
			"corrupt inode %Lu ((a)extents = %d).  Unmount and run xfs_repair.",
			ip->i_ino, nex);
		return XFS_ERROR(EFSCORRUPTED);
	}

	real_size = 0;
	if (nex == 0)
		ifp->if_u1.if_extents = NULL;
	else if (nex <= XFS_INLINE_EXTS)
		ifp->if_u1.if_extents = ifp->if_u2.if_inline_ext;
	else {
		ifp->if_u1.if_extents = kmem_alloc(size, KM_SLEEP);
		ASSERT(ifp->if_u1.if_extents != NULL);
		real_size = size;
	}
	ifp->if_bytes = size;
	ifp->if_real_bytes = real_size;
	if (size) {
		xfs_validate_extents(
			(xfs_bmbt_rec_32_t *)XFS_DFORK_PTR_ARCH(dip, whichfork, ARCH_CONVERT),
			nex, XFS_EXTFMT_INODE(ip));
		bcopy(XFS_DFORK_PTR_ARCH(dip, whichfork, ARCH_CONVERT), ifp->if_u1.if_extents,
		      size);
		xfs_bmap_trace_exlist("xfs_iformat_extents", ip, nex,
			whichfork);
		if (whichfork != XFS_DATA_FORK ||
			XFS_EXTFMT_INODE(ip) == XFS_EXTFMT_NOSTATE)
				if (xfs_check_nostate_extents(
				    ifp->if_u1.if_extents, nex))
					return XFS_ERROR(EFSCORRUPTED);
	}
	ifp->if_flags |= XFS_IFEXTENTS;
	return 0;
}

/*
 * The file has too many extents to fit into
 * the inode, so they are in B-tree format.
 * Allocate a buffer for the root of the B-tree
 * and copy the root into it.  The i_extents
 * field will remain NULL until all of the
 * extents are read in (when they are needed).
 */
STATIC int
xfs_iformat_btree(
	xfs_inode_t		*ip,
	xfs_dinode_t		*dip,
	int			whichfork)
{
	xfs_bmdr_block_t	*dfp;
	xfs_ifork_t		*ifp;
	/* REFERENCED */
	int			nrecs;
	int			size;

	ifp = XFS_IFORK_PTR(ip, whichfork);
	dfp = (xfs_bmdr_block_t *)XFS_DFORK_PTR_ARCH(dip, whichfork, ARCH_CONVERT);
	size = XFS_BMAP_BROOT_SPACE(dfp);
	nrecs = XFS_BMAP_BROOT_NUMRECS(dfp);

	/*
	 * blow out if -- fork has less extents than can fit in
	 * fork (fork shouldn't be a btree format), root btree
	 * block has more records than can fit into the fork,
	 * or the number of extents is greater than the number of
	 * blocks.
	 */
	if (XFS_IFORK_NEXTENTS(ip, whichfork) <= ifp->if_ext_max
	    || XFS_BMDR_SPACE_CALC(nrecs) >
			XFS_DFORK_SIZE_ARCH(dip, ip->i_mount, whichfork, ARCH_CONVERT)
	    || XFS_IFORK_NEXTENTS(ip, whichfork) > ip->i_d.di_nblocks) {
		xfs_fs_cmn_err(CE_WARN, ip->i_mount,
			"corrupt inode %Lu (btree).  Unmount and run xfs_repair.",
			ip->i_ino);
		return XFS_ERROR(EFSCORRUPTED);
	}

	ifp->if_broot_bytes = size;
	ifp->if_broot = kmem_alloc(size, KM_SLEEP);
	ASSERT(ifp->if_broot != NULL);
	/*
	 * Copy and convert from the on-disk structure
	 * to the in-memory structure.
	 */
	xfs_bmdr_to_bmbt(dfp, XFS_DFORK_SIZE_ARCH(dip, ip->i_mount, whichfork, ARCH_CONVERT),
		ifp->if_broot, size);
	ifp->if_flags &= ~XFS_IFEXTENTS;
	ifp->if_flags |= XFS_IFBROOT;

	return 0;
}

/*
 * xfs_xlate_dinode_core - translate an xfs_inode_core_t between ondisk
 * and native format
 *
 * buf  = on-disk representation 
 * dip  = native representation 
 * dir  = direction - +ve -> disk to native
 *                    -ve -> native to disk
 * arch = on-disk architecture
 */
 
void 
xfs_xlate_dinode_core(xfs_caddr_t buf, xfs_dinode_core_t *dip, 
    int dir, xfs_arch_t arch)
{
    xfs_dinode_core_t   *buf_core;
    xfs_dinode_core_t   *mem_core;
    
    ASSERT(dir);
    
    buf_core=(xfs_dinode_core_t*)buf;
    mem_core=(xfs_dinode_core_t*)dip;
    
    if (arch == ARCH_NOCONVERT) {
        if (dir>0) {
            bcopy((xfs_caddr_t)buf_core, (xfs_caddr_t)mem_core, sizeof(xfs_dinode_core_t));
        } else {
            bcopy((xfs_caddr_t)mem_core, (xfs_caddr_t)buf_core, sizeof(xfs_dinode_core_t));
        }
        return;
    }
    
    INT_XLATE(buf_core->di_magic,       mem_core->di_magic,        dir, arch);
    INT_XLATE(buf_core->di_mode,        mem_core->di_mode,         dir, arch);
    INT_XLATE(buf_core->di_version,     mem_core->di_version,      dir, arch);
    INT_XLATE(buf_core->di_format,      mem_core->di_format,       dir, arch);
    INT_XLATE(buf_core->di_onlink,      mem_core->di_onlink,       dir, arch);
    INT_XLATE(buf_core->di_uid,         mem_core->di_uid,          dir, arch);
    INT_XLATE(buf_core->di_gid,         mem_core->di_gid,          dir, arch);
    INT_XLATE(buf_core->di_nlink,       mem_core->di_nlink,        dir, arch);
    INT_XLATE(buf_core->di_projid,      mem_core->di_projid,       dir, arch);
    
    if (dir>0) {
        bcopy(buf_core->di_pad, mem_core->di_pad, sizeof(buf_core->di_pad));
    } else {
        bcopy(mem_core->di_pad, buf_core->di_pad, sizeof(buf_core->di_pad));
    }
    
    INT_XLATE(buf_core->di_atime.t_sec, mem_core->di_atime.t_sec,  dir, arch);
    INT_XLATE(buf_core->di_atime.t_nsec,mem_core->di_atime.t_nsec, dir, arch);
    
    INT_XLATE(buf_core->di_mtime.t_sec, mem_core->di_mtime.t_sec,  dir, arch);
    INT_XLATE(buf_core->di_mtime.t_nsec,mem_core->di_mtime.t_nsec, dir, arch);
    
    INT_XLATE(buf_core->di_ctime.t_sec, mem_core->di_ctime.t_sec,  dir, arch);
    INT_XLATE(buf_core->di_ctime.t_nsec,mem_core->di_ctime.t_nsec, dir, arch);
    
    INT_XLATE(buf_core->di_size,        mem_core->di_size,         dir, arch);
    INT_XLATE(buf_core->di_nblocks,     mem_core->di_nblocks,      dir, arch);
    INT_XLATE(buf_core->di_extsize,     mem_core->di_extsize,      dir, arch);
    
    INT_XLATE(buf_core->di_nextents,    mem_core->di_nextents,     dir, arch);
    INT_XLATE(buf_core->di_anextents,   mem_core->di_anextents,    dir, arch);
    INT_XLATE(buf_core->di_forkoff,     mem_core->di_forkoff,      dir, arch);
    INT_XLATE(buf_core->di_aformat,     mem_core->di_aformat,      dir, arch);
    INT_XLATE(buf_core->di_dmevmask,    mem_core->di_dmevmask,     dir, arch);
    INT_XLATE(buf_core->di_dmstate,     mem_core->di_dmstate,      dir, arch);
    INT_XLATE(buf_core->di_flags,       mem_core->di_flags,        dir, arch);
    INT_XLATE(buf_core->di_gen,         mem_core->di_gen,          dir, arch);
    
}

/*
 * Given a mount structure and an inode number, return a pointer
 * to a newly allocated in-core inode coresponding to the given
 * inode number.
 * 
 * Initialize the inode's attributes and extent pointers if it
 * already has them (it will not if the inode has no links).
 */
int
xfs_iread(
	xfs_mount_t	*mp,
	xfs_trans_t	*tp,
	xfs_ino_t	ino,
	xfs_inode_t	**ipp,
	xfs_daddr_t		bno)
{
	xfs_buf_t	*bp;
	xfs_dinode_t	*dip;
	xfs_inode_t	*ip;
	int		error;

	ASSERT(xfs_inode_zone != NULL);

	ip = kmem_zone_zalloc(xfs_inode_zone, KM_SLEEP);
	ip->i_ino = ino;
	ip->i_dev = mp->m_dev;
	ip->i_mount = mp;

	/*
	 * Get pointer's to the on-disk inode and the buffer containing it.
	 * If the inode number refers to a block outside the file system
	 * then xfs_itobp() will return NULL.  In this case we should
	 * return NULL as well.  Set i_blkno to 0 so that xfs_itobp() will
	 * know that this is a new incore inode.
	 */
	error = xfs_itobp(mp, tp, ip, &dip, &bp, bno);

	if (error != 0) {
		kmem_zone_free(xfs_inode_zone, ip);
		return error;
	}

	/*
	 * Initialize inode's trace buffers.
	 * Do this before xfs_iformat in case it adds entries.
	 */
#ifdef XFS_BMAP_TRACE
	ip->i_xtrace = ktrace_alloc(XFS_BMAP_KTRACE_SIZE, KM_SLEEP);
#endif
#ifdef XFS_BMBT_TRACE
	ip->i_btrace = ktrace_alloc(XFS_BMBT_KTRACE_SIZE, KM_SLEEP);
#endif
#ifdef XFS_RW_TRACE
	ip->i_rwtrace = ktrace_alloc(XFS_RW_KTRACE_SIZE, KM_SLEEP);
#endif
#ifdef XFS_STRAT_TRACE
	ip->i_strat_trace = ktrace_alloc(XFS_STRAT_KTRACE_SIZE, KM_SLEEP);
#endif
#ifdef XFS_ILOCK_TRACE
	ip->i_lock_trace = ktrace_alloc(XFS_ILOCK_KTRACE_SIZE, KM_SLEEP);
#endif
#ifdef XFS_DIR2_TRACE
	ip->i_dir_trace = ktrace_alloc(XFS_DIR2_KTRACE_SIZE, KM_SLEEP);
#endif

	/*
	 * If we got something that isn't an inode it means someone
	 * (nfs or dmi) has a stale handle.
	 */
        if (INT_GET(dip->di_core.di_magic, ARCH_CONVERT) != XFS_DINODE_MAGIC) {
		kmem_zone_free(xfs_inode_zone, ip);
		xfs_trans_brelse(tp, bp);
		return XFS_ERROR(EINVAL);
	}

	/*
	 * If the on-disk inode is already linked to a directory
	 * entry, copy all of the inode into the in-core inode.
	 * xfs_iformat() handles copying in the inode format
	 * specific information.
	 * Otherwise, just get the truly permanent information.
	 */
	if (!INT_ISZERO(dip->di_core.di_mode, ARCH_CONVERT)) {
                xfs_xlate_dinode_core((xfs_caddr_t)&dip->di_core, 
                     &(ip->i_d), 1, ARCH_CONVERT);
		error = xfs_iformat(ip, dip);
		if (error)  {
			kmem_zone_free(xfs_inode_zone, ip);
			xfs_trans_brelse(tp, bp);
			return error;
		}
	} else {
		ip->i_d.di_magic = INT_GET(dip->di_core.di_magic, ARCH_CONVERT);
		ip->i_d.di_version = INT_GET(dip->di_core.di_version, ARCH_CONVERT);
		ip->i_d.di_gen = INT_GET(dip->di_core.di_gen, ARCH_CONVERT);
		/*
		 * Make sure to pull in the mode here as well in
		 * case the inode is released without being used.
		 * This ensures that xfs_inactive() will see that
		 * the inode is already free and not try to mess
		 * with the uninitialized part of it.
		 */
		ip->i_d.di_mode = 0;
		/*
		 * Initialize the per-fork minima and maxima for a new
		 * inode here.  xfs_iformat will do it for old inodes.
		 */
		ip->i_df.if_ext_max =
			XFS_IFORK_DSIZE(ip) / (uint)sizeof(xfs_bmbt_rec_t);
	}	

	/*
	 * The inode format changed when we moved the link count and
	 * made it 32 bits long.  If this is an old format inode,
	 * convert it in memory to look like a new one.  If it gets
	 * flushed to disk we will convert back before flushing or
	 * logging it.  We zero out the new projid field and the old link
	 * count field.  We'll handle clearing the pad field (the remains
	 * of the old uuid field) when we actually convert the inode to
	 * the new format. We don't change the version number so that we
	 * can distinguish this from a real new format inode.
	 */
	if (ip->i_d.di_version == XFS_DINODE_VERSION_1) {
		ip->i_d.di_nlink = ip->i_d.di_onlink;
		ip->i_d.di_onlink = 0;
		ip->i_d.di_projid = 0;
	}

	ip->i_delayed_blks = 0;

	/*
	 * Mark the buffer containing the inode as something to keep
	 * around for a while.  This helps to keep recently accessed
	 * meta-data in-core longer.
	 */
	 XFS_BUF_SET_REF(bp, XFS_INO_REF);

	/*
	 * Use xfs_trans_brelse() to release the buffer containing the
	 * on-disk inode, because it was acquired with xfs_trans_read_buf()
	 * in xfs_itobp() above.  If tp is NULL, this is just a normal
	 * brelse().  If we're within a transaction, then xfs_trans_brelse()
	 * will only release the buffer if it is not dirty within the
	 * transaction.  It will be OK to release the buffer in this case,
	 * because inodes on disk are never destroyed and we will be
	 * locking the new in-core inode before putting it in the hash
	 * table where other processes can find it.  Thus we don't have
	 * to worry about the inode being changed just because we released
	 * the buffer.
	 */
	xfs_trans_brelse(tp, bp);
	*ipp = ip;
	return 0;
}

/*
 * Read in extents from a btree-format inode.
 * Allocate and fill in if_extents.  Real work is done in xfs_bmap.c.
 */
int
xfs_iread_extents(
	xfs_trans_t	*tp,
	xfs_inode_t	*ip,
	int		whichfork)
{
	int		error;
	xfs_ifork_t	*ifp;
	size_t		size;

	if (XFS_IFORK_FORMAT(ip, whichfork) != XFS_DINODE_FMT_BTREE)
		return XFS_ERROR(EFSCORRUPTED);
	size = XFS_IFORK_NEXTENTS(ip, whichfork) * (uint)sizeof(xfs_bmbt_rec_t);
	ifp = XFS_IFORK_PTR(ip, whichfork);
	/*
	 * We know that the size is legal (it's checked in iformat_btree)
	 */
	ifp->if_u1.if_extents = kmem_alloc(size, KM_SLEEP);
	ASSERT(ifp->if_u1.if_extents != NULL);
	ifp->if_lastex = NULLEXTNUM;
	ifp->if_bytes = ifp->if_real_bytes = (int)size;
	ifp->if_flags |= XFS_IFEXTENTS;
	error = xfs_bmap_read_extents(tp, ip, whichfork);
	if (error) {
		kmem_free(ifp->if_u1.if_extents, size);
		ifp->if_u1.if_extents = NULL;
		ifp->if_bytes = ifp->if_real_bytes = 0;
		ifp->if_flags &= ~XFS_IFEXTENTS;
		return error;
	}
	xfs_validate_extents((xfs_bmbt_rec_32_t *)ifp->if_u1.if_extents,
		XFS_IFORK_NEXTENTS(ip, whichfork), XFS_EXTFMT_INODE(ip));
	return 0;
}

/*
 * Reallocate the space for if_broot based on the number of records
 * being added or deleted as indicated in rec_diff.  Move the records
 * and pointers in if_broot to fit the new size.  When shrinking this
 * will eliminate holes between the records and pointers created by
 * the caller.  When growing this will create holes to be filled in
 * by the caller.
 *
 * The caller must not request to add more records than would fit in
 * the on-disk inode root.  If the if_broot is currently NULL, then
 * if we adding records one will be allocated.  The caller must also
 * not request that the number of records go below zero, although
 * it can go to zero.
 *
 * ip -- the inode whose if_broot area is changing
 * ext_diff -- the change in the number of records, positive or negative,
 *	 requested for the if_broot array.
 */
void
xfs_iroot_realloc(
	xfs_inode_t 		*ip,
	int 			rec_diff,
	int			whichfork)
{
	int			cur_max;
	xfs_ifork_t		*ifp;
	xfs_bmbt_block_t	*new_broot;
	int			new_max;
	size_t			new_size;
	char			*np;
	char			*op;

	/*
	 * Handle the degenerate case quietly.
	 */
	if (rec_diff == 0) {
		return;
	}

	ifp = XFS_IFORK_PTR(ip, whichfork);
	if (rec_diff > 0) {
		/*
		 * If there wasn't any memory allocated before, just
		 * allocate it now and get out.
		 */
		if (ifp->if_broot_bytes == 0) {
			new_size = (size_t)XFS_BMAP_BROOT_SPACE_CALC(rec_diff);
			ifp->if_broot = (xfs_bmbt_block_t*)kmem_alloc(new_size,
								     KM_SLEEP);
			ifp->if_broot_bytes = (int)new_size;
			return;
		}

		/*
		 * If there is already an existing if_broot, then we need
		 * to realloc() it and shift the pointers to their new
		 * location.  The records don't change location because
		 * they are kept butted up against the btree block header.
		 */
		cur_max = XFS_BMAP_BROOT_MAXRECS(ifp->if_broot_bytes);
		new_max = cur_max + rec_diff;
		new_size = (size_t)XFS_BMAP_BROOT_SPACE_CALC(new_max);
		ifp->if_broot = (xfs_bmbt_block_t *) 
		  kmem_realloc(ifp->if_broot,
				new_size,
				(size_t)XFS_BMAP_BROOT_SPACE_CALC(cur_max), /* old size */
				KM_SLEEP);
		op = (char *)XFS_BMAP_BROOT_PTR_ADDR(ifp->if_broot, 1,
						      ifp->if_broot_bytes);
		np = (char *)XFS_BMAP_BROOT_PTR_ADDR(ifp->if_broot, 1,
						      (int)new_size);
		ifp->if_broot_bytes = (int)new_size;
		ASSERT(ifp->if_broot_bytes <=
			XFS_IFORK_SIZE(ip, whichfork) + XFS_BROOT_SIZE_ADJ);
		ovbcopy(op, np, cur_max * (uint)sizeof(xfs_dfsbno_t));
		return;
	}

	/*
	 * rec_diff is less than 0.  In this case, we are shrinking the
	 * if_broot buffer.  It must already exist.  If we go to zero
	 * records, just get rid of the root and clear the status bit.
	 */
	ASSERT((ifp->if_broot != NULL) && (ifp->if_broot_bytes > 0));
	cur_max = XFS_BMAP_BROOT_MAXRECS(ifp->if_broot_bytes);
	new_max = cur_max + rec_diff;
	ASSERT(new_max >= 0);
	if (new_max > 0)
		new_size = (size_t)XFS_BMAP_BROOT_SPACE_CALC(new_max);
	else
		new_size = 0;
	if (new_size > 0) {
		new_broot = (xfs_bmbt_block_t *)kmem_alloc(new_size, KM_SLEEP);
		/*
		 * First copy over the btree block header.
		 */
		bcopy(ifp->if_broot, new_broot, sizeof(xfs_bmbt_block_t));
	} else {
		new_broot = NULL;
		ifp->if_flags &= ~XFS_IFBROOT;
	}

	/*
	 * Only copy the records and pointers if there are any.
	 */
	if (new_max > 0) {
		/*
		 * First copy the records.
		 */
		op = (char *)XFS_BMAP_BROOT_REC_ADDR(ifp->if_broot, 1,
						     ifp->if_broot_bytes);
		np = (char *)XFS_BMAP_BROOT_REC_ADDR(new_broot, 1,
						     (int)new_size);
		bcopy(op, np, new_max * (uint)sizeof(xfs_bmbt_rec_t));	

		/*
		 * Then copy the pointers.
		 */
		op = (char *)XFS_BMAP_BROOT_PTR_ADDR(ifp->if_broot, 1,
						     ifp->if_broot_bytes);
		np = (char *)XFS_BMAP_BROOT_PTR_ADDR(new_broot, 1,
						     (int)new_size);
		bcopy(op, np, new_max * (uint)sizeof(xfs_dfsbno_t));
	}
	kmem_free(ifp->if_broot, ifp->if_broot_bytes);
	ifp->if_broot = new_broot;
	ifp->if_broot_bytes = (int)new_size;
	ASSERT(ifp->if_broot_bytes <=
		XFS_IFORK_SIZE(ip, whichfork) + XFS_BROOT_SIZE_ADJ);
	return;
}

/*
 * This is called when the amount of space needed for if_extents
 * is increased or decreased.  The change in size is indicated by
 * the number of extents that need to be added or deleted in the
 * ext_diff parameter.
 *
 * If the amount of space needed has decreased below the size of the
 * inline buffer, then switch to using the inline buffer.  Otherwise,
 * use kmem_realloc() or kmem_alloc() to adjust the size of the buffer
 * to what is needed.
 *
 * ip -- the inode whose if_extents area is changing
 * ext_diff -- the change in the number of extents, positive or negative,
 *	 requested for the if_extents array.
 */
void
xfs_iext_realloc(
	xfs_inode_t	*ip,
	int		ext_diff,
	int		whichfork)
{
	int		byte_diff;
	xfs_ifork_t	*ifp;
	int		new_size;
	uint		rnew_size;

	if (ext_diff == 0) {
		return;
	}

	ifp = XFS_IFORK_PTR(ip, whichfork);
	byte_diff = ext_diff * (uint)sizeof(xfs_bmbt_rec_t);
	new_size = (int)ifp->if_bytes + byte_diff;
	ASSERT(new_size >= 0);

	if (new_size == 0) {
		if (ifp->if_u1.if_extents != ifp->if_u2.if_inline_ext) {
			ASSERT(ifp->if_real_bytes != 0);
			kmem_free(ifp->if_u1.if_extents, ifp->if_real_bytes);
		}
		ifp->if_u1.if_extents = NULL;
		rnew_size = 0;
	} else if (new_size <= sizeof(ifp->if_u2.if_inline_ext)) {
		/*
		 * If the valid extents can fit in if_inline_ext,
		 * copy them from the malloc'd vector and free it.
		 */
		if (ifp->if_u1.if_extents != ifp->if_u2.if_inline_ext) {
			/*
			 * For now, empty files are format EXTENTS,
			 * so the if_extents pointer is null.
			 */
			if (ifp->if_u1.if_extents) {
				bcopy(ifp->if_u1.if_extents,
				      ifp->if_u2.if_inline_ext, new_size);
				kmem_free(ifp->if_u1.if_extents,
					  ifp->if_real_bytes);
			}
			ifp->if_u1.if_extents = ifp->if_u2.if_inline_ext;
		}
		rnew_size = 0;
	} else {
		rnew_size = new_size;
		if ((rnew_size & (rnew_size - 1)) != 0)
			rnew_size = xfs_iroundup(rnew_size);
		/*
		 * Stuck with malloc/realloc.
		 */
		if (ifp->if_u1.if_extents == ifp->if_u2.if_inline_ext) {
			ifp->if_u1.if_extents = (xfs_bmbt_rec_t *)
				kmem_alloc(rnew_size, KM_SLEEP);
			bcopy(ifp->if_u2.if_inline_ext, ifp->if_u1.if_extents,
			      sizeof(ifp->if_u2.if_inline_ext));
		} else if (rnew_size != ifp->if_real_bytes) {
			ifp->if_u1.if_extents = (xfs_bmbt_rec_t *)
			  kmem_realloc(ifp->if_u1.if_extents,
					rnew_size,
					ifp->if_real_bytes,
					KM_SLEEP);
		}
	}
	ifp->if_real_bytes = rnew_size;
	ifp->if_bytes = new_size;
}


/*
 * This is called when the amount of space needed for if_data
 * is increased or decreased.  The change in size is indicated by
 * the number of bytes that need to be added or deleted in the
 * byte_diff parameter.
 *
 * If the amount of space needed has decreased below the size of the
 * inline buffer, then switch to using the inline buffer.  Otherwise,
 * use kmem_realloc() or kmem_alloc() to adjust the size of the buffer
 * to what is needed.
 *
 * ip -- the inode whose if_data area is changing
 * byte_diff -- the change in the number of bytes, positive or negative,
 *	 requested for the if_data array.
 */
void
xfs_idata_realloc(
	xfs_inode_t	*ip,
	int		byte_diff,
	int		whichfork)
{
	xfs_ifork_t	*ifp;
	int		new_size;
	int		real_size;

	if (byte_diff == 0) {
		return;
	}

	ifp = XFS_IFORK_PTR(ip, whichfork);
	new_size = (int)ifp->if_bytes + byte_diff;
	ASSERT(new_size >= 0);

	if (new_size == 0) {
		if (ifp->if_u1.if_data != ifp->if_u2.if_inline_data) {
			kmem_free(ifp->if_u1.if_data, ifp->if_real_bytes);
		}
		ifp->if_u1.if_data = NULL;
		real_size = 0;
	} else if (new_size <= sizeof(ifp->if_u2.if_inline_data)) {
		/*
		 * If the valid extents/data can fit in if_inline_ext/data,
		 * copy them from the malloc'd vector and free it.
		 */
		if (ifp->if_u1.if_data == NULL) {
			ifp->if_u1.if_data = ifp->if_u2.if_inline_data;
		} else if (ifp->if_u1.if_data != ifp->if_u2.if_inline_data) {
			ASSERT(ifp->if_real_bytes != 0);
			bcopy(ifp->if_u1.if_data, ifp->if_u2.if_inline_data,
			      new_size);
			kmem_free(ifp->if_u1.if_data, ifp->if_real_bytes);
			ifp->if_u1.if_data = ifp->if_u2.if_inline_data;
		}
		real_size = 0;
	} else {
		/*
		 * Stuck with malloc/realloc.
		 * For inline data, the underlying buffer must be
		 * a multiple of 4 bytes in size so that it can be
		 * logged and stay on word boundaries.  We enforce
		 * that here.
		 */
		real_size = roundup(new_size, 4);
		if (ifp->if_u1.if_data == NULL) {
			ASSERT(ifp->if_real_bytes == 0);
			ifp->if_u1.if_data = kmem_alloc(real_size, KM_SLEEP);
		} else if (ifp->if_u1.if_data != ifp->if_u2.if_inline_data) {
			/*
			 * Only do the realloc if the underlying size
			 * is really changing.
			 */
			if (ifp->if_real_bytes != real_size) {
				ifp->if_u1.if_data =
					kmem_realloc(ifp->if_u1.if_data,
							real_size,
							ifp->if_real_bytes,
							KM_SLEEP);
			}
		} else {
			ASSERT(ifp->if_real_bytes == 0);
			ifp->if_u1.if_data = kmem_alloc(real_size, KM_SLEEP);
			bcopy(ifp->if_u2.if_inline_data, ifp->if_u1.if_data,
			      ifp->if_bytes);
		}
	}
	ifp->if_real_bytes = real_size;
	ifp->if_bytes = new_size;
	ASSERT(ifp->if_bytes <= XFS_IFORK_SIZE(ip, whichfork));
}


/*
 * Map inode to disk block and offset.
 *
 * mp -- the mount point structure for the current file system
 * tp -- the current transaction
 * ino -- the inode number of the inode to be located
 * imap -- this structure is filled in with the information necessary
 *	 to retrieve the given inode from disk
 * flags -- flags to pass to xfs_dilocate indicating whether or not
 *	 lookups in the inode btree were OK or not
 */
int
xfs_imap(
	xfs_mount_t	*mp,
	xfs_trans_t	*tp,
	xfs_ino_t	ino,
	xfs_imap_t	*imap,
	uint		flags)
{
	xfs_fsblock_t	fsbno;
	int		len;
	int		off;
	int		error;

	fsbno = imap->im_blkno ?
		XFS_DADDR_TO_FSB(mp, imap->im_blkno) : NULLFSBLOCK;
	error = xfs_dilocate(mp, tp, ino, &fsbno, &len, &off, flags);
	if (error != 0) {
		return error;
	}
	imap->im_blkno = XFS_FSB_TO_DADDR(mp, fsbno);
	imap->im_len = XFS_FSB_TO_BB(mp, len);
	imap->im_agblkno = XFS_FSB_TO_AGBNO(mp, fsbno);
	imap->im_ioffset = (ushort)off;
	imap->im_boffset = (ushort)(off << mp->m_sb.sb_inodelog);
	return 0;
}

void
xfs_idestroy_fork(
	xfs_inode_t	*ip,
	int		whichfork)
{
	xfs_ifork_t	*ifp;

	ifp = XFS_IFORK_PTR(ip, whichfork);
	if (ifp->if_broot != NULL) {
		kmem_free(ifp->if_broot, ifp->if_broot_bytes);
		ifp->if_broot = NULL;
	}

	/*
	 * If the format is local, then we can't have an extents
	 * array so just look for an inline data array.  If we're
	 * not local then we may or may not have an extents list,
	 * so check and free it up if we do.
	 */
	if (XFS_IFORK_FORMAT(ip, whichfork) == XFS_DINODE_FMT_LOCAL) {
		if ((ifp->if_u1.if_data != ifp->if_u2.if_inline_data) && 
		    (ifp->if_u1.if_data != NULL)) {
			ASSERT(ifp->if_real_bytes != 0);
			kmem_free(ifp->if_u1.if_data, ifp->if_real_bytes);
			ifp->if_u1.if_data = NULL;
			ifp->if_real_bytes = 0;
		}
	} else if ((ifp->if_flags & XFS_IFEXTENTS) &&
		   (ifp->if_u1.if_extents != NULL) &&
		   (ifp->if_u1.if_extents != ifp->if_u2.if_inline_ext)) {
		ASSERT(ifp->if_real_bytes != 0);
		kmem_free(ifp->if_u1.if_extents, ifp->if_real_bytes);
		ifp->if_u1.if_extents = NULL;
		ifp->if_real_bytes = 0;
	}
	ASSERT(ifp->if_u1.if_extents == NULL ||
	       ifp->if_u1.if_extents == ifp->if_u2.if_inline_ext);
	ASSERT(ifp->if_real_bytes == 0);
	if (whichfork == XFS_ATTR_FORK) {
		kmem_zone_free(xfs_ifork_zone, ip->i_afp);
		ip->i_afp = NULL;
	}
}

/*
 * xfs_iroundup: round up argument to next power of two
 */
uint
xfs_iroundup(
	uint	v)
{
	int i;
	uint m;

	if ((v & (v - 1)) == 0)
		return v;
	ASSERT((v & 0x80000000) == 0);
	if ((v & (v + 1)) == 0)
		return v + 1;
	for (i = 0, m = 1; i < 31; i++, m <<= 1) {
		if (v & m)
			continue;
		v |= m;
		if ((v & (v + 1)) == 0)
			return v + 1;
	}
	ASSERT(0);
	return( 0 );
}

/*
 * xfs_iextents_copy()
 *
 * This is called to copy the REAL extents (as opposed to the delayed
 * allocation extents) from the inode into the given buffer.  It
 * returns the number of bytes copied into the buffer.
 *
 * If there are no delayed allocation extents, then we can just
 * bcopy() the extents into the buffer.  Otherwise, we need to
 * examine each extent in turn and skip those which are delayed.
 */
int
xfs_iextents_copy(
	xfs_inode_t		*ip,
	xfs_bmbt_rec_32_t	*buffer,
	int			whichfork)
{
	int			copied;
	xfs_bmbt_rec_32_t	*dest_ep;
	xfs_bmbt_rec_t		*ep;
#ifdef DEBUG
	xfs_exntfmt_t		fmt = XFS_EXTFMT_INODE(ip);
#endif
#ifdef XFS_BMAP_TRACE
	static char		fname[] = "xfs_iextents_copy";
#endif
	int			i;
	xfs_ifork_t		*ifp;
	int			nrecs;
	xfs_fsblock_t		start_block;

	ifp = XFS_IFORK_PTR(ip, whichfork);
	ASSERT(ismrlocked(&ip->i_lock, MR_UPDATE|MR_ACCESS));
	ASSERT(ifp->if_bytes > 0);

	nrecs = ifp->if_bytes / (uint)sizeof(xfs_bmbt_rec_t);
	xfs_bmap_trace_exlist(fname, ip, nrecs, whichfork);
	ASSERT(nrecs > 0);
	if (nrecs == XFS_IFORK_NEXTENTS(ip, whichfork)) {
		/*
		 * There are no delayed allocation extents,
		 * so just copy everything.
		 */
		ASSERT(ifp->if_bytes <= XFS_IFORK_SIZE(ip, whichfork));
		ASSERT(ifp->if_bytes ==
		       (XFS_IFORK_NEXTENTS(ip, whichfork) *
		        (uint)sizeof(xfs_bmbt_rec_t)));
		bcopy(ifp->if_u1.if_extents, buffer, ifp->if_bytes);
		xfs_validate_extents(buffer, nrecs, fmt);
		return ifp->if_bytes;
	}

	ASSERT(whichfork == XFS_DATA_FORK);
	/*
	 * There are some delayed allocation extents in the
	 * inode, so copy the extents one at a time and skip
	 * the delayed ones.  There must be at least one
	 * non-delayed extent.
	 */
	ASSERT(nrecs > ip->i_d.di_nextents);
	ep = ifp->if_u1.if_extents;
	dest_ep = buffer;
	copied = 0;
	for (i = 0; i < nrecs; i++) {
		start_block = xfs_bmbt_get_startblock(ep);
		if (ISNULLSTARTBLOCK(start_block)) {
			/*
			 * It's a delayed allocation extent, so skip it.
			 */
			ep++;
			continue;
		}

		*dest_ep = *(xfs_bmbt_rec_32_t *)ep;
		dest_ep++;
		ep++;
		copied++;
	}
	ASSERT(copied != 0);
	ASSERT(copied == ip->i_d.di_nextents);
	ASSERT((copied * (uint)sizeof(xfs_bmbt_rec_t)) <= XFS_IFORK_DSIZE(ip));
	xfs_validate_extents(buffer, copied, fmt);

	return (copied * (uint)sizeof(xfs_bmbt_rec_t));
}		  

/*
 * Each of the following cases stores data into the same region
 * of the on-disk inode, so only one of them can be valid at
 * any given time. While it is possible to have conflicting formats
 * and log flags, e.g. having XFS_ILOG_?DATA set when the fork is
 * in EXTENTS format, this can only happen when the fork has
 * changed formats after being modified but before being flushed.
 * In these cases, the format always takes precedence, because the
 * format indicates the current state of the fork.
 */
STATIC int
xfs_iflush_fork(
	xfs_inode_t		*ip,
	xfs_dinode_t		*dip,
	xfs_inode_log_item_t	*iip,
	int			whichfork,
	xfs_buf_t		*bp)
{
	char			*cp;
	xfs_ifork_t		*ifp;
	xfs_mount_t		*mp;
#ifdef XFS_TRANS_DEBUG
	int			first;
#endif
	static const short	brootflag[2] =
		{ XFS_ILOG_DBROOT, XFS_ILOG_ABROOT };
	static const short	dataflag[2] =
		{ XFS_ILOG_DDATA, XFS_ILOG_ADATA };
	static const short	extflag[2] =
		{ XFS_ILOG_DEXT, XFS_ILOG_AEXT };

	if (iip == NULL)
		return 0;
	ifp = XFS_IFORK_PTR(ip, whichfork);
	/*
	 * This can happen if we gave up in iformat in an error path,
	 * for the attribute fork.
	 */
	if (ifp == NULL) {
		ASSERT(whichfork == XFS_ATTR_FORK);
		return 0;
	}
	cp = XFS_DFORK_PTR_ARCH(dip, whichfork, ARCH_CONVERT);
	mp = ip->i_mount;
	switch (XFS_IFORK_FORMAT(ip, whichfork)) {
	case XFS_DINODE_FMT_LOCAL:
		if ((iip->ili_format.ilf_fields & dataflag[whichfork]) &&
		    (ifp->if_bytes > 0)) {
			ASSERT(ifp->if_u1.if_data != NULL);
			ASSERT(ifp->if_bytes <= XFS_IFORK_SIZE(ip, whichfork));
			bcopy(ifp->if_u1.if_data, cp, ifp->if_bytes);
		}
		if (whichfork == XFS_DATA_FORK) {
			if (XFS_DIR_SHORTFORM_VALIDATE_ONDISK(mp, dip)) {
				return XFS_ERROR(EFSCORRUPTED);
			}
		}
		break;

	case XFS_DINODE_FMT_EXTENTS:
		ASSERT((ifp->if_flags & XFS_IFEXTENTS) ||
		       !(iip->ili_format.ilf_fields & extflag[whichfork]));
		ASSERT((ifp->if_u1.if_extents != NULL) || (ifp->if_bytes == 0));
		ASSERT((ifp->if_u1.if_extents == NULL) || (ifp->if_bytes > 0));
		if ((iip->ili_format.ilf_fields & extflag[whichfork]) &&
		    (ifp->if_bytes > 0)) {
			ASSERT(XFS_IFORK_NEXTENTS(ip, whichfork) > 0);
			(void)xfs_iextents_copy(ip, (xfs_bmbt_rec_32_t *)cp,
				whichfork);
		}
		break;

	case XFS_DINODE_FMT_BTREE:
		if ((iip->ili_format.ilf_fields & brootflag[whichfork]) &&
		    (ifp->if_broot_bytes > 0)) {
			ASSERT(ifp->if_broot != NULL);
			ASSERT(ifp->if_broot_bytes <=
			       (XFS_IFORK_SIZE(ip, whichfork) +
				XFS_BROOT_SIZE_ADJ));
			xfs_bmbt_to_bmdr(ifp->if_broot, ifp->if_broot_bytes,
				(xfs_bmdr_block_t *)cp,
				XFS_DFORK_SIZE_ARCH(dip, mp, whichfork, ARCH_CONVERT));
		}
		break;

	case XFS_DINODE_FMT_DEV:
		if (iip->ili_format.ilf_fields & XFS_ILOG_DEV) {
			ASSERT(whichfork == XFS_DATA_FORK);
			INT_SET(dip->di_u.di_dev, ARCH_CONVERT, ip->i_df.if_u2.if_rdev);
		}
		break;
		
	case XFS_DINODE_FMT_UUID:
		if (iip->ili_format.ilf_fields & XFS_ILOG_UUID) {
			ASSERT(whichfork == XFS_DATA_FORK);
			bcopy(&ip->i_df.if_u2.if_uuid, &dip->di_u.di_muuid,
				sizeof(uuid_t));
		}
		break;

	default:
		ASSERT(0);
		break;
	}

	return 0;
}
