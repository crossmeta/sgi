/*
 *
 * CROSSMETA Windows porting changes. http://www.crossmeta.org
 * Copyright (c) 2001 Supramani Sammandam.  suprasam _at_ crossmeta.org
 */
#if 0
#include <xfs_support/support.h>
#include <xfs_types.h>
#include <winnt/xfs_winnt.h>
#include <sys/stat.h>
#include <winnt/xfs_cred.h>
#include <winnt/xfs_behavior.h>
#include <winnt/xfs_vfs.h>
#include <winnt/xfs_vnode.h>
#include <sys/mount.h>
#endif
#include <xfs/xfs.h>
#pragma warning(disable:4013)	/* disable unknown pragma warning */

STATIC	int	_xfs_open(vnode_t *, int, cred_t *);
STATIC	int	_xfs_close(vnode_t *, int, cred_t *);
STATIC	int	_xfs_read(vnode_t *, struct uio *, int, cred_t *);
STATIC	int	_xfs_write(vnode_t *, struct uio *, int, cred_t *);
STATIC	int	_xfs_ioctl(vnode_t *, int, caddr_t, int, cred_t *);
STATIC	int	_xfs_getattr(vnode_t *, int, vattr_t *, cred_t *);
STATIC	int	_xfs_setattr(vnode_t *, int, vattr_t *, cred_t *);
STATIC	int	_xfs_access(vnode_t *, int, cred_t *);
STATIC	int	_xfs_lookup(vnode_t *, vnode_t **, struct componentname *);
STATIC	int	_xfs_create(vnode_t *, vnode_t **, struct componentname *,
				vattr_t *);
STATIC	int	_xfs_remove(vnode_t *, vnode_t *, struct componentname *);
STATIC	int	_xfs_link(vnode_t *, vnode_t *, struct componentname *);
STATIC	int	_xfs_rename(vnode_t *, vnode_t *, struct componentname *,
				vnode_t *, vnode_t *, struct componentname *);
STATIC	int	_xfs_mkdir(vnode_t *, vnode_t **, struct componentname *,
				vattr_t *);
STATIC	int	_xfs_rmdir(vnode_t *, vnode_t *, struct componentname *);
STATIC	int	_xfs_readdir(vnode_t *, struct uio *, cred_t *, int *);
STATIC	int	_xfs_symlink(vnode_t *, vnode_t **, struct componentname *,
				vattr_t *, char *);
STATIC	int	_xfs_readlink(vnode_t *, struct uio *, cred_t *);
STATIC	void	_xfs_inactive(vnode_t *);
STATIC	void	_xfs_reclaim(vnode_t *);
STATIC	int	_xfs_fsync(vnode_t *, int, cred_t *);
STATIC	int	_xfs_bmap(vnode_t *, daddr_t, vnode_t **, daddr_t *, 
			 int *, int *);
STATIC	int	_xfs_strategy(vnode_t *, struct buf *);
STATIC	int	_xfs_fid(vnode_t *, struct fid *);
STATIC	int	_xfs_rwlock(vnode_t *, int);
STATIC	void	_xfs_rwunlock(vnode_t *);
STATIC	int	_xfs_seek(vnode_t *, off_t, off_t *);
STATIC	int	_xfs_cmp(vnode_t *, vnode_t *);
STATIC	int	_xfs_prepare_write(vnode_t *, int , off64_t, uint_t, cred_t *);
STATIC	int	_xfs_commit_write(vnode_t *, int , off64_t, uint_t, cred_t *);
STATIC	int	_xfs_get_eattr(vnode_t *, char *, char *, int *, int, cred_t *);
STATIC	int	_xfs_set_eattr(vnode_t *, char *, char *, int, int, cred_t *);
STATIC	int	_xfs_list_eattr(vnode_t *, char *, int, int, 
			struct attrlist_cursor_kern *,  cred_t *);

void
xfs_delete_gap_list(
	xfs_iocore_t	*ip,
	xfs_fileoff_t	offset_fsb,
	xfs_extlen_t	count_fsb);

/*
 * XFS file system vnode operations structure
 * 
 * Just acts as a layer over the real XFS vnode operations to take care
 * of FreeBSD Vnode interface by massaging arguments & vnode locks.
 * In SVR4 VFS/VNODE VOP locks are not held across calls so drop the locks
 * before we enter XFS world.
 */
struct vnodeops xfs_vnodeops = {
	_xfs_open,
	_xfs_close,
	_xfs_read,
	_xfs_write,
	_xfs_ioctl,
	_xfs_getattr,
	_xfs_setattr,
	_xfs_access,
	_xfs_lookup,
	_xfs_create,
	_xfs_remove,
	_xfs_link,
	_xfs_rename,
	_xfs_mkdir,
	_xfs_rmdir,
	_xfs_readdir,
	_xfs_symlink,
	_xfs_readlink,
	_xfs_fid,
	_xfs_fsync,
	_xfs_rwlock,
	_xfs_rwunlock,
	_xfs_bmap,
	_xfs_strategy,
	_xfs_inactive,
	_xfs_reclaim,
	_xfs_seek,
	_xfs_prepare_write,
	_xfs_commit_write,
	_xfs_get_eattr,
	_xfs_set_eattr,
	_xfs_list_eattr,
};


STATIC int
_xfs_open(struct vnode *vp, int mode, cred_t *cred)
{
	vnode_t *newvp;

	return (xfs_open(vn_fbhv(vp), &newvp, mode, cred));
}

STATIC int
_xfs_close(struct vnode *vp, int flag, cred_t *cred)
{

	return (xfs_close(vn_fbhv(vp), flag, 0, cred));
}

STATIC int
_xfs_getattr(struct vnode *vp, int flags, vattr_t *vap, cred_t *cred)
{
	int error;

	vap->va_mask = AT_ALL;
	error = xfs_getattr(vn_fbhv(vp), vap, flags, cred);
#if 0
	if (!error && VN_CACHED(vp)) {
		VOP_RWRDLOCK(vp);
		vnode_pager_getsize(vp, &vap->va_size);
		VOP_RWUNLOCK(vp);
	}
#endif
	return (error);
}

STATIC int
_xfs_setattr(struct vnode *vp, int flags, vattr_t *vap, cred_t *cred)
{
	int error;

	error = xfs_setattr(vn_fbhv(vp), vap, flags, cred);
	return (error);
}

STATIC int
_xfs_read(vnode_t *vp, struct uio *uiop, int ioflag, cred_t *cred)
{

	return (xfs_read(vn_fbhv(vp), uiop, ioflag, cred, NULL));
}

STATIC int
_xfs_write(vnode_t *vp, struct uio *uiop, int ioflag, cred_t *cred)
{
	bhv_desc_t *bdp = vn_fbhv(vp);

	if (ioflag & IO_PAGEIO) {
		xfs_inode_t *ip;
		xfs_mount_t *mp;
		xfs_iocore_t *io;
		struct buf *bp;
		unsigned count;
		int error;

		ip = XFS_BHVTOI(bdp);
		io = &ip->i_iocore;
		mp = ip->i_mount;

		if (XFS_FORCED_SHUTDOWN(mp))
			return (EIO);

		bp = getphysbuf(NODEV);
		XFS_BUF_SET_TARGET(bp, &mp->m_ddev_targ);
		bp->b_offset = uiop->uio_offset >> 9;
		bp->b_blkno = -1;
		count = uiop->uio_resid;
		if (count & BBMASK)
			count = (count + BBMASK) & ~BBMASK;
		bp->b_bcount = bp->b_bufsize = count;
		bp->b_data = uiop->uio_iov->iov_base;
		XFS_STRAT_WRITE(mp, io, bp);
		error = geterror(bp);
		putphysbuf(bp);
		return (error);
	}

	return (xfs_write(bdp, uiop, ioflag, cred, NULL));
}

STATIC int
_xfs_prepare_write(vnode_t *vp, int ioflag, off64_t wroff, uint_t count,
			cred_t *cred)
{
	struct uio auio;
	struct iovec aiov;
	bhv_desc_t *bdp;
	int error;
	
	ASSERT(!(ioflag & IO_PAGEIO));
	bdp = vn_fbhv(vp);
	aiov.iov_base = NULL;
	aiov.iov_len = count;
	auio.uio_iov = &aiov;
	auio.uio_iovcnt = 1;
	auio.uio_offset = wroff;
	auio.uio_resid = count;
	auio.uio_segflag = UIO_USERSPACE;
	auio.uio_rw = UIO_WRITE;
	auio.uio_filp = NULL;
	auio.uio_procp = NULL;

	error = xfs_write(bdp, &auio, ioflag, cred, NULL);
	if (!error) {
		xfs_inode_t *ip;
		xfs_mount_t *mp;
		xfs_iocore_t *io;
		off64_t blocks;

		ip = XFS_BHVTOI(bdp);
		io = &ip->i_iocore;
		mp = ip->i_mount;
		blocks =  XFS_FSB_TO_B(mp, ip->i_d.di_nblocks + 
					ip->i_delayed_blks);
		vnode_pager_setsize(vp, XFS_SIZE(mp, io), blocks);
	}
	return (error);
}

STATIC int
_xfs_commit_write(vnode_t *vp, int ioflag, off64_t wroff, uint_t count,
			 cred_t *cr)
{
	bhv_desc_t *bdp = vn_fbhv(vp);
	xfs_inode_t *ip;
	xfs_mount_t *mp;
	xfs_iocore_t *io;
	off64_t isize;
	off64_t blocks;
	PFSRTL_COMMON_FCB_HEADER fhdrp;

	ip = XFS_BHVTOI(bdp);
	io = &ip->i_iocore;
	mp = ip->i_mount;

#if 0
	XFS_ILOCK(mp, io, XFS_ILOCK_EXCL);
	xfs_delete_gap_list(io, XFS_B_TO_FSBT(mp, wroff),
			 XFS_B_TO_FSB(mp, count));
	xfs_free_gap_list(io);
	XFS_IUNLOCK(mp, io, XFS_ILOCK_EXCL);
#endif
	io->io_new_size = 0;
	io->io_write_offset = 0;

	isize = wroff + count;
	if (isize > XFS_SIZE(mp, io)) {
		XFS_SETSIZE(mp, io, isize);
		blocks =  XFS_FSB_TO_B(mp, ip->i_d.di_nblocks + 
					ip->i_delayed_blks);
	}

	fhdrp = (PFSRTL_COMMON_FCB_HEADER)vp->v_fcbhdr;
	vnode_pager_setsize(vp, XFS_SIZE(mp, io),
				 fhdrp->AllocationSize.QuadPart);
	return (0);
}

STATIC int
_xfs_get_eattr(vnode_t *vp, char *name, char *val, int *lenp,
		 int flags, cred_t *cr)
{

	return (xfs_attr_get(vn_fbhv(vp), name, val, lenp, flags, cr));
}

STATIC int
_xfs_set_eattr(vnode_t *vp, char *name, char *val, int len,
		 int flags, cred_t *cr)
{

	return (xfs_attr_set(vn_fbhv(vp), name, val, len, flags, cr));
}

STATIC int
_xfs_list_eattr(vnode_t *vp, char *buffer, int bufsize, int flags, 
			attrlist_cursor_kern_t *cursor,  cred_t *cr)
{

	return (xfs_attr_list(vn_fbhv(vp), buffer, bufsize, flags, cursor, cr));
}

STATIC int
_xfs_ioctl(vnode_t *vp, int cmd, caddr_t data, int flag, cred_t *cred)
{
	int error;

	error = xfs_ioctl(vn_fbhv(vp), cmd, data, flag, cred);
	if (error < 0)
		error = -error;
	return (error);
}


STATIC int
_xfs_access(struct vnode *vp, int mode, cred_t *cr)
{

	return (xfs_access(vn_fbhv(vp), mode, cr));
}

STATIC int
_xfs_lookup(struct vnode *dvp, vnode_t **vpp, struct componentname *cnp)
{
	pathname_t pn, *pnp = &pn;
	struct vnode *newvp;
	int error = 0;
	int nlock, lockparent, wantparent;
	int flags = cnp->cn_flags;

	if ((flags & ISLASTCN) && (dvp->v_vfsp->vfs_flag & VFS_RDONLY) &&
	    (cnp->cn_nameiop == NDELETE || cnp->cn_nameiop == RENAME))
		return (EROFS);

	*vpp = NULL;
	newvp = NULL;
	if (dvp->v_type != VDIR)
		return (ENOTDIR);
	lockparent = 0;
	nlock = LOCK_NONE;
	if (flags & ISLASTCN) {
		if (flags & LOCKPARENT)
			lockparent++;
		if (flags & LOCKLEAF)
			nlock = LOCK_EXCL;
	}
	wantparent = (flags & (LOCKPARENT|WANTPARENT));
	error = ncache_lookup(dvp, vpp, cnp);
	if (error) {
		ulong_t vpid;

		if (error == ENOENT) {
			VOP_RWUNLOCK(dvp);
			return (error);
		}
		newvp = *vpp;
		vpid = newvp->v_id;
		/*
		 * See the comment starting `Step through' in ufs/ufs_lookup.c
		 * for an explanation of the locking protocol
		 */
		if (dvp == newvp) {
			if (!lockparent)
				VOP_RWUNLOCK(newvp);
			VREF(newvp);
			error = 0;
		} else if (flags & ISDOTDOT) {
			VOP_RWUNLOCK(dvp);
			error = vget(newvp, nlock);
			if (lockparent)
				VOP_RWWRLOCK(dvp);
			else
				VOP_RWRDLOCK(dvp);
		} else {
			error = vget(newvp, nlock);
		}
		if (!error) {
			if (vpid == newvp->v_id) {
				if (!lockparent)
					VOP_RWUNLOCK(dvp);
				*vpp = newvp;
				return (0);
			}
			ncache_purge(newvp);
			/*
			 * What we have is something else, so release it
			 */
			if (nlock)
				vput(newvp);
			else
				vrele(newvp);
		}
		*vpp = NULL;
	}

	pnp->pn_complen = cnp->cn_namelen;
	pnp->pn_hash 	= cnp->cn_hash;
	pnp->pn_path 	= cnp->cn_nameptr;
	error = xfs_lookup(vn_fbhv(dvp), cnp->cn_nameptr, vpp, pnp,
				0, NULL, cnp->cn_cred);
	newvp = *vpp;
	if (!error) {
		goto done;
	}

	/*
	 * Name not found. We have to return a special error code
	 * for create and rename
	 */
	ASSERT(newvp == NULL);
	if ((cnp->cn_nameiop == CREATE || cnp->cn_nameiop == RENAME) &&
	    (flags & ISLASTCN) && error == ENOENT) {
		if (dvp->v_vfsp->vfs_flag & VFS_RDONLY)
			return (EROFS);
		else
			error = EJUSTRETURN;
	}

	/*
	 * Handle RENAME case...
	 */
	if (cnp->cn_nameiop == RENAME && wantparent && (flags & ISLASTCN)) {
		if (dvp == newvp) {
			vrele(newvp);
			return (EISDIR);
		}
		goto done;
	}

 done:
	if ((cnp->cn_flags & MAKEENTRY) && (cnp->cn_nameiop != CREATE))
		ncache_enter(dvp, *vpp, cnp);
	if (cnp->cn_nameiop != LOOKUP && (flags & ISLASTCN))
		cnp->cn_flags |= SAVENAME;
	/*
	 * We first lock the parent, then the child node to avoid any deadlock
	 */
	if (lockparent)
		VOP_RWWRLOCK(dvp);
	if (nlock && newvp)
		VOP_RWWRLOCK(newvp);
	return (error);
}

static int
_xfs_create(vnode_t *dvp, vnode_t **vpp, struct componentname *cnp,
		vattr_t *vap)
{
	int error;

	*vpp = NULL;
	VOP_RWUNLOCK(dvp);
	error = xfs_create(vn_fbhv(dvp), cnp->cn_nameptr, vap, 0, 0,
				vpp, cnp->cn_cred);
	vrele(dvp);
	return (error);
}

static int
_xfs_remove(struct vnode *dvp, struct vnode *vp, struct componentname *cnp)
{
	int error;

	if (vp == dvp)
		vrele(vp);
	else
		vput(vp);
	VOP_RWUNLOCK(dvp);
	error = xfs_remove(vn_fbhv(dvp), cnp->cn_nameptr, cnp->cn_cred);
	vrele(dvp);
	return (error);
}

static int
_xfs_link(vnode_t *tdvp, vnode_t *vp, struct componentname *cnp)
{
	int error;

	if (tdvp != vp)
		VOP_RWUNLOCK(tdvp);
	error =  xfs_link(vn_fbhv(tdvp), vp, cnp->cn_nameptr, cnp->cn_cred);
	vrele(tdvp);
	return (error);
}

static int
_xfs_rename(vnode_t *fdvp, vnode_t *fvp, struct componentname *fcnp,
		vnode_t *tdvp, vnode_t *tvp, struct componentname *tcnp)
{
	pathname_t pn, *pnp = &pn;
	int error;

	pnp->pn_complen = tcnp->cn_namelen;
	pnp->pn_hash = tcnp->cn_hash;
	pnp->pn_path = tcnp->cn_nameptr;

	if (fvp->v_type == VDIR) {
		if (tvp && tvp->v_type == VDIR)
			ncache_purge(tdvp);
		ncache_purge(fdvp);
	}
	if (tvp) 
		vput(tvp);
	VOP_RWUNLOCK(tdvp);
	vrele(fvp);

	error = xfs_rename(vn_fbhv(fdvp), fcnp->cn_nameptr,
				tdvp, tcnp->cn_nameptr, pnp, fcnp->cn_cred);
	vrele(tdvp);
	vrele(fdvp);
	return (error);
}

static int
_xfs_mkdir(vnode_t *dvp, vnode_t **vpp, struct componentname *cnp, vattr_t *vap)
{
	int error;

	*vpp = NULL;
	VOP_RWUNLOCK(dvp);
	error = xfs_mkdir(vn_fbhv(dvp), cnp->cn_nameptr, vap, 
				vpp, cnp->cn_cred);
	vrele(dvp);
	return (error);
}

static int
_xfs_rmdir(vnode_t *dvp, vnode_t *vp, struct componentname *cnp)
{
	int error;

	ncache_purge(dvp);
	ncache_purge(vp);
	if (vp == dvp)
		vrele(vp);
	else
		vput(vp);
	VOP_RWUNLOCK(dvp);
	error = xfs_rmdir(vn_fbhv(dvp), cnp->cn_nameptr, NULL, cnp->cn_cred);
	vrele(dvp);
	return (error);
}

static int
_xfs_readdir(struct vnode *dvp, struct uio *uio, cred_t *cred, int *eofp)
{

	return (xfs_readdir(vn_fbhv(dvp), uio, cred, eofp));
}

static int
_xfs_symlink(vnode_t *dvp, vnode_t **vpp, struct componentname *cnp,
		struct vattr *vap, char *target)
{
	int error;

	error = xfs_symlink(vn_fbhv(dvp), cnp->cn_nameptr, vap, target,
				vpp, cnp->cn_cred);
	vput(dvp);
	return (error);
}

STATIC int
_xfs_readlink(vnode_t *vp, struct uio *uiop, cred_t *cred)
{

	return (xfs_readlink(vn_fbhv(vp), uiop, cred));
}

STATIC int
_xfs_fid(vnode_t *vp, struct fid *fid)
{

	return (xfs_fid(vn_fbhv(vp), fid));
}

static int
_xfs_fsync(struct vnode *vp, int waitfor, struct ucred *cred)
{
	int error, flags = 0;

	if (waitfor)
		flags |= FSYNC_WAIT;
	error = (xfs_fsync(vn_fbhv(vp), flags, cred,
			(xfs_off_t)0, (xfs_off_t)-1));
	if (error)
		printf("xfs_fsync: vp (%x) error %d\n", vp, error);
	return (error);
}

STATIC int
_xfs_bmap(vnode_t *vp, daddr_t bn, vnode_t **vpp, daddr_t *bnp,
	 int *runp, int *runb)
{

	printf("xfs_bmap: not implemented\n");
	if (vpp != NULL)
		*vpp= vp;
	if (bnp != NULL)
		*bnp = bn * btodb(vp->v_vfsp->vfs_bsize);
	if (runp != NULL)
		*runp = 0;
	if (runb != NULL)
		*runb = 0;
	return (0);
}

STATIC int
_xfs_strategy(vnode_t *vp, struct buf *bp)
{

	printf("xfs_strategy: not implemented\n");
	return (0);
}

STATIC void
_xfs_inactive(vnode_t *vp)
{
	int ret;

	ASSERT(vp->v_flag & VINACT);

	ret = xfs_inactive(vn_fbhv(vp), NOCRED);
	ASSERT(ret == VN_INACTIVE_CACHE);
}

STATIC void
_xfs_reclaim(vnode_t *vp)
{
	int error;

	/*
 	 * We really need a flag in vop_reclaim for this
	 * Too bad we are forcing it be a unmount all the time
	 */
	error = xfs_reclaim(vn_fbhv(vp), FSYNC_INVAL);
	vp->v_data = NULL;
	ASSERT(error == 0);
	ASSERT(KeGetCurrentIrql() == 0);
}

STATIC int
_xfs_rwlock(vnode_t *vp, int mode)
{
	vrwlock_t locktype;

	switch (mode) {
	case LOCK_EXCL:
		locktype = VRWLOCK_WRITE;
		break;

	case LOCK_SHARED:
		locktype = VRWLOCK_READ;
		break;

	case LOCK_EXCL_NOWAIT:
		locktype = VRWLOCK_TRY_WRITE;
		break;

	case LOCK_SHARED_NOWAIT:
		locktype = VRWLOCK_TRY_READ;
		break;

	default:
		return (EINVAL);
	}

	if (!xfs_rwlock(vn_fbhv(vp), locktype))
		return (EWOULDBLOCK);
	if (locktype == VRWLOCK_WRITE || locktype == VRWLOCK_TRY_WRITE)
		vn_flagset(vp, VXLOCK);
	return (0);
}

static void
_xfs_rwunlock(vnode_t *vp)
{
	vrwlock_t locktype;

	if (vp->v_flag & VXLOCK) {
		locktype = VRWLOCK_WRITE;
		vn_flagclr(vp, VXLOCK);
	} else {
		locktype = VRWLOCK_READ;
	}

	xfs_rwunlock(vn_fbhv(vp), locktype);
}

STATIC int
_xfs_seek(vnode_t *vp, off_t off, off_t *newoffp)
{

	return (xfs_seek(vn_fbhv(vp), (xfs_off_t)off, (xfs_off_t *)newoffp));
}


STATIC int
_xfs_cmp(vnode_t *vp1, vnode_t *vp2)
{

	return (vp1 == vp2);
}

int
__xfs_toss_pages(vnode_t *vp, xfs_off_t first, xfs_off_t last, int fl)
{

	if (first != 0 || last != -1) {
		/*
		 * For regular files vnode_pager_setsize will take
		 * care of discarding pages that are not needed.
		 */ 
		if (vp->v_type != VREG)
			printf("__xfs_toss_pages: range not supported\n");
		return;
	}

	return (vinvalbuf(vp, 0, get_current_cred(), curproc()));
}

int
__xfs_flushinval_pages(vnode_t *vp, xfs_off_t first, xfs_off_t last, int fl)
{

	if (first != 0)
		printf("__xfs_flushinval_pages: range not supported\n");

	if (vp->v_type == VREG)
		vnode_pager_uncache(vp);
	return (vinvalbuf(vp, V_SAVE, get_current_cred(), curproc()));
}

int
__xfs_vnode_change(vnode_t *vp, unsigned cmd, unsigned val)
{
	return (0);
}

int
__xfs_link_removed(vnode_t *vp, vnode_t *dvp, int linkzero)
{


}

int
__xfs_check_holes(xfs_iocore_t *io, xfs_off_t offset, size_t count)
{
	xfs_mount_t *mp;
	xfs_fileoff_t offset_fsb;
	xfs_extlen_t count_fsb;
	xfs_fileoff_t last_fsb;
	xfs_fileoff_t gap_offset_fsb;
	xfs_extlen_t gap_count_fsb;
	xfs_fileoff_t gap_end_fsb;
	xfs_gap_t *cur;
	int hole = 0;
	int i;

	mp = io->io_mount;
	offset_fsb = XFS_B_TO_FSBT(mp, offset);
	last_fsb = XFS_B_TO_FSB(mp, ((xfs_ufsize_t)(offset + count)));
	count_fsb = (xfs_filblks_t)(last_fsb - offset_fsb);
KdPrint(("xfs_check_holes: off %x cnt %x %x %x\n", (int)offset, count,
		offset_fsb, last_fsb));
	xfs_build_gap_list(io, offset, count);

	cur = io->io_gap_list;
	while (cur != NULL) {
		gap_offset_fsb = cur->xg_offset_fsb;
		gap_count_fsb = cur->xg_count_fsb;

		/*
		 * Since the entries are sorted by offset we stop the
		 * search if gap offset exceeds.
		 */
		if (gap_offset_fsb >= last_fsb)
			break;

		gap_end_fsb = gap_offset_fsb + gap_count_fsb;
		if (gap_end_fsb <= offset_fsb) {
			cur = cur->xg_next;
			continue;
		}
		if (gap_end_fsb > last_fsb) {
		KdPrint(("xfs_check_holes: off %x cnt %x HOLE\n", (int)offset, count));
			hole = 1;
			break;
		}
		cur = cur->xg_next;
	}

	xfs_free_gap_list(io);
	return (hole);
}
