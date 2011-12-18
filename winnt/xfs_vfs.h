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
#ifndef __XFS_VFS_H__
#define __XFS_VFS_H__

#include <sys/vfs.h>
#include <sys/mount.h>
#ifdef __KERNEL_STRICT_NAMES
typedef __kernel_fsid_t fsid_t;
#endif


#define vfs_fbhv(vfsp)	((bhv_head_t *)(vfsp->vfs_data))->bh_first
					/* 1st on vfs behavior chain */
#define VFS_FOPS(vfsp)  ((vfsops_t *)(vfs_fbhv(vfsp)->bd_ops))
					/* ops for 1st behavior */


#define bhvtovfs(bdp)	((struct vfs *)BHV_VOBJ(bdp))
#define VFS_BHVHEAD(vfsp) ((bhv_head_t *)(vfsp)->vfs_data)

#define VFS_FSTYPE_ANY		-1	/* fstype arg to vfs_devsearch* , 	
					   vfs_busydev if the filesystem type
					   is irrelevant for the search */


#define VFS_GRPID	0x0008		/* group-ID assigned from directory */
#define VFS_NOTRUNC	0x0020		/* does not truncate long file names */
#define VFS_MLOCK       0x0100          /* lock vfs so that subtree is stable */
#define VFS_MWAIT       0x0200          /* waiting for access lock */
#define VFS_MWANT       0x0400          /* waiting for update */

#define VFS_OFFLINE	0x2000		/* filesystem is being unmounted */

#define SYNC_NOWAIT     0x0000          /* start delayed writes */
#define SYNC_ATTR	0x0001		/* sync attributes */
#define SYNC_CLOSE	0x0002		/* close file system down */
#define SYNC_DELWRI	0x0004		/* look at delayed writes */
#define SYNC_WAIT	0x0008		/* wait for i/o to complete */
#define SYNC_FSDATA	0x0020		/* flush fs data (e.g. superblocks) */
#define SYNC_BDFLUSH	0x0010		/* BDFLUSH is calling -- don't block */
#define SYNC_PDFLUSH	0x0040		/* push v_dpages */


/*
 * WARNING: fields should be same size & order as struct mount_args
 */
struct mounta {
	char		*spec;
	char		*dir;
	int		flags;
	char		*dataptr;
	int		datalen;
};

#define VFS_REMOVEBHV(vfsp, bdp)	bhv_remove(VFS_BHVHEAD(vfsp), bdp)

#define PVFS_UNMOUNT(bdp,f,cr, rv)			\
{							\
	rv = ENOSYS;					\
	printf("PVFS_UNMOUNT: unimplemented\n"); 	\
}

#define PVFS_SYNC(bdp,f,cr, rv)				\
{							\
	rv = ENOSYS;					\
	printf("PVFS_SYNC: unimplemented\n"); 		\
}
/*
 * Set and clear vfs_flag bits.
 */
#define vfs_setflag(vfsp,f)     { /* pl_t s = mp_mutex_spinlock(&vfslock); */ \
                                  (vfsp)->vfs_flag |= (f); \
                                  /* mp_mutex_spinunlock(&vfslock, s); */ }


#define	vfsp_wait(V,P,S)        mp_sv_wait(&(V)->vfs_wait,P,&vfslock,s)
#define	vfsp_waitsig(V,P,S)     mp_sv_wait_sig(&(V)->vfs_wait,P,&vfslock,s)

static __inline__
vfs_insertbhv(vfs_t *vfsp, bhv_desc_t *bdp, vfsops_t *vfsops, void *mount)
{

	/*
	 * Initialize behavior desc with ops and data and then
	 * attach it to the vfs. 
	 */
	bhv_desc_init(bdp, mount, vfsp, vfsops);
	bhv_insert_initial(vfsp->vfs_data, bdp);
}

static __inline__ int
vfs_busy(struct vfs *vfsp)
{

	printf("vfs_busy: unimplemented\n");
	return (0);
}

static __inline__ void
vfs_unbusy(struct vfs *vfsp)
{

}

static __inline__ struct vfs *
vfs_busydev(dev_t dev, int type)
{

	printf("vfs_busydev: unimplemented\n");
	return (NULL);
}

/*
 * Lock a filesystem to prevent access to it while mounting.
 * Returns error if already locked.
 */
static __inline__ int
vfs_lock(struct vfs *vfsp)
{

	printf("vfs_lock: unimplemented\n");
	return (0);
}

/*                                                                              
 * Lock a filesystem and mark it offline,
 * to prevent access to it while unmounting.
 * Returns error if already locked.
 */      
static __inline__ int
vfs_lock_offline(struct vfs *vfsp)
{

	printf("vfs_lock_offline: unimplemented\n");
	return (0);
}

/*
 * Unlock a locked filesystem.
 */
static __inline__ void
vfs_unlock(struct vfs *vfsp)
{

	printf("vfs_unlock: unimplemented\n");
}


#endif	/* __XFS_VFS_H__ */
