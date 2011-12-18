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
#include <linux/bitops.h>
#include <linux/locks.h>
#include <linux/smp_lock.h>
#include <linux/xfs_iops.h>
#include <linux/blkdev.h>
#include <linux/init.h>
#include <linux/page_buf.h>

/* xfs_vfs[ops].c */
extern void vfsinit(void);
extern int  xfs_init(int fstype);
extern void xfs_cleanup(void);

extern void dmapi_init(void);
extern void dmapi_uninit(void);

static struct super_operations linvfs_sops;


#define	MS_DATA		0x04

#define MNTOPT_LOGBUFS  "logbufs"       /* number of XFS log buffers */
#define MNTOPT_LOGBSIZE "logbsize"      /* size of XFS log buffers */
#define MNTOPT_LOGDEV	"logdev"	/* log device */
#define MNTOPT_RTDEV	"rtdev"		/* realtime I/O device */
#define MNTOPT_DMAPI    "dmapi"         /* DMI enabled (DMAPI / XDSM) */
#define MNTOPT_XDSM     "xdsm"          /* DMI enabled (DMAPI / XDSM) */
#define MNTOPT_BIOSIZE  "biosize"       /* log2 of preferred buffered io size */
#define MNTOPT_WSYNC    "wsync"         /* safe-mode nfs compatible mount */
#define MNTOPT_NOATIME  "noatime"       /* don't modify access times on reads */
#define MNTOPT_INO64    "ino64"         /* force inodes into 64-bit range */
#define MNTOPT_NOALIGN  "noalign"       /* turn off stripe alignment */
#define MNTOPT_SUNIT    "sunit"         /* data volume stripe unit */
#define MNTOPT_SWIDTH   "swidth"        /* data volume stripe width */
#define MNTOPT_NORECOVERY "norecovery"  /* don't run XFS recovery */
#define MNTOPT_OSYNCISDSYNC "osyncisdsync" /* o_sync == o_dsync on this fs */
#define MNTOPT_QUOTA    "quota"         /* disk quotas */
#define MNTOPT_MRQUOTA  "mrquota"       /* don't turnoff if SB has quotas on */
#define MNTOPT_NOSUID   "nosuid"        /* disallow setuid program execution */
#define MNTOPT_NOQUOTA  "noquota"       /* no quotas */
#define MNTOPT_UQUOTA   "usrquota"      /* user quota enabled */
#define MNTOPT_GQUOTA   "grpquota"      /* group quota enabled */
#define MNTOPT_UQUOTANOENF "uqnoenforce"/* user quota limit enforcement */
#define MNTOPT_GQUOTANOENF "gqnoenforce"/* group quota limit enforcement */
#define MNTOPT_QUOTANOENF  "qnoenforce" /* same as uqnoenforce */
#define MNTOPT_RO       "ro"            /* read only */
#define MNTOPT_RW       "rw"            /* read/write */

STATIC int
mountargs_xfs(
	char		*options,
	struct xfs_args	*args)
{
	char	*this_char, *value, *eov;
	int  logbufs = -1;
	int  logbufsize = -1;
	int dsunit, dswidth, vol_dsunit, vol_dswidth;
	int iosize;
	int error;

	iosize = dsunit = dswidth = vol_dsunit = vol_dswidth = 0;
	memset(args, 0, sizeof(*args));
	args->version = 3;
	for (this_char = strtok (options, ",");
	     this_char != NULL;
	     this_char = strtok (NULL, ",")) {

		if ((value = strchr (this_char, '=')) != NULL)
			*value++ = 0;

		if (!strcmp(this_char, MNTOPT_LOGBUFS)) {
			if (!strcmp(value, "none")) {
				logbufs = 0;
				printk(
			    "mount: this FS is trash after writing to it\n");
			} else {
				logbufs = simple_strtoul(value, &eov, 10);
				if (logbufs < 2 || logbufs > 8) {
					printk(
					"mount: Illegal logbufs amount: %d\n",
						logbufs);
					return 1;
				}
			}
		} else if (!strcmp(this_char, MNTOPT_LOGBSIZE)) {
			logbufsize = simple_strtoul(value, &eov, 10);
			if (logbufsize != 16*1024 && logbufsize != 32*1024) {
				printk(
			"mount: Illegal logbufsize: %d (not 16k or 32k)\n",
						logbufsize);
				return 1;
			}
		} else if (!strcmp(this_char, MNTOPT_LOGDEV)) {
			struct nameidata nd;

			if (!value)
				continue;

			error = 0;
			lock_kernel();
			if (path_init(value, LOOKUP_FOLLOW, &nd))
				error = path_walk(value, &nd);
			
			if (error) {
				unlock_kernel();
				printk(
			"mount: Invalid log device \"%s\", error = %d\n",
							       value, error);
				return 1;
			}
			args->logdev = nd.dentry->d_inode->i_rdev;
			path_release(&nd);
			unlock_kernel();
		} else if (!strcmp(this_char, MNTOPT_DMAPI)) {
			args->flags |= XFSMNT_DMAPI;
		} else if (!strcmp(this_char, MNTOPT_XDSM)) {
			args->flags |= XFSMNT_DMAPI;
                } else if (!strcmp(this_char, MNTOPT_RTDEV)) {
			struct nameidata nd;

                        if (!value)
                                continue;

			error = 0;
                        lock_kernel();
                        if (path_init(value, LOOKUP_FOLLOW, &nd))
				error = path_walk(value, &nd);

                        if (error) {
				unlock_kernel();
                                printk(
			"mount: Invalid realtime device \"%s\", error = %d\n",
							       value, error);
                                return 1;
                        }
                        args->rtdev = nd.dentry->d_inode->i_rdev;       
			path_release(&nd);
			unlock_kernel();
		} else if (!strcmp(this_char, MNTOPT_BIOSIZE)) {
			iosize = simple_strtoul(value, &eov, 10);
			if (iosize > 255 || iosize <= 0) {
				printk(
			"mount: illegal biosize %d, value out of bounds\n",
						iosize);
				return 1;
			}
			args->flags |= XFSMNT_IOSIZE;
			args->iosizelog = (uint8_t) iosize;
		} else if (!strcmp(this_char, MNTOPT_WSYNC)) {
			args->flags |= XFSMNT_WSYNC;
		} else if (!strcmp(this_char, MNTOPT_NOATIME)) {
			args->flags |= XFSMNT_NOATIME;
		} else if (!strcmp(this_char, MNTOPT_OSYNCISDSYNC)) {
			args->flags |= XFSMNT_OSYNCISDSYNC;
		} else if (!strcmp(this_char, MNTOPT_NORECOVERY)) {
			args->flags |= XFSMNT_NORECOVERY;
		} else if (!strcmp(this_char, MNTOPT_INO64)) {
#ifdef XFS_BIG_FILESYSTEMS
			args->flags |= XFSMNT_INO64;
#else
			printk("mount: ino64 option not allowed on this system\n");
			return 1;
#endif
		} else if (!strcmp(this_char, MNTOPT_UQUOTA)) {
			args->flags |= XFSMNT_UQUOTA | XFSMNT_UQUOTAENF;
		} else if (!strcmp(this_char, MNTOPT_QUOTA)) {
			args->flags |= XFSMNT_UQUOTA | XFSMNT_UQUOTAENF;
		} else if (!strcmp(this_char, MNTOPT_UQUOTANOENF)) {
			args->flags |= XFSMNT_UQUOTA;
			args->flags &= ~XFSMNT_UQUOTAENF;
		} else if (!strcmp(this_char, MNTOPT_QUOTANOENF)) {
			args->flags |= XFSMNT_UQUOTA;
			args->flags &= ~XFSMNT_UQUOTAENF;
		} else if (!strcmp(this_char, MNTOPT_MRQUOTA)) {
			args->flags |= XFSMNT_QUOTAMAYBE;
		} else if (!strcmp(this_char, MNTOPT_GQUOTA)) {
			args->flags |= XFSMNT_GQUOTA | XFSMNT_GQUOTAENF;
		} else if (!strcmp(this_char, MNTOPT_GQUOTANOENF)) {
			args->flags |= XFSMNT_GQUOTA;
			args->flags &= ~XFSMNT_GQUOTAENF;
		} else if (!strcmp(this_char, MNTOPT_NOALIGN)) {
			args->flags |= XFSMNT_NOALIGN;
		} else if (!strcmp(this_char, MNTOPT_SUNIT)) {
			dsunit = simple_strtoul(value, &eov, 10);
		} else if (!strcmp(this_char, MNTOPT_SWIDTH)) {
			dswidth = simple_strtoul(value, &eov, 10);
		} else if (!strcmp(this_char, MNTOPT_RO)) {
			args->flags |= MS_RDONLY;
		} else if (!strcmp(this_char, MNTOPT_NOSUID)) {
			args->flags |= MS_NOSUID;
		} else {
			printk(
			"mount: unknown mount option \"%s\".\n", this_char);
			return 1;
                }
                    
	}

	if (args->flags & XFSMNT_NORECOVERY) {
		if ((args->flags & MS_RDONLY) == 0) {
			printk(
			"mount: no-recovery XFS mounts must be read-only.\n");
			return 1;
		}
	}

	if ((args->flags & XFSMNT_NOALIGN) && (dsunit || dswidth)) {
		printk(
"mount: sunit and swidth options are incompatible with the noalign option\n");
		return 1;
	}

	if (dsunit && !dswidth || !dsunit && dswidth) {
		printk(
"mount: both sunit and swidth options have to be specified\n");
		return 1;
	}

	if (dsunit && (dswidth % dsunit != 0)) {
		printk(
"mount: stripe width (%d) has to be a multiple of the stripe unit (%d)\n",
			dswidth, dsunit);
		return 1;
	}

	if ((args->flags & XFSMNT_NOALIGN) != XFSMNT_NOALIGN) {

		if (dsunit) { 
			args->sunit = dsunit;
			args->flags |= XFSMNT_RETERR;
		} else 
			args->sunit = vol_dsunit;	
		dswidth ? (args->swidth = dswidth) : 
			  (args->swidth = vol_dswidth);
	} else 
		args->sunit = args->swidth = 0;

	args->logbufs = logbufs;
	args->logbufsize = logbufsize;
	return 0;
}

int
spectodevs(
	struct super_block *sb,
	struct xfs_args *args,
	dev_t	*ddevp,
	dev_t	*logdevp,
	dev_t	*rtdevp)
{
        *ddevp = sb->s_dev;
        if (args->logdev)
                *logdevp = args->logdev;
        else
                *logdevp = *ddevp;
        if (args->rtdev)
                *rtdevp = args->rtdev;
        else
                *rtdevp = 0;
	return 0;
}

static struct inode_operations linvfs_meta_ops = {
};

static struct address_space_operations linvfs_meta_aops = {
	sync_page:		block_sync_page,
};

struct inode *
linvfs_make_inode(kdev_t kdev, struct super_block *sb)
{
	struct inode *inode = get_empty_inode();

	inode->i_dev = kdev;
	inode->i_op = &linvfs_meta_ops;
	inode->i_mapping->a_ops = &linvfs_meta_aops;
	inode->i_sb = sb;


	pagebuf_lock_enable(inode);

	return inode;
}

void
linvfs_release_inode(struct inode *inode)
{
	if (inode) {
		pagebuf_delwri_flush(inode, PBDF_WAIT, NULL);
		pagebuf_lock_disable(inode);
		truncate_inode_pages(&inode->i_data, 0L);
		iput(inode);
	}
}


struct super_block *
linvfs_read_super(
	struct super_block *sb,
	void		*data,
	int		silent)
{
	vfsops_t	*vfsops;
	extern vfsops_t xfs_vfsops;
	vfs_t		*vfsp;
	vnode_t		*cvp, *rootvp;

	struct mounta	ap;
	struct mounta	*uap = &ap;
	char		spec[256];
	struct xfs_args	arg, *args = &arg;
	int		error;
	statvfs_t	statvfs;
	struct		inode *ip, *cip;

	/* first mount pagebuf delayed write daemon not running yet */
	if (pagebuf_daemon_start() < 0) {
		goto fail_daemon;
	}

	/*  Setup the uap structure  */

	memset(uap, 0, sizeof(struct mounta));

	sprintf(spec, bdevname(sb->s_dev));
	uap->spec = spec;

	uap->flags = MS_DATA;

	memset(args, 0, sizeof(struct xfs_args));
	if (mountargs_xfs((char *)data, args) != 0) {
		return NULL;
	}

	args->fsname = uap->spec;
  
	uap->dataptr = (char *)args;
	uap->datalen = sizeof(*args);

	/*  Kludge in XFS until we have other VFS/VNODE FSs  */

	vfsops = &xfs_vfsops;

	/*  Set up the vfs_t structure  */

	vfsp = vfs_allocate();

	if (sb->s_flags & MS_RDONLY)
                vfsp->vfs_flag |= VFS_RDONLY;

	/*  Setup up the cvp structure  */

	cip = (struct inode *)kmem_alloc(sizeof(struct inode),0);
	bzero(cip, sizeof(*cip));

	atomic_set(&cip->i_count, 1);

	cvp = LINVFS_GET_VN_ADDRESS(cip);

	cvp->v_type   = VDIR;
	cvp->v_number = 1;		/* Place holder */
	cvp->v_inode  = cip;

#ifdef  CONFIG_XFS_VNODE_TRACING
	cvp->v_trace = ktrace_alloc(VNODE_TRACE_SIZE, KM_SLEEP);
#endif  /* CONFIG_XFS_VNODE_TRACING */

	vn_trace_entry(cvp, "linvfs_read_super", (inst_t *)__return_address);

        cvp->v_flag |= VMOUNTING;

	vn_bhv_head_init(VN_BHV_HEAD(cvp), "vnode");	/* for DMAPI */

	LINVFS_SET_CVP(sb, cvp);
	vfsp->vfs_super = sb;


	sb->s_blocksize = 512;
	sb->s_blocksize_bits = ffs(sb->s_blocksize) - 1;
	set_blocksize(sb->s_dev, 512);

	sb->s_op = &linvfs_sops;
	sb->dq_op = NULL;

	LINVFS_SET_VFS(sb, vfsp);

	VFSOPS_MOUNT(vfsops, vfsp, cvp, uap, NULL, sys_cred, error);
	if (error)
		goto fail_vfsop;

	VFS_STATVFS(vfsp, &statvfs, NULL, error);
	if (error)
		goto fail_unmount;

	sb->s_magic = XFS_SB_MAGIC;
	sb->s_dirt = 1;  /*  Make sure we get regular syncs  */

	/* For kernels which have the s_maxbytes field - set it */
#ifdef MAX_NON_LFS
	sb->s_maxbytes = XFS_MAX_FILE_OFFSET;
#endif

        VFS_ROOT(vfsp, &rootvp, error);
        if (error)
                goto fail_unmount;

	ip = LINVFS_GET_IP(rootvp);

	linvfs_set_inode_ops(ip);

	sb->s_root = d_alloc_root(ip);

	if (!sb->s_root)
		goto fail_vnrele;

	if (is_bad_inode((struct inode *) sb->s_root))
		goto fail_vnrele;

	/* Don't set the VFS_DMI flag until here because we don't want
	 * to send events while replaying the log.
	 */
	if (args->flags & XFSMNT_DMAPI)
		vfsp->vfs_flag |= VFS_DMI;

	vn_trace_exit(rootvp, "linvfs_read_super", (inst_t *)__return_address);

	return(sb);

fail_vnrele:
	VN_RELE(rootvp);

fail_unmount:
	VFS_UNMOUNT(vfsp, 0, sys_cred, error);
	/*  We need to do something here to shut down the 
		VNODE/VFS layer.  */

fail_vfsop:
	vfs_deallocate(vfsp);

#ifdef  CONFIG_XFS_VNODE_TRACING
	ktrace_free(cvp->v_trace);

	cvp->v_trace = NULL;
#endif  /* CONFIG_XFS_VNODE_TRACING */

	kfree(cvp->v_inode);
        
fail_daemon:

	return(NULL);
}

void
linvfs_set_inode_ops(
	struct inode	*inode)
{
	vnode_t	*vp;

	vp = LINVFS_GET_VN_ADDRESS(inode);

	inode->i_mode = VTTOIF(vp->v_type);

	if (vp->v_type == VNON) {
		make_bad_inode(inode);
	} else if (S_ISREG(inode->i_mode)) {
		inode->i_op = &linvfs_file_inode_operations;
		inode->i_fop = &linvfs_file_operations;
		inode->i_mapping->a_ops = &linvfs_aops;
	} else if (S_ISDIR(inode->i_mode)) {
		inode->i_op = &linvfs_dir_inode_operations;
		inode->i_fop = &linvfs_dir_operations;
	} else if (S_ISLNK(inode->i_mode)) {
		inode->i_op = &linvfs_symlink_inode_operations;
		if (inode->i_blocks)
			inode->i_mapping->a_ops = &linvfs_aops;
	} else {
		inode->i_op = &linvfs_file_inode_operations;
		init_special_inode(inode, inode->i_mode, inode->i_rdev);
	}
}

void
linvfs_read_inode(
	struct inode	*inode)
{
	vfs_t		*vfsp = LINVFS_GET_VFS(inode->i_sb);

	if (vfsp) {
		vn_initialize(vfsp, inode, 1);
	} else {
		make_bad_inode(inode);
		return;
	}

	inode->i_version = ++event;
}



/*
 * The write method is only used to
 * trace interesting events in the life of a vnode.
 */

#ifdef	CONFIG_XFS_VNODE_TRACING

void
linvfs_write_inode(
	struct inode	*inode,
        int             sync)
{
	vnode_t	*vp = LINVFS_GET_VP(inode);

	if (vp) {
		vn_trace_entry(vp, "linvfs_write_inode",
					(inst_t *)__return_address);
	}
}
#endif	/* CONFIG_XFS_VNODE_TRACING */


void
linvfs_delete_inode(
	struct inode	*inode)
{
	vnode_t	*vp = LINVFS_GET_VP(inode);

	if (vp) {

		vn_trace_entry(vp, "linvfs_delete_inode",
					(inst_t *)__return_address);
		/*
		 * Remove the vnode, the nlink count
		 * is zero & the unlink will complete.
		 */
		vp->v_flag |= VPURGE;
		vn_remove(vp);

	} else {
printk("linvfs_delete_inode: NOVP!: inode/0x%p ino/%ld icnt/%d\n",
inode, inode->i_ino, atomic_read(&inode->i_count));
	BUG();
	}

	clear_inode(inode);
}


void
linvfs_clear_inode(
	struct inode	*inode)
{
	vnode_t		*vp = LINVFS_GET_VP(inode);

	if (vp) {

		vn_trace_entry(vp, "linvfs_clear_inode",
					(inst_t *)__return_address);
		/*
		 * Do all our cleanup, and remove
		 * this vnode.
		 */
		vp->v_flag |= VPURGE;
		vn_remove(vp);
	}
}

void 
linvfs_put_inode(
	struct inode	*inode)
{
	vnode_t		*vp = LINVFS_GET_VP(inode);

    	if (vp) vn_put(vp);
}

void
linvfs_put_super(
	struct super_block *sb)
{
	int		error;
	int		sector_size = 512;
	struct inode	*rootip;
	kdev_t		dev = sb->s_dev;
	vfs_t 		*vfsp = LINVFS_GET_VFS(sb);
	vnode_t		*rootvp, *cvp;

	/*
	 * Find the root vnode/inode, we can't
	 * use sb->s_root->d_inode 'cause it
	 * appears to be gone already?
	 */
	VFS_ROOT(vfsp, &rootvp, error);

	if (error) {
		printk("XFS unmount got error %d\n", error);
		printk("linvfs_put_super: vfsp/0x%p left dangling!\n", vfsp);
		return;
	}

	VN_RELE(rootvp);	/* Release the hold taken by VFS_ROOT */

	rootip = LINVFS_GET_IP(rootvp);

	VFS_DOUNMOUNT(vfsp, 0, NULL, sys_cred, error); 

	if (error) {
		printk("XFS unmount got error %d\n", error);
		printk("linvfs_put_super: vfsp/0x%p left dangling!\n", vfsp);
		return;
	}

	vfs_deallocate(vfsp);

	cvp = LINVFS_GET_CVP(sb);

#ifdef  CONFIG_XFS_VNODE_TRACING
	ktrace_free(cvp->v_trace);

	cvp->v_trace = NULL;
#endif  /* CONFIG_XFS_VNODE_TRACING */

	kfree(cvp->v_inode);

	/*  Do something to get rid of the VNODE/VFS layer here  */

	/* Reset device block size */
	if (hardsect_size[MAJOR(dev)])
		sector_size = hardsect_size[MAJOR(dev)][MINOR(dev)];

	set_blocksize(dev, sector_size);
}


void
linvfs_write_super(
	struct super_block *sb)
{
 	vfs_t		*vfsp = LINVFS_GET_VFS(sb); 
 	int		error; 

	if (sb->s_flags & MS_RDONLY) {
		return;
	}

	VFS_SYNC(vfsp, SYNC_FSDATA|SYNC_BDFLUSH|SYNC_NOWAIT|SYNC_ATTR,
		sys_cred, error);

	sb->s_dirt = 1;  /*  Keep the syncs coming.  */
}


int
linvfs_statfs(
	struct super_block *sb,
	struct statfs	*buf)
{
	vfs_t		*vfsp = LINVFS_GET_VFS(sb);
	vnode_t		*rootvp;
	statvfs_t	stat;

	int		error;
	
	VFS_ROOT(vfsp, &rootvp, error);
	if (error){
		return(-error);
	}

	VFS_STATVFS(vfsp, &stat, rootvp, error);

	VN_RELE(rootvp);

	if (error){
		return(-error);
	}


	buf->f_type = XFS_SB_MAGIC;
	buf->f_bsize = stat.f_bsize;
	buf->f_blocks = stat.f_blocks;
	buf->f_bfree = stat.f_bfree;
	buf->f_bavail = stat.f_bavail;
	buf->f_files = stat.f_files;
	buf->f_ffree = stat.f_ffree;
	buf->f_fsid.val[0] = stat.f_fsid;
	buf->f_fsid.val[1] = 0;
	buf->f_namelen = stat.f_namemax;

	return 0;
}


int
linvfs_remount(
	struct super_block *sb,
	int		*flags,
	char		*options)
{
	struct xfs_args args;
	vfs_t *vfsp;
	vnode_t *cvp;

	vfsp = LINVFS_GET_VFS(sb);
	cvp = LINVFS_GET_CVP(sb);

	if ((*flags & MS_RDONLY) == (sb->s_flags & MS_RDONLY))
		return 0;

	if (mountargs_xfs(options, &args) != 0)
                return -ENOSPC;

	if (*flags & MS_RDONLY || args.flags & MS_RDONLY) {
		int error;
		int save = sb->s_flags;
		xfs_mount_t *mp = XFS_BHVTOM(vfsp->vfs_fbhv);

		sb->s_flags |= MS_RDONLY;

		XFS_bflush(mp->m_ddev_targ);
		VFS_SYNC(vfsp, SYNC_ATTR|SYNC_WAIT|SYNC_CLOSE,
				  sys_cred, error);
		if (error) {
			sb->s_flags=save;
			return -error;
		}
		
		XFS_log_write_unmount_ro(vfsp->vfs_fbhv);
		vfsp->vfs_flag |= VFS_RDONLY;
	} else {
		vfsp->vfs_flag &= ~VFS_RDONLY;
		sb->s_flags &= ~MS_RDONLY;
	}

	return 0;
}


int
linvfs_dmapi_mount(
	struct super_block *sb,
	char		*dir_name)
{
	vfsops_t	*vfsops;
	extern vfsops_t xfs_vfsops;
	char		fsname[256];
	vnode_t		*cvp;	/* covered vnode */
	vfs_t		*vfsp; /* mounted vfs */
	int		error;

	vfsp = LINVFS_GET_VFS(sb);
	if ( ! (vfsp->vfs_flag & VFS_DMI) )
		return 0;
	cvp = LINVFS_GET_CVP(sb);
	sprintf(fsname, bdevname(sb->s_dev));

	/*  Kludge in XFS until we have other VFS/VNODE FSs  */
	vfsops = &xfs_vfsops;

	VFSOPS_DMAPI_MOUNT(vfsops, vfsp, cvp, dir_name, fsname, error);
	if (error) {
		vfsp->vfs_flag &= ~VFS_DMI;
		return -error;
	}
	return 0;
}


int
linvfs_quotactl(
	struct super_block *sb,
	int		cmd,
	int		type,
	int		id,
	caddr_t		addr)
{
	xfs_mount_t	*mp;
	vfs_t		*vfsp;
	int		error;

	if (!IS_XQM_CMD(cmd))
		return -EINVAL;

	if (type == USRQUOTA)
		type = XFS_DQ_USER;
	else if (type == GRPQUOTA)
		type = XFS_DQ_GROUP;
	else
		return -EINVAL;

	vfsp = LINVFS_GET_VFS(sb);
	mp = XFS_BHVTOM(vfsp->vfs_fbhv);
	ASSERT(mp);

	error = xfs_quotactl(mp, vfsp, cmd, id, type, addr);
	
	if (error)
		return -error;
		
	return 0;
}


static struct super_operations linvfs_sops = {
	read_inode:		linvfs_read_inode,
#ifdef	CONFIG_XFS_VNODE_TRACING
	write_inode:		linvfs_write_inode,
#endif
#ifdef CONFIG_XFS_DMAPI
	dmapi_mount_event:	linvfs_dmapi_mount,
#endif
#ifdef CONFIG_XFS_QUOTA
	quotactl:		linvfs_quotactl,
#endif
	put_inode:		linvfs_put_inode,
	delete_inode:		linvfs_delete_inode,
	clear_inode:		linvfs_clear_inode,
	put_super:		linvfs_put_super,
	write_super:		linvfs_write_super,
	statfs:			linvfs_statfs,
	remount_fs:		linvfs_remount
};

DECLARE_FSTYPE_DEV(xfs_fs_type, XFS_NAME, linvfs_read_super);

int __init init_xfs_fs(void)
{
	struct sysinfo	si;

	si_meminfo(&si);

	physmem = si.totalram;

	cred_init();

	vfsinit();
	xfs_init(0);

#ifdef CONFIG_XFS_GRIO
        xfs_grio_init();
#endif
	dmapi_init();
	return register_filesystem(&xfs_fs_type);
}


#ifdef MODULE

EXPORT_NO_SYMBOLS;

int init_module(void)
{
	printk(KERN_INFO 
		"XFS filesystem Copyright (c) 2000 Silicon Graphics, Inc.\n");
	return init_xfs_fs();
}

void cleanup_module(void)
{
        dmapi_uninit();
#ifdef CONFIG_XFS_GRIO
        xfs_grio_uninit();
#endif
	xfs_cleanup();
        unregister_filesystem(&xfs_fs_type);
}

#endif
