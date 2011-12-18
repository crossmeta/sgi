/*
 *
 * CROSSMETA Windows porting changes. http://www.crossmeta.org
 * Copyright (c) 2001 Supramani Sammandam.  suprasam _at_ crossmeta.org
 */
#include <xfs/xfs.h>
#include <sys/sysproto.h>
#include <sys/namei.h>
#include <sys/vfs_ntfsd.h>

#pragma warning(disable:4013)	/* disable undefined; assuming extern */


#define	NONROOT_MOUNT	ROOT_UNMOUNT
#define	M_BHV	M_XFS

/*
 * XFS VFS operations for Windows NT
 */
STATIC int	_xfs_mount(struct vfs *, vnode_t *, struct mount_args *,
				cred_t *);
STATIC int	_xfs_mountroot(struct vfs *, enum whymountroot);
STATIC int 	_xfs_unmount(struct vfs *, int, cred_t *);
STATIC int	_xfs_root(struct vfs *, struct vnode **);
STATIC int	_xfs_statfs(struct vfs *, struct statfs *);
STATIC int	_xfs_sync(struct vfs *, int, cred_t *);
STATIC int 	_xfs_vget(struct vfs *, vnode_t **, struct fid *);

STATIC int	xfs_fs_unload(void);
extern int 	xfs_init(int);
extern void	xfs_cleanup(void);
extern void	xfs_mod_incr_usage(void);
extern void	xfs_mod_decr_usage(void);
extern void	dmapi_init(void);
extern void	dmapi_uninit(void);


struct vfsops xfs_vfsops = {
	_xfs_mount,
	_xfs_unmount,
	_xfs_root,
	_xfs_statfs,
	_xfs_sync,
	_xfs_vget,
	NULL,		/* xfs_mountroot */

};

int
xfs_fs_init(struct vfssw *vswp, int fstype)
{
	vswp->vsw_vfsops = &xfs_vfsops;
	vswp->vsw_unload = xfs_fs_unload;

	xfs_init(fstype);

#ifdef CONFIG_XFS_GRIO
	xfs_grio_init();
#endif

	dmapi_init();
}

STATIC int
xfs_fs_unload(void)
{

	dmapi_uninit();

#ifdef CONFIG_XFS_GRIO
	xfs_grio_uninit();
#endif

	xfs_cleanup();
}

/*
 * _xfs_mount
 *
 * Called when mounting local physical media
 */
static int
_xfs_mount(struct vfs *vfsp, vnode_t *mvp, struct mount_args *uap, cred_t *cr)
{
	struct xfs_args xargs, *ap;
	struct nameidata nd;
	struct vnode *bvp;
	dev_t logdev, rtdev;
	dev_t ddev;
	int error;

	/*
	 * We need a directory vnode to mount
	 */
	if (mvp->v_type != VDIR)
		return (XFS_ERROR(ENOTDIR));

	if (((uap->flags & MNT_UPDATE) == 0) &&
			((vn_count(mvp) != 1) || (mvp->v_flag & VROOT)))
		return (XFS_ERROR(EBUSY));

	try {
		error = xfs_mountargs((struct mounta *)uap, &xargs);
		if (error)
			return (error);

	} except(EXCEPTION_EXECUTE_HANDLER) {
		return (XFS_ERROR(EFAULT));
	}


	/*
	 * Resolve path name of special file being mounted.
	 */
	ap = &xargs;
	NDINIT(&nd, LOOKUP, FOLLOW, UIO_USERSPACE, ap->fsname, curproc());
	error = namei(&nd);
	if (error)
		return (error);

	bvp = nd.ni_vp;
	if (!vn_isdisk(bvp, &error)) {
		vrele(bvp);
		return (XFS_ERROR(error));
	}

	/* There is no XLV or XVM device */
	ddev = logdev = bvp->v_rdev;
	rtdev = 0;

	/*
	 * Ensure that this isn't already mounted,
	 * unless this is a REMOUNT request
	 */
	if (vfs_devsearch(ddev) == NULL) {
		ASSERT((uap->flags & MNT_UPDATE) == 0);
	} else {
		if ((uap->flags & MNT_UPDATE) == 0) {
			vrele(bvp);
			return (XFS_ERROR(EBUSY));
		}
	}

	/*
	 * Proceed further without releasing the bvp so that vfinddev()
	 * in xfs_cmountfs can locate it.
	 * Allocate behavior objects & initialize them
	 */
	vfsp->vfs_data = kmem_alloc(sizeof (bhv_head_t), KM_SLEEP);
	bhv_head_init(vfsp->vfs_data, "vfs");

	error = xfs_cmountfs(vfsp, ddev, logdev, rtdev, NONROOT_MOUNT,
				&xargs, uap, cr);
	if (error == 0) {
		struct vfsmount *mp = VFSTOMP(vfsp);

		strncpy(mp->mnt_stat.f_mntonname, uap->path, MNAMELEN);
		strncpy(mp->mnt_stat.f_mntfromname, ap->fsname, MNAMELEN);
		vfs_add(mvp, vfsp);
		xfs_mod_incr_usage();
	} 
	/* Looks like all vnode references are dropped in xfs_cmountfs */
	return (error);
}

/*
 * Mount this FFS file system as root file system
 *	The reason parameter can have the following values
 *		ROOT_INIT	initial mount request to mount as readonly
 *		ROOT_REMOUNT	after fsck is done this request is made
 *		ROOT_UNMOUNT	during shutdown 
 */
STATIC int
_xfs_mountroot(struct vfs *vfsp, enum whymountroot reason)
{

}


/*
 * unmount system call
 */
int
_xfs_unmount(struct vfs *vfsp, int mntflags, cred_t *cr)
{
	int error;

	error = xfs_unmount(vfs_fbhv(vfsp), 0, cr);
	if (!error) {
		vfs_remove(vfsp);
		kmem_free(vfsp->vfs_data, sizeof (bhv_head_t));
		vfsp->vfs_data = NULL;
		xfs_mod_decr_usage();
	}
	return (error);
}


int
_xfs_root(struct vfs *vfsp, struct vnode **vpp)
{

	return (xfs_root(vfs_fbhv(vfsp), vpp));
}

/*
 * Get file system statistics.
 */
int
_xfs_statfs(struct vfs *vfsp, struct statfs *sbp)
{
	statvfs_t stat;
	struct vfsmount *mp;
	register statvfs_t *statp = &stat;

	/*
	 * Just call XFS statvfs version and them map to
	 * FreeBSD statfs structure
	 */
	xfs_statvfs(vfs_fbhv(vfsp), statp, NULL);
	sbp->f_flags = vfsp->vfs_flag;
	sbp->f_bsize = statp->f_bsize;
	sbp->f_iosize = 16 * vfsp->vfs_bsize;
	sbp->f_blocks = statp->f_blocks;
	sbp->f_bfree = statp->f_bfree;
	sbp->f_bavail = statp->f_bavail;
	sbp->f_files = statp->f_files;
	sbp->f_ffree = statp->f_ffree;
	mp = VFSTOMP(vfsp);
	if (sbp != &mp->mnt_stat) {
		sbp->f_type = xfs_fstype;
		bcopy(mp->mnt_stat.f_mntonname, sbp->f_mntonname, MNAMELEN);
		bcopy(mp->mnt_stat.f_mntfromname, sbp->f_mntfromname, MNAMELEN);
		bcopy((caddr_t)&vfsp->vfs_fsid, (caddr_t)&sbp->f_fsid, 
			sizeof (fsid_t));
		strncpy(sbp->f_fstypename, "xfs", MFSNAMELEN);
	}
	return (0);
}

int
_xfs_sync(struct vfs *vfsp, int waitfor, cred_t *cred)
{

	xfs_sync(vfs_fbhv(vfsp), SYNC_WAIT | SYNC_ATTR, cred);
	return (0);
}

int
_xfs_vget(struct vfs *vfsp, vnode_t **vpp, struct fid *fidp)
{

	return (xfs_vget(vfs_fbhv(vfsp), vpp, fidp));
}


struct vnode *
make_specvp(dev_t dev, enum vtype type)
{
	struct vnode *vp;

	if (vfinddev(dev, VCHR, &vp))
		return (vp);
	else
		return (NULL);
}

