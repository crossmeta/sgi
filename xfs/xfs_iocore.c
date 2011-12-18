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


/* ARGSUSED */
static int
xfs_rsync_fn(
	xfs_inode_t	*ip,
	int		ioflag,
	xfs_off_t		start,
	xfs_off_t		end)
{
	xfs_mount_t	*mp = ip->i_mount;
	int		error = 0;

	if (ioflag & IO_SYNC) {
		xfs_ilock(ip, XFS_ILOCK_SHARED);
		xfs_iflock(ip);
		error = xfs_iflush(ip, XFS_IFLUSH_SYNC);
		xfs_iunlock(ip, XFS_ILOCK_SHARED);
		return error;
	} else {
		if (ioflag & IO_DSYNC) {
			xfs_log_force(mp, (xfs_lsn_t)0,
					XFS_LOG_FORCE | XFS_LOG_SYNC );
		}
	}

	return error;
}


static xfs_fsize_t
xfs_size_fn(
	xfs_inode_t	*ip)
{
	return (ip->i_d.di_size);
}

static xfs_fsize_t
xfs_setsize_fn(
	xfs_inode_t	*ip,
	xfs_fsize_t	newsize)
{
	xfs_fsize_t	isize;

	xfs_ilock(ip, XFS_ILOCK_EXCL);
	if (newsize  > ip->i_d.di_size) {
		ip->i_d.di_size = newsize;
		ip->i_update_core = 1;
		ip->i_update_size = 1;
		isize = newsize;
	} else {
		isize = ip->i_d.di_size;
	}
	xfs_iunlock(ip, XFS_ILOCK_EXCL);

	return isize;
}


xfs_ioops_t	xfs_iocore_xfs = {
	(xfs_dio_write_t)xfs_dio_write,
	(xfs_dio_read_t)xfs_dio_read,
	(xfs_strat_write_t) xfs_strat_write,
	(xfs_bmapi_t) xfs_bmapi,
	(xfs_bmap_eof_t) xfs_bmap_eof,
	(xfs_rsync_t) xfs_rsync_fn,
	(xfs_lck_map_shared_t) xfs_ilock_map_shared,
	(xfs_lock_t) xfs_ilock,
	(xfs_lock_demote_t) xfs_ilock_demote,
	(xfs_lock_nowait_t) xfs_ilock_nowait,
	(xfs_unlk_t) xfs_iunlock,
	(xfs_chgtime_t) xfs_ichgtime,
	(xfs_size_t) xfs_size_fn,
	(xfs_setsize_t) xfs_setsize_fn,
	(xfs_lastbyte_t) xfs_file_last_byte,
#ifdef CELL_CAPABLE
        (xfs_checklock_t) fs_nosys, /* (xfs_checklock_t) xfs_checklock */
#endif
};

void
xfs_iocore_inode_reinit(
	xfs_inode_t	*ip)
{
	xfs_iocore_t	*io = &ip->i_iocore;

	io->io_flags = XFS_IOCORE_ISXFS;
	if (ip->i_d.di_flags & XFS_DIFLAG_REALTIME) {
		io->io_flags |= XFS_IOCORE_RT;
	}

	io->io_dmevmask = ip->i_d.di_dmevmask;
	io->io_dmstate = ip->i_d.di_dmstate;
}

void
xfs_iocore_inode_init(
	xfs_inode_t	*ip)
{
	xfs_iocore_t	*io = &ip->i_iocore;
	xfs_mount_t	*mp = ip->i_mount;

	io->io_mount = mp;
	io->io_lock = &ip->i_lock;
	io->io_iolock = &ip->i_iolock;
	mutex_init(&io->io_rlock, MUTEX_DEFAULT, "xfs_rlock");

	xfs_iocore_reset(io);

	io->io_obj = (void *)ip;

	xfs_iocore_inode_reinit(ip);
}

void
xfs_iocore_reset(
	xfs_iocore_t	*io)
{
	xfs_mount_t	*mp = io->io_mount;

	/*
	 * initialize read/write io sizes
	 */
	ASSERT(mp->m_readio_log <= 0xff);
	ASSERT(mp->m_writeio_log <= 0xff);

	io->io_readio_log = (uchar_t) mp->m_readio_log;
	io->io_writeio_log = (uchar_t) mp->m_writeio_log;
	io->io_max_io_log = (uchar_t) mp->m_writeio_log;
	io->io_readio_blocks = mp->m_readio_blocks;
	io->io_writeio_blocks = mp->m_writeio_blocks;
}

void
xfs_iocore_destroy(
	xfs_iocore_t	*io)
{
	mutex_destroy(&io->io_rlock);
}

