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

#define IRELE(ip)		VN_RELE(XFS_ITOV(ip))

extern void grioinit(void);

/*
 * xfs_get_file_extents()
 *	This routine creates the cononical forms of all the extents
 *	for the given file and returns them to the user.
 *
 *  RETURNS:
 *	0 on success
 *	non zero on failure
 */
int
xfs_get_file_extents(
	sysarg_t sysarg_file_id,
	sysarg_t sysarg_extents_addr,
	sysarg_t sysarg_count)
{
	int			i, recsize, num_extents = 0;
	int			error = 0;
	dev_t			fs_dev;
	xfs_ino_t		ino;
	xfs_inode_t 		*ip;
	xfs_bmbt_rec_t 		*ep;
	xfs_bmbt_irec_t 	thisrec;
	grio_bmbt_irec_t	*grec;
	grio_file_id_t		fileid;
	xfs_caddr_t			extents, count;

	if (copy_from_user(&fileid, SYSARG_TO_PTR(sysarg_file_id), sizeof(grio_file_id_t))) {
		error = -XFS_ERROR(EFAULT);
		return( error );
	}

	fs_dev 		= fileid.fs_dev;
	ino		= fileid.ino;

	/*
	 * Get sysarg arguements
	 */
	extents		= (xfs_caddr_t)SYSARG_TO_PTR(sysarg_extents_addr);
	count		= (xfs_caddr_t)SYSARG_TO_PTR(sysarg_count);

	/*
 	 * Get the inode
	 */
	if (!(ip = xfs_get_inode( fs_dev, ino ))) {
		error = -XFS_ERROR(ENOENT);
		if (copy_to_user((xfs_caddr_t)count, &num_extents, 
				sizeof( num_extents))) {

			error = -XFS_ERROR(EFAULT);
		}
		return( error );
	}

	/*
	 * Get the number of extents in the file.
	 */
	num_extents = ip->i_d.di_nextents;

	if (num_extents) {

		/*
		 * Copy the extents if they exist.
		 */
		ASSERT(num_extents <  XFS_MAX_INCORE_EXTENTS);

		/*
		 * Read in the file extents from disk if necessary.
		 */
		if (!(ip->i_df.if_flags & XFS_IFEXTENTS)) {
			error = xfs_iread_extents(NULL, ip, XFS_DATA_FORK);
			if (error) {
				goto out;
			}
		}

		recsize = sizeof(grio_bmbt_irec_t) * num_extents;
		grec = kmem_alloc(recsize, KM_SLEEP );

		ep = ip->i_df.if_u1.if_extents;

		ASSERT( ep );

		for (i = 0; i < num_extents; i++, ep++) {
			/*
 			 * copy extent numbers;
 			 */
			xfs_bmbt_get_all(ep, &thisrec);
			grec[i].br_startoff 	= thisrec.br_startoff;
			grec[i].br_startblock 	= thisrec.br_startblock;
			grec[i].br_blockcount 	= thisrec.br_blockcount;
		}

		if (copy_to_user((xfs_caddr_t)extents, grec, recsize )) {
			error = -XFS_ERROR(EFAULT);
		}
		kmem_free(grec, recsize);
	}

	/* 
	 * copy out to user space along with count.
 	 */
	if (copy_to_user((xfs_caddr_t)count,  &num_extents, sizeof( num_extents))) {
		error = -XFS_ERROR(EFAULT);
	}

 out:
	xfs_iunlock( ip, XFS_ILOCK_SHARED );
	IRELE( ip );
	return( error );
}

/*
 * xfs_get_file_rt()
 *	This routine determines if the given file has real time
 *	extents. If so a 1 is written to the user memory pointed at
 *	by rt, if not a 0 is written.
 *
 *
 * RETURNS:
 *	0 on success
 *	non zero on failure
 */
int
xfs_get_file_rt( 
	sysarg_t sysarg_file_id,
	sysarg_t sysarg_rt)
{
	int 		inodert = 0, error = 0;
	dev_t		fs_dev;
	xfs_ino_t	ino;
	xfs_inode_t 	*ip;
	xfs_caddr_t		rt;
	grio_file_id_t	fileid;


	if ( copy_from_user(&fileid, SYSARG_TO_PTR(sysarg_file_id), sizeof(grio_file_id_t))) {
		error = -XFS_ERROR(EFAULT);
		return( error );
	}

	rt		= (xfs_caddr_t)SYSARG_TO_PTR(sysarg_rt);
	fs_dev 		= fileid.fs_dev;
	ino		= fileid.ino;

	/*
 	 * Get the inode.
	 */
	if (!(ip = xfs_get_inode( fs_dev, ino ))) {
		return -XFS_ERROR( ENOENT );
	}

	/*
	 * Check if the inode is marked as real time.
	 */
	if (ip->i_d.di_flags & XFS_DIFLAG_REALTIME) {
		inodert = 1;
	}

	/*
 	 * Copy the results to user space.
 	 */
	if (copy_to_user((xfs_caddr_t)rt, &inodert, sizeof(int)) ) {
		error = -XFS_ERROR(EFAULT);
	}

	xfs_iunlock( ip, XFS_ILOCK_SHARED );
	IRELE( ip );
	return( error );

}


/*
 * xfs_get_block_size()
 *	This routine determines the block size of the given
 *	file system copies it to the user memory pointed at by fs_size.
 *
 * RETURNS:
 *	0 on success
 *	non zero on failure
 */
int
xfs_get_block_size(
	sysarg_t sysarg_fs_dev, 
	sysarg_t sysarg_fs_size)
{
	int 		error = 0;
	dev_t		fs_dev;
	xfs_caddr_t		fs_size;
	struct vfs	*vfsp;

	fs_dev 		= (dev_t)sysarg_fs_dev;
	fs_size		= (xfs_caddr_t)SYSARG_TO_PTR(sysarg_fs_size);

	if ( vfsp = vfs_devsearch( fs_dev, xfs_fstype ) ) {
		if (copy_to_user((xfs_caddr_t)fs_size, &(vfsp->vfs_bsize), 
				sizeof(u_int)) ) {

			error = -XFS_ERROR(EFAULT);
		}
	} else {
		error = -XFS_ERROR( EIO );
	}
	return( error );
}



/*
 * xfs_grio_get_inumber
 *	Convert a users file descriptor to an inode number.
 *
 * RETURNS:
 *	64 bit inode number
 *
 * CAVEAT:
 *	this must be called from context of the user process
 */
xfs_ino_t 
xfs_grio_get_inumber( int fdes )
{
	xfs_ino_t   ino=-1;
        struct file *filp;
        
        filp = fget(fdes);
        if (filp) {
                struct dentry *dentry = filp->f_dentry;
                struct inode *inode = dentry->d_inode;
            
                ino=inode->i_ino;
                fput(filp);    
        }
        printk("xfs_grio_get_inumber fd %d -> ino %Ld\n",
                fdes, ino);
        
        return ino;

}


/*
 * xfs_grio_get_fs_dev
 *	Convert a users file descriptor to a file system device.
 *
 * RETURNS:
 *	the dev_t of the file system where the file resides
 *
 * CAVEAT:
 *	this must be called from the context of the user process
 */
dev_t 
xfs_grio_get_fs_dev( int fdes )
{
	dev_t		dev=0;
        struct file     *filp;

        filp = fget(fdes);
        if (filp) {
                struct dentry *dentry = filp->f_dentry;
                struct inode *inode = dentry->d_inode;
            
                dev=inode->i_dev;
                
                fput(filp);    
        }
        printk("xfs_grio_get_fs_dev fd %d -> dev 0x%x (%d)\n",
                fdes, dev, dev);
                
	return( dev );
}

/* stubbed out - real version is in grio module */
int grio_strategy(xfs_buf_t *bp) 
{
	return pagebuf_iorequest(bp);
}

/* make XFS GRIO syscall available thru /dev/grio ioctl */

int xfs_grio_ioctl (
	struct inode	*inode,
	struct file	*filp,
	unsigned int	cmd,
	unsigned long	arg)
{
        
        if (cmd!=XFS_GRIO_CMD) {
            printk("xfs_grio_ioctl command %d (expect %d)\n", 
                    cmd, XFS_GRIO_CMD);
            return -EINVAL;
        }
        
        /* XXX PORT do we need to check for FORCED_SHUTDOWN here? */

	switch (cmd) {

	case XFS_GRIO_CMD: {
		xfs_grio_ioctl_t   grioio;

		if (copy_from_user(&grioio, (xfs_grio_ioctl_t *)arg,
						sizeof(xfs_grio_ioctl_t)))
			return -XFS_ERROR(EFAULT);
                
                return grio_config(grioio.cmd,
                                        grioio.arg1, grioio.arg2,
                                        grioio.arg3, grioio.arg4);
	}

	default:
		return -EINVAL;
	}
}

static atomic_t xfs_grio_isopen = ATOMIC_INIT(0);

static int xfs_grio_open(struct inode *inode, struct file *file)
{
        MOD_INC_USE_COUNT;
        return 0;
}

static int xfs_grio_release(struct inode *inode, struct file *file)
{
        MOD_DEC_USE_COUNT;
        return 0;
}

static struct file_operations xfs_grio_fops = {
        ioctl:          xfs_grio_ioctl,
        open:           xfs_grio_open,
        release:        xfs_grio_release,
};

static struct miscdevice xfs_grio_dev=
{
        XFS_GRIO_MINOR,
        "xfs_grio",
        &xfs_grio_fops
};

void xfs_grio_init(void)
{
        printk("xfs_grio_init\n");
        printk("sizeof(grio_cmd_t)=%d\n", sizeof(grio_cmd_t));
        printk("%d %d %d %d %d\n", 
            offsetof(grio_cmd_t,gr_fp),
            offsetof(grio_cmd_t,other_end),
            offsetof(grio_cmd_t,memloc),
            offsetof(grio_cmd_t,cmd_info),
            offsetof(grio_cmd_t,gr_usecs_bw));
        printk("%d %d %d %d\n", 
            offsetof(struct end_info,gr_end_type),
            offsetof(struct end_info,gr_dev),
            offsetof(struct end_info,gr_ino),
            sizeof(struct end_info));
        grioinit();
        misc_register(&xfs_grio_dev);
}

void xfs_grio_uninit(void)
{
        printk("xfs_grio_uninit\n");
        misc_deregister(&xfs_grio_dev);
}

