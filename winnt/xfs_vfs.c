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
#include <linux/locks.h>
#include <linux/smp_lock.h>


/*
 * VFS global data.
 */
spinlock_t	vfslock;	/* spinlock protecting rootvfs and vfs_flag */
sema_t 		synclock;	/* sync in progress; initialized in sinit() */
                                

/*
 * Allocate and initialize a new vfs
 */
vfs_t *
vfs_allocate(void)
{
	vfs_t	*vfsp;

        vfsp = kmalloc(sizeof(vfs_t), GFP_KERNEL);
	memset(vfsp, 0, sizeof(vfs_t));
	ASSERT(vfsp);
	VFS_INIT(vfsp);
	return (vfsp);
}

void
vfs_deallocate(vfs_t *vfsp)
{
        VFS_FREE(vfsp);
        kfree(vfsp);
}

/*
 * Implement a simplified multi-access, single-update protocol for vfs.
 *
 * Only one update-lock (mount/unmount) can be held at a time; if another
 * process holds the vfs stucture with update capabilities, or is waiting
 * to acquire update rights, other update calls fail.
 *
 * Multiple accessors are allowed: vfs_busycnt tracks the number of
 * concurrent accesses.  Update permission sleeps until the last access
 * has finished, but leaves the VFS_MWANT flag to hold (if called via
 * vfs_busy) or reject (called via vfs_busydev or vfs_lock) subsequent
 * accesses/updates.
 * Note that traverse understands the vfs locking model and waits for
 * any update to complete, and retries the mount-point traversal.
 *
 * Accessors include: vfs_syncall, traverse, VFS_STATVFS, and quota checking.
 */
STATIC int
vfs_lock_flags(struct vfs *vfsp, int flags)
{
	register int error;
	long s;

	spin_lock_irqsave(&vfslock, s);
	if (vfsp->vfs_flag & (VFS_MLOCK|VFS_MWANT)) {
		spin_unlock_irqrestore(&vfslock, s);
		return EBUSY;
	}

	while (vfsp->vfs_busycnt) {
		ASSERT(vfsp->vfs_busycnt > 0);
		ASSERT(!(vfsp->vfs_flag & (VFS_MLOCK|VFS_OFFLINE)));
		vfsp->vfs_flag |= VFS_MWAIT|VFS_MWANT;
		vfsp_waitsig(vfsp, PVFS, s); /* REMOVED setting error. */
		error = 0; /* JIMJIM always no error */
		spin_lock_irqsave(&vfslock, s);
		if (error) {
			ASSERT(vfsp->vfs_flag & VFS_MWANT);
			vfsp->vfs_flag &= ~VFS_MWANT;
			if (vfsp->vfs_flag & VFS_MWAIT) {
				vfsp->vfs_flag &= ~VFS_MWAIT;
				sv_broadcast(&vfsp->vfs_wait);
			}
			spin_unlock_irqrestore(&vfslock, s);
			return EINTR;
		}
		ASSERT(vfsp->vfs_flag & VFS_MWANT);
		vfsp->vfs_flag &= ~VFS_MWANT;
	}

	ASSERT((vfsp->vfs_flag & (VFS_MLOCK|VFS_OFFLINE)) != VFS_OFFLINE);
	if (vfsp->vfs_flag & VFS_MLOCK) {
		error = EBUSY;
	} else {
		vfsp->vfs_flag |= VFS_MLOCK|flags;
		error = 0;
	}
	spin_unlock_irqrestore(&vfslock, s);
	return error;
}

/*
 * Lock a filesystem to prevent access to it while mounting.
 * Returns error if already locked.
 */
int
vfs_lock(struct vfs *vfsp)
{
	return (vfs_lock_flags(vfsp, 0));
}

/*
 * Lock a filesystem and mark it offline,
 * to prevent access to it while unmounting.
 * Returns error if already locked.
 */
int
vfs_lock_offline(struct vfs *vfsp)
{
	return (vfs_lock_flags(vfsp, VFS_OFFLINE));
}

/*
 * Unlock a locked filesystem.
 */
void
vfs_unlock(register struct vfs *vfsp)
{
	long s;

	spin_lock_irqsave(&vfslock, s);
	ASSERT((vfsp->vfs_flag & (VFS_MWANT|VFS_MLOCK)) == VFS_MLOCK);
	vfsp->vfs_flag &= ~(VFS_MLOCK|VFS_OFFLINE);

	/*
	 * Wake accessors (traverse() or vfs_syncall())
	 * waiting for the lock to clear.
	 */
	if (vfsp->vfs_flag & VFS_MWAIT) {
		vfsp->vfs_flag &= ~VFS_MWAIT;
		sv_broadcast(&vfsp->vfs_wait);
	}
	spin_unlock_irqrestore(&vfslock, s);
}

/*
 * Get access permission for vfsp.
 */
int
vfs_busy(struct vfs *vfsp)
{
	long s;

	spin_lock_irqsave(&vfslock, s);
	ASSERT((vfsp->vfs_flag & (VFS_MLOCK|VFS_OFFLINE)) != VFS_OFFLINE);
	while (vfsp->vfs_flag & (VFS_MLOCK|VFS_MWANT)) {
		ASSERT(vfsp->vfs_flag & VFS_MWANT || vfsp->vfs_busycnt == 0);
		if (vfsp->vfs_flag & VFS_OFFLINE) {
			spin_unlock_irqrestore(&vfslock, s);
			return EBUSY;
		}
		vfsp->vfs_flag |= VFS_MWAIT;
		vfsp_waitsig(vfsp, PVFS, s);	/* JIMJIM this has no sig "nificance". */
		spin_lock_irqsave(&vfslock, s);
	}

	ASSERT(vfsp->vfs_busycnt >= 0);
	vfsp->vfs_busycnt++;
	spin_unlock_irqrestore(&vfslock, s);

	return 0;
}

/*
 * Given a <dev, filesystem-type> pair, return the vfs-entry for it.
 * If the type sent in is VFS_FSTYPE_ANY, then this'll only try to
 * match the device number.
 *
 * This extra parameter was necessary since duplicate vfs entries with
 * the same vfs_dev are possible because of lofs.
 */
struct vfs *
vfs_busydev(dev_t dev, int type)
{
	long s;
	struct vfs *vfsp;
	kdev_t	kdev = MKDEV(MAJOR(dev), MINOR(dev));
	struct super_block *sb;

	lock_kernel();
	sb = get_super(kdev);
	unlock_kernel();

	if (!sb)
		return NULL;
	
	vfsp = LINVFS_GET_VFS(sb);
again:
		spin_lock_irqsave(&vfslock, s);
		if (vfsp->vfs_dev == dev &&
		    (type == VFS_FSTYPE_ANY || type == vfsp->vfs_fstype)) {

			if (vfsp->vfs_flag & VFS_OFFLINE) {
				spin_unlock_irqrestore(&vfslock, s);
				return NULL;
			}
			if (vfsp->vfs_flag & (VFS_MLOCK|VFS_MWANT)) {
				ASSERT(vfsp->vfs_flag & VFS_MWANT ||
				       vfsp->vfs_busycnt == 0);
				vfsp->vfs_flag |= VFS_MWAIT;
				vfsp_wait(vfsp, 0, s);	 /* JIMJIM removed PZERO */
				goto again;
			}

			ASSERT(vfsp->vfs_busycnt >= 0);
			vfsp->vfs_busycnt++;
		}
		spin_unlock_irqrestore(&vfslock, s);
		return vfsp;
}

void
vfs_unbusy(struct vfs *vfsp)
{
	long s;

	spin_lock_irqsave(&vfslock, s);
	ASSERT(!(vfsp->vfs_flag & (VFS_MLOCK|VFS_OFFLINE)));
	ASSERT(vfsp->vfs_busycnt > 0);
	if (--vfsp->vfs_busycnt == 0)
		vfs_unbusy_wakeup(vfsp);
	spin_unlock_irqrestore(&vfslock, s);
}

void
vfs_unbusy_wakeup(register struct vfs *vfsp)
{
        /*
         * If there's an updater (mount/unmount) waiting for the vfs lock,
         * wake up only it.  Updater should be the first on the sema queue.
         *
         * Otherwise, wake all accessors (traverse() or vfs_syncall())
         * waiting for the lock to clear.
         */
        if (vfsp->vfs_flag & VFS_MWANT) {
                sv_signal(&vfsp->vfs_wait);
        } else
        if (vfsp->vfs_flag & VFS_MWAIT) {
                vfsp->vfs_flag &= ~VFS_MWAIT;
                sv_broadcast(&vfsp->vfs_wait);
        }
}


/*
 * Search the vfs list for a specified device.  Returns a pointer to it
 * or NULL if no suitable entry is found.
 *
 * Any calls to this routine (as opposed to vfs_busydev) should
 * considered extremely suspicious.  Once the vfs_spinunlock is done,
 * there is likely to be nothing guaranteeing that the vfs pointer
 * returned continues to point to a vfs.  There are numerous bugs
 * which would quickly become intolerable if the frequency of unmount
 * was to rise above its typically low level.
 */
struct vfs *
vfs_devsearch(dev_t dev, int fstype)
{
	register struct vfs *vfsp;

	long s;

	spin_lock_irqsave(&vfslock, s);
	vfsp = vfs_devsearch_nolock(dev, fstype);
	spin_unlock_irqrestore(&vfslock, s);
	return vfsp;
}

/*
 * Same as vfs_devsearch without locking the list.
 * Useful for debugging code, but put it here anyway.
 */
struct vfs *
vfs_devsearch_nolock(dev_t dev, int fstype)
{
        register struct vfs *vfsp;
	kdev_t	kdev = MKDEV(MAJOR(dev), MINOR(dev));
	struct super_block *sb;

	lock_kernel();
	sb = get_super(kdev);

	if (sb) {
		vfsp = LINVFS_GET_VFS(sb);
                if ((vfsp->vfs_dev == dev) &&
                    (fstype == VFS_FSTYPE_ANY || fstype == vfsp->vfs_fstype)) {
			unlock_kernel();
                        return vfsp;
		}
	}
	unlock_kernel();
        return NULL;
}

void
vfsinit(void)
{
	/*
	 * Initialize vfs stuff.
	 */
	spin_lock_init(&vfslock);

	/*
	 * Initialize vnode stuff.
	 */
	vn_init();
}

/*
 * Called by fs dependent VFS_MOUNT code to link the VFS base file system 
 * dependent behavior with the VFS virtual object.
 */
void
vfs_insertbhv(
	vfs_t *vfsp, 
	bhv_desc_t *bdp, 
	vfsops_t *vfsops, 	
	void *mount)
{
	/* 
	 * Initialize behavior desc with ops and data and then
	 * attach it to the vfs.
	 */
	bhv_desc_init(bdp, mount, vfsp, vfsops);
	bhv_insert_initial(&vfsp->vfs_bh, bdp);
}
