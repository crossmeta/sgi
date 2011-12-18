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
 *
 * CROSSMETA Windows porting changes. http://www.crossmeta.org
 * Copyright (c) 2001 Supramani Sammandam.  suprasam _at_ crossmeta.org
 */

#include <xfs.h>
#include <linux/xfs_iops.h>

/*
 * Private vnode spinlock manipulation.
 *
 * Nested means there is no need to deal with interrupts (disable/enable)
 * so we can be quick about it.
 */
#define NESTED_VN_LOCK(vp)      spin_lock(&(vp)->v_lock)
#define NESTED_VN_UNLOCK(vp)    spin_unlock(&(vp)->v_lock)

uint64_t vn_generation;		/* vnode generation number */
atomic_t vn_vnumber;		/* # of vnodes ever allocated */

spinlock_t	vnumber_lock = SPIN_LOCK_UNLOCKED;

/*
 * Dedicated vnode inactive/reclaim sync semaphores.
 * Prime number of hash buckets since address is used as the key.
 */
#define NVSYNC                  37
#define vptosync(v)             (&vsync[((unsigned long)v) % NVSYNC])
sv_t vsync[NVSYNC];

/*
 * Translate stat(2) file types to vnode types and vice versa.
 * Aware of numeric order of S_IFMT and vnode type values.
 */
enum vtype iftovt_tab[] = {
	VNON, VFIFO, VCHR, VNON, VDIR, VNON, VBLK, VNON,
	VREG, VNON, VLNK, VNON, VSOCK, VNON, VNON, VNON
};

u_short vttoif_tab[] = {
	0, S_IFREG, S_IFDIR, S_IFBLK, S_IFCHR, S_IFLNK, S_IFIFO, 0, S_IFSOCK
};

void
vn_init(void)
{
        register sv_t *svp;
        register int i;

	for (svp = vsync, i = 0; i < NVSYNC; i++, svp++)
		init_sv(svp, SV_DEFAULT, "vsy", i);
}


/*
 * Clean a vnode of filesystem-specific data and prepare it for reuse.
 */
int
vn_reclaim(struct vnode *vp, int flag)
{
	int error, s;

	XFS_STATS_INC(vn_reclaim);

	vn_trace_entry(vp, "vn_reclaim", (inst_t *)__return_address);

	/*
	 * Only make the VOP_RECLAIM call if there are behaviors
	 * to call.
	 */
	if (vp->v_fbhv != NULL) {
		VOP_RECLAIM(vp, flag, error);
		if (error)
			return -error;
	}
	ASSERT(vp->v_fbhv == NULL);

	s = VN_LOCK(vp);

	vp->v_flag &= (VRECLM|VWAIT|VLOCK);
	VN_UNLOCK(vp, s);

	vp->v_type = VNON;
	vp->v_fbhv = NULL;

#ifdef  CONFIG_XFS_VNODE_TRACING
	ktrace_free(vp->v_trace);

	vp->v_trace = NULL;
#endif  /* CONFIG_XFS_VNODE_TRACING */

	return 0;
}

STATIC void
vn_wakeup(struct vnode *vp)
{
	int s = VN_LOCK(vp);
	if (vp->v_flag & VWAIT) {
		sv_broadcast(vptosync(vp));
	}
	vp->v_flag &= ~(VRECLM|VWAIT);
	VN_UNLOCK(vp, s);
}


struct vnode *
vn_address(struct inode *inode)
{
	vnode_t		*vp;


	vp = (vnode_t *)(&((inode)->u.xfs_i.vnode));

	if (vp->v_inode == NULL)
		return NULL;
	/*
	 * Catch half-constructed linux-inode/vnode/xfs-inode setups.
	 */
	if (vp->v_fbhv == NULL)
		return NULL;

	return vp;
}


struct vnode *
vn_initialize(vfs_t *vfsp, struct inode *inode, int from_readinode)
{
	struct vnode	*vp;
	xfs_inode_t	*ip;
	xfs_mount_t	*mp;
	int		s = 0;

	
	XFS_STATS_INC(vn_active);

	vp = LINVFS_GET_VN_ADDRESS(inode);

	vp->v_inode = inode;

	vp->v_flag = 0;

	atomic_inc(&vn_vnumber);

	spinlock_init(&vp->v_lock, "v_lock");
	if (from_readinode)
		s = VN_LOCK(vp);

	spin_lock(&vnumber_lock);
	vn_generation += 1;
	vp->v_number = vn_generation;
	spin_unlock(&vnumber_lock);

	ASSERT(vp->v_number);

	ASSERT(VN_CACHED(vp) == 0);

	/* Initialize the first behavior and the behavior chain head. */
	vn_bhv_head_init(VN_BHV_HEAD(vp), "vnode");

#ifdef	CONFIG_XFS_VNODE_TRACING
	vp->v_trace = ktrace_alloc(VNODE_TRACE_SIZE, KM_SLEEP);
#endif	/* CONFIG_XFS_VNODE_TRACING */

	/*
	 * Check to see if we've been called from
	 * read_inode, and we need to "stitch" it all
	 * together right now.
	 */
	if (from_readinode) {
		mp = XFS_BHVTOM(vfsp->vfs_fbhv);	ASSERT(mp);

		if (xfs_vn_iget(vp, mp, NULL, (xfs_ino_t)inode->i_ino,
								0, &ip, 0)) {
			panic("vn_initialize: vp/0x%p inode/0x%p bad xfs_iget!",
								vp, inode);
		}

		vp->v_vfsp  = vfsp;
		vp->v_inode = inode;
		vp->v_type  = IFTOVT(ip->i_d.di_mode);

		linvfs_set_inode_ops(inode);
		{ int error;
		error = linvfs_revalidate_core(inode, ATTR_COMM);
		if(error)
			printk("vn_initialize: linvfs_revalidate_core "
				"error %d\n",error);
		}
		VN_UNLOCK(vp, s);
	}

	vn_trace_exit(vp, "vn_initialize", (inst_t *)__return_address);

	return vp;
}

struct vnode *
vn_alloc(struct vfs *vfsp, __uint64_t ino, enum vtype type, int flags)
{
	struct inode	*inode;
	struct vnode	*vp;
	xfs_ino_t	inum = (xfs_ino_t) ino;

	XFS_STATS_INC(vn_alloc);

	inode = get_empty_inode();

	if (inode == NULL)
		return NULL;

	inode->i_sb    = vfsp->vfs_super;
	inode->i_dev   = vfsp->vfs_super->s_dev;
	inode->i_ino   = inum;
	inode->i_flags = 0;
	atomic_set(&inode->i_count, 1);
	inode->i_state = 0;

	vn_initialize(vfsp, inode, 0);

	vp = LINVFS_GET_VN_ADDRESS(inode);

	ASSERT((vp->v_flag & VPURGE) == 0);

	vp->v_vfsp  = vfsp;
	vp->v_type  = type;
	vp->v_inode = inode;

	vn_trace_exit(vp, "vn_alloc", (inst_t *)__return_address);

	return vp;
}


/*
 * Free an isolated vnode.
 * The vnode must not have any other references.
 */
void
vn_free(struct vnode *vp)
{
	struct inode *inode;

	XFS_STATS_INC(vn_free);

	vn_trace_entry(vp, "vn_free", (inst_t *)__return_address);

	ASSERT(vn_count(vp) == 1);

	ASSERT((vp->v_flag & VPURGE) == 0);
	vp->v_fbhv = NULL;
	inode = LINVFS_GET_IP(vp);
	inode->i_sb = NULL;
	iput(inode);
}


/*
 * Get a reference on a vnode.
 */
vnode_t *
vn_get(struct vnode *vp, vmap_t *vmap, uint flags)
{
	struct inode	*inode;
	xfs_ino_t	inum;

	XFS_STATS_INC(vn_get);

	inode = ihold(vmap->v_vfsp->vfs_super, vmap->v_ino);

	if (inode == NULL)		/* Inode not present */
		return NULL;

	inum = inode->i_ino;

	/*
	 * Verify that the linux inode we just 'grabbed'
	 * is still the one we want.
	 */
	if (vmap->v_number != vp->v_number) {
printk("vn_get: vp/0x%p inode/0x%p v_number %Ld/%Ld\n",
vp, inode, vmap->v_number, vp->v_number);

		goto fail;
	}

	if (vmap->v_ino != inum) {
printk("vn_get: vp/0x%p inode/0x%p v_ino %Ld/%Ld\n",
vp, inode, vmap->v_ino, inum);

		goto fail;
	}

	vn_trace_exit(vp, "vn_get", (inst_t *)__return_address);

	ASSERT((vp->v_flag & VPURGE) == 0);

	return vp;

fail:
	iput(inode);

	return NULL;
}


/*
 * "Temporary" routine to return the linux inode
 * hold count, after everybody else can directly
 * reference the inode (header magic!), this
 * routine is dead meat..
 */
int
vn_count(struct vnode *vp)
{
	struct inode *inode;

	inode = LINVFS_GET_IP(vp);

	ASSERT(inode);

	return atomic_read(&inode->i_count);
}

/*
 * "hash" the linux inode.
 */
void
vn_insert_in_linux_hash(struct vnode *vp)
{
	struct inode *inode;

	vn_trace_entry(vp, "vn_insert_in_linux_hash",
				(inst_t *)__return_address);

	inode = LINVFS_GET_IP(vp);

	ASSERT(inode);

	ASSERT(list_empty(&inode->i_hash));

	insert_inode_hash(inode);	/* Add to i_hash	*/
	mark_inode_dirty(inode);	/* Add to i_list	*/
}


/*
 * "revalidate" the linux inode.
 */
int
vn_revalidate(struct vnode *vp, int flags)
{
	int		error;
	struct inode	*inode;
	vattr_t		va;

	vn_trace_entry(vp, "vn_revalidate", (inst_t *)__return_address);

	va.va_mask = AT_STAT|AT_GENCOUNT;

	ASSERT(vp->v_bh.bh_first != NULL);

	VOP_GETATTR(vp, &va, flags & ATTR_LAZY, NULL, error);

	if (! error) {
		inode = LINVFS_GET_IP(vp);

		ASSERT(inode);

		inode->i_mode       = VTTOIF(va.va_type) | va.va_mode;
		inode->i_nlink      = va.va_nlink;
		inode->i_uid        = va.va_uid;
		inode->i_gid        = va.va_gid;
		inode->i_rdev       = va.va_rdev;
		inode->i_blksize    = va.va_blksize;
		inode->i_generation = va.va_gencount;
		if ((flags & ATTR_COMM) ||
		    S_ISREG(inode->i_mode) ||
		    S_ISDIR(inode->i_mode) ||
		    S_ISLNK(inode->i_mode)) {
			inode->i_size       = va.va_size;
			inode->i_blocks     = va.va_nblocks;
			inode->i_atime      = va.va_atime.tv_sec;
			inode->i_mtime      = va.va_mtime.tv_sec;
			inode->i_ctime      = va.va_ctime.tv_sec;
		}
	} else {
		vn_trace_exit(vp, "vn_revalidate.error",
					(inst_t *)__return_address);
	}

	return -error;
}


/*
 * purge a vnode from the cache
 * At this point the vnode is guaranteed to have no references (vn_count == 0)
 * The caller has to make sure that there are no ways someone could
 * get a handle (via vn_get) on the vnode (usually done via a mount/vfs lock).
 */
void
vn_purge(struct vnode *vp, vmap_t *vmap)
{
	vn_trace_entry(vp, "vn_purge", (inst_t *)__return_address);

	ASSERT(vp->v_flag & VPURGE);

again:
	/*
	 * Check whether vp has already been reclaimed since our caller
	 * sampled its version while holding a filesystem cache lock that
	 * its VOP_RECLAIM function acquires.
	 */
	NESTED_VN_LOCK(vp);
	if (vp->v_number != vmap->v_number) {
		NESTED_VN_UNLOCK(vp);
		return;
	}

	/*
	 * If vp is being reclaimed or inactivated, wait until it is inert,
	 * then proceed.  Can't assume that vnode is actually reclaimed
	 * just because the reclaimed flag is asserted -- a vn_alloc
	 * reclaim can fail.
	 */
	if (vp->v_flag & (VINACT | VRECLM)) {
		ASSERT(vn_count(vp) == 0);
		vp->v_flag |= VWAIT;
		sv_wait(vptosync(vp), PINOD, &vp->v_lock, 0);
		goto again;
	}

	/*
	 * Another process could have raced in and gotten this vnode...
	 */
	if (vn_count(vp) > 0) {
		NESTED_VN_UNLOCK(vp);
		return;
	}

	XFS_STATS_DEC(vn_active);
	vp->v_flag |= VRECLM;
	NESTED_VN_UNLOCK(vp);

	/*
	 * Call VOP_RECLAIM and clean vp. The FSYNC_INVAL flag tells
	 * vp's filesystem to flush and invalidate all cached resources.
	 * When vn_reclaim returns, vp should have no private data,
	 * either in a system cache or attached to v_data.
	 */
	if (vn_reclaim(vp, FSYNC_INVAL) != 0)
		panic("vn_purge: cannot reclaim");

	/*
	 * Wakeup anyone waiting for vp to be reclaimed.
	 */
	vn_wakeup(vp);
}

/*
 * Add a reference to a referenced vnode.
 */
struct vnode *
vn_hold(struct vnode *vp)
{
	register int s = VN_LOCK(vp);
	struct inode *inode;

	XFS_STATS_INC(vn_hold);

	inode = LINVFS_GET_IP(vp);

	inode = igrab(inode);

	ASSERT(inode);

	VN_UNLOCK(vp, s);

	return vp;
}

/*
 * Release a vnode. 
 */
void
vn_rele(struct vnode *vp)
{
	iput(LINVFS_GET_IP(vp));
}

/*
 *  Call VOP_INACTIVE on last reference.
 */
void
vn_put(struct vnode *vp)
{
	int	s;
	int	vcnt;
	/* REFERENCED */
	int cache;

	XFS_STATS_INC(vn_rele);

	vn_trace_entry(vp, "vn_rele", (inst_t *)__return_address);

	s = VN_LOCK(vp);

	vcnt = vn_count(vp);

	ASSERT(vcnt > 0);

	/*
	 * Since we always get called from put_inode we know 
	 * that i_count won't be decremented after we
	 * return. 
	 */
	if (vcnt == 1) {
		/*
		 * It is absolutely, positively the case that
		 * the lock manager will not be releasing vnodes
		 * without first having released all of its locks.
		 */
		ASSERT(!(vp->v_flag & VLOCKHOLD));

		/*
		 * As soon as we turn this on, noone can find us in vn_get
		 * until we turn off VINACT or VRECLM
		 */
		vp->v_flag |= VINACT;
		VN_UNLOCK(vp, s);

		/*
		 * Do not make the VOP_INACTIVE call if there
		 * are no behaviors attached to the vnode to call.
		 */
		if (vp->v_fbhv != NULL) {
			VOP_INACTIVE(vp, get_current_cred(), cache);
		}

		s = VN_LOCK(vp);

		vp->v_flag &= ~(VINACT|VWAIT|VRECLM|VGONE);

	}

	VN_UNLOCK(vp, s);

	vn_trace_exit(vp, "vn_rele", (inst_t *)__return_address);
}


/*
 * Finish the removal of a vnode.
 */
void
vn_remove(struct vnode *vp)
{
	/* REFERENCED */
	vmap_t  vmap;

	XFS_STATS_INC(vn_remove);

	vn_trace_exit(vp, "vn_remove", (inst_t *)__return_address);

	/*
	 * After the following purge the vnode
	 * will no longer exist.
	 */
	VMAP(vp, XFS_BHVTOI(vp->v_fbhv), vmap);

	vn_purge(vp, &vmap);

	vp->v_inode = NULL;		/* No more references to inode */
}


#ifdef	CONFIG_XFS_VNODE_TRACING

#define	cpuid smp_processor_id
#define	current_pid() (current->pid)

/*
 * Vnode tracing code.
 */
void
vn_trace_entry(vnode_t *vp, char *func, inst_t *ra)
{
	ktrace_enter(	vp->v_trace,
/*  0 */		(void *)(__psint_t)VNODE_KTRACE_ENTRY,
/*  1 */		(void *)func,
/*  2 */		0,
/*  3 */		(void *)(vp->v_inode ? vn_count(vp): -9),
/*  4 */		(void *)ra,
/*  5 */		(void *)(__psunsigned_t)vp->v_flag,
/*  6 */		(void *)(__psint_t)cpuid(),
/*  7 */		(void *)(__psint_t)current_pid(),
/*  8 */		(void *)__return_address,
/*  9 */		0, 0, 0, 0, 0, 0, 0);
}

void
vn_trace_exit(vnode_t *vp, char *func, inst_t *ra)
{
	ktrace_enter(	vp->v_trace,
/*  0 */		(void *)(__psint_t)VNODE_KTRACE_EXIT,
/*  1 */		(void *)func,
/*  2 */		0,
/*  3 */		(void *)(vp->v_inode ? vn_count(vp) : -9),
/*  4 */		(void *)ra,
/*  5 */		(void *)(__psunsigned_t)vp->v_flag,
/*  6 */		(void *)(__psint_t)cpuid(),
/*  7 */		(void *)(__psint_t)current_pid(),
/*  8 */		(void *)__return_address,
/*  9 */		0, 0, 0, 0, 0, 0, 0);
}

void
vn_trace_hold(vnode_t *vp, char *file, int line, inst_t *ra)
{
	ktrace_enter(	vp->v_trace,
/*  0 */		(void *)(__psint_t)VNODE_KTRACE_HOLD,
/*  1 */		(void *)file,
/*  2 */		(void *)(__psint_t)line,
/*  3 */		(void *)(vp->v_inode ? vn_count(vp) : -9),
/*  4 */		(void *)ra,
/*  5 */		(void *)(__psunsigned_t)vp->v_flag,
/*  6 */		(void *)(__psint_t)cpuid(),
/*  7 */		(void *)(__psint_t)current_pid(),
/*  8 */		(void *)__return_address,
/*  9 */		0, 0, 0, 0, 0, 0, 0);
}

void
vn_trace_ref(vnode_t *vp, char *file, int line, inst_t *ra)
{
	ktrace_enter(	vp->v_trace,
/*  0 */		(void *)(__psint_t)VNODE_KTRACE_REF,
/*  1 */		(void *)file,
/*  2 */		(void *)(__psint_t)line,
/*  3 */		(void *)(vp->v_inode ? vn_count(vp) : -9),
/*  4 */		(void *)ra,
/*  5 */		(void *)(__psunsigned_t)vp->v_flag,
/*  6 */		(void *)(__psint_t)cpuid(),
/*  7 */		(void *)(__psint_t)current_pid(),
/*  8 */		(void *)__return_address,
/*  9 */		0, 0, 0, 0, 0, 0, 0);
}

void
vn_trace_rele(vnode_t *vp, char *file, int line, inst_t *ra)
{
	ktrace_enter(	vp->v_trace,
/*  0 */		(void *)(__psint_t)VNODE_KTRACE_RELE,
/*  1 */		(void *)file,
/*  2 */		(void *)(__psint_t)line,
/*  3 */		(void *)(vp->v_inode ? vn_count(vp) : -9),
/*  4 */		(void *)ra,
/*  5 */		(void *)(__psunsigned_t)vp->v_flag,
/*  6 */		(void *)(__psint_t)cpuid(),
/*  7 */		(void *)(__psint_t)current_pid(),
/*  8 */		(void *)__return_address,
/*  9 */		0, 0, 0, 0, 0, 0, 0);
}
#endif	/* CONFIG_XFS_VNODE_TRACING */
